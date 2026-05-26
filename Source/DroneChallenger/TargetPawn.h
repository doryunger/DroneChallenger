#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "TargetPawn.generated.h"

class UCesiumGlobeAnchorComponent;
class UStaticMeshComponent;
class UPointLightComponent;
class ADroneActor;
class APatrolPath;

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
	TObjectPtr<UPointLightComponent> Beacon;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UCesiumGlobeAnchorComponent> GlobeAnchor;

	bt::BehaviorTree* Tree = nullptr;
	TObjectPtr<ADroneActor> CachedDrone;

	bool bIsCaptured = false;
	bool bDroneInFOV = false;
	bool bDroneInCaptureRange = false;
	float CaptureTimer = 0.0f;
	float LastDeltaTime = 0.0f;
	float CurrentSpeed = 0.0f;
	float SplineDistance = 0.0f;

	void BuildTree();
	void UpdateDroneState();
	bool ComputeDroneInFOV() const;
	void AdvanceAlongPath();
};
