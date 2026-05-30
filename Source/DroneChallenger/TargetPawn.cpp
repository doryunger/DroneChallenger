#include "TargetPawn.h"
#include "TargetAIController.h"
#include "PatrolPath.h"
#include "Windows/WindowsHWrapper.h"
#ifdef OPAQUE
#undef OPAQUE
#endif
#ifdef TRANSPARENT
#undef TRANSPARENT
#endif
#include "CesiumGlobeAnchorComponent.h"
#include "CesiumGeoreference.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Materials/Material.h"
#include "Components/SplineComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "DroneActor.h"

THIRD_PARTY_INCLUDES_START
#include "bt/BehaviorTree.h"
#include "bt/DecisionEmitter.h"
#include "bt/MonitorServer.h"
#include "bt/SchemaLoader.h"
#include "bt/Status.h"
THIRD_PARTY_INCLUDES_END

ATargetPawn::ATargetPawn()
{
	PrimaryActorTick.bCanEverTick = true;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetRelativeScale3D(FVector(35.0f, 35.0f, 35.0f));

	{
		static ConstructorHelpers::FObjectFinder<UStaticMesh> MeshFinder(TEXT("/Game/PS1_Style_Hatchback_Car/meshes/SM_hatchback_car_car"));
		if (MeshFinder.Succeeded())
			Mesh->SetStaticMesh(MeshFinder.Object);
	}

	{
		static ConstructorHelpers::FObjectFinder<UMaterialInstance> MatFinder(TEXT("/Game/PS1_Style_Hatchback_Car/material_instances/MI_car_red"));
		if (MatFinder.Succeeded())
			Mesh->SetMaterial(0, MatFinder.Object);
	}

	auto MakeWheel = [&](const TCHAR* Name, const TCHAR* AssetPath) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* W = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		W->SetupAttachment(Mesh);
		W->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		static ConstructorHelpers::FObjectFinder<UStaticMesh> Finder(AssetPath);
		if (Finder.Succeeded()) W->SetStaticMesh(Finder.Object);
		return W;
	};

	Wheel1 = MakeWheel(TEXT("Wheel1"), TEXT("/Game/PS1_Style_Hatchback_Car/meshes/SM_hatchback_car_wheel1"));
	Wheel2 = MakeWheel(TEXT("Wheel2"), TEXT("/Game/PS1_Style_Hatchback_Car/meshes/SM_hatchback_car_wheel2"));
	Wheel3 = MakeWheel(TEXT("Wheel3"), TEXT("/Game/PS1_Style_Hatchback_Car/meshes/SM_hatchback_car_wheel3"));
	Wheel4 = MakeWheel(TEXT("Wheel4"), TEXT("/Game/PS1_Style_Hatchback_Car/meshes/SM_hatchback_car_wheel4"));

	Beacon = CreateDefaultSubobject<UPointLightComponent>(TEXT("Beacon"));
	Beacon->SetupAttachment(Mesh);
	Beacon->SetIntensity(0.0f);
	Beacon->SetVisibility(false);

	BeaconPole = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BeaconPole"));
	BeaconPole->SetupAttachment(Mesh);
	BeaconPole->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BeaconPole->SetAbsolute(false, false, true);
	BeaconPole->SetWorldScale3D(FVector(0.5f, 0.5f, 100.0f));
	BeaconPole->SetVisibility(false);
	{
		static ConstructorHelpers::FObjectFinder<UStaticMesh> CylFinder(TEXT("/Engine/BasicShapes/Cylinder"));
		if (CylFinder.Succeeded()) BeaconPole->SetStaticMesh(CylFinder.Object);
	}
	{
		static ConstructorHelpers::FObjectFinder<UMaterial> MatFinder(TEXT("/Game/M_BeaconGlow"));
		if (MatFinder.Succeeded()) BeaconPole->SetMaterial(0, MatFinder.Object);
	}

	BeaconBeamLight = CreateDefaultSubobject<USpotLightComponent>(TEXT("BeaconBeamLight"));
	BeaconBeamLight->SetupAttachment(Mesh);
	BeaconBeamLight->SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f));
	BeaconBeamLight->SetIntensity(1e8f);
	BeaconBeamLight->SetAttenuationRadius(200000.0f);
	BeaconBeamLight->SetInnerConeAngle(1.0f);
	BeaconBeamLight->SetOuterConeAngle(4.0f);
	BeaconBeamLight->SetLightColor(FLinearColor(1.0f, 0.0f, 0.0f));
	BeaconBeamLight->SetVisibility(false);

	GlobeAnchor = CreateDefaultSubobject<UCesiumGlobeAnchorComponent>(TEXT("GlobeAnchor"));

	AIControllerClass = ATargetAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
}

