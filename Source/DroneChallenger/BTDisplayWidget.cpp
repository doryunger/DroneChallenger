#include "BTDisplayWidget.h"
#include "Misc/Paths.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Blueprint/WidgetTree.h"
#include "Styling/CoreStyle.h"
#include "Rendering/DrawElements.h"
#include "WebBrowser.h"

FString UBTDisplayWidget::GetMonitorURL() const
{
	FString HtmlPath = FPaths::ConvertRelativePathToFull(
		FPaths::ProjectContentDir() / TEXT("HUD/arborist/viewer/viewer.html"));
	HtmlPath.ReplaceInline(TEXT("\\"), TEXT("/"));
	return TEXT("file:///") + HtmlPath;
}

FText UBTDisplayWidget::GetToggleLabel() const
{
	return FText::FromString(bExpanded ? TEXT(">") : TEXT("< BT"));
}

void UBTDisplayWidget::TogglePanel()
{
	bExpanded = !bExpanded;
}

void UBTDisplayWidget::NotifyContentReady()
{
	bContentReady = true;
}

void UBTDisplayWidget::HandleConsoleMessage(const FString& Message, const FString& Source, int32 Line)
{
	if (Message.Contains(TEXT("BT_VIEWER_READY")))
		bContentReady = true;
}

void UBTDisplayWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (WidgetTree)
	{
		UWebBrowser* Browser = nullptr;
		WidgetTree->ForEachWidget([&Browser](UWidget* W) {
			if (!Browser) Browser = Cast<UWebBrowser>(W);
		});
		if (Browser)
			Browser->OnConsoleMessage.AddDynamic(this, &UBTDisplayWidget::HandleConsoleMessage);
	}

	CachedBtn = Cast<UButton>(GetWidgetFromName(TEXT("Button_0")));
	if (!CachedBtn) return;
	UButton* Btn = CachedBtn;

	if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Btn->Slot))
		CanvasSlot->SetAutoSize(true);

	FButtonStyle Style = Btn->GetStyle();
	Style.Normal.DrawAs  = ESlateBrushDrawType::NoDrawType;
	Style.Hovered.DrawAs = ESlateBrushDrawType::NoDrawType;
	Style.Pressed.DrawAs = ESlateBrushDrawType::NoDrawType;
	Style.Normal.TintColor   = FSlateColor(FLinearColor::Transparent);
	Style.Hovered.TintColor  = FSlateColor(FLinearColor::Transparent);
	Style.Pressed.TintColor  = FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.25f));
	Style.NormalPadding  = FMargin(12.f, 4.f);
	Style.PressedPadding = FMargin(11.f, 3.f);
	Btn->SetStyle(Style);

	if (WidgetTree)
	{
		WidgetTree->ForEachWidget([](UWidget* W) {
			UTextBlock* TB = Cast<UTextBlock>(W);
			if (!TB) return;
			TB->SetJustification(ETextJustify::Center);
			TB->SetAutoWrapText(false);
			TB->SetShadowOffset(FVector2D(2.f, 2.f));
			TB->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 1.f));
			if (UButtonSlot* BtnSlot = Cast<UButtonSlot>(TB->Slot))
			{
				BtnSlot->SetHorizontalAlignment(HAlign_Center);
				BtnSlot->SetVerticalAlignment(VAlign_Center);
				BtnSlot->SetPadding(FMargin(0.f));
			}
		});
	}
}

void UBTDisplayWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	StripePhase = FMath::Fmod(StripePhase + InDeltaTime * 40.f, 1000.f);
	if (!bContentReady)
	{
		LoadingElapsed += InDeltaTime;
		if (LoadingElapsed >= 15.f)
			bContentReady = true;
	}
}

