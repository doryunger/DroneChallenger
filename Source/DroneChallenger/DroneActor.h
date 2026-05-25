#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "DroneFlightController.h"
#include "DroneActor.generated.h"

class UInputMappingContext;
class UInputAction;
class USpringArmComponent;
class UCameraComponent;
class UBoxComponent;
class UPoseableMeshComponent;
class UStaticMeshComponent;
class UStaticMesh;
class UAudioComponent;
class USoundBase;
struct FInputActionValue;

UCLASS()
class DRONECHALLENGER_API ADroneActor : public APawn
{
	GENERATED_BODY()

public:
	ADroneActor();

	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

protected:
	virtual void BeginPlay() override;
	virtual void PossessedBy(AController* NewController) override;

private:
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UBoxComponent> PhysicsBody;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UPoseableMeshComponent> VisualMesh;

	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UStaticMeshComponent> PropFL_A;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UStaticMeshComponent> PropFL_B;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UStaticMeshComponent> PropFR_A;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UStaticMeshComponent> PropFR_B;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UStaticMeshComponent> PropRL_A;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UStaticMeshComponent> PropRL_B;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UStaticMeshComponent> PropRR_A;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UStaticMeshComponent> PropRR_B;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UAudioComponent> MotorAudio;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USpringArmComponent> SpringArm;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UCameraComponent> ChaseCamera;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UCameraComponent> FPVCamera;

	UPROPERTY(EditAnywhere, Category = "Drone|Rotors")
	TObjectPtr<UStaticMesh> PropellerMesh_CCW;

	UPROPERTY(EditAnywhere, Category = "Drone|Rotors")
	TObjectPtr<UStaticMesh> PropellerMesh_CW;

	UPROPERTY(EditAnywhere, Category = "Drone|Audio")
	TObjectPtr<USoundBase> MotorLoopSound;

	UPROPERTY(EditAnywhere, Category = "Drone|Audio")
	TObjectPtr<USoundBase> StartupSound;

	UPROPERTY(EditAnywhere, Category = "Drone|Input")
	TObjectPtr<UInputMappingContext> InputMappingContext;

	UPROPERTY(EditAnywhere, Category = "Drone|Input")
	TObjectPtr<UInputAction> IA_Throttle;

	UPROPERTY(EditAnywhere, Category = "Drone|Input")
	TObjectPtr<UInputAction> IA_PitchRoll;

	UPROPERTY(EditAnywhere, Category = "Drone|Input")
	TObjectPtr<UInputAction> IA_Yaw;

	UPROPERTY(EditAnywhere, Category = "Drone|Input")
	TObjectPtr<UInputAction> IA_SwitchCamera;

	UPROPERTY(EditAnywhere, Category = "Drone|Physics", meta = (ClampMin = "0.1"))
	float Mass = 1.5f;

	UPROPERTY(EditAnywhere, Category = "Drone|Physics", meta = (ClampMin = "1.0"))
	float MaxThrustPerRotor = 600.0f;

	UPROPERTY(EditAnywhere, Category = "Drone|Physics", meta = (ClampMin = "0.0"))
	float LinearDamping = 0.5f;

	UPROPERTY(EditAnywhere, Category = "Drone|Physics", meta = (ClampMin = "0.0"))
	float AngularDamping = 5.0f;

	UPROPERTY(EditAnywhere, Category = "Drone|Physics", meta = (ClampMin = "1.0"))
	float ArmLength = 32.5f;

	UPROPERTY(EditAnywhere, Category = "Drone|Physics")
	float YawTorqueCoeff = 3000.0f;

	UPROPERTY(EditAnywhere, Category = "Drone|Rotors", meta = (ClampMin = "0.0"))
	float MaxSpinRate = 4320.0f;

	UPROPERTY(EditAnywhere, Category = "Drone|Rotors", meta = (ClampMin = "0.1"))
	float MotorSpoolRate = 8.0f;

	FDroneFlightController FlightController;
	float RotorThrottle[4]        = { 0.0f, 0.0f, 0.0f, 0.0f };
	float RotorSpinAngle[4]       = { 0.0f, 0.0f, 0.0f, 0.0f };
	float RotorCurrentSpinRate[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	FDroneControlInput ControlInput;

	bool bFPVMode = false;

	void InitHoverThrottle();
	void ApplyRotorForces();
	void UpdateRotorVisuals(float DeltaTime);
	void UpdateMotorAudio();
	void RegisterInputMappingContext(AController* InController);

	void OnThrottle(const FInputActionValue& Value);
	void OnThrottleCompleted(const FInputActionValue& Value);
	void OnPitchRoll(const FInputActionValue& Value);
	void OnPitchRollCompleted(const FInputActionValue& Value);
	void OnYaw(const FInputActionValue& Value);
	void OnYawCompleted(const FInputActionValue& Value);
	void OnSwitchCamera(const FInputActionValue& Value);
};