ATargetPawn::~ATargetPawn()
{
	if (Monitor) { Monitor->stop(); delete Monitor; Monitor = nullptr; }
	delete Tree;    Tree    = nullptr;
	delete Emitter; Emitter = nullptr;
}

void ATargetPawn::BeginPlay()
{
	Super::BeginPlay();

	CachedDrone = Cast<ADroneActor>(UGameplayStatics::GetActorOfClass(GetWorld(), ADroneActor::StaticClass()));
	CachedGeoreference = ACesiumGeoreference::GetDefaultGeoreference(this);

	Mesh->SetRelativeScale3D(FVector(1.5f, 1.5f, 1.5f));

	// Position beacon pole so it spans 0–100 m above the car.
	// bAbsoluteScale is true so pole scale ignores parent, but position is still
	// in parent-local space — divide by mesh scale to get the right world offset.
	static constexpr float RuntimeMeshScale = 1.5f;
	static constexpr float BeaconCenterM    = 50.0f;   // midpoint of 100 m pole
	BeaconPole->SetRelativeLocation(FVector(0.0f, 0.0f, BeaconCenterM * 100.0f / RuntimeMeshScale));

	// Record the drone's editor-placed position. Tiles are guaranteed loaded here because
	// the camera starts at this location. The drone stays here while placement runs —
	// no aerial waiting state for the player.
	DroneEditorPos = IsValid(CachedDrone)
		? CachedDrone->GetActorLocation()
		: GetActorLocation();

	{
		ACesiumGeoreference* Georef = CachedGeoreference;
		const FString NodesPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir() / TEXT("Graph/nodes.csv"));
		const FString EdgesPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir() / TEXT("Graph/edges.csv"));

		if (Georef && Graph.Load(NodesPath, EdgesPath, Georef, GetWorld()) && !Graph.NodeIds.IsEmpty())
		{
			UE_LOG(LogTemp, Log, TEXT("TargetPawn: graph loaded %d nodes — starting placement search (radius %.0f cm)"),
				Graph.NodeIds.Num(), PlacementRadius);

			GetWorldTimerManager().SetTimer(
				PlacementTimer, this, &ATargetPawn::TryInitialPlacement, 1.0f, true);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("TargetPawn: graph failed to load — no placement possible"));
		}
	}

	UE_LOG(LogTemp, Log, TEXT("TargetPawn: mesh loaded = %s"),
		(Mesh && Mesh->GetStaticMesh()) ? TEXT("yes") : TEXT("NO — car will be invisible"));

	BuildTree();
}

void ATargetPawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	PositionLogTimer += DeltaTime;
	if (PositionLogTimer >= 10.0f)
	{
		PositionLogTimer = 0.0f;
		const FVector CarPos = GetActorLocation();

		FVector LonLatH(0, 0, 0);
		if (CachedGeoreference)
			LonLatH = CachedGeoreference->TransformUnrealPositionToLongitudeLatitudeHeight(CarPos);

		float DistToDrone = -1.0f;
		if (CachedDrone)
			DistToDrone = FVector::Distance(CarPos, CachedDrone->GetActorLocation()) / 100.0f;

		UE_LOG(LogTemp, Log, TEXT("TargetPawn: UE_Z=%.0f | lon=%.6f lat=%.6f alt=%.0fm | drone dist=%.0fm"),
			CarPos.Z, LonLatH.X, LonLatH.Y, LonLatH.Z, DistToDrone);
	}

	if (!bPlacementDone || !Tree) return;

	LastDeltaTime = DeltaTime;
	UpdateDroneState();
	if (bDroneInFOV) {
		CurrentTrackingTime += DeltaTime;
		BestTrackingTime = FMath::Max(BestTrackingTime, CurrentTrackingTime);
	} else if (bWasInFOV) {
		CurrentTrackingTime = 0.0f;
	}
	bWasInFOV = bDroneInFOV;
	Tree->tick();
}

