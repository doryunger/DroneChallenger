#include "DroneMiniMapWidget.h"
#include "DroneActor.h"
#include "TargetPawn.h"
#include "DroneGraph.h"
#include "Rendering/DrawElements.h"

static constexpr float kRoadLineThickness  = 1.0f;
static constexpr float kFrustumThickness   = 1.5f;
static constexpr float kDroneIconSize      = 5.f;
static constexpr float kTargetDotRadius    = 4.f;
static constexpr float kCompassTickLength  = 8.f;
static constexpr int32 kArcSegments        = 64;

namespace
{
    FLinearColor kRoadColor       { 0.25f, 0.30f, 0.45f, 1.f };
    FLinearColor kFrustumColor    { 0.54f, 0.70f, 0.98f, 0.5f };
    FLinearColor kDroneColor      { 0.54f, 0.70f, 0.98f, 1.f };
    FLinearColor kTargetColor     { 0.96f, 0.54f, 0.54f, 1.f };
    FLinearColor kTargetFOVColor  { 0.96f, 0.88f, 0.54f, 1.f };
    FLinearColor kCompassColor    { 0.78f, 0.80f, 0.90f, 0.8f };
    FLinearColor kHeadingColor    { 0.54f, 0.70f, 0.98f, 1.f };
    FLinearColor kRingColor       { 0.30f, 0.32f, 0.45f, 0.6f };
}

void UDroneMiniMapWidget::Init(ADroneActor* InDrone, ATargetPawn* InTarget)
{
    Drone  = InDrone;
    Target = InTarget;
}

void UDroneMiniMapWidget::NativeConstruct()
{
    Super::NativeConstruct();
    bCanEverTick = true;
}

void UDroneMiniMapWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    if (Drone)
    {
        CachedDronePos = Drone->GetActorLocation();
        CachedDroneYaw = Drone->GetActorRotation().Yaw;
    }
    if (Target)
    {
        CachedTargetPos    = Target->GetActorLocation();
        bCachedTargetInFOV = Target->IsDroneInFOV();
    }
}

FVector2D UDroneMiniMapWidget::WorldToMap(const FVector2D& Center, float Scale, const FVector& WorldPos) const
{
    const float RelX = WorldPos.X - CachedDronePos.X;
    const float RelY = WorldPos.Y - CachedDronePos.Y;
    return FVector2D(Center.X + RelY * Scale, Center.Y - RelX * Scale);
}

