#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "DroneGraph.h"
#include "DroneFlightController.h"
#include "TargetPawn.generated.h"

class UCesiumGlobeAnchorComponent;
class UStaticMeshComponent;
class UPointLightComponent;
class USpotLightComponent;
class ADroneActor;
class APatrolPath;
class ACesiumGeoreference;

namespace bt { class BehaviorTree; class DecisionEmitter; class MonitorServer; }
class FDroneHUDServer;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnTargetCaptured, ATargetPawn*);
DECLARE_MULTICAST_DELEGATE(FOnAltitudeStable);

UCLASS()
class DRONECHALLENGER_API ATargetPawn : public APawn
{
	GENERATED_BODY()

public:
	ATargetPawn();
	virtual ~ATargetPawn() override;

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	void NotifyLoadingDismissed();

	UPROPERTY(EditAnywhere, Category = "Target|Patrol")
	TObjectPtr<APatrolPath> PatrolPath;

	UPROPERTY(EditAnywhere, Category = "Target|Movement")
	float PatrolSpeed = 500.0f;

	UPROPERTY(EditAnywhere, Category = "Target|Movement")
	float EvadeSpeed = 1000.0f;

	UPROPERTY(EditAnywhere, Category = "Target|Detection")
	float DetectionRange = 20000.0f;

	UPROPERTY(EditAnywhere, Category = "Target|Detection")
	float DetectionFovDeg = 90.0f;

	UPROPERTY(EditAnywhere, Category = "Target|Detection")
	float CaptureRadius = 300.0f;

	UPROPERTY(EditAnywhere, Category = "Target|Detection")
	float CaptureRequiredTime = 3.0f;

	FOnTargetCaptured OnCaptured;
	FOnAltitudeStable OnAltitudeStable;

	[[nodiscard]] const bt::DecisionEmitter* GetEmitter() const { return Emitter; }

	UPROPERTY(BlueprintReadOnly, Category = "Target|Tracking") float CurrentTrackingTime = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Target|Tracking") float BestTrackingTime = 0.0f;
	[[nodiscard]] const FDroneGraph& GetGraph() const { return Graph; }
	[[nodiscard]] bool IsDroneInFOV()      const { return bDroneInFOV; }
	[[nodiscard]] bool HasDroneMoved()     const { return bDroneHasEverMoved; }
	[[nodiscard]] bool IsHUDServerRunning() const;
	[[nodiscard]] bool IsCarStalled()      const { return bDemoModeRequested && bCarStalled; }

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

	bt::BehaviorTree*    Tree      = nullptr;
	bt::DecisionEmitter* Emitter   = nullptr;
	bt::MonitorServer*   Monitor   = nullptr;
	FDroneHUDServer*     HUDServer = nullptr;

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
	bool bDroneHasEverMoved = false;
	bool bAltitudeConfirmed = false;
	float CaptureTimer = 0.0f;
	float LastDeltaTime = 0.0f;
	float CurrentSpeed = 0.0f;
	float SplineDistance = 0.0f;
	float PositionLogTimer = 0.0f;

	void BuildTree();
	void WriteGraphDataJS();
	void UpdateDroneState();
	bool ComputeDroneInFOV() const;
	void AdvanceAlongPath();
	void AdvanceAlongGraph();
	bool PlaceDroneNearCar(const FVector& CarPos, const FVector& CarForward);
	void TryInitialPlacement();
	bool IsSceneStreamingReady();
	void RevalidateDronePlacement();
	void PeriodicTerrainSnap();
	bool ShouldAcceptAltitude(float CandidateZ);
	void CheckAltitudeStability();
	void RetryHUDServer();

	enum class EDemoPhase : uint8 { Inactive, WaitingToStart, Climb, Spin, Correct, Chase, Done };

	bool       bDemoModeRequested          = false;
	EDemoPhase DemoPhase                  = EDemoPhase::Inactive;
	float      DemoClimbTargetZ           = 0.0f;
	float      DemoSpinStartYaw           = 0.0f;
	int32      DemoWaypointIndex          = 0;

	static constexpr float DemoClimbHeightCm          = 3000.0f;
	static constexpr float DemoSpinTotalDeltaDeg      = 180.0f;
	static constexpr float DemoSpinYawInput           = 0.4f;
	static constexpr float DemoCorrectYawToleranceDeg = 5.0f;
	static constexpr float DemoChaseHighAltAboveCarCm = 8000.0f;
	static constexpr float DemoChaseLowAltAboveCarCm  = 250.0f;
	static constexpr float DemoChaseGlideStartDistCm  = 30000.0f;
	static constexpr float DemoWaypointArriveDistCm   = 500.0f;

	void TickDemoAutopilot(float DeltaTime);
	FDroneControlInput ComputeDemoSteering(const FVector& DronePos, const FVector& TargetXY, float TargetZ) const;
	bool ComputeYawErrorToTarget(const FVector& DronePos, const FVector& TargetXY, float& OutYawError) const;

	bool    bCarStalled       = false;
	bool    bCarStallBaseline = false;
	FVector CarStallLastPos   = FVector::ZeroVector;
	float   CarStallTimer     = 0.0f;
	static constexpr float CarStallCheckInterval = 2.0f;
	static constexpr float CarStallMinMoveCm     = 50.0f;

	FVector      DroneEditorPos;
	float        PlacementRadius    = 20000.0f;
	bool         bPlacementDone     = false;

	float  PlacementTileProgress = -1.f;
	int32  PlacementStablePolls  = 0;
	static constexpr int32 RequiredPlacementStablePolls = 6;
	static constexpr float PostPlacementRecheckDelay = 5.0f;
	static constexpr float PostStreamingReadyBuffer = 8.0f;
	double StreamingReadySeconds = 0.0;

	TArray<float> AltitudeHistory;
	TArray<float> AltStabilitySamples;
	static constexpr int32 AltHistorySize         = 5;
	static constexpr float AltSpikeThreshold      = 500.0f;
	static constexpr int32 AltStabilitySampleCount = 8;
	static constexpr float AltStabilityThresholdCm = 30.0f;
	FTimerHandle PlacementTimer;
	FTimerHandle TerrainSnapTimer;
	FTimerHandle HUDServerRetryTimer;
	FTimerHandle AltStabilityTimer;
	FTimerHandle PostPlacementRecheckTimer;
};