void ATargetPawn::BuildTree()
{
	FString FilePath = FPaths::ProjectContentDir() / TEXT("BT/target.yaml");
	FString YamlContent;

	if (!FFileHelper::LoadFileToString(YamlContent, *FilePath))
	{
		UE_LOG(LogTemp, Error, TEXT("TargetPawn: failed to load %s"), *FilePath);
		return;
	}

	bt::LoaderRegistry Reg;

	Reg.actions["stop"] = [this]() -> bt::Status
	{
		CurrentSpeed = 0.0f;
		return bt::Status::SUCCESS;
	};

	Reg.actions["set_speed_normal"] = [this]() -> bt::Status
	{
		CurrentSpeed = PatrolSpeed;
		return bt::Status::SUCCESS;
	};

	Reg.actions["set_speed_fast"] = [this]() -> bt::Status
	{
		CurrentSpeed = EvadeSpeed;
		return bt::Status::SUCCESS;
	};

	Reg.actions["advance_along_path"] = [this]() -> bt::Status
	{
		AdvanceAlongPath();
		return bt::Status::SUCCESS;
	};

	Reg.actions["pulse_beacon_slow"] = [this]() -> bt::Status
	{
		float T = GetWorld()->GetTimeSeconds();
		float Intensity = (FMath::Sin(T * PI) + 1.0f) * 2500.0f;
		Beacon->SetVisibility(true);
		Beacon->SetLightColor(FLinearColor::Yellow);
		Beacon->SetIntensity(Intensity);
		return bt::Status::SUCCESS;
	};

	Reg.actions["pulse_beacon_fast"] = [this]() -> bt::Status
	{
		float T = GetWorld()->GetTimeSeconds();
		float Intensity = (FMath::Sin(T * 4.0f * PI) + 1.0f) * 4000.0f;
		Beacon->SetVisibility(true);
		Beacon->SetLightColor(FLinearColor::Red);
		Beacon->SetIntensity(Intensity);
		return bt::Status::SUCCESS;
	};

	Reg.actions["deactivate_beacon"] = [this]() -> bt::Status
	{
		Beacon->SetVisibility(false);
		return bt::Status::SUCCESS;
	};

	Reg.actions["increment_capture_timer"] = [this]() -> bt::Status
	{
		CaptureTimer += LastDeltaTime;
		if (CaptureTimer >= CaptureRequiredTime && !bIsCaptured)
		{
			bIsCaptured = true;
			OnCaptured.Broadcast(this);
		}
		return bt::Status::SUCCESS;
	};

	Reg.actions["plan_evade_path"] = [this]() -> bt::Status
	{
		if (bOnEvadePath && PathNodeIndex + 1 < CurrentPath.Num())
			return bt::Status::SUCCESS;

		if (Graph.IsEmpty() || !CachedDrone) return bt::Status::SUCCESS;

		const FVector CarPos   = GetActorLocation();
		const FVector DronePos = CachedDrone->GetActorLocation();
		const FVector AwayDir  = FVector(CarPos.X - DronePos.X, CarPos.Y - DronePos.Y, 0.0f).GetSafeNormal();

		const int32 NearestNode = Graph.FindNearestNode(CarPos);
		if (NearestNode == INDEX_NONE) return bt::Status::SUCCESS;

		const int32 PrevNode = (bOnEvadePath && CurrentPath.Num() > 1 && PathNodeIndex > 0)
			? CurrentPath[PathNodeIndex - 1] : INDEX_NONE;

		const TArray<int32>* Neighbours = Graph.Adjacency.Find(NearestNode);
		int32 EvadeStart = NearestNode;
		if (Neighbours && !Neighbours->IsEmpty())
		{
			float BestDot = -2.0f;
			for (int32 N : *Neighbours)
			{
				if (N == PrevNode) continue;
				if (const FVector* NPos = Graph.NodeWorldPos.Find(N))
				{
					const FVector ToN = FVector(NPos->X - CarPos.X, NPos->Y - CarPos.Y, 0.0f).GetSafeNormal();
					const float Dot = FVector::DotProduct(ToN, AwayDir);
					if (Dot > BestDot) { BestDot = Dot; EvadeStart = N; }
				}
			}
		}

		CurrentPath   = Graph.GeneratePath(EvadeStart, 50000.0f);
		PathNodeIndex = 0;
		bOnEvadePath  = true;
		UE_LOG(LogTemp, Log, TEXT("TargetPawn: evade segment planned — %d waypoints from node %d"),
			CurrentPath.Num(), EvadeStart);
		return bt::Status::SUCCESS;
	};

	Reg.actions["manage_patrol_path"] = [this]() -> bt::Status
	{
		bOnEvadePath = false;
		if (CurrentPath.IsEmpty() || PathNodeIndex + 1 >= CurrentPath.Num())
		{
			const int32 From     = CurrentPath.IsEmpty()
				? Graph.FindNearestNode(GetActorLocation())
				: CurrentPath.Last();
			const int32 PrevFrom = CurrentPath.Num() >= 2
				? CurrentPath[CurrentPath.Num() - 2]
				: INDEX_NONE;
			CurrentPath   = Graph.GeneratePath(From, 150000.0f, PrevFrom);
			PathNodeIndex = 0;

			if (CurrentPath.Num() >= 2)
			{
				const FVector* P0 = Graph.NodeWorldPos.Find(CurrentPath[0]);
				const FVector* P1 = Graph.NodeWorldPos.Find(CurrentPath[1]);
				if (P0 && P1)
				{
					const FVector Dir = FVector(P1->X - P0->X, P1->Y - P0->Y, 0.0f).GetSafeNormal();
					if (!Dir.IsNearlyZero())
					{
						FRotator FaceDir = Dir.Rotation();
						FaceDir.Yaw += 90.0f;
						SetActorRotation(FaceDir);
					}
				}
			}

			UE_LOG(LogTemp, Log, TEXT("TargetPawn: patrol path regenerated — %d waypoints from node %d"),
				CurrentPath.Num(), From);
		}
		return bt::Status::SUCCESS;
	};

	Reg.conditions["is_captured"] = [this]() { return bIsCaptured; };
	Reg.conditions["drone_in_fov"] = [this]() { return bDroneInFOV; };
	Reg.conditions["drone_in_capture_range"] = [this]() { return bDroneInCaptureRange; };

	std::string YamlStr(TCHAR_TO_UTF8(*YamlContent));
	bt::BehaviorTree Loaded = bt::SchemaLoader::load(YamlStr, Reg);
	Tree = new bt::BehaviorTree(std::move(Loaded));

	delete Emitter;
	Emitter = new bt::DecisionEmitter(32);
	Tree->setEmitter(Emitter);

	if (Monitor) { Monitor->stop(); delete Monitor; }
	const std::string UiDir = TCHAR_TO_UTF8(*(FPaths::ProjectDir() / TEXT("ArboristUI")));
	Monitor = new bt::MonitorServer(*Tree, *Emitter, UiDir);
	Monitor->start(8080);
}

