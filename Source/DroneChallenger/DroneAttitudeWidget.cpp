#include "DroneAttitudeWidget.h"
#include "DroneActor.h"
#include "Rendering/DrawElements.h"

namespace
{
    FLinearColor kArcColor      { 0.54f, 0.70f, 0.98f, 0.7f };
    FLinearColor kHorizonColor  { 0.64f, 0.87f, 0.64f, 1.f };
    FLinearColor kRefColor      { 0.78f, 0.80f, 0.90f, 0.9f };
    FLinearColor kPitchColor    { 0.78f, 0.80f, 0.90f, 0.4f };
}

void UDroneAttitudeWidget::Init(ADroneActor* InDrone)
{
    Drone = InDrone;
}

void UDroneAttitudeWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    if (Drone)
    {
        const FRotator Rot = Drone->GetActorRotation();
        CachedPitch = Rot.Pitch;
        CachedRoll  = Rot.Roll;
    }
}

int32 UDroneAttitudeWidget::NativePaint(
    const FPaintArgs& Args, const FGeometry& AllottedGeometry,
    const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
    int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
    LayerId = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

    const FVector2D Size    = AllottedGeometry.GetLocalSize();
    const FVector2D Center  = Size * 0.5f;
    const float     HalfW   = Center.X;
    const float     HalfH   = Center.Y;
    const auto      Geo     = AllottedGeometry.ToPaintGeometry();

    auto MakeLines = [&](const TArray<FVector2D>& Points, const FLinearColor& Color, float Thickness, bool bAA = true)
    {
        FSlateDrawElement::MakeLines(OutDrawElements, LayerId, Geo, Points,
            ESlateDrawEffect::None, Color, bAA, Thickness);
    };

    constexpr int32 kArcSegs = 40;
    const float ArcRadius = HalfW * 0.7f;
    {
        TArray<FVector2D> ArcPts;
        ArcPts.Reserve(kArcSegs + 1);
        for (int32 i = 0; i <= kArcSegs; ++i)
        {
            const float A = FMath::Lerp(PI, 2.f * PI, (float)i / kArcSegs);
            ArcPts.Add(Center + FVector2D(ArcRadius * FMath::Cos(A), ArcRadius * FMath::Sin(A)));
        }
        MakeLines(ArcPts, kArcColor, 1.5f);

        const float WingLen = HalfW - ArcRadius - 4.f;
        MakeLines({ Center + FVector2D(-ArcRadius, 0.f), Center + FVector2D(-ArcRadius - WingLen, 0.f) }, kArcColor, 1.5f);
        MakeLines({ Center + FVector2D( ArcRadius, 0.f), Center + FVector2D( ArcRadius + WingLen, 0.f) }, kArcColor, 1.5f);
    }

    const TArray<float> PitchMarks = { -20.f, -10.f, 10.f, 20.f };
    for (float Mark : PitchMarks)
    {
        const float OffsetY = -(Mark / MaxDisplayPitchDeg) * HalfH;
        const float TickHalfW = HalfW * 0.2f;
        MakeLines(
            { FVector2D(Center.X - TickHalfW, Center.Y + OffsetY),
              FVector2D(Center.X + TickHalfW, Center.Y + OffsetY) },
            kPitchColor, 1.f, false);
    }

    {
        const float PitchOffset = FMath::Clamp(-(CachedPitch / MaxDisplayPitchDeg) * HalfH, -HalfH, HalfH);
        const float RollRad     = FMath::DegreesToRadians(CachedRoll);
        const float CosR = FMath::Cos(RollRad);
        const float SinR = FMath::Sin(RollRad);
        const float HLen = HalfW * 0.85f;

        const FVector2D HorizonCenter(Center.X, Center.Y + PitchOffset);
        const FVector2D LeftPt ( HorizonCenter.X - HLen * CosR, HorizonCenter.Y - HLen * SinR);
        const FVector2D RightPt( HorizonCenter.X + HLen * CosR, HorizonCenter.Y + HLen * SinR);
        MakeLines({ LeftPt, RightPt }, kHorizonColor, 2.f);
    }

    {
        constexpr float RefGap  = 12.f;
        constexpr float RefLen  = 18.f;
        constexpr float RefH    = 5.f;
        MakeLines({
            FVector2D(Center.X - RefGap - RefLen, Center.Y),
            FVector2D(Center.X - RefGap,           Center.Y),
            FVector2D(Center.X - RefGap,           Center.Y + RefH) }, kRefColor, 1.5f);
        MakeLines({
            FVector2D(Center.X + RefGap + RefLen, Center.Y),
            FVector2D(Center.X + RefGap,           Center.Y),
            FVector2D(Center.X + RefGap,           Center.Y + RefH) }, kRefColor, 1.5f);
        MakeLines({ FVector2D(Center.X, Center.Y - 3.f), FVector2D(Center.X, Center.Y + 3.f) }, kRefColor, 1.5f);
    }

    return LayerId + 1;
}
