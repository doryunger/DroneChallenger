#include "DroneActor.h"
#include "Components/BoxComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/AudioComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Kismet/GameplayStatics.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputActionValue.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/SkeletalMesh.h"

ADroneActor::ADroneActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;

	PhysicsBody = CreateDefaultSubobject<UBoxComponent>(TEXT("PhysicsBody"));
	PhysicsBody->SetBoxExtent(FVector(35.0f, 40.0f, 11.0f));
	PhysicsBody->SetSimulatePhysics(true);
	PhysicsBody->SetEnableGravity(true);
	PhysicsBody->SetCollisionProfileName(TEXT("Pawn"));
	RootComponent = PhysicsBody;

	VisualMesh = CreateDefaultSubobject<UPoseableMeshComponent>(TEXT("VisualMesh"));
	VisualMesh->SetupAttachment(PhysicsBody);
	VisualMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	VisualMesh->SetSimulatePhysics(false);

	MotorAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("MotorAudio"));
	MotorAudio->SetupAttachment(PhysicsBody);
	MotorAudio->bAutoActivate = false;

	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(PhysicsBody);
	SpringArm->TargetArmLength = 180.0f;
	SpringArm->SocketOffset    = FVector(0.0f, 0.0f, 20.0f);
	SpringArm->SetRelativeRotation(FRotator(-15.0f, 0.0f, 0.0f));
	SpringArm->bInheritPitch    = false;
	SpringArm->bInheritRoll     = false;
	SpringArm->bInheritYaw      = true;
	SpringArm->bDoCollisionTest = true;
	SpringArm->bEnableCameraLag = false;

	ChaseCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("ChaseCamera"));
	ChaseCamera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
	ChaseCamera->SetActive(true);

	auto MakeBlade = [&](const TCHAR* Name) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* Comp = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Comp->SetupAttachment(VisualMesh);
		Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		return Comp;
	};
	PropFL_A = MakeBlade(TEXT("PropFL_A")); PropFL_B = MakeBlade(TEXT("PropFL_B"));
	PropFR_A = MakeBlade(TEXT("PropFR_A")); PropFR_B = MakeBlade(TEXT("PropFR_B"));
	PropRL_A = MakeBlade(TEXT("PropRL_A")); PropRL_B = MakeBlade(TEXT("PropRL_B"));
	PropRR_A = MakeBlade(TEXT("PropRR_A")); PropRR_B = MakeBlade(TEXT("PropRR_B"));

	FPVCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FPVCamera"));
	FPVCamera->SetupAttachment(VisualMesh);
	FPVCamera->SetRelativeLocation(FVector(18.0f, 0.0f, 9.0f));
	FPVCamera->SetRelativeRotation(FRotator(-15.0f, 0.0f, 0.0f));
	FPVCamera->FieldOfView = 90.0f;
	FPVCamera->SetActive(false);

	{
		static ConstructorHelpers::FObjectFinder<USkeletalMesh> Finder(
			TEXT("/Game/RealisticDroneV2/RealisticDroneContent/Meshes/SKM_RealisticDrone"));
		if (Finder.Succeeded())
			VisualMesh->SetSkinnedAssetAndUpdate(Finder.Object);
	}
	{
		static ConstructorHelpers::FObjectFinder<UStaticMesh> Finder(
			TEXT("/Game/RealisticDroneV2/RealisticDroneContent/Meshes/SM_RealisticDronePropeller_Left"));
		if (Finder.Succeeded())
			PropellerMesh_CCW = Finder.Object;
	}
	{
		static ConstructorHelpers::FObjectFinder<UStaticMesh> Finder(
			TEXT("/Game/RealisticDroneV2/RealisticDroneContent/Meshes/SM_RealisticDronePropeller_Right"));
		if (Finder.Succeeded())
			PropellerMesh_CW = Finder.Object;
	}
	{
		static ConstructorHelpers::FObjectFinder<UInputMappingContext> Finder(TEXT("/Game/IMC_Drone"));
		if (Finder.Succeeded())
			InputMappingContext = Finder.Object;
	}
	{
		static ConstructorHelpers::FObjectFinder<UInputAction> Finder(TEXT("/Game/IA_Throttle"));
		if (Finder.Succeeded())
			IA_Throttle = Finder.Object;
	}
	{
		static ConstructorHelpers::FObjectFinder<UInputAction> Finder(TEXT("/Game/IA_PithRoll"));
		if (Finder.Succeeded())
			IA_PitchRoll = Finder.Object;
	}
	{
		static ConstructorHelpers::FObjectFinder<UInputAction> Finder(TEXT("/Game/IA_Yaw"));
		if (Finder.Succeeded())
			IA_Yaw = Finder.Object;
	}
	{
		static ConstructorHelpers::FObjectFinder<UInputAction> Finder(TEXT("/Game/IA_SwitchCamera"));
		if (Finder.Succeeded())
			IA_SwitchCamera = Finder.Object;
	}
}