void ATargetPawn::UpdateDroneState()
{
	if (bIsCaptured) return;

	bDroneInFOV = ComputeDroneInFOV();

	if (IsValid(CachedDrone))
	{
		const float Dist = FVector::Distance(GetActorLocation(), CachedDrone->GetActorLocation());
		bDroneInCaptureRange = (Dist <= CaptureRadius);

		FHitResult LOSHit;
		FCollisionQueryParams LOSParams(NAME_None, false, this);
		LOSParams.AddIgnoredActor(CachedDrone);
		const bool bBlocked = GetWorld()->LineTraceSingleByChannel(
			LOSHit,
			GetActorLocation() + FVector(0.0f, 0.0f, 200.0f),
			CachedDrone->GetActorLocation(),
			ECC_Visibility, LOSParams);
		bDroneHasLOS = !bBlocked;
	}
	else
	{
		bDroneInCaptureRange = false;
		bDroneHasLOS         = false;
	}

	if (!bDroneInCaptureRange)
		CaptureTimer = 0.0f;

	const bool bShowBeacon = !bDroneHasLOS;
	if (BeaconPole->IsVisible() != bShowBeacon)
	{
		BeaconPole->SetVisibility(bShowBeacon);
		BeaconBeamLight->SetVisibility(bShowBeacon);
	}
}

