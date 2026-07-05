#include "DroneOptionsWidget.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/PlatformMisc.h"

// ── 5×7 pixel glyphs (bit 4 = leftmost pixel) ────────────────────────────────
struct FOGlyph { uint8 Rows[7]; };

static const FOGlyph OG_A = {{ 14, 17, 17, 31, 17, 17, 17 }};
static const FOGlyph OG_B = {{ 30, 17, 17, 30, 17, 17, 30 }};
static const FOGlyph OG_C = {{ 14, 17, 16, 16, 16, 17, 14 }};
static const FOGlyph OG_E = {{ 31, 16, 16, 30, 16, 16, 31 }};
static const FOGlyph OG_I = {{ 31,  4,  4,  4,  4,  4, 31 }};
static const FOGlyph OG_K = {{ 17, 18, 20, 24, 20, 18, 17 }};
static const FOGlyph OG_M = {{ 17, 27, 21, 17, 17, 17, 17 }};
static const FOGlyph OG_N = {{ 17, 25, 21, 19, 17, 17, 17 }};
static const FOGlyph OG_O = {{ 14, 17, 17, 17, 17, 17, 14 }};
static const FOGlyph OG_P = {{ 30, 17, 17, 30, 16, 16, 16 }};
static const FOGlyph OG_S = {{ 14, 17, 16, 14,  1, 17, 14 }};
static const FOGlyph OG_T = {{ 31,  4,  4,  4,  4,  4,  4 }};
static const FOGlyph OG_U = {{ 17, 17, 17, 17, 17, 17, 14 }};
static const FOGlyph OG_X = {{ 17, 17, 10,  4, 10, 17, 17 }};

static const FOGlyph* GetOptionsGlyph(TCHAR C)
{
    switch (C)
    {
    case 'A': return &OG_A;
    case 'B': return &OG_B;
    case 'C': return &OG_C;
    case 'E': return &OG_E;
    case 'I': return &OG_I;
    case 'K': return &OG_K;
    case 'M': return &OG_M;
    case 'N': return &OG_N;
    case 'O': return &OG_O;
    case 'P': return &OG_P;
    case 'S': return &OG_S;
    case 'T': return &OG_T;
    case 'U': return &OG_U;
    case 'X': return &OG_X;
    default:  return nullptr;
    }
}

static const FLinearColor kOGradient[7] = {
    FLinearColor(1.00f, 0.98f, 0.72f),
    FLinearColor(1.00f, 0.90f, 0.35f),
    FLinearColor(0.98f, 0.76f, 0.08f),
    FLinearColor(0.90f, 0.60f, 0.02f),
    FLinearColor(0.76f, 0.44f, 0.01f),
    FLinearColor(0.58f, 0.28f, 0.00f),
    FLinearColor(0.36f, 0.12f, 0.00f),
};

static void SlateBox_Opt(const FGeometry& Geom, FSlateWindowElementList& Out, int32 Layer,
    float X, float Y, float W, float H, const FLinearColor& Col)
{
    FSlateDrawElement::MakeBox(Out, Layer,
        Geom.ToPaintGeometry(FVector2f(W, H), FSlateLayoutTransform(FVector2f(X, Y))),
        FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")), ESlateDrawEffect::None, Col);
}

// ── helpers ───────────────────────────────────────────────────────────────────

float UDroneOptionsWidget::PixelWordWidth(const FString& Word, float PW)
{
    if (Word.IsEmpty()) return 0.f;
    return (float)(Word.Len() * 5 + (Word.Len() - 1)) * PW;
}

void UDroneOptionsWidget::DrawPixelWord(
    const FGeometry& Geom, FSlateWindowElementList& Out, int32& Layer,
    const FString& Word, float StartX, float TopY, float PW, float Alpha)
{
    float CurX = StartX;
    for (int32 ci = 0; ci < Word.Len(); ++ci)
    {
        if (const FOGlyph* G = GetOptionsGlyph(Word[ci]))
        {
            for (int32 row = 0; row < 7; ++row)
            {
                FLinearColor Col = kOGradient[row];
                Col.A = Alpha;
                for (int32 col = 0; col < 5; ++col)
                    if (G->Rows[row] & (1u << (4 - col)))
                        SlateBox_Opt(Geom, Out, Layer, CurX + col * PW, TopY + row * PW, PW, PW, Col);
            }
        }
        CurX += 6.f * PW;
    }
    ++Layer;
}

// ── public API ────────────────────────────────────────────────────────────────

bool UDroneOptionsWidget::IsShowing() const
{
    return State != EState::Hidden;
}

bool UDroneOptionsWidget::NativeSupportsKeyboardFocus() const
{
    return true;
}

void UDroneOptionsWidget::Show()
{
    if (State == EState::FadingIn || State == EState::Visible) return;
    State     = EState::FadingIn;
    FadeAlpha = 0.f;
    SetVisibility(ESlateVisibility::Visible);

    UGameplayStatics::SetGamePaused(GetWorld(), true);

    if (APlayerController* PC = GetOwningPlayer())
    {
        FInputModeUIOnly Mode;
        Mode.SetWidgetToFocus(TakeWidget());
        Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        PC->SetInputMode(Mode);
        PC->SetShowMouseCursor(true);
    }
}

void UDroneOptionsWidget::Hide()
{
    if (State == EState::Hidden || State == EState::FadingOut) return;
    State = EState::FadingOut;
}

// ── tick / input ──────────────────────────────────────────────────────────────

void UDroneOptionsWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    if (State == EState::FadingIn)
    {
        FadeAlpha += InDeltaTime / 0.35f;
        if (FadeAlpha >= 1.f)
        {
            FadeAlpha = 1.f;
            State     = EState::Visible;
        }
    }
    else if (State == EState::FadingOut)
    {
        FadeAlpha -= InDeltaTime / 0.28f;
        if (FadeAlpha <= 0.f)
        {
            FadeAlpha = 0.f;
            State     = EState::Hidden;
            SetVisibility(ESlateVisibility::Collapsed);

            UGameplayStatics::SetGamePaused(GetWorld(), false);

            if (APlayerController* PC = GetOwningPlayer())
            {
                PC->SetInputMode(FInputModeGameOnly());
                PC->SetShowMouseCursor(false);
            }
        }
    }
}

