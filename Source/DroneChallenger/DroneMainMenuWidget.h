#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Materials/MaterialInterface.h"
#include "Styling/SlateBrush.h"
#include "DroneMainMenuWidget.generated.h"

DECLARE_DELEGATE(FOnMenuContinue);

UCLASS()
class DRONECHALLENGER_API UDroneMainMenuWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    FOnMenuContinue OnContinue;

    void NotifyPreviewCaptured();

protected:
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
    virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
        const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
        int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
    virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual void NativeOnFocusLost(const FFocusEvent& InFocusEvent) override;
    virtual bool NativeSupportsKeyboardFocus() const override;

private:
    enum class EMenuState : uint8 { Title, Dialog };

    EMenuState State       = EMenuState::Title;
    float      ElapsedTime = 0.f;
    float      FadeAlpha   = 1.f;
    bool       bDismissing    = false;
    bool       bContinueFired = false;

    mutable FVector2D CtaMin  = FVector2D::ZeroVector;
    mutable FVector2D CtaMax  = FVector2D::ZeroVector;
    mutable FVector2D MousePos = FVector2D::ZeroVector;

    UPROPERTY() TObjectPtr<UMaterialInterface> CarMat;
    FSlateBrush    CarBrush;
    bool           bCarBrushReady = false;

    UPROPERTY() TObjectPtr<UMaterialInterface> DroneMat;
    FSlateBrush    DroneBrush;
    bool           bDroneBrushReady = false;

    bool bPreviewCaptured = false;

    static float PixelWordWidth(const FString& Word, float PixelW);

    void DrawPixelWord(const FGeometry& Geom, FSlateWindowElementList& Out, int32& Layer,
        const FString& Word, float StartX, float TopY, float PixelW, float Alpha,
        FLinearColor FlatColor = FLinearColor(0.f, 0.f, 0.f, 0.f)) const;

    void DrawTitleScreen(const FGeometry& Geom, FSlateWindowElementList& Out, int32& Layer) const;
    void DrawDialogScreen(const FGeometry& Geom, FSlateWindowElementList& Out, int32& Layer) const;
};
