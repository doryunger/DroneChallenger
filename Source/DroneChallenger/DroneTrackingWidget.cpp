#include "DroneTrackingWidget.h"
#include "DroneActor.h"
#include "TargetPawn.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"

namespace
{
    FString FormatTime(float Seconds)
    {
        const int32 Mins = FMath::FloorToInt(Seconds / 60.f);
        const int32 Secs = FMath::FloorToInt(Seconds) % 60;
        return FString::Printf(TEXT("%d:%02d"), Mins, Secs);
    }
}

void UDroneTrackingWidget::Init(ADroneActor* InDrone, ATargetPawn* InTarget)
{
    Drone  = InDrone;
    Target = InTarget;
}

float UDroneTrackingWidget::GetCurrentTrackingTime() const
{
    return Target ? Target->CurrentTrackingTime : 0.f;
}

float UDroneTrackingWidget::GetBestTrackingTime() const
{
    return Target ? Target->BestTrackingTime : 0.f;
}

bool UDroneTrackingWidget::IsCurrentlyTracking() const
{
    return Target && Target->IsDroneInFOV();
}

FText UDroneTrackingWidget::GetCurrentTimeText() const
{
    return FText::FromString(FormatTime(Target ? Target->CurrentTrackingTime : 0.f));
}

FText UDroneTrackingWidget::GetBestTimeText() const
{
    return FText::FromString(FormatTime(Target ? Target->BestTrackingTime : 0.f));
}

FSlateColor UDroneTrackingWidget::GetTrackingColor() const
{
    const bool bTracking = Target && Target->IsDroneInFOV();
    return bTracking
        ? FSlateColor(FLinearColor(0.50f, 0.92f, 0.50f, 1.f))
        : FSlateColor(FLinearColor(0.78f, 0.80f, 0.88f, 0.90f));
}

void UDroneTrackingWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    if (!Target) return;
    CachedCurrentTime = Target->CurrentTrackingTime;
    CachedBestTime    = Target->BestTrackingTime;
    bCachedTracking   = Target->IsDroneInFOV();
    bCachedHasMoved   = Target->HasDroneMoved();
}

int32 UDroneTrackingWidget::NativePaint(
    const FPaintArgs& Args, const FGeometry& AllottedGeometry,
    const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
    int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
    LayerId = Super::NativePaint(Args, AllottedGeometry, MyCullingRect,
        OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

    const FVector2D Size = AllottedGeometry.GetLocalSize();
    const float W = Size.X;
    const float H = Size.Y;
    const float S = FMath::Min(W, H);

    const int32 FontSz = FMath::Clamp(FMath::RoundToInt(S * 0.023f), 10, 30);
    const FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Bold", FontSz);

    auto Measure = [&](const FString& Str) -> FVector2D
    {
        return FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Str, Font);
    };

    const FString CurrLine = FString::Printf(TEXT("Current Tracking Time:  %s"), *FormatTime(CachedCurrentTime));
    const FString BestLine = FString::Printf(TEXT("Best Tracking Time:  %s"),    *FormatTime(CachedBestTime));

    const FVector2D CurrSz = Measure(CurrLine);
    const FVector2D BestSz = Measure(BestLine);

    const float LineGap = S * 0.006f;
    const float PX      = W * 0.012f;
    const float Off     = FMath::Max(1.f, S * 0.0009f);

    const FLinearColor kStroke { 0.f,   0.f,   0.f,   0.90f };
    const FLinearColor kWhite  { 1.f,   1.f,   1.f,   1.f   };
    const FLinearColor kGreen  { 0.18f, 0.68f, 0.18f, 1.f   };
    const FLinearColor kCurrCol = (bCachedTracking && bCachedHasMoved) ? kGreen : kWhite;

    auto DrawStroked = [&](const FString& Str, float X, float Y,
        const FLinearColor& FgCol, const FVector2D& Sz)
    {
        const float OX[4] = { -Off,  Off, -Off,  Off };
        const float OY[4] = { -Off, -Off,  Off,  Off };
        for (int32 i = 0; i < 4; ++i)
        {
            FSlateDrawElement::MakeText(OutDrawElements, LayerId,
                AllottedGeometry.ToPaintGeometry(
                    FVector2f((float)Sz.X + 2.f, (float)Sz.Y + 2.f),
                    FSlateLayoutTransform(FVector2f(X + OX[i], Y + OY[i]))),
                FText::FromString(Str), Font, ESlateDrawEffect::None, kStroke);
        }
        FSlateDrawElement::MakeText(OutDrawElements, LayerId,
            AllottedGeometry.ToPaintGeometry(
                FVector2f((float)Sz.X + 2.f, (float)Sz.Y + 2.f),
                FSlateLayoutTransform(FVector2f(X, Y))),
            FText::FromString(Str), Font, ESlateDrawEffect::None, FgCol);
    };

    float Y = H * 0.018f;
    DrawStroked(CurrLine, PX, Y, kCurrCol, CurrSz);
    Y += (float)CurrSz.Y + LineGap;
    DrawStroked(BestLine, PX, Y, kWhite,   BestSz);

    return LayerId + 1;
}