bool ATargetPawn::ComputeDroneInFOV() const
{
	if (!CachedDrone) return false;

	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC || PC->GetPawn() != CachedDrone) return false;

	float Dist = FVector::Distance(GetActorLocation(), CachedDrone->GetActorLocation());
	if (Dist > DetectionRange) return false;

	FVector CamDir = PC->GetControlRotation().Vector();
	FVector ToTarget = (GetActorLocation() - CachedDrone->GetActorLocation()).GetSafeNormal();

	return FVector::DotProduct(CamDir, ToTarget) >= 0.707f;
}

void ATargetPawn::PlaceDroneNearCar(const FVector& CarPos, const FVector& CarForward)
{
	if (!IsValid(CachedDrone)) return;

	FCollisionQueryParams IgnoreActors;
	IgnoreActors.AddIgnoredActor(this);
	IgnoreActors.AddIgnoredActor(CachedDrone);

	static constexpr float Heights[]   = {  500.0f, 1000.0f, 2000.0f, 3000.0f };
	static constexpr float Distances[] = { 2000.0f, 1500.0f, 1000.0f,  500.0f, 0.0f };

	FVector Behind(CarPos.X, CarPos.Y, CarPos.Z + 3000.0f);
	for (float Height : Heights)
	{
		bool bFound = false;
		for (float Dist : Distances)
		{
			const FVector Candidate(
				CarPos.X - CarForward.X * Dist,
				CarPos.Y - CarForward.Y * Dist,
				CarPos.Z + Height);

			if (!GetWorld()->OverlapAnyTestByChannel(
				Candidate, FQuat::Identity, ECC_WorldStatic,
				FCollisionShape::MakeSphere(400.0f), IgnoreActors))
			{
				Behind = Candidate;
				bFound = true;
				break;
			}
		}
		if (bFound) break;
	}

	CachedDrone->SetActorLocation(Behind, false, nullptr, ETeleportType::TeleportPhysics);

	const FRotator FaceRotator(0.0f, CarForward.Rotation().Yaw, 0.0f);
	CachedDrone->SetActorRotation(FaceRotator, ETeleportType::TeleportPhysics);
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
		PC->SetControlRotation(FaceRotator);

	UE_LOG(LogTemp, Log, TEXT("TargetPawn: drone placed at (%.0f, %.0f, %.0f)"),
		Behind.X, Behind.Y, Behind.Z);
}

