#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Rendering/DrawElements.h"
#include "DroneCountdownWidget.generated.h"

UCLASS()
class DRONECHALLENGER_API UDroneCountdownWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	void StartCountdown();
	void StopTimer();

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
		int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	static constexpr float TotalSeconds = 600.f;
	float RemainingSeconds = TotalSeconds;
	bool  bStarted         = false;
	bool  bStopped         = false;
};