FReply UDroneOptionsWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
    if (InKeyEvent.GetKey() == EKeys::Escape)
    {
        Hide();
        return FReply::Handled();
    }
    return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

FReply UDroneOptionsWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    MousePos = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
    return FReply::Unhandled();
}

FReply UDroneOptionsWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (State != EState::Visible) return FReply::Handled();

    const FVector2D P = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());

    if (P.X >= MenuMin.X && P.X <= MenuMax.X && P.Y >= MenuMin.Y && P.Y <= MenuMax.Y)
    {
        ExecuteMainMenu();
        return FReply::Handled();
    }
    if (P.X >= ExitMin.X && P.X <= ExitMax.X && P.Y >= ExitMin.Y && P.Y <= ExitMax.Y)
    {
        ExecuteExit();
        return FReply::Handled();
    }
    return FReply::Handled();
}

FCursorReply UDroneOptionsWidget::NativeOnCursorQuery(const FGeometry& InGeometry, const FPointerEvent& InCursorEvent)
{
    const FVector2D P = InGeometry.AbsoluteToLocal(InCursorEvent.GetScreenSpacePosition());
    if ((P.X >= MenuMin.X && P.X <= MenuMax.X && P.Y >= MenuMin.Y && P.Y <= MenuMax.Y) ||
        (P.X >= ExitMin.X && P.X <= ExitMax.X && P.Y >= ExitMin.Y && P.Y <= ExitMax.Y))
        return FCursorReply::Cursor(EMouseCursor::Hand);
    return FCursorReply::Cursor(EMouseCursor::Default);
}

void UDroneOptionsWidget::NativeOnFocusLost(const FFocusEvent& InFocusEvent)
{
    Super::NativeOnFocusLost(InFocusEvent);
    if (State == EState::FadingIn || State == EState::Visible)
        SetFocus();
}

// ── paint ─────────────────────────────────────────────────────────────────────

