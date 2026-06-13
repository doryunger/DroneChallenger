#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Brushes/SlateDynamicImageBrush.h"
#include "DroneLoadingWidget.generated.h"

UCLASS()
class DRONECHALLENGER_API UDroneLoadingWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    void Dismiss();

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
    virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
        const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
        int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
    static UTexture2D* LoadPNGTexture(const FString& Path);

    enum class EPhase : uint8 { CarCross, CarWait, DroneCross, DroneWait };

    float  ElapsedTime = 0.f;
    float  FadeAlpha   = 1.f;
    bool   bDismissed  = false;
    EPhase Phase       = EPhase::CarCross;
    float  PhaseTimer  = 0.f;

    UPROPERTY() TObjectPtr<UTexture2D> CarTex;
    UPROPERTY() TObjectPtr<UTexture2D> DroneTex;
    TSharedPtr<FSlateDynamicImageBrush> CarBrush;
    TSharedPtr<FSlateDynamicImageBrush> DroneBrush;
};