void ADroneActor::BeginPlay()
{
	Super::BeginPlay();

	PhysicsBody->SetMassOverrideInKg(NAME_None, Mass);
	PhysicsBody->SetLinearDamping(LinearDamping);
	PhysicsBody->SetAngularDamping(AngularDamping);

	InitHoverThrottle();

	if (PropellerMesh_CCW)
	{
		PropFL_A->SetStaticMesh(PropellerMesh_CCW); PropFL_B->SetStaticMesh(PropellerMesh_CCW);
		PropRR_A->SetStaticMesh(PropellerMesh_CCW); PropRR_B->SetStaticMesh(PropellerMesh_CCW);
	}
	if (PropellerMesh_CW)
	{
		PropFR_A->SetStaticMesh(PropellerMesh_CW); PropFR_B->SetStaticMesh(PropellerMesh_CW);
		PropRL_A->SetStaticMesh(PropellerMesh_CW); PropRL_B->SetStaticMesh(PropellerMesh_CW);
	}

	static const FName PropBones[4] = {
		TEXT("LeftFrontPropeller1"), TEXT("RightFrontPropeller1"),
		TEXT("LeftBackPropeller1"),  TEXT("RightBackPropeller1"),
	};
	UStaticMeshComponent* BladesA[4] = { PropFL_A, PropFR_A, PropRL_A, PropRR_A };
	UStaticMeshComponent* BladesB[4] = { PropFL_B, PropFR_B, PropRL_B, PropRR_B };

	for (int32 i = 0; i < 4; ++i)
	{
		const FVector BonePos = VisualMesh->GetBoneLocation(PropBones[i], EBoneSpaces::ComponentSpace);
		BladesA[i]->SetRelativeLocation(BonePos);
		BladesB[i]->SetRelativeLocation(BonePos);
	}

	if (MotorLoopSound)
	{
		MotorAudio->SetSound(MotorLoopSound);
		MotorAudio->SetPitchMultiplier(0.6f);
		MotorAudio->Play();
	}

	if (StartupSound)
		UGameplayStatics::PlaySoundAtLocation(this, StartupSound, GetActorLocation());
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
	const FVector2D Axis = Value.Get<FVector2D>();
	ControlInput.Pitch   = Axis.X;
	ControlInput.Roll    = Axis.Y;
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

void ADroneActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!PhysicsBody || !PhysicsBody->IsSimulatingPhysics())
		return;

	const FVector WorldAngVelDeg = PhysicsBody->GetPhysicsAngularVelocityInDegrees();
	const FVector BodyAngVelDeg  = GetActorRotation().UnrotateVector(WorldAngVelDeg);

	SmoothedInput.Throttle = FMath::FInterpConstantTo(SmoothedInput.Throttle, ControlInput.Throttle, DeltaTime, InputRampRate);
	SmoothedInput.Pitch    = FMath::FInterpConstantTo(SmoothedInput.Pitch,    ControlInput.Pitch,    DeltaTime, InputRampRate);
	SmoothedInput.Roll     = FMath::FInterpConstantTo(SmoothedInput.Roll,     ControlInput.Roll,     DeltaTime, InputRampRate);
	SmoothedInput.Yaw      = ControlInput.Yaw;

	FlightController.Update(SmoothedInput, GetActorUpVector(), BodyAngVelDeg, DeltaTime, RotorThrottle);
	ApplyRotorForces();
	UpdateRotorVisuals(DeltaTime);
	UpdateMotorAudio();
}

void ADroneActor::ApplyRotorForces()
{
	const FVector RotorOffsets[4] = {
		FVector( ArmLength, -ArmLength, 0.0f),
		FVector( ArmLength,  ArmLength, 0.0f),
		FVector(-ArmLength, -ArmLength, 0.0f),
		FVector(-ArmLength,  ArmLength, 0.0f),
	};

	static constexpr float YawSpin[4] = { 1.0f, -1.0f, -1.0f, 1.0f };

	const FQuat   ActorQuat      = GetActorQuat();
	const FVector LocalUp        = GetActorUpVector();
	FVector       TotalYawTorque = FVector::ZeroVector;

	for (int32 i = 0; i < 4; ++i)
	{
		const float Thrust = RotorThrottle[i] * MaxThrustPerRotor;

		const FVector WorldOffset = ActorQuat.RotateVector(RotorOffsets[i]);
		const FVector WorldPos    = GetActorLocation() + WorldOffset;
		PhysicsBody->AddForceAtLocation(LocalUp * Thrust, WorldPos, NAME_None);

		TotalYawTorque += LocalUp * (YawSpin[i] * RotorThrottle[i] * YawTorqueCoeff);
	}

	PhysicsBody->AddTorqueInRadians(TotalYawTorque, NAME_None, false);
}

void ADroneActor::UpdateRotorVisuals(float DeltaTime)
{
	UStaticMeshComponent* BladesA[4] = { PropFL_A, PropFR_A, PropRL_A, PropRR_A };
	UStaticMeshComponent* BladesB[4] = { PropFL_B, PropFR_B, PropRL_B, PropRR_B };
	static constexpr float SpinDir[4] = { 1.0f, -1.0f, -1.0f, 1.0f };

	for (int32 i = 0; i < 4; ++i)
	{
		const float TargetRate = RotorThrottle[i] * MaxSpinRate;
		RotorCurrentSpinRate[i] = FMath::FInterpTo(RotorCurrentSpinRate[i], TargetRate, DeltaTime, MotorSpoolRate);

		RotorSpinAngle[i] = FMath::Fmod(
			RotorSpinAngle[i] + RotorCurrentSpinRate[i] * SpinDir[i] * DeltaTime,
			360.0f);

		if (BladesA[i]) BladesA[i]->SetRelativeRotation(FRotator(0.0f, RotorSpinAngle[i],          0.0f));
		if (BladesB[i]) BladesB[i]->SetRelativeRotation(FRotator(0.0f, RotorSpinAngle[i] + 180.0f, 0.0f));
	}
}

void ADroneActor::UpdateMotorAudio()
{
	if (!MotorAudio || !MotorAudio->IsActive())
		return;

	float AvgThrottle = 0.0f;
	for (float T : RotorThrottle)
		AvgThrottle += T;
	AvgThrottle *= 0.25f;

	const float PitchMult = FMath::Lerp(0.6f, 1.8f, AvgThrottle);
	MotorAudio->SetPitchMultiplier(PitchMult);
}
