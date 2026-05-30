#include "DroneCrosshairWidget.h"
#include "DroneActor.h"
#include "Rendering/DrawElements.h"

namespace
{
	FLinearColor kCrosshairColor { 0.90f, 0.90f, 0.90f, 0.85f };
	FLinearColor kChaseColor     { 0.70f, 0.70f, 0.70f, 0.50f };
}

void UDroneCrosshairWidget::Init(ADroneActor* InDrone)
{
	Drone = InDrone;
}

void UDroneCrosshairWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (Drone)
		bCachedFPVMode = Drone->IsFPVMode();
}

int32 UDroneCrosshairWidget::NativePaint(
	const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
	int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	LayerId = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements,
		LayerId, InWidgetStyle, bParentEnabled);

	const FVector2D Center = AllottedGeometry.GetLocalSize() * 0.5f;
	const auto      Geo    = AllottedGeometry.ToPaintGeometry();

	auto MakeLines = [&](const TArray<FVector2D>& Points, const FLinearColor& Color, float Thickness)
	{
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, Geo, Points,
			ESlateDrawEffect::None, Color, true, Thickness);
	};

	if (bCachedFPVMode)
	{
		MakeLines({
			FVector2D(Center.X, Center.Y - LineGap - LineLength),
			FVector2D(Center.X, Center.Y - LineGap) },
			kCrosshairColor, LineThickness);

		MakeLines({
			FVector2D(Center.X, Center.Y + LineGap),
			FVector2D(Center.X, Center.Y + LineGap + LineLength) },
			kCrosshairColor, LineThickness);

		MakeLines({
			FVector2D(Center.X - LineGap - LineLength, Center.Y),
			FVector2D(Center.X - LineGap,              Center.Y) },
			kCrosshairColor, LineThickness);

		MakeLines({
			FVector2D(Center.X + LineGap,              Center.Y),
			FVector2D(Center.X + LineGap + LineLength, Center.Y) },
			kCrosshairColor, LineThickness);

		constexpr int32 kDotSegs = 12;
		TArray<FVector2D> Dot;
		Dot.Reserve(kDotSegs + 1);
		for (int32 i = 0; i <= kDotSegs; ++i)
		{
			const float A = 2.f * PI * i / kDotSegs;
			Dot.Add(Center + FVector2D(DotRadius * FMath::Cos(A), DotRadius * FMath::Sin(A)));
		}
		MakeLines(Dot, kCrosshairColor, LineThickness);
	}
	else
	{
		constexpr int32 kCircleSegs = 32;
		TArray<FVector2D> Circle;
		Circle.Reserve(kCircleSegs + 1);
		for (int32 i = 0; i <= kCircleSegs; ++i)
		{
			const float A = 2.f * PI * i / kCircleSegs;
			Circle.Add(Center + FVector2D(ChaseCircleRadius * FMath::Cos(A),
			                              ChaseCircleRadius * FMath::Sin(A)));
		}
		MakeLines(Circle, kChaseColor, LineThickness);

		MakeLines({
			FVector2D(Center.X - 3.f, Center.Y),
			FVector2D(Center.X + 3.f, Center.Y) },
			kChaseColor, LineThickness);

		MakeLines({
			FVector2D(Center.X, Center.Y - 3.f),
			FVector2D(Center.X, Center.Y + 3.f) },
			kChaseColor, LineThickness);
	}

	return LayerId + 1;
}
