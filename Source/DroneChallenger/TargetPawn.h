#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "DroneGraph.h"
#include "TargetPawn.generated.h"

class UCesiumGlobeAnchorComponent;
class UStaticMeshComponent;
class UPointLightComponent;
class USpotLightComponent;
class ADroneActor;
class APatrolPath;
class ACesiumGeoreference;

namespace bt { class BehaviorTree; class DecisionEmitter; class MonitorServer; }

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
	float DetectionRange = 50000.0f;

	UPROPERTY(EditAnywhere, Category = "Target|Detection")
	float CaptureRadius = 100.0f;

	UPROPERTY(EditAnywhere, Category = "Target|Detection")
	float CaptureRequiredTime = 2.0f;

	FOnTargetCaptured OnCaptured;

	[[nodiscard]] const bt::DecisionEmitter* GetEmitter() const { return Emitter; }

	UPROPERTY(BlueprintReadOnly, Category = "Target|Tracking") float CurrentTrackingTime = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Target|Tracking") float BestTrackingTime = 0.0f;
	[[nodiscard]] const FDroneGraph& GetGraph() const { return Graph; }
	[[nodiscard]] bool IsDroneInFOV() const { return bDroneInFOV; }

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
	TObjectPtr<UStaticMeshComponent> BeaconPole;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USpotLightComponent> BeaconBeamLight;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UCesiumGlobeAnchorComponent> GlobeAnchor;

	bt::BehaviorTree*    Tree    = nullptr;
	bt::DecisionEmitter* Emitter = nullptr;
	bt::MonitorServer*   Monitor = nullptr;

	UPROPERTY()
	TObjectPtr<ADroneActor> CachedDrone;

	UPROPERTY()
	TObjectPtr<ACesiumGeoreference> CachedGeoreference;

	FDroneGraph    Graph;
	TArray<int32>  CurrentPath;
	int32          PathNodeIndex = 0;

	bool bIsCaptured = false;
	bool bDroneInFOV = false;
	bool bWasInFOV = false;
	bool bDroneInCaptureRange = false;
	bool bDroneHasLOS = false;
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
	void PeriodicTerrainSnap();
	bool ShouldAcceptAltitude(float CandidateZ);

	FVector      DroneEditorPos;
	float        PlacementRadius    = 20000.0f;
	bool         bPlacementDone     = false;

	TArray<float> AltitudeHistory;
	static constexpr int32 AltHistorySize    = 5;
	static constexpr float AltSpikeThreshold = 500.0f;  // 5 m spike tolerance
	FTimerHandle PlacementTimer;
	FTimerHandle TerrainSnapTimer;
};
