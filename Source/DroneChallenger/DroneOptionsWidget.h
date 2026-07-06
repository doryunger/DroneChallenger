#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DroneOptionsWidget.generated.h"

UCLASS()
class DRONECHALLENGER_API UDroneOptionsWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    void Show();
    void Hide();
    bool IsShowing() const;

protected:
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
    virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
        const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
        int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
    virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual FCursorReply NativeOnCursorQuery(const FGeometry& InGeometry, const FPointerEvent& InCursorEvent) override;
    virtual void NativeOnFocusLost(const FFocusEvent& InFocusEvent) override;
    virtual bool NativeSupportsKeyboardFocus() const override;

private:
    enum class EState : uint8 { Hidden, FadingIn, Visible, FadingOut };

    EState    State     = EState::Hidden;
    float     FadeAlpha = 0.f;
    FVector2D MousePos  = FVector2D::ZeroVector;

    mutable FVector2D RestartMin = FVector2D::ZeroVector;
    mutable FVector2D RestartMax = FVector2D::ZeroVector;
    mutable FVector2D MenuMin = FVector2D::ZeroVector;
    mutable FVector2D MenuMax = FVector2D::ZeroVector;
    mutable FVector2D ExitMin = FVector2D::ZeroVector;
    mutable FVector2D ExitMax = FVector2D::ZeroVector;
    mutable bool bRestartHovered = false;
    mutable bool bMenuHovered = false;
    mutable bool bExitHovered = false;

    static float PixelWordWidth(const FString& Word, float PW);
    static void  DrawPixelWord(const FGeometry& Geom, FSlateWindowElementList& Out, int32& Layer,
                               const FString& Word, float X, float Y, float PW, float Alpha);
    void ExecuteRestart();
    void ExecuteMainMenu();
    void ExecuteExit();
};
