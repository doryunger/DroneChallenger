#include "DroneActor.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputActionValue.h"

ADroneActor::ADroneActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DroneBody"));
	RootComponent = MeshComponent;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
		MeshComponent->SetStaticMesh(CubeMesh.Object);

	// Flat box proportions: 50cm wide, 50cm deep, 10cm tall — drone-like footprint.
	MeshComponent->SetRelativeScale3D(FVector(0.5f, 0.5f, 0.1f));
	MeshComponent->SetSimulatePhysics(true);
	MeshComponent->SetEnableGravity(true);

	// Chase camera — spring arm behind and above, level regardless of drone attitude.
	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(RootComponent);
	SpringArm->TargetArmLength = 500.0f;
	SpringArm->SocketOffset    = FVector(0.0f, 0.0f, 150.0f);
	SpringArm->bInheritPitch        = false;
	SpringArm->bInheritRoll         = false;
	SpringArm->bInheritYaw          = true;
	SpringArm->bDoCollisionTest     = true;
	SpringArm->bEnableCameraLag     = true;
	SpringArm->CameraLagSpeed       = 10.0f;
	SpringArm->CameraLagMaxDistance = 300.0f;

	ChaseCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("ChaseCamera"));
	ChaseCamera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
	ChaseCamera->SetActive(true);

	// FPV camera — fixed at the drone nose between the two front arms, tilts with the drone.
	FPVCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FPVCamera"));
	FPVCamera->SetupAttachment(RootComponent);
	FPVCamera->SetRelativeLocation(FVector(25.0f, 0.0f, 8.0f));
	FPVCamera->SetActive(false);
}

void ADroneActor::BeginPlay()
{
	Super::BeginPlay();

	MeshComponent->SetMassOverrideInKg(NAME_None, Mass);
	MeshComponent->SetLinearDamping(LinearDamping);
	MeshComponent->SetAngularDamping(AngularDamping);

	InitHoverThrottle();
}

void ADroneActor::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	RegisterInputMappingContext(NewController);
}

void ADroneActor::RegisterInputMappingContext(AController* InController)
{
	if (!InputMappingContext)
		return;

	APlayerController* PC = Cast<APlayerController>(InController);
	if (!PC)
		return;

	UEnhancedInputLocalPlayerSubsystem* Subsystem =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer());
	if (Subsystem)
		Subsystem->AddMappingContext(InputMappingContext, 0);
}

void ADroneActor::InitHoverThrottle()
{
	const float GravityMag = FMath::Abs(GetWorld()->GetGravityZ());
	const float HoverThrottle = (Mass * GravityMag) / (4.0f * MaxThrustPerRotor);

	FlightController.HoverThrottle = FMath::Clamp(HoverThrottle, 0.0f, 1.0f);

	for (float& T : RotorThrottle)
		T = FlightController.HoverThrottle;

	UE_LOG(LogTemp, Log, TEXT("DroneActor: hover throttle = %.3f (%.1f%%) — gravity = %.1f cm/s²"),
		HoverThrottle, HoverThrottle * 100.0f, GravityMag);

	if (HoverThrottle > 1.0f)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("DroneActor: MaxThrustPerRotor too low — drone cannot hover. Increase MaxThrustPerRotor."));
	}
}

void ADroneActor::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EIC)
	{
		UE_LOG(LogTemp, Warning, TEXT("DroneActor: PlayerInputComponent is not UEnhancedInputComponent. Check project settings."));
		return;
	}

	if (IA_Throttle)
	{
		EIC->BindAction(IA_Throttle, ETriggerEvent::Triggered, this, &ADroneActor::OnThrottle);
		EIC->BindAction(IA_Throttle, ETriggerEvent::Completed, this, &ADroneActor::OnThrottleCompleted);
	}
	if (IA_PitchRoll)
	{
		EIC->BindAction(IA_PitchRoll, ETriggerEvent::Triggered, this, &ADroneActor::OnPitchRoll);
		EIC->BindAction(IA_PitchRoll, ETriggerEvent::Completed, this, &ADroneActor::OnPitchRollCompleted);
	}
	if (IA_Yaw)
	{
		EIC->BindAction(IA_Yaw, ETriggerEvent::Triggered, this, &ADroneActor::OnYaw);
		EIC->BindAction(IA_Yaw, ETriggerEvent::Completed, this, &ADroneActor::OnYawCompleted);
	}
	if (IA_SwitchCamera)
		EIC->BindAction(IA_SwitchCamera, ETriggerEvent::Started, this, &ADroneActor::OnSwitchCamera);
}