int32 UBTDisplayWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
	int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	if (!CachedBtn) CachedBtn = Cast<UButton>(GetWidgetFromName(TEXT("Button_0")));
	const UCanvasPanelSlot* BtnSlot = CachedBtn ? Cast<UCanvasPanelSlot>(CachedBtn->Slot) : nullptr;
	const FSlateBrush* White = FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
	if (BtnSlot)
	{
		const FVector2D WSize     = AllottedGeometry.GetLocalSize();
		const FAnchors  Anchors   = BtnSlot->GetAnchors();
		FVector2D LocalSize = CachedBtn->GetDesiredSize();
		if (LocalSize.X < 1.f || LocalSize.Y < 1.f)
			LocalSize = BtnSlot->GetSize();

		const FVector2D AnchorPos(WSize.X * Anchors.Minimum.X, WSize.Y * Anchors.Minimum.Y);
		const FVector2D LocalPos  = AnchorPos + BtnSlot->GetPosition() - LocalSize * BtnSlot->GetAlignment();

		if (LocalSize.X > 1.f && LocalSize.Y > 1.f)
		{
			FSlateDrawElement::MakeBox(OutDrawElements, LayerId,
				AllottedGeometry.ToPaintGeometry(
					FVector2f((float)LocalSize.X, (float)LocalSize.Y),
					FSlateLayoutTransform(FVector2f((float)LocalPos.X, (float)LocalPos.Y))),
				White, ESlateDrawEffect::None,
				FLinearColor(0.10f, 0.07f, 0.01f, 0.95f));

			const float SliceH  = FMath::Max(3.f, LocalSize.Y * 0.08f);
			const float Angle   = 0.50f;
			const float HalfW   = FMath::Max(5.f, LocalSize.X * 0.12f);
			const float Cycle   = LocalSize.X;
			const float Phase0  = FMath::Fmod(StripePhase * 0.6f, Cycle);
			static const FLinearColor StripeColor(0.85f, 0.60f, 0.04f, 0.55f);

			for (float Y = 0.f; Y < LocalSize.Y; Y += SliceH)
			{
				const float RowTop = (float)LocalPos.Y + Y;
				const float RowH   = FMath::Min(SliceH, (float)LocalSize.Y - Y);
				const float DiagX  = Y * Angle;

				for (int32 S = 0; S < 2; ++S)
				{
					float Cx = FMath::Fmod(Phase0 + S * Cycle * 0.5f - DiagX, Cycle);
					if (Cx < 0.f) Cx += Cycle;

					for (int32 Copy = -1; Copy <= 1; ++Copy)
					{
						const float AbsCX = (float)LocalPos.X + Cx + Copy * Cycle;
						const float CL = FMath::Max(AbsCX - HalfW, (float)LocalPos.X);
						const float CR = FMath::Min(AbsCX + HalfW, (float)(LocalPos.X + LocalSize.X));
						if (CR > CL)
							FSlateDrawElement::MakeBox(OutDrawElements, LayerId,
								AllottedGeometry.ToPaintGeometry(
									FVector2f(CR - CL, RowH),
									FSlateLayoutTransform(FVector2f(CL, RowTop))),
								White, ESlateDrawEffect::None, StripeColor);
					}
				}
			}

			if (CachedBtn->IsHovered())
			{
				FSlateDrawElement::MakeBox(OutDrawElements, LayerId,
					AllottedGeometry.ToPaintGeometry(
						FVector2f((float)LocalSize.X, (float)LocalSize.Y),
						FSlateLayoutTransform(FVector2f((float)LocalPos.X, (float)LocalPos.Y))),
					White, ESlateDrawEffect::None,
					FLinearColor(1.f, 1.f, 1.f, 0.12f));
			}

			++LayerId;
		}
	}

	LayerId = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements,
		LayerId, InWidgetStyle, bParentEnabled);

	if (bExpanded && !bContentReady)
	{
		const FVector2D WSize = AllottedGeometry.GetLocalSize();
		const float CX     = WSize.X * 0.5f;
		const float CY     = WSize.Y * 0.5f;
		const float Radius = FMath::Min(WSize.X, WSize.Y) * 0.06f;
		const float DotR   = FMath::Max(2.f, Radius * 0.25f);
		const float Phase  = FMath::Fmod(LoadingElapsed * 2.5f, 1.0f);
		const float HeadDeg = Phase * 360.f;

		for (int32 I = 0; I < 8; ++I)
		{
			const float DotDeg  = I * 45.f;
			const float Diff    = FMath::Fmod(HeadDeg - DotDeg + 360.f, 360.f);
			float A = (Diff < 315.f) ? (1.f - Diff / 315.f) : 0.05f;
			A = FMath::Pow(A, 1.5f) * 0.95f;

			const float Rad = FMath::DegreesToRadians(DotDeg);
			const float DX2 = CX + FMath::Sin(Rad) * Radius;
			const float DY2 = CY - FMath::Cos(Rad) * Radius;

			FSlateDrawElement::MakeBox(OutDrawElements, LayerId,
				AllottedGeometry.ToPaintGeometry(
					FVector2f(DotR * 2.f, DotR * 2.f),
					FSlateLayoutTransform(FVector2f(DX2 - DotR, DY2 - DotR))),
				White, ESlateDrawEffect::None,
				FLinearColor(0.85f, 0.62f, 0.04f, A));
		}
		++LayerId;
	}

	return LayerId;
}