void ATargetPawn::TryInitialPlacement()
{
	UWorld* World = GetWorld();
	if (!World) return;

	// Collect candidates within the current search radius, shuffled for variety.
	// The drone stays at its editor position throughout — no aerial waiting view.
	// Tiles are loaded here, so traces succeed immediately for nearby nodes.
	TArray<TPair<float, int32>> Candidates;
	for (int32 NodeId : Graph.NodeIds)
	{
		const FVector* P = Graph.NodeWorldPos.Find(NodeId);
		if (!P) continue;
		const float Dist2D = FVector2D::Distance(
			FVector2D(DroneEditorPos.X, DroneEditorPos.Y),
			FVector2D(P->X, P->Y));
		if (Dist2D <= PlacementRadius)
			Candidates.Add({ Dist2D, NodeId });
	}

	for (int32 i = Candidates.Num() - 1; i > 0; --i)
		Candidates.Swap(i, FMath::RandRange(0, i));

	if (Candidates.IsEmpty())
	{
		PlacementRadius += 10000.0f;
		UE_LOG(LogTemp, Warning, TEXT("TargetPawn: no nodes within radius — expanding to %.0f cm"), PlacementRadius);
		return;
	}

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);
	if (IsValid(CachedDrone)) Params.AddIgnoredActor(CachedDrone);

	static constexpr int32 MaxTries    = 20;
	static constexpr float TraceHalf   = 5000.0f;

	const int32 NumToTry = FMath::Min(MaxTries, Candidates.Num());
	for (int32 i = 0; i < NumToTry; ++i)
	{
		const int32   NodeId  = Candidates[i].Value;
		const FVector* NodePos = Graph.NodeWorldPos.Find(NodeId);
		if (!NodePos) continue;

		FHitResult Hit;
		if (!World->LineTraceSingleByChannel(Hit,
			FVector(NodePos->X, NodePos->Y, DroneEditorPos.Z + TraceHalf),
			FVector(NodePos->X, NodePos->Y, DroneEditorPos.Z - TraceHalf),
			ECC_WorldStatic, Params))
			continue;

		const FVector CarPos(NodePos->X, NodePos->Y, Hit.ImpactPoint.Z + 50.0f);
		SetActorLocation(CarPos);

		CurrentPath   = Graph.GeneratePath(NodeId, 150000.0f);
		PathNodeIndex = 0;

		FVector CarForward = FVector::ForwardVector;
		if (CurrentPath.Num() >= 2)
		{
			if (const FVector* NextPos = Graph.NodeWorldPos.Find(CurrentPath[1]))
				CarForward = FVector(NextPos->X - CarPos.X, NextPos->Y - CarPos.Y, 0.0f).GetSafeNormal();
		}

		PlaceDroneNearCar(CarPos, CarForward);

		AltitudeHistory.Empty();
		AltitudeHistory.Add(CarPos.Z);

		World->GetTimerManager().ClearTimer(PlacementTimer);
		World->GetTimerManager().SetTimer(
			TerrainSnapTimer, this, &ATargetPawn::PeriodicTerrainSnap, 60.0f, true);
		bPlacementDone = true;

		UE_LOG(LogTemp, Log,
			TEXT("TargetPawn: placed at node %d — car Z=%.0f, path=%d waypoints, radius=%.0f cm"),
			NodeId, CarPos.Z, CurrentPath.Num(), PlacementRadius);
		return;
	}

	PlacementRadius += 5000.0f;
	UE_LOG(LogTemp, Log,
		TEXT("TargetPawn: %d candidates all missed — tiles still loading, radius now %.0f cm"),
		NumToTry, PlacementRadius);
}

bool ATargetPawn::ShouldAcceptAltitude(float CandidateZ)
{
	if (AltitudeHistory.IsEmpty())
	{
		AltitudeHistory.Add(CandidateZ);
		return true;
	}

	const float BaselineZ = AltitudeHistory.Last();
	const float Delta     = CandidateZ - BaselineZ;

	if (FMath::Abs(Delta) <= AltSpikeThreshold)
	{
		AltitudeHistory.Add(CandidateZ);
		if (AltitudeHistory.Num() > AltHistorySize)
			AltitudeHistory.RemoveAt(0);
		return true;
	}

	// Sharp change — check if the whole recent history agrees with this trend.
	// If history is short, defer: store the candidate but don't move yet.
	if (AltitudeHistory.Num() < AltHistorySize)
	{
		AltitudeHistory.Add(CandidateZ);
		return false;
	}

	// Sustained trend: every sample in history moved in the same direction.
	const bool bAllUp   = AltitudeHistory.Last() > AltitudeHistory[0] + AltSpikeThreshold;
	const bool bAllDown = AltitudeHistory.Last() < AltitudeHistory[0] - AltSpikeThreshold;

	if (bAllUp || bAllDown)
	{
		AltitudeHistory.Empty();
		AltitudeHistory.Add(CandidateZ);
		return true;
	}

	// Isolated spike — reject, keep history stable.
	return false;
}

