#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DroneResultWidget.generated.h"

UCLASS()
class DRONECHALLENGER_API UDroneResultWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    void ShowResult(bool bWon);

protected:
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
    virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
        const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
        int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual FCursorReply NativeOnCursorQuery(const FGeometry& InGeometry, const FPointerEvent& InCursorEvent) override;

private:
    bool       bWon       = false;
    float      FadeAlpha  = 0.f;
    float      ElapsedTime = 0.f;
    FVector2D  MousePos   = FVector2D::ZeroVector;
    mutable bool  bBtnValid   = false;
    mutable bool  bBtnHovered = false;
    mutable float BtnX        = 0.f;
    mutable float BtnY        = 0.f;
    mutable float BtnW        = 0.f;
    mutable float BtnH        = 0.f;
};
