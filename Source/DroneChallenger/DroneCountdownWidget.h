#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Rendering/DrawElements.h"
#include "DroneCountdownWidget.generated.h"

class ATargetPawn;

UCLASS()
class DRONECHALLENGER_API UDroneCountdownWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	void Init(ATargetPawn* InTarget);
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

	UPROPERTY() TObjectPtr<ATargetPawn> Target;

	// Vicinity (capture) countdown: appears once the drone is within capture range and counts
	// down from CaptureRequiredTime to 0, disappearing the instant range is lost -- mirrors
	// ATargetPawn::UpdateDroneState's own CaptureTimer, which resets to 0 the moment
	// bDroneInCaptureRange goes false, so there's no separate "losing vicinity" bookkeeping
	// needed here, just read the same two values fresh every tick.
	bool  bCachedInCaptureRange  = false;
	float CachedCaptureRemaining = 0.f;
};