void ATargetPawn::PeriodicTerrainSnap()
{
	UWorld* World = GetWorld();
	if (!World) return;

	const FVector CarPos = GetActorLocation();

	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);

	if (!World->LineTraceSingleByChannel(Hit,
		FVector(CarPos.X, CarPos.Y, CarPos.Z + 2000.0f),
		FVector(CarPos.X, CarPos.Y, CarPos.Z - 5000.0f),
		ECC_WorldStatic, Params))
		return;

	if (Hit.ImpactPoint.Z >= CarPos.Z + 500.0f)
		return;

	if (ShouldAcceptAltitude(Hit.ImpactPoint.Z + 50.0f))
		SetActorLocation(FVector(CarPos.X, CarPos.Y, Hit.ImpactPoint.Z + 50.0f));
}

void ATargetPawn::AdvanceAlongPath()
{
	if (!Graph.IsEmpty())
	{
		AdvanceAlongGraph();
		return;
	}

	if (!PatrolPath || CurrentSpeed <= 0.0f) return;

	USplineComponent* Spline = PatrolPath->GetSpline();
	float SplineLength = Spline->GetSplineLength();
	if (SplineLength <= 0.0f) return;

	SplineDistance += CurrentSpeed * LastDeltaTime;
	SplineDistance = FMath::Fmod(SplineDistance, SplineLength);

	FVector NewLocation = Spline->GetLocationAtDistanceAlongSpline(SplineDistance, ESplineCoordinateSpace::World);
	FVector Tangent = Spline->GetTangentAtDistanceAlongSpline(SplineDistance, ESplineCoordinateSpace::World).GetSafeNormal();

	FHitResult Hit;
	FCollisionQueryParams SplineParams;
	SplineParams.AddIgnoredActor(this);
	FVector TraceStart = NewLocation + FVector(0.0f, 0.0f, 500.0f);
	FVector TraceEnd   = NewLocation - FVector(0.0f, 0.0f, 2000.0f);
	if (GetWorld()->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_WorldStatic, SplineParams))
		NewLocation.Z = Hit.ImpactPoint.Z + 50.0f;

	SetActorLocation(NewLocation);
	if (!Tangent.IsNearlyZero())
		SetActorRotation(Tangent.Rotation());
}


void ATargetPawn::AdvanceAlongGraph()
{
	if (CurrentPath.IsEmpty() || CurrentSpeed <= 0.0f) return;

	const int32 WaypointIndex = PathNodeIndex + 1;

	if (WaypointIndex >= CurrentPath.Num())
		return;

	const FVector* WaypointPtr = Graph.NodeWorldPos.Find(CurrentPath[WaypointIndex]);
	if (!WaypointPtr) return;

	const FVector CurrentPos = GetActorLocation();
	const float   Dist2D     = FMath::Sqrt(
		FMath::Square(WaypointPtr->X - CurrentPos.X) +
		FMath::Square(WaypointPtr->Y - CurrentPos.Y));

	if (Dist2D < 500.0f)
	{
		FHitResult Hit;
		FCollisionQueryParams Params;
		Params.AddIgnoredActor(this);
		const FVector WP = *WaypointPtr;
		if (GetWorld()->LineTraceSingleByChannel(Hit,
			FVector(WP.X, WP.Y, CurrentPos.Z + 2000.0f),
			FVector(WP.X, WP.Y, CurrentPos.Z - 5000.0f),
			ECC_WorldStatic, Params))
		{
			const float NewZ = Hit.ImpactPoint.Z + 50.0f;
			if (FVector* NodePos = Graph.NodeWorldPos.Find(CurrentPath[WaypointIndex]))
				NodePos->Z = NewZ;
			if (ShouldAcceptAltitude(NewZ))
				SetActorLocation(FVector(CurrentPos.X, CurrentPos.Y, NewZ));
		}

		PathNodeIndex = WaypointIndex;
		return;
	}

	const FVector MoveDir2D = FVector(
		WaypointPtr->X - CurrentPos.X,
		WaypointPtr->Y - CurrentPos.Y,
		0.0f).GetSafeNormal();

	SetActorLocation(FVector(
		CurrentPos.X + MoveDir2D.X * CurrentSpeed * LastDeltaTime,
		CurrentPos.Y + MoveDir2D.Y * CurrentSpeed * LastDeltaTime,
		CurrentPos.Z));
	FRotator FaceDir = MoveDir2D.Rotation();
	FaceDir.Yaw += 90.0f;
	SetActorRotation(FaceDir);
}
