#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DroneLoadingWidget.generated.h"

UCLASS()
class DRONECHALLENGER_API UDroneLoadingWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    void Dismiss();

protected:
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
    virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
        const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
        int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
    float ElapsedTime = 0.f;
    float FadeAlpha   = 1.f;
    bool  bDismissed  = false;
};
