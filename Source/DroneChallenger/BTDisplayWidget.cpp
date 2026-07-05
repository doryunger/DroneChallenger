#include "BTDisplayWidget.h"
#include "Misc/Paths.h"
#include "Components/Border.h"
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
	if (!bExpanded)
	{
		if (CachedBrowser) CachedBrowser->SetVisibility(ESlateVisibility::Hidden);
		if (CachedPanel)   CachedPanel->SetVisibility(ESlateVisibility::Hidden);
		if (CachedSpinner) CachedSpinner->SetVisibility(ESlateVisibility::Hidden);
		if (UWorld* W = GetWorld())
			W->GetTimerManager().ClearTimer(PanelOpenTimerHandle);
		if (CachedBrowser)
			CachedBrowser->ExecuteJavascript(TEXT("if(window.setPollRate) window.setPollRate(3000);"));
		return;
	}

	if (CachedPanel)   CachedPanel->SetVisibility(ESlateVisibility::Visible);
	if (CachedSpinner) CachedSpinner->SetVisibility(ESlateVisibility::Visible);
	if (CachedBrowser) CachedBrowser->SetVisibility(ESlateVisibility::Hidden);

	if (!bURLLoaded)
		ReloadBrowser();

	if (CachedBrowser)
		CachedBrowser->ExecuteJavascript(TEXT("if(window.setPollRate) window.setPollRate(250);"));

	if (bPageLoaded)
	{
		RevealBrowserPage();
		return;
	}

	PanelOpenPolls = 0;
	if (UWorld* W = GetWorld())
	{
		W->GetTimerManager().SetTimer(PanelOpenTimerHandle,
			this, &UBTDisplayWidget::OnPanelOpenTimer, 0.5f, true);
	}
}

void UBTDisplayWidget::OnPanelOpenTimer()
{
	if (!bExpanded) return;

	++PanelOpenPolls;
	const float Elapsed = PanelOpenPolls * 0.5f;

	if (bPageLoaded || Elapsed >= MaxPanelOpenWait)
		RevealBrowserPage();
}

void UBTDisplayWidget::ReloadBrowser()
{
	if (!CachedBrowser) return;
	FString URL = FPaths::ConvertRelativePathToFull(
		FPaths::ProjectContentDir() / TEXT("HUD/arborist/viewer/viewer.html"));
	URL.ReplaceInline(TEXT("\\"), TEXT("/"));
	URL = TEXT("file:///") + URL + FString::Printf(TEXT("?cb=%lld"), FDateTime::Now().GetTicks());
	CachedBrowser->LoadURL(URL);
	bURLLoaded = true;
	UE_LOG(LogTemp, Log, TEXT("BTDisplay: ReloadBrowser → %s"), *URL);
}

void UBTDisplayWidget::RevealBrowserPage()
{
	bPageLoaded = true;
	if (UWorld* W = GetWorld())
		W->GetTimerManager().ClearTimer(PanelOpenTimerHandle);
	if (bExpanded)
	{
		if (CachedBrowser) CachedBrowser->SetVisibility(ESlateVisibility::Visible);
		if (CachedSpinner) CachedSpinner->SetVisibility(ESlateVisibility::Hidden);
	}
	UE_LOG(LogTemp, Log, TEXT("BTDisplay: page loaded — spinner hidden, browser shown"));
}

void UBTDisplayWidget::HandleConsoleMessage(const FString& Message, const FString& Source, int32 Line)
{
	if (Message.Contains(TEXT("VIEWER_PAGE_LOADED")))
		bPageLoaded = true;
}

void UBTDisplayWidget::OnAfterConstruct()
{
	if (!CachedBrowser)
	{
		UE_LOG(LogTemp, Warning, TEXT("BTDisplay: CachedBrowser null in OnAfterConstruct"));
		return;
	}
	if (CachedPanel)   CachedPanel->SetVisibility(ESlateVisibility::Hidden);
	if (CachedSpinner) CachedSpinner->SetVisibility(ESlateVisibility::Hidden);
	CachedBrowser->SetVisibility(ESlateVisibility::Hidden);
	if (CachedBtn)     CachedBtn->SetVisibility(ESlateVisibility::Hidden);
	UE_LOG(LogTemp, Log, TEXT("BTDisplay: panel+browser+button hidden, preload active"));
}

void UBTDisplayWidget::NotifyLoadingDismissed()
{
	if (UWorld* W = GetWorld())
	{
		W->GetTimerManager().SetTimer(
			ButtonRevealHandle, this, &UBTDisplayWidget::RevealToggleButton, ButtonRevealDelay, false);
	}
}

void UBTDisplayWidget::RevealToggleButton()
{
	if (!CachedBtn) CachedBtn = Cast<UButton>(GetWidgetFromName(TEXT("Button_0")));
	if (CachedBtn) CachedBtn->SetVisibility(ESlateVisibility::Visible);
	UE_LOG(LogTemp, Log, TEXT("BTDisplay: toggle button revealed"));
}

