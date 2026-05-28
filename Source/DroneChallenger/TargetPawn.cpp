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
#include "Components/SplineComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "DroneActor.h"

THIRD_PARTY_INCLUDES_START
#include "bt/BehaviorTree.h"
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

	GlobeAnchor = CreateDefaultSubobject<UCesiumGlobeAnchorComponent>(TEXT("GlobeAnchor"));

	AIControllerClass = ATargetAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
}

ATargetPawn::~ATargetPawn()
{
	delete Tree;
	Tree = nullptr;
}

void ATargetPawn::BeginPlay()
{
	Super::BeginPlay();

	CachedDrone = Cast<ADroneActor>(UGameplayStatics::GetActorOfClass(GetWorld(), ADroneActor::StaticClass()));
	CachedGeoreference = ACesiumGeoreference::GetDefaultGeoreference(this);

	Mesh->SetRelativeScale3D(FVector(3.0f, 3.0f, 3.0f));

	// The drone's editor-placed position is our tile-loading anchor.
	// Terrain at this XY is guaranteed to be loaded because the camera starts here.
	// All node searches radiate outward from this point.
	if (IsValid(CachedDrone))
	{
		const FVector DronePosEditor = CachedDrone->GetActorLocation();
		DroneEditorXY = FVector2D(DronePosEditor.X, DronePosEditor.Y);
		DroneEditorZ  = DronePosEditor.Z;
	}
	else
	{
		const FVector CarPosEditor = GetActorLocation();
		DroneEditorXY = FVector2D(CarPosEditor.X, CarPosEditor.Y);
		DroneEditorZ  = CarPosEditor.Z;
	}

	{
		ACesiumGeoreference* Georef = CachedGeoreference;
		const FString NodesPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir() / TEXT("Graph/nodes.csv"));
		const FString EdgesPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir() / TEXT("Graph/edges.csv"));

		if (Georef && Graph.Load(NodesPath, EdgesPath, Georef, GetWorld()) && !Graph.NodeIds.IsEmpty())
		{
			UE_LOG(LogTemp, Log, TEXT("TargetPawn: graph loaded %d nodes — searching for placement within %.0f cm of drone editor pos"),
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
}

void ATargetPawn::UpdateDroneState()
{
	if (bIsCaptured) return;

	bDroneInFOV = ComputeDroneInFOV();

	if (CachedDrone)
	{
		float Dist = FVector::Distance(GetActorLocation(), CachedDrone->GetActorLocation());
		bDroneInCaptureRange = (Dist <= CaptureRadius);
	}
	else
	{
		bDroneInCaptureRange = false;
	}

	if (!bDroneInCaptureRange)
	{
		CaptureTimer = 0.0f;
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

	// Collect graph nodes within the current search radius of the drone's editor position.
	// Sorted nearest-first so we land as close to the drone as possible.
	TArray<TPair<float, int32>> Candidates;
	for (int32 NodeId : Graph.NodeIds)
	{
		const FVector* P = Graph.NodeWorldPos.Find(NodeId);
		if (!P) continue;
		const float Dist2D = FVector2D::Distance(DroneEditorXY, FVector2D(P->X, P->Y));
		if (Dist2D <= PlacementRadius)
			Candidates.Add({ Dist2D, NodeId });
	}
	for (int32 i = Candidates.Num() - 1; i > 0; --i)
	{
		const int32 j = FMath::RandRange(0, i);
		Candidates.Swap(i, j);
	}

	if (Candidates.IsEmpty())
	{
		PlacementRadius += 5000.0f;
		UE_LOG(LogTemp, Warning,
			TEXT("TargetPawn: no graph nodes within %.0f cm of drone — expanding to %.0f cm"),
			PlacementRadius - 5000.0f, PlacementRadius);
		return;
	}

	// Try the closest nodes until one gives a valid terrain hit.
	static constexpr int32 MaxTriesPerAttempt = 15;
	static constexpr float TraceHalfHeight    = 3000.0f;   // ±30m around drone editor Z

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);

	const int32 NumToTry = FMath::Min(MaxTriesPerAttempt, Candidates.Num());
	for (int32 i = 0; i < NumToTry; ++i)
	{
		const int32 NodeId = Candidates[i].Value;
		const FVector* NodePos = Graph.NodeWorldPos.Find(NodeId);
		if (!NodePos) continue;

		FHitResult Hit;
		if (!World->LineTraceSingleByChannel(Hit,
			FVector(NodePos->X, NodePos->Y, DroneEditorZ + TraceHalfHeight),
			FVector(NodePos->X, NodePos->Y, DroneEditorZ - TraceHalfHeight),
			ECC_WorldStatic, Params))
			continue;

		// Valid terrain hit — commit placement.
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

		World->GetTimerManager().ClearTimer(PlacementTimer);
		bPlacementDone = true;

		UE_LOG(LogTemp, Log,
			TEXT("TargetPawn: placed at node %d — car Z=%.0f, path=%d waypoints, radius=%.0f cm"),
			NodeId, CarPos.Z, CurrentPath.Num(), PlacementRadius);
		return;
	}

	// All candidates missed — tiles may not be loaded yet for those nodes.
	// Widen slightly so the next attempt also covers more area.
	PlacementRadius += 1000.0f;
	UE_LOG(LogTemp, Log,
		TEXT("TargetPawn: all %d candidates missed terrain trace — tiles still loading (radius now %.0f cm)"),
		NumToTry, PlacementRadius);
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
			FVector(WP.X, WP.Y, WP.Z + 200000.0f),
			FVector(WP.X, WP.Y, WP.Z - 200000.0f),
			ECC_WorldStatic, Params))
		{
			if (FVector* NodePos = Graph.NodeWorldPos.Find(CurrentPath[WaypointIndex]))
				NodePos->Z = Hit.ImpactPoint.Z + 50.0f;
			SetActorLocation(FVector(CurrentPos.X, CurrentPos.Y, Hit.ImpactPoint.Z + 50.0f));
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