int32 UDroneMiniMapWidget::NativePaint(
    const FPaintArgs& Args, const FGeometry& AllottedGeometry,
    const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
    int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
    LayerId = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

    const FVector2D Size   = AllottedGeometry.GetLocalSize();
    const FVector2D Center = Size * 0.5f;
    const float     Radius = FMath::Min(Center.X, Center.Y) - 2.f;
    const float     Scale  = Radius / MapRadiusCm;
    const auto      Geo    = AllottedGeometry.ToPaintGeometry();

    auto MakeLines = [&](const TArray<FVector2D>& Points, const FLinearColor& Color, float Thickness, bool bAA = true)
    {
        FSlateDrawElement::MakeLines(OutDrawElements, LayerId, Geo, Points,
            ESlateDrawEffect::None, Color, bAA, Thickness);
    };

    {
        TArray<FVector2D> Ring;
        Ring.Reserve(kArcSegments + 1);
        for (int32 i = 0; i <= kArcSegments; ++i)
        {
            const float A = 2.f * PI * i / kArcSegments;
            Ring.Add(Center + FVector2D(Radius * FMath::Cos(A), Radius * FMath::Sin(A)));
        }
        MakeLines(Ring, kRingColor, 1.5f);
    }

    const TArray<TPair<FString, float>> CompassTicks = {
        { TEXT("N"),   0.f },
        { TEXT("E"),  90.f },
        { TEXT("S"), 180.f },
        { TEXT("W"), 270.f },
    };
    for (const auto& [Label, BearingDeg] : CompassTicks)
    {
        const float BearingRad = FMath::DegreesToRadians(BearingDeg);
        const FVector2D Dir(FMath::Sin(BearingRad), -FMath::Cos(BearingRad));
        const FVector2D Outer = Center + Dir * Radius;
        const FVector2D Inner = Center + Dir * (Radius - kCompassTickLength);
        MakeLines({ Inner, Outer }, kCompassColor, 1.5f);

        const FVector2D TextPos = Center + Dir * (Radius + 6.f) - FVector2D(6.f, 6.f);
        const FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Regular", 9);
        FSlateDrawElement::MakeText(OutDrawElements, LayerId,
            AllottedGeometry.ToPaintGeometry(TextPos, FVector2D(14.f, 14.f)),
            FText::FromString(Label), Font, ESlateDrawEffect::None, kCompassColor);
    }

    {
        const float HeadingRad = FMath::DegreesToRadians(CachedDroneYaw - 90.f);
        const FVector2D HeadingDir(FMath::Cos(HeadingRad), FMath::Sin(HeadingRad));
        const FVector2D TickPos = Center + HeadingDir * Radius;
        const FVector2D TickInner = Center + HeadingDir * (Radius - kCompassTickLength * 1.5f);
        MakeLines({ TickInner, TickPos }, kHeadingColor, 2.5f);
    }

    if (Target)
    {
        const FDroneGraph& Graph = Target->GetGraph();
        for (const auto& [NodeId, Neighbors] : Graph.Adjacency)
        {
            const FVector* PosA = Graph.NodeWorldPos.Find(NodeId);
            if (!PosA) continue;
            const float DistA = FVector2D::Distance(
                FVector2D(PosA->X, PosA->Y),
                FVector2D(CachedDronePos.X, CachedDronePos.Y));
            if (DistA > MapRadiusCm * 1.5f) continue;

            const FVector2D ScreenA = WorldToMap(Center, Scale, *PosA);

            for (int32 NeighborId : Neighbors)
            {
                if (NeighborId <= NodeId) continue;
                const FVector* PosB = Graph.NodeWorldPos.Find(NeighborId);
                if (!PosB) continue;
                const float DistB = FVector2D::Distance(
                    FVector2D(PosB->X, PosB->Y),
                    FVector2D(CachedDronePos.X, CachedDronePos.Y));
                if (DistB > MapRadiusCm * 1.5f) continue;

                const FVector2D ScreenB = WorldToMap(Center, Scale, *PosB);
                MakeLines({ ScreenA, ScreenB }, kRoadColor, kRoadLineThickness, false);
            }
        }
    }

    {
        const float YawRad   = FMath::DegreesToRadians(CachedDroneYaw - 90.f);
        const float HalfFOV  = FMath::DegreesToRadians(FPVHorizontalFOVDeg * 0.5f);
        const float FrustLen = DetectionRangeCm * Scale;
        const float LeftAngle  = YawRad - HalfFOV;
        const float RightAngle = YawRad + HalfFOV;
        const FVector2D LeftEnd  = Center + FVector2D(FMath::Cos(LeftAngle),  FMath::Sin(LeftAngle))  * FrustLen;
        const FVector2D RightEnd = Center + FVector2D(FMath::Cos(RightAngle), FMath::Sin(RightAngle)) * FrustLen;
        MakeLines({ Center, LeftEnd },  kFrustumColor, kFrustumThickness);
        MakeLines({ Center, RightEnd }, kFrustumColor, kFrustumThickness);
        MakeLines({ LeftEnd, RightEnd }, kFrustumColor, kFrustumThickness * 0.5f);
    }

    if (Target)
    {
        const FVector2D TargetScreen = WorldToMap(Center, Scale, CachedTargetPos);
        const float DistFromCenter   = FVector2D::Distance(TargetScreen, Center);
        if (DistFromCenter <= Radius)
        {
            const FLinearColor DotColor = bCachedTargetInFOV ? kTargetFOVColor : kTargetColor;
            TArray<FVector2D> DotRing;
            DotRing.Reserve(17);
            for (int32 i = 0; i <= 16; ++i)
            {
                const float A = 2.f * PI * i / 16.f;
                DotRing.Add(TargetScreen + FVector2D(kTargetDotRadius * FMath::Cos(A),
                                                      kTargetDotRadius * FMath::Sin(A)));
            }
            MakeLines(DotRing, DotColor, 1.5f);
        }
    }

    {
        const float YawRad = FMath::DegreesToRadians(CachedDroneYaw - 90.f);
        const FVector2D Fwd = FVector2D(FMath::Cos(YawRad), FMath::Sin(YawRad));
        const FVector2D Perp(-Fwd.Y, Fwd.X);
        const FVector2D Tip   = Center + Fwd   * kDroneIconSize * 2.f;
        const FVector2D Left  = Center - Fwd   * kDroneIconSize + Perp * kDroneIconSize;
        const FVector2D Right = Center - Fwd   * kDroneIconSize - Perp * kDroneIconSize;
        MakeLines({ Left, Tip, Right }, kDroneColor, 1.5f);
        MakeLines({ Left, Right },      kDroneColor, 1.5f);
    }

    return LayerId + 1;
}