bool UBTDisplayWidget::Initialize()
{
	const bool bResult = Super::Initialize();

	if (WidgetTree && !CachedBrowser)
	{
		WidgetTree->ForEachWidget([this](UWidget* W) {
			if (!CachedBrowser)
				CachedBrowser = Cast<UWebBrowser>(W);
		});
	}

	if (CachedBrowser)
	{
		if (FBoolProperty* Prop = CastField<FBoolProperty>(
				UWebBrowser::StaticClass()->FindPropertyByName(FName(TEXT("bSupportsTransparency")))))
		{
			Prop->SetPropertyValue_InContainer(CachedBrowser, true);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("BTDisplay: Initialize — CachedBrowser %s"),
		CachedBrowser ? TEXT("found") : TEXT("NOT FOUND"));

	return bResult;
}

void UBTDisplayWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (CachedBrowser)
	{
		CachedBrowser->OnConsoleMessage.AddDynamic(this, &UBTDisplayWidget::HandleConsoleMessage);

		FString RealURL = FPaths::ConvertRelativePathToFull(
			FPaths::ProjectContentDir() / TEXT("HUD/arborist/viewer/viewer.html"));
		RealURL.ReplaceInline(TEXT("\\"), TEXT("/"));
		RealURL = TEXT("file:///") + RealURL + FString::Printf(TEXT("?cb=%lld"), FDateTime::Now().GetTicks());
		CachedBrowser->LoadURL(RealURL);
		bURLLoaded = true;
		UE_LOG(LogTemp, Log, TEXT("BTDisplay: preload issued → %s"), *RealURL);
	}

	if (UWorld* W = GetWorld())
		W->GetTimerManager().SetTimerForNextTick(this, &UBTDisplayWidget::OnAfterConstruct);

	CachedPanel   = Cast<UBorder>(GetWidgetFromName(TEXT("PanelBackground")));
	CachedSpinner = GetWidgetFromName(TEXT("LoadingSpinner"));
	UE_LOG(LogTemp, Log, TEXT("BTDisplay: CachedPanel %s  CachedSpinner %s"),
		CachedPanel   ? TEXT("found") : TEXT("NOT FOUND"),
		CachedSpinner ? TEXT("found") : TEXT("NOT FOUND"));

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
	if (bExpanded && !bPageLoaded)
		LoadingPhase = FMath::Fmod(LoadingPhase + InDeltaTime * 0.85f, 1.f);
	StripePhase = FMath::Fmod(StripePhase + InDeltaTime * 40.f, 1000.f);
}

int32 UBTDisplayWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
	int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	if (!CachedBtn) CachedBtn = Cast<UButton>(GetWidgetFromName(TEXT("Button_0")));
	const UCanvasPanelSlot* BtnSlot = CachedBtn ? Cast<UCanvasPanelSlot>(CachedBtn->Slot) : nullptr;
	const FSlateBrush* White = FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
	if (BtnSlot && CachedBtn->GetVisibility() == ESlateVisibility::Visible)
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

	if (bExpanded && !bPageLoaded)
	{
		if (CachedPanel)
		{
			const FGeometry PanelGeom = CachedPanel->GetCachedGeometry();
			const FVector2D AbsPos    = PanelGeom.GetAbsolutePosition();
			const FVector2D AbsSize   = PanelGeom.GetAbsoluteSize();
			if (AbsSize.X > 1.f && AbsSize.Y > 1.f)
			{
				CachedPanelPos  = AllottedGeometry.AbsoluteToLocal(AbsPos);
				CachedPanelSize = AllottedGeometry.AbsoluteToLocal(AbsPos + AbsSize) - CachedPanelPos;
			}
		}

		const FVector2D DrawPos  = CachedPanelSize.X > 1.f ? CachedPanelPos  : FVector2D::ZeroVector;
		const FVector2D DrawSize = CachedPanelSize.X > 1.f ? CachedPanelSize : AllottedGeometry.GetLocalSize();

		if (DrawSize.X > 1.f)
		{
			FSlateDrawElement::MakeBox(OutDrawElements, LayerId,
				AllottedGeometry.ToPaintGeometry(
					FVector2f((float)DrawSize.X, (float)DrawSize.Y),
					FSlateLayoutTransform(FVector2f((float)DrawPos.X, (float)DrawPos.Y))),
				White, ESlateDrawEffect::None,
				FLinearColor(FColor(0x13, 0x14, 0x1f, 0xFF)));
			++LayerId;

			const float SmallDim = FMath::Min((float)DrawSize.X, (float)DrawSize.Y);
			const float SpinR    = SmallDim * 0.055f;
			const float DotSz    = FMath::Max(3.f, SmallDim * 0.018f);
			const float CX       = (float)DrawPos.X + (float)DrawSize.X * 0.5f;
			const float CY       = (float)DrawPos.Y + (float)DrawSize.Y * 0.5f;

			constexpr int32 N = 8;
			for (int32 i = 0; i < N; ++i)
			{
				const float Angle = 2.f * PI * i / N - LoadingPhase * 2.f * PI;
				const float DotX  = CX + SpinR * FMath::Cos(Angle) - DotSz * 0.5f;
				const float DotY  = CY + SpinR * FMath::Sin(Angle) - DotSz * 0.5f;
				const float Alpha = FMath::Pow((float)(N - i) / N, 1.8f) * 0.90f;

				FSlateDrawElement::MakeBox(OutDrawElements, LayerId,
					AllottedGeometry.ToPaintGeometry(
						FVector2f(DotSz, DotSz),
						FSlateLayoutTransform(FVector2f(DotX, DotY))),
					White, ESlateDrawEffect::None,
					FLinearColor(0.72f, 0.80f, 1.00f, Alpha));
			}

			++LayerId;
		}
	}

	LayerId = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements,
		LayerId, InWidgetStyle, bParentEnabled);

	return LayerId;
}
