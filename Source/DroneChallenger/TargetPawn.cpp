#include "TargetPawn.h"
#include "DroneHUD.h"
#include "DroneHUDServer.h"
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
#include "bt/SchemaParser.h"
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
	if (Monitor)    { Monitor->stop(); delete Monitor; Monitor = nullptr; }
	if (HUDServer)  { HUDServer->Shutdown(); delete HUDServer; HUDServer = nullptr; }
	delete Tree;    Tree    = nullptr;
	delete Emitter; Emitter = nullptr;
}

bool ATargetPawn::IsHUDServerRunning() const
{
	return HUDServer != nullptr && HUDServer->IsRunning();
}

void ATargetPawn::BeginPlay()
{
	Super::BeginPlay();

	CachedDrone = Cast<ADroneActor>(UGameplayStatics::GetActorOfClass(GetWorld(), ADroneActor::StaticClass()));
	CachedGeoreference = ACesiumGeoreference::GetDefaultGeoreference(this);

	Mesh->SetRelativeScale3D(FVector(1.5f, 1.5f, 1.5f));

	static constexpr float RuntimeMeshScale = 1.5f;
	static constexpr float BeaconCenterM    = 50.0f;
	BeaconPole->SetRelativeLocation(FVector(0.0f, 0.0f, BeaconCenterM * 100.0f / RuntimeMeshScale));

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

			WriteGraphDataJS();

			const FString UiDir = FPaths::ConvertRelativePathToFull(
				FPaths::ProjectDir() / TEXT("HUD"));
			HUDServer = new FDroneHUDServer(UiDir);
			if (HUDServer->Start(8081))
			{
				if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
				{
					if (ADroneHUD* HUD = Cast<ADroneHUD>(PC->GetHUD()))
						HUD->NotifyHUDServerReady();
				}
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("TargetPawn: HUD server failed to bind 8081 — retrying in 2 s"));
				GetWorldTimerManager().SetTimer(HUDServerRetryTimer, this,
					&ATargetPawn::RetryHUDServer, 2.0f, false);
			}
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

static std::string SerializeSchemaNode(const bt::SchemaNode& n)
{
    const char* type = "Action";
    switch (n.type)
    {
        case bt::SchemaNodeType::SEQUENCE:  type = "Sequence";  break;
        case bt::SchemaNodeType::SELECTOR:  type = "Selector";  break;
        case bt::SchemaNodeType::CONDITION: type = "Condition"; break;
        case bt::SchemaNodeType::PARALLEL:  type = "Parallel";  break;
        default: break;
    }
    std::string out = "{\"name\":\"" + n.name + "\",\"type\":\"" + type + "\",\"children\":[";
    for (size_t i = 0; i < n.children.size(); ++i)
    {
        if (i > 0) out += ',';
        out += SerializeSchemaNode(*n.children[i]);
    }
    return out + "]}";
}

static std::string SerializeSchemaDoc(const bt::SchemaDoc& doc)
{
    std::string out = "{\"name\":\"" + doc.subtreeName + "\",\"type\":\"Selector\",\"children\":[";
    for (size_t i = 0; i < doc.behaviors.size(); ++i)
    {
        if (i > 0) out += ',';
        const auto& beh = doc.behaviors[i];
        out += "{\"name\":\"" + beh.name + "\",\"type\":\"Sequence\",\"children\":[";
        bool first = true;
        if (!beh.condition.empty())
        {
            out += "{\"name\":\"" + beh.condition + "\",\"type\":\"Condition\",\"children\":[]}";
            first = false;
        }
        if (beh.tree)
        {
            if (!first) out += ',';
            out += SerializeSchemaNode(*beh.tree);
        }
        out += "]}";
    }
    return out + "]}";
}

static FString SerializeHistory(const std::deque<bt::TickRecord>& history)
{
    std::string out;
    out.reserve(4096);
    out += '[';
    bool first = true;
    for (const auto& rec : history)
    {
        if (!first) out += ',';
        first = false;
        out += "{\"tick\":";
        out += std::to_string(rec.tickNumber);
        out += ",\"behavior\":\"";
        out += rec.behaviorName;
        out += "\",\"status\":\"";
        out += std::string(bt::toString(rec.result));
        out += "\",\"activePath\":[";
        bool firstAP = true;
        for (const auto& ap : rec.activePath)
        {
            if (!firstAP) out += ',';
            firstAP = false;
            out += "{\"name\":\"";
            out += ap.name;
            out += "\",\"status\":\"";
            out += std::string(bt::toString(ap.status));
            out += "\"}";
        }
        out += "]}";
    }
    out += ']';
    return UTF8_TO_TCHAR(out.c_str());
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

	// Push live state to HUD server every frame regardless of placement
	if (HUDServer && CachedDrone)
	{
		const FVector DronePos = CachedDrone->GetActorLocation();
		const FVector TargPos  = GetActorLocation();

		FVector DroneLonLatH(0, 0, 0);
		FVector TargLonLatH(0, 0, 0);
		if (CachedGeoreference)
		{
			DroneLonLatH = CachedGeoreference->TransformUnrealPositionToLongitudeLatitudeHeight(DronePos);
			TargLonLatH  = CachedGeoreference->TransformUnrealPositionToLongitudeLatitudeHeight(TargPos);
		}

		FHUDState HS;
		HS.DroneX      = DronePos.X;
		HS.DroneY      = DronePos.Y;
		HS.DroneZ      = DronePos.Z;
		HS.DroneYaw    = CachedDrone->GetActorRotation().Yaw;
		HS.AltM        = FMath::Max(DroneLonLatH.Z - TargLonLatH.Z, 0.f);
		HS.TargetX     = TargPos.X;
		HS.TargetY     = TargPos.Y;
		HS.TargetZ     = TargPos.Z;
		HS.bTargetInFOV = bDroneInFOV;
		HUDServer->SetState(HS);
	}

	if (!bPlacementDone || !Tree) return;

	LastDeltaTime = DeltaTime;
	UpdateDroneState();
	if (!bDroneHasEverMoved && CachedDrone &&
	    CachedDrone->GetVelocity().SizeSquared() > 500.f)
	{
		bDroneHasEverMoved = true;
	}

	if (bDroneHasEverMoved)
	{
		if (bDroneInFOV) {
			CurrentTrackingTime += DeltaTime;
			BestTrackingTime = FMath::Max(BestTrackingTime, CurrentTrackingTime);
		} else if (bWasInFOV) {
			CurrentTrackingTime = 0.0f;
		}
	}
	bWasInFOV = bDroneInFOV;
	Tree->tick();
	if (HUDServer && Emitter && !Emitter->history().empty())
		HUDServer->SetBTHistoryJson(SerializeHistory(Emitter->history()));
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
	bt::SchemaDoc Doc = bt::SchemaParser::parse(YamlStr);
	if (HUDServer)
		HUDServer->SetBTTreeJson(UTF8_TO_TCHAR(SerializeSchemaDoc(Doc).c_str()));
	bt::BehaviorTree Loaded = bt::SchemaLoader::load(Doc, Reg);
	Tree = new bt::BehaviorTree(std::move(Loaded));

	delete Emitter;
	Emitter = new bt::DecisionEmitter(32);
	Tree->setEmitter(Emitter);

}

