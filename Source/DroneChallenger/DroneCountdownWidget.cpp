#include "DroneCountdownWidget.h"
#include "DroneGameMode.h"
#include "Styling/CoreStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"

void UDroneCountdownWidget::StartCountdown()
{
	bStarted = true;
}

void UDroneCountdownWidget::StopTimer()
{
	bStopped = true;
}

void UDroneCountdownWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!bStarted || bStopped) return;

	if (ADroneGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ADroneGameMode>() : nullptr)
	{
		const float T = GM->GetRemainingTime();
		if (T >= 0.f)
			RemainingSeconds = T;
	}
}

int32 UDroneCountdownWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
	int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	LayerId = Super::NativePaint(Args, AllottedGeometry, MyCullingRect,
		OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

	if (!bStarted) return LayerId;

	const FVector2D Size = AllottedGeometry.GetLocalSize();
	const float W = Size.X;
	const float H = Size.Y;

	const float Rem     = FMath::Max(0.f, RemainingSeconds);
	const int32 TotalSec = FMath::FloorToInt(Rem);
	const int32 Minutes  = TotalSec / 60;
	const int32 Seconds  = TotalSec % 60;
	const FString TimeStr = FString::Printf(TEXT("%02d:%02d"), Minutes, Seconds);

	// White above 5 min, orange 2–5 min, red below 2 min
	FLinearColor TimerColor = FLinearColor::White;
	if (Rem <= 120.f)
		TimerColor = FLinearColor(1.f, 0.12f, 0.05f, 1.f);
	else if (Rem <= 300.f)
		TimerColor = FLinearColor(1.f, 0.50f, 0.04f, 1.f);

	const int32 TitleFontSz = FMath::Clamp(FMath::RoundToInt(H * 0.0153f), 9, 14);
	const int32 TimerFontSz = FMath::Clamp(FMath::RoundToInt(H * 0.0425f), 19, 36);

	const FSlateFontInfo TitleFont = FCoreStyle::GetDefaultFontStyle("Regular", TitleFontSz);
	const FSlateFontInfo TimerFont = FCoreStyle::GetDefaultFontStyle("Bold",    TimerFontSz);

	static const FString TitleStr = TEXT("Remaining Mission Time");

	const TSharedRef<FSlateFontMeasure> FM =
		FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	const FVector2D TitleSz = FM->Measure(TitleStr, TitleFont);
	const FVector2D TimerSz = FM->Measure(TimeStr,  TimerFont);

	const float PadX = H * 0.017f;
	const float PadY = H * 0.012f;
	const float Gap  = H * 0.007f;

	const float ContentW = FMath::Max((float)TitleSz.X, (float)TimerSz.X);
	const float BoxW = ContentW + PadX * 2.f;
	const float BoxH = (float)TitleSz.Y + Gap + (float)TimerSz.Y + PadY * 2.f;
	const float BoxX = W * 0.5f - BoxW * 0.5f;
	const float BoxY = H * 0.015f;

	static const FSlateBrush* White = FCoreStyle::Get().GetBrush("WhiteBrush");

	FSlateDrawElement::MakeBox(OutDrawElements, LayerId,
		AllottedGeometry.ToPaintGeometry(
			FVector2f(BoxW, BoxH),
			FSlateLayoutTransform(FVector2f(BoxX, BoxY))),
		White, ESlateDrawEffect::None,
		FLinearColor(0.f, 0.f, 0.f, 0.88f));
	++LayerId;

	FSlateDrawElement::MakeText(OutDrawElements, LayerId,
		AllottedGeometry.ToPaintGeometry(
			FVector2f((float)TitleSz.X, (float)TitleSz.Y),
			FSlateLayoutTransform(FVector2f(
				BoxX + (BoxW - (float)TitleSz.X) * 0.5f,
				BoxY + PadY))),
		FText::FromString(TitleStr), TitleFont, ESlateDrawEffect::None,
		FLinearColor::White);

	FSlateDrawElement::MakeText(OutDrawElements, LayerId,
		AllottedGeometry.ToPaintGeometry(
			FVector2f((float)TimerSz.X, (float)TimerSz.Y),
			FSlateLayoutTransform(FVector2f(
				BoxX + (BoxW - (float)TimerSz.X) * 0.5f,
				BoxY + PadY + (float)TitleSz.Y + Gap))),
		FText::FromString(TimeStr), TimerFont, ESlateDrawEffect::None,
		TimerColor);

	return LayerId + 1;
}
