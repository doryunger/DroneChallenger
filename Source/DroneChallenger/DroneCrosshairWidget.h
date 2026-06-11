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