void ATargetPawn::RetryHUDServer()
{
	if (!HUDServer || HUDServer->IsRunning()) return;
	if (HUDServer->Start(8081))
	{
		UE_LOG(LogTemp, Log, TEXT("TargetPawn: HUD server bound on retry"));
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
		{
			if (ADroneHUD* HUD = Cast<ADroneHUD>(PC->GetHUD()))
				HUD->NotifyHUDServerReady();
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("TargetPawn: HUD server retry failed — trying again in 2 s"));
		GetWorldTimerManager().SetTimer(HUDServerRetryTimer, this,
			&ATargetPawn::RetryHUDServer, 2.0f, false);
	}
}

void ATargetPawn::UpdateDroneState()
{
	if (bIsCaptured) return;

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

	bDroneInFOV = ComputeDroneInFOV();

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
	if (!CachedDrone || !bDroneHasLOS) return false;

	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC || PC->GetPawn() != CachedDrone) return false;

	const float Dist = FVector::Distance(GetActorLocation(), CachedDrone->GetActorLocation());
	if (Dist > DetectionRange) return false;

	const FVector CamDir   = PC->GetControlRotation().Vector();
	const FVector ToTarget = (GetActorLocation() - CachedDrone->GetActorLocation()).GetSafeNormal();

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

void ATargetPawn::WriteGraphDataJS()
{
	FString Out;
	Out.Reserve(Graph.NodeIds.Num() * 40 + Graph.Adjacency.Num() * 15);
	Out += TEXT("const GRAPH_NODES={");
	bool bFirstNode = true;
	for (const int32 Id : Graph.NodeIds)
	{
		const FVector* P = Graph.NodeWorldPos.Find(Id);
		if (!P) continue;
		if (!bFirstNode) Out += TEXT(",");
		Out += FString::Printf(TEXT("%d:[%.0f,%.0f,%.0f]"), Id, P->X, P->Y, P->Z);
		bFirstNode = false;
	}
	Out += TEXT("};\nconst GRAPH_EDGES=[");
	bool bFirstEdge = true;
	for (const auto& [From, Neighbors] : Graph.Adjacency)
	{
		for (const int32 To : Neighbors)
		{
			if (To <= From) continue;
			if (!bFirstEdge) Out += TEXT(",");
			Out += FString::Printf(TEXT("[%d,%d]"), From, To);
			bFirstEdge = false;
		}
	}
	Out += TEXT("];");

	const FString OutPath = FPaths::ConvertRelativePathToFull(
		FPaths::ProjectDir() / TEXT("HUD/minimap/munich_graph.js"));
	if (FFileHelper::SaveStringToFile(Out, *OutPath))
	{
		UE_LOG(LogTemp, Log, TEXT("TargetPawn: wrote munich_graph.js (%d nodes) to %s"),
			Graph.NodeIds.Num(), *OutPath);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("TargetPawn: failed to write munich_graph.js to %s"), *OutPath);
	}
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
		AltStabilitySamples.Empty();
		World->GetTimerManager().SetTimer(
			AltStabilityTimer, this, &ATargetPawn::CheckAltitudeStability, 0.5f, true);
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

void ATargetPawn::CheckAltitudeStability()
{
	AltStabilitySamples.Add(GetActorLocation().Z);

	if (AltStabilitySamples.Num() > AltStabilitySampleCount)
		AltStabilitySamples.RemoveAt(0);

	if (AltStabilitySamples.Num() < AltStabilitySampleCount) return;

	float MaxDelta = 0.f;
	for (int32 i = 1; i < AltStabilitySamples.Num(); ++i)
		MaxDelta = FMath::Max(MaxDelta,
			FMath::Abs(AltStabilitySamples[i] - AltStabilitySamples[i - 1]));

	if (MaxDelta < AltStabilityThresholdCm)
	{
		GetWorld()->GetTimerManager().ClearTimer(AltStabilityTimer);
		OnAltitudeStable.Broadcast();
	}
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