// --- Input callbacks ---

void ADroneActor::OnThrottle(const FInputActionValue& Value)
{
	ControlInput.Throttle = Value.Get<float>();
}

void ADroneActor::OnThrottleCompleted(const FInputActionValue&)
{
	ControlInput.Throttle = 0.0f;
}

void ADroneActor::OnPitchRoll(const FInputActionValue& Value)
{
	const FVector2D Axis   = Value.Get<FVector2D>();
	ControlInput.Pitch     = Axis.X;
	ControlInput.Roll      = Axis.Y;
}

void ADroneActor::OnPitchRollCompleted(const FInputActionValue&)
{
	ControlInput.Pitch = 0.0f;
	ControlInput.Roll  = 0.0f;
}

void ADroneActor::OnYaw(const FInputActionValue& Value)
{
	ControlInput.Yaw = Value.Get<float>();
}

void ADroneActor::OnYawCompleted(const FInputActionValue&)
{
	ControlInput.Yaw = 0.0f;
}

void ADroneActor::OnSwitchCamera(const FInputActionValue&)
{
	bFPVMode = !bFPVMode;
	ChaseCamera->SetActive(!bFPVMode);
	FPVCamera->SetActive(bFPVMode);
}

// --- Tick ---

void ADroneActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!MeshComponent || !MeshComponent->IsSimulatingPhysics())
		return;

	// Angular velocity in drone body frame (deg/s): X=roll, Y=pitch, Z=yaw.
	const FVector WorldAngVelDeg = MeshComponent->GetPhysicsAngularVelocityInDegrees();
	const FVector BodyAngVelDeg  = GetActorRotation().UnrotateVector(WorldAngVelDeg);

	FlightController.Update(ControlInput, BodyAngVelDeg, DeltaTime, RotorThrottle);
	ApplyRotorForces();
}

void ADroneActor::ApplyRotorForces()
{
	// Rotor positions in drone body frame (cm). X=forward, Y=right.
	// Order: FL(0), FR(1), RL(2), RR(3).
	const FVector RotorOffsets[4] = {
		FVector( ArmLength, -ArmLength, 0.0f), // FL
		FVector( ArmLength,  ArmLength, 0.0f), // FR
		FVector(-ArmLength, -ArmLength, 0.0f), // RL
		FVector(-ArmLength,  ArmLength, 0.0f), // RR
	};

	// +1 = CCW rotor (produces positive / CW yaw reaction torque).
	// -1 = CW  rotor (produces negative / CCW yaw reaction torque).
	static constexpr float YawSpin[4] = { 1.0f, -1.0f, -1.0f, 1.0f };

	const FQuat   ActorQuat    = GetActorQuat();
	const FVector LocalUp      = GetActorUpVector();
	FVector       TotalYawTorque = FVector::ZeroVector;

	for (int32 i = 0; i < 4; ++i)
	{
		const float Thrust = RotorThrottle[i] * MaxThrustPerRotor;

		// Apply thrust at rotor's world-space position so roll/pitch torques arise naturally.
		const FVector WorldOffset = ActorQuat.RotateVector(RotorOffsets[i]);
		const FVector WorldPos    = GetActorLocation() + WorldOffset;
		MeshComponent->AddForceAtLocation(LocalUp * Thrust, WorldPos, NAME_None);

		// Accumulate yaw reaction torque (CCW/CW drag from spinning rotors).
		TotalYawTorque += LocalUp * (YawSpin[i] * RotorThrottle[i] * YawTorqueCoeff);
	}

	MeshComponent->AddTorqueInRadians(TotalYawTorque, NAME_None, false);
}
