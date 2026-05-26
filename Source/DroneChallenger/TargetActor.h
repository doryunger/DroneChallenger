#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TargetActor.generated.h"

class UCesiumGlobeAnchorComponent;
class UStaticMeshComponent;
class UPointLightComponent;
class ADroneActor;

namespace bt { class BehaviorTree; }

DECLARE_MULTICAST_DELEGATE_OneParam(FOnTargetCaptured, ATargetActor*);

UCLASS()
class DRONECHALLENGER_API ATargetActor : public AActor
{
	GENERATED_BODY()

public:
	ATargetActor();
	virtual ~ATargetActor() override;

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(EditAnywhere, Category = "Target")
	float DetectionRange = 30000.0f;

	UPROPERTY(EditAnywhere, Category = "Target")
	float CaptureRadius = 3000.0f;

	UPROPERTY(EditAnywhere, Category = "Target")
	float CaptureRequiredTime = 2.0f;

	FOnTargetCaptured OnCaptured;

private:
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UCesiumGlobeAnchorComponent> GlobeAnchor;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UPointLightComponent> Beacon;

	bt::BehaviorTree* Tree = nullptr;
	TObjectPtr<ADroneActor> CachedDrone;

	bool bIsCaptured = false;
	bool bDroneInFOV = false;
	bool bDroneInCaptureRange = false;
	float CaptureTimer = 0.0f;
	float LastDeltaTime = 0.0f;

	void BuildTree();
	void UpdateDroneState();
	bool ComputeDroneInFOV() const;
};
