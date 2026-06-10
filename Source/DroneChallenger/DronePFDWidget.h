#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DronePFDWidget.generated.h"

class ADroneActor;

UCLASS()
class DRONECHALLENGER_API UDronePFDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void Init(ADroneActor* InDrone);

	UPROPERTY(EditDefaultsOnly, Category = "PFD")
	float MaxDisplayPitchDeg = 30.f;

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
		int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	UPROPERTY()
	TObjectPtr<ADroneActor> Drone;

	float CachedPitch      = 0.f;
	float CachedRoll       = 0.f;
	float CachedSpeedMs    = 0.f;
	float CachedAltM       = 0.f;
	float CachedVertSpeedMs = 0.f;
};
