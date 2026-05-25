#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "DroneFlightController.h"
#include "DroneActor.generated.h"

class UInputMappingContext;
class UInputAction;
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
	TObjectPtr<UStaticMeshComponent> MeshComponent;

	// --- Input assets: create in editor, assign in Details panel ---
	UPROPERTY(EditAnywhere, Category = "Drone|Input")
	TObjectPtr<UInputMappingContext> InputMappingContext;

	UPROPERTY(EditAnywhere, Category = "Drone|Input")
	TObjectPtr<UInputAction> IA_Throttle;  // Axis1D — W/S or gamepad left stick Y

	UPROPERTY(EditAnywhere, Category = "Drone|Input")
	TObjectPtr<UInputAction> IA_PitchRoll; // Axis2D — arrow keys or gamepad right stick

	UPROPERTY(EditAnywhere, Category = "Drone|Input")
	TObjectPtr<UInputAction> IA_Yaw;       // Axis1D — Q/E or gamepad left stick X

	// --- Physics ---
	UPROPERTY(EditAnywhere, Category = "Drone|Physics", meta = (ClampMin = "0.1"))
	float Mass = 1.5f;

	UPROPERTY(EditAnywhere, Category = "Drone|Physics", meta = (ClampMin = "1.0"))
	float MaxThrustPerRotor = 600.0f;

	UPROPERTY(EditAnywhere, Category = "Drone|Physics", meta = (ClampMin = "0.0"))
	float LinearDamping = 0.5f;

	UPROPERTY(EditAnywhere, Category = "Drone|Physics", meta = (ClampMin = "0.0"))
	float AngularDamping = 5.0f;

	// Distance from drone centre to each rotor, along each body axis (cm).
	UPROPERTY(EditAnywhere, Category = "Drone|Physics", meta = (ClampMin = "1.0"))
	float ArmLength = 25.0f;

	// Yaw reaction torque produced per rotor per unit throttle (kg*cm²/s²).
	UPROPERTY(EditAnywhere, Category = "Drone|Physics", meta = (ClampMin = "0.0"))
	float YawTorqueCoeff = 3000.0f;

	// --- Flight controller ---
	FDroneFlightController FlightController;
	float RotorThrottle[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	FDroneControlInput ControlInput;

	void InitHoverThrottle();
	void ApplyRotorForces();
	void RegisterInputMappingContext(AController* InController);

	// Input callbacks
	void OnThrottle(const FInputActionValue& Value);
	void OnThrottleCompleted(const FInputActionValue& Value);
	void OnPitchRoll(const FInputActionValue& Value);
	void OnPitchRollCompleted(const FInputActionValue& Value);
	void OnYaw(const FInputActionValue& Value);
	void OnYawCompleted(const FInputActionValue& Value);
};
