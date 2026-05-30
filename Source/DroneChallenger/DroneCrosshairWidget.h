#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DroneCrosshairWidget.generated.h"

class ADroneActor;

UCLASS()
class DRONECHALLENGER_API UDroneCrosshairWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void Init(ADroneActor* InDrone);

	UPROPERTY(EditDefaultsOnly, Category = "Crosshair")
	float LineLength = 10.f;

	UPROPERTY(EditDefaultsOnly, Category = "Crosshair")
	float LineGap = 4.f;

	UPROPERTY(EditDefaultsOnly, Category = "Crosshair")
	float LineThickness = 1.5f;

	UPROPERTY(EditDefaultsOnly, Category = "Crosshair")
	float DotRadius = 2.f;

	UPROPERTY(EditDefaultsOnly, Category = "Crosshair")
	float ChaseCircleRadius = 14.f;

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
		int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	UPROPERTY()
	TObjectPtr<ADroneActor> Drone;

	bool bCachedFPVMode = false;
};
