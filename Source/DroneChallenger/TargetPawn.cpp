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
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Components/PointLightComponent.h"
#include "Components/SplineComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
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
	Mesh->SetRelativeScale3D(FVector(3.0f, 3.0f, 3.0f));

	{
		static ConstructorHelpers::FObjectFinder<UStaticMesh> Finder(TEXT("/Engine/BasicShapes/Sphere"));
		if (Finder.Succeeded())
			Mesh->SetStaticMesh(Finder.Object);
	}

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
	BuildTree();
}

void ATargetPawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!Tree) return;

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

void ATargetPawn::AdvanceAlongPath()
{
	if (!PatrolPath || CurrentSpeed <= 0.0f) return;

	USplineComponent* Spline = PatrolPath->GetSpline();
	float SplineLength = Spline->GetSplineLength();
	if (SplineLength <= 0.0f) return;

	SplineDistance += CurrentSpeed * LastDeltaTime;
	SplineDistance = FMath::Fmod(SplineDistance, SplineLength);

	FVector NewLocation = Spline->GetLocationAtDistanceAlongSpline(SplineDistance, ESplineCoordinateSpace::World);
	FVector Tangent = Spline->GetTangentAtDistanceAlongSpline(SplineDistance, ESplineCoordinateSpace::World).GetSafeNormal();

	FHitResult Hit;
	FVector TraceStart = NewLocation + FVector(0.0f, 0.0f, 500.0f);
	FVector TraceEnd   = NewLocation - FVector(0.0f, 0.0f, 2000.0f);
	if (GetWorld()->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_WorldStatic))
	{
		NewLocation.Z = Hit.ImpactPoint.Z + 50.0f;
	}

	SetActorLocation(NewLocation);
	if (!Tangent.IsNearlyZero())
	{
		SetActorRotation(Tangent.Rotation());
	}
}
