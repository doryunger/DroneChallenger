#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "DroneGraph.h"
#include "TargetPawn.generated.h"

class UCesiumGlobeAnchorComponent;
class UStaticMeshComponent;
class UPointLightComponent;
class ADroneActor;
class APatrolPath;
class ACesiumGeoreference;

namespace bt { class BehaviorTree; }

DECLARE_MULTICAST_DELEGATE_OneParam(FOnTargetCaptured, ATargetPawn*);

UCLASS()
class DRONECHALLENGER_API ATargetPawn : public APawn
{
	GENERATED_BODY()

public:
	ATargetPawn();
	virtual ~ATargetPawn() override;

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(EditAnywhere, Category = "Target|Patrol")
	TObjectPtr<APatrolPath> PatrolPath;

	UPROPERTY(EditAnywhere, Category = "Target|Movement")
	float PatrolSpeed = 500.0f;

	UPROPERTY(EditAnywhere, Category = "Target|Movement")
	float EvadeSpeed = 1000.0f;

	UPROPERTY(EditAnywhere, Category = "Target|Detection")
	float DetectionRange = 30000.0f;

	UPROPERTY(EditAnywhere, Category = "Target|Detection")
	float CaptureRadius = 3000.0f;

	UPROPERTY(EditAnywhere, Category = "Target|Detection")
	float CaptureRequiredTime = 2.0f;

	FOnTargetCaptured OnCaptured;

private:
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Wheel1;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Wheel2;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Wheel3;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Wheel4;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UPointLightComponent> Beacon;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UCesiumGlobeAnchorComponent> GlobeAnchor;

	bt::BehaviorTree* Tree = nullptr;

	UPROPERTY()
	TObjectPtr<ADroneActor> CachedDrone;

	UPROPERTY()
	TObjectPtr<ACesiumGeoreference> CachedGeoreference;

	FDroneGraph    Graph;
	TArray<int32>  CurrentPath;
	int32          PathNodeIndex = 0;

	bool bIsCaptured = false;
	bool bDroneInFOV = false;
	bool bDroneInCaptureRange = false;
	bool bOnEvadePath = false;
	float CaptureTimer = 0.0f;
	float LastDeltaTime = 0.0f;
	float CurrentSpeed = 0.0f;
	float SplineDistance = 0.0f;
	float PositionLogTimer = 0.0f;

	void BuildTree();
	void UpdateDroneState();
	bool ComputeDroneInFOV() const;
	void AdvanceAlongPath();
	void AdvanceAlongGraph();
	void PlaceDroneNearCar(const FVector& CarPos, const FVector& CarForward);
	void TryInitialPlacement();

	// Drone's editor-snapped position — this is where tiles are loaded.
	// All placement searches radiate outward from here.
	FVector2D    DroneEditorXY;
	float        DroneEditorZ      = 0.0f;

	bool         bPlacementDone    = false;
	float        PlacementRadius   = 5000.0f;   // UE cm, grows each failed attempt
	FTimerHandle PlacementTimer;
};
