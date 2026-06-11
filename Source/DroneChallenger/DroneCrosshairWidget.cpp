#include "DroneCrosshairWidget.h"
#include "DroneActor.h"
#include "Rendering/DrawElements.h"

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

	if (!bCachedFPVMode) return LayerId + 1;

	const FVector2D Size   = AllottedGeometry.GetLocalSize();
	const FVector2D Center = Size * 0.5f;
	const auto      Geo    = AllottedGeometry.ToPaintGeometry();
	const float     S      = FMath::Min(Size.X, Size.Y);

	const float Len   = S * 0.028f;
	const float Gap   = S * 0.011f;
	const float Thick = S * 0.002f;
	const float DotR  = S * 0.004f;
	const float ShadowOff   = S * 0.001f;
	const float ShadowThick = Thick * 1.75f;

	const FLinearColor kShadow{ 0.f,  0.f,  0.f,  0.75f };
	const FLinearColor kBright{ 1.f,  1.f,  1.f,  0.95f };

	auto MakeLines = [&](const TArray<FVector2D>& Points, const FLinearColor& Color, float Thickness)
	{
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, Geo, Points,
			ESlateDrawEffect::None, Color, true, Thickness);
	};

	auto Seg = [&](FVector2D A, FVector2D B)
	{
		const FVector2D Off(ShadowOff, ShadowOff);
		MakeLines({ A + Off, B + Off }, kShadow, ShadowThick);
		MakeLines({ A, B }, kBright, Thick);
	};

	Seg({ Center.X,             Center.Y - Gap - Len }, { Center.X,             Center.Y - Gap       });
	Seg({ Center.X,             Center.Y + Gap       }, { Center.X,             Center.Y + Gap + Len });
	Seg({ Center.X - Gap - Len, Center.Y             }, { Center.X - Gap,        Center.Y             });
	Seg({ Center.X + Gap,       Center.Y             }, { Center.X + Gap + Len,  Center.Y             });

	constexpr int32 kSegs = 16;
	TArray<FVector2D> Dot;
	Dot.Reserve(kSegs + 1);
	for (int32 i = 0; i <= kSegs; ++i)
	{
		const float A = 2.f * UE_PI * i / kSegs;
		Dot.Add(Center + FVector2D(DotR * FMath::Cos(A), DotR * FMath::Sin(A)));
	}
	const FVector2D Off(ShadowOff, ShadowOff);
	TArray<FVector2D> DotShadow;
	DotShadow.Reserve(kSegs + 1);
	for (const FVector2D& P : Dot) DotShadow.Add(P + Off);
	MakeLines(DotShadow, kShadow, ShadowThick);
	MakeLines(Dot, kBright, Thick);

	return LayerId + 1;
}