int32 UDroneOptionsWidget::NativePaint(
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

    // dim overlay
    SlateBox_Opt(AllottedGeometry, OutDrawElements, LayerId,
        0.f, 0.f, W, H, FLinearColor(0.f, 0.f, 0.f, FadeAlpha * 0.72f));
    ++LayerId;

    // dialog box
    const float DW  = W * 0.48f;
    const float DH  = H * 0.54f;
    const float DX  = (W - DW) * 0.5f;
    const float DY  = (H - DH) * 0.5f;
    const float Bdr = FMath::Max(2.f, S * 0.0038f);

    SlateBox_Opt(AllottedGeometry, OutDrawElements, LayerId, DX, DY, DW, DH,
        FLinearColor(0.00f, 0.00f, 0.06f, 0.99f * FadeAlpha));
    ++LayerId;

    const FLinearColor GoldBdr(0.70f, 0.42f, 0.02f, FadeAlpha);
    SlateBox_Opt(AllottedGeometry, OutDrawElements, LayerId, DX,             DY,             DW,  Bdr, GoldBdr);
    SlateBox_Opt(AllottedGeometry, OutDrawElements, LayerId, DX,             DY + DH - Bdr,  DW,  Bdr, GoldBdr);
    SlateBox_Opt(AllottedGeometry, OutDrawElements, LayerId, DX,             DY,              Bdr, DH,  GoldBdr);
    SlateBox_Opt(AllottedGeometry, OutDrawElements, LayerId, DX + DW - Bdr,  DY,              Bdr, DH,  GoldBdr);
    ++LayerId;

    // title
    const FString Title  = TEXT("OPTIONS");
    const float TitlePW  = FMath::Max(3.f, FMath::Floor(DW * 0.80f / (Title.Len() * 6.f)));
    const float TitleW   = PixelWordWidth(Title, TitlePW);
    const float TitleH   = 7.f * TitlePW;
    const float TitleX   = DX + (DW - TitleW) * 0.5f;
    const float TitleY   = DY + DH * 0.07f;
    DrawPixelWord(AllottedGeometry, OutDrawElements, LayerId, Title, TitleX, TitleY, TitlePW, FadeAlpha);

    // divider
    const float DivY = TitleY + TitleH + TitlePW * 2.f;
    SlateBox_Opt(AllottedGeometry, OutDrawElements, LayerId,
        DX + Bdr, DivY, DW - Bdr * 2.f, FMath::Max(1.f, S * 0.0018f),
        FLinearColor(0.52f, 0.30f, 0.01f, FadeAlpha * 0.65f));
    ++LayerId;

    // option layout — entries centered in area below divider, with explicit gap
    const float OptPW     = FMath::Max(2.f, FMath::Floor(TitlePW * 0.30f));
    const float OptH      = 7.f * OptPW;
    const float OptPadX   = OptPW * 5.f;
    const float OptPadY   = OptPW * 2.5f;

    const FString Opt1 = TEXT("BACK TO MAIN MENU");
    const FString Opt2 = TEXT("EXIT");
    const float Opt1W  = PixelWordWidth(Opt1, OptPW);
    const float Opt2W  = PixelWordWidth(Opt2, OptPW);

    const float AreaTop  = DivY + S * 0.018f;
    const float AreaBot  = DY + DH - S * 0.018f;

    const float EntryH   = OptH + OptPadY * 2.f;
    const float EntryGap = S * 0.04f;
    const float TotalH   = 2.f * EntryH + EntryGap;
    const float StartY   = AreaTop + (AreaBot - AreaTop - TotalH) * 0.5f;

    const float Entry1Top = StartY;
    const float Entry2Top = StartY + EntryH + EntryGap;

    const float Opt1X  = DX + (DW - Opt1W) * 0.5f;
    const float Opt1Y  = Entry1Top + OptPadY;
    const float Opt2X  = DX + (DW - Opt2W) * 0.5f;
    const float Opt2Y  = Entry2Top + OptPadY;

    // update mutable hit rects and hover state
    MenuMin = FVector2D(Opt1X - OptPadX, Entry1Top);
    MenuMax = FVector2D(Opt1X + Opt1W + OptPadX, Entry1Top + EntryH);
    ExitMin = FVector2D(Opt2X - OptPadX, Entry2Top);
    ExitMax = FVector2D(Opt2X + Opt2W + OptPadX, Entry2Top + EntryH);

    bMenuHovered = MousePos.X >= MenuMin.X && MousePos.X <= MenuMax.X &&
                   MousePos.Y >= MenuMin.Y && MousePos.Y <= MenuMax.Y;
    bExitHovered = MousePos.X >= ExitMin.X && MousePos.X <= ExitMax.X &&
                   MousePos.Y >= ExitMin.Y && MousePos.Y <= ExitMax.Y;

    // hover rectangle helper
    auto DrawHoverRect = [&](FVector2D Min, FVector2D Max)
    {
        const float RW = Max.X - Min.X;
        const float RH = Max.Y - Min.Y;
        SlateBox_Opt(AllottedGeometry, OutDrawElements, LayerId,
            Min.X, Min.Y, RW, RH,
            FLinearColor(0.04f, 0.07f, 0.22f, FadeAlpha * 0.88f));
        const float B   = FMath::Max(1.f, Bdr * 0.5f);
        const FLinearColor HC(0.90f, 0.72f, 0.20f, FadeAlpha);
        SlateBox_Opt(AllottedGeometry, OutDrawElements, LayerId, Min.X,     Min.Y,     RW, B,  HC);
        SlateBox_Opt(AllottedGeometry, OutDrawElements, LayerId, Min.X,     Max.Y - B, RW, B,  HC);
        SlateBox_Opt(AllottedGeometry, OutDrawElements, LayerId, Min.X,     Min.Y,     B,  RH, HC);
        SlateBox_Opt(AllottedGeometry, OutDrawElements, LayerId, Max.X - B, Min.Y,     B,  RH, HC);
    };

    if (bMenuHovered) DrawHoverRect(MenuMin, MenuMax);
    if (bExitHovered) DrawHoverRect(ExitMin, ExitMax);
    ++LayerId;

    DrawPixelWord(AllottedGeometry, OutDrawElements, LayerId, Opt1, Opt1X, Opt1Y, OptPW, FadeAlpha);
    DrawPixelWord(AllottedGeometry, OutDrawElements, LayerId, Opt2, Opt2X, Opt2Y, OptPW, FadeAlpha);

    return LayerId;
}

// ── actions ───────────────────────────────────────────────────────────────────

void UDroneOptionsWidget::ExecuteMainMenu()
{
    UGameplayStatics::SetGamePaused(GetWorld(), false);
    UGameplayStatics::OpenLevel(GetWorld(), FName("/Game/Maps/MainMenu"));
}

void UDroneOptionsWidget::ExecuteExit()
{
    UGameplayStatics::SetGamePaused(GetWorld(), false);
    if (APlayerController* PC = GetOwningPlayer())
        PC->ConsoleCommand(TEXT("quit"));
}
