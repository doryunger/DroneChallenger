#include "DroneMainMenuWidget.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"
#include "InputCoreTypes.h"
#include "Engine/TextureRenderTarget2D.h"

static void SlateBox(const FGeometry& Geom, FSlateWindowElementList& Out, int32 Layer,
    float X, float Y, float W, float H, const FLinearColor& Col)
{
    FSlateDrawElement::MakeBox(Out, Layer,
        Geom.ToPaintGeometry(FVector2f(W, H), FSlateLayoutTransform(FVector2f(X, Y))),
        FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")), ESlateDrawEffect::None, Col);
}

struct FPixelGlyph { uint8 Rows[7]; };

static const FPixelGlyph GlyphA = {{ 14, 17, 17, 31, 17, 17, 17 }};
static const FPixelGlyph GlyphB = {{ 30, 17, 17, 30, 17, 17, 30 }};
static const FPixelGlyph GlyphC = {{ 14, 17, 16, 16, 16, 17, 14 }};
static const FPixelGlyph GlyphD = {{ 30, 17, 17, 17, 17, 17, 30 }};
static const FPixelGlyph GlyphE = {{ 31, 16, 16, 30, 16, 16, 31 }};
static const FPixelGlyph GlyphF = {{ 31, 16, 16, 30, 16, 16, 16 }};
static const FPixelGlyph GlyphG = {{ 14, 17, 16, 19, 17, 17, 14 }};
static const FPixelGlyph GlyphH = {{ 17, 17, 17, 31, 17, 17, 17 }};
static const FPixelGlyph GlyphI = {{ 31,  4,  4,  4,  4,  4, 31 }};
static const FPixelGlyph GlyphK = {{ 17, 18, 20, 24, 20, 18, 17 }};
static const FPixelGlyph GlyphL = {{ 16, 16, 16, 16, 16, 16, 31 }};
static const FPixelGlyph GlyphM = {{ 17, 27, 21, 17, 17, 17, 17 }};
static const FPixelGlyph GlyphN = {{ 17, 25, 21, 19, 17, 17, 17 }};
static const FPixelGlyph GlyphO = {{ 14, 17, 17, 17, 17, 17, 14 }};
static const FPixelGlyph GlyphP = {{ 30, 17, 17, 30, 16, 16, 16 }};
static const FPixelGlyph GlyphR = {{ 30, 17, 17, 30, 20, 18, 17 }};
static const FPixelGlyph GlyphS = {{ 14, 17, 16, 14,  1, 17, 14 }};
static const FPixelGlyph GlyphT = {{ 31,  4,  4,  4,  4,  4,  4 }};
static const FPixelGlyph GlyphU = {{ 17, 17, 17, 17, 17, 17, 14 }};
static const FPixelGlyph GlyphY = {{ 17, 17, 10,  4,  4,  4,  4 }};

static const FPixelGlyph* GetPixelGlyph(TCHAR C)
{
    switch (C)
    {
    case 'A': return &GlyphA;
    case 'B': return &GlyphB;
    case 'C': return &GlyphC;
    case 'D': return &GlyphD;
    case 'E': return &GlyphE;
    case 'F': return &GlyphF;
    case 'G': return &GlyphG;
    case 'H': return &GlyphH;
    case 'I': return &GlyphI;
    case 'K': return &GlyphK;
    case 'L': return &GlyphL;
    case 'M': return &GlyphM;
    case 'N': return &GlyphN;
    case 'O': return &GlyphO;
    case 'P': return &GlyphP;
    case 'R': return &GlyphR;
    case 'S': return &GlyphS;
    case 'T': return &GlyphT;
    case 'U': return &GlyphU;
    case 'Y': return &GlyphY;
    default:  return nullptr;
    }
}

float UDroneMainMenuWidget::PixelWordWidth(const FString& Word, float PixelW)
{
    if (Word.IsEmpty()) return 0.f;
    return (float)(Word.Len() * 5 + (Word.Len() - 1)) * PixelW;
}

void UDroneMainMenuWidget::DrawPixelWord(
    const FGeometry& Geom, FSlateWindowElementList& Out, int32& Layer,
    const FString& Word, float StartX, float TopY, float PixelW, float Alpha,
    FLinearColor FlatColor) const
{
    const bool bFlat = FlatColor.A > 0.f;
    static const FLinearColor Gradient[7] = {
        FLinearColor(1.00f, 0.98f, 0.72f),
        FLinearColor(1.00f, 0.90f, 0.35f),
        FLinearColor(0.98f, 0.76f, 0.08f),
        FLinearColor(0.90f, 0.60f, 0.02f),
        FLinearColor(0.76f, 0.44f, 0.01f),
        FLinearColor(0.58f, 0.28f, 0.00f),
        FLinearColor(0.36f, 0.12f, 0.00f),
    };

    const float CharStep = 6.f * PixelW;

    float CurX = StartX;
    for (int32 ci = 0; ci < Word.Len(); ++ci)
    {
        if (const FPixelGlyph* G = GetPixelGlyph(Word[ci]))
        {
            for (int32 row = 0; row < 7; ++row)
            {
                FLinearColor Col = bFlat ? FlatColor : Gradient[row];
                Col.A = Alpha;
                for (int32 col = 0; col < 5; ++col)
                    if (G->Rows[row] & (1u << (4 - col)))
                        SlateBox(Geom, Out, Layer,
                            CurX + col * PixelW, TopY + row * PixelW,
                            PixelW, PixelW, Col);
            }
        }
        CurX += CharStep;
    }
    ++Layer;
}

void UDroneMainMenuWidget::DrawTitleScreen(
    const FGeometry& Geom, FSlateWindowElementList& Out, int32& Layer) const
{
    const FVector2D Size = Geom.GetLocalSize();
    const float W = Size.X;
    const float H = Size.Y;

    const FString Word1 = TEXT("DRONE");
    const FString Word2 = TEXT("CHALLENGER");

    const float PixelW = FMath::Max(4.f, FMath::Floor(W * 0.82f / 92.f));
    const float CharH  = 7.f * PixelW;
    const float W1     = PixelWordWidth(Word1, PixelW);
    const float W2     = PixelWordWidth(Word2, PixelW);
    const float GapW   = 4.f * PixelW;
    const float TotalW = W1 + GapW + W2;

    const float TitleX1 = (W - TotalW) * 0.5f;
    const float TitleX2 = TitleX1 + W1 + GapW;
    const float TitleY  = H * 0.08f;

    DrawPixelWord(Geom, Out, Layer, Word1, TitleX1, TitleY, PixelW, FadeAlpha);
    DrawPixelWord(Geom, Out, Layer, Word2, TitleX2, TitleY, PixelW, FadeAlpha);

    const float LineY = TitleY + CharH + PixelW * 2.f;
    const float LineW = TotalW * 0.60f;
    SlateBox(Geom, Out, Layer,
        (W - LineW) * 0.5f, LineY, LineW, FMath::Max(2.f, PixelW * 0.28f),
        FLinearColor(0.68f, 0.40f, 0.02f, FadeAlpha * 0.85f));
    ++Layer;

    if (State != EMenuState::Dialog)
    {
        const float FlashAlpha  = (FMath::Sin(ElapsedTime * 3.2f) * 0.5f + 0.5f) * FadeAlpha;
        const FString PressText = TEXT("PRESS ANY KEY TO START");
        const float PressPixelW = FMath::Max(2.f, FMath::Floor(PixelW * 0.43f));
        const float PressW      = PixelWordWidth(PressText, PressPixelW);
        const float PressX      = (W - PressW) * 0.5f;
        const float PressY      = H * 0.34f;
        DrawPixelWord(Geom, Out, Layer, PressText, PressX, PressY, PressPixelW, FlashAlpha,
            FLinearColor(0.92f, 0.62f, 0.03f, 1.0f));
    }

    if (bDroneBrushReady)
    {
        const float DroneW = W * 0.20f;
        const float DroneH = DroneW;
        FSlateDrawElement::MakeBox(Out, Layer,
            Geom.ToPaintGeometry(FVector2f(DroneW, DroneH),
                FSlateLayoutTransform(FVector2f(W * 0.20f, H * 0.40f))),
            &DroneBrush, ESlateDrawEffect::None,
            FLinearColor(1.f, 1.f, 1.f, FadeAlpha));
        ++Layer;
    }

    if (bCarBrushReady)
    {
        const float CarW = W * 0.50f;
        const float CarH = CarW * (72.f / 128.f);
        FSlateDrawElement::MakeBox(Out, Layer,
            Geom.ToPaintGeometry(FVector2f(CarW, CarH),
                FSlateLayoutTransform(FVector2f(W * 0.46f, H * 0.40f))),
            &CarBrush, ESlateDrawEffect::None,
            FLinearColor(1.f, 1.f, 1.f, FadeAlpha));
        ++Layer;
    }
}

void UDroneMainMenuWidget::DrawDialogScreen(
    const FGeometry& Geom, FSlateWindowElementList& Out, int32& Layer) const
{
    const FVector2D Size = Geom.GetLocalSize();
    const float W = Size.X;
    const float H = Size.Y;
    const float S = FMath::Min(W, H);
    const auto& MS = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();

    SlateBox(Geom, Out, Layer, 0.f, 0.f, W, H,
        FLinearColor(0.f, 0.f, 0.f, 0.45f * FadeAlpha));
    ++Layer;

    const float DW  = W * 0.62f;
    const float DH  = H * 0.84f;
    const float DX  = (W - DW) * 0.5f;
    const float DY  = (H - DH) * 0.5f;
    const float Bdr = FMath::Max(2.f, S * 0.0038f);

    SlateBox(Geom, Out, Layer, DX, DY, DW, DH,
        FLinearColor(0.00f, 0.00f, 0.06f, 0.99f * FadeAlpha));
    ++Layer;

    const FLinearColor GoldBdr(0.70f, 0.42f, 0.02f, FadeAlpha);
    SlateBox(Geom, Out, Layer, DX,             DY,              DW,  Bdr, GoldBdr);
    SlateBox(Geom, Out, Layer, DX,             DY + DH - Bdr,   DW,  Bdr, GoldBdr);
    SlateBox(Geom, Out, Layer, DX,             DY,              Bdr, DH,  GoldBdr);
    SlateBox(Geom, Out, Layer, DX + DW - Bdr,  DY,             Bdr, DH,  GoldBdr);
    ++Layer;

    const FString HdrTxt = TEXT("MISSION BRIEFING");

    float TitlePW = FMath::Max(3.f, FMath::Floor(DW * 0.88f / 95.f));
    float HdrPW   = FMath::Max(2.f, FMath::Floor(TitlePW * 0.52f));
    int32 ISz     = FMath::Clamp(FMath::RoundToInt(S * 0.026f), 12, 30);

    {
        const FSlateFontInfo TmpIF = FCoreStyle::GetDefaultFontStyle("Regular", ISz);
        const float TmpIH      = (float)MS->Measure(TEXT("A"), TmpIF).Y;
        const float TmpILineGp = TmpIH * 1.40f;
        const float Available  = DH * 0.77f - 9.f * TitlePW;
        const float Needed     = 9.f * TmpILineGp + 18.f * HdrPW + TmpIH;
        if (Needed > Available && Available > 0.f)
        {
            const float ScaleF = Available / Needed;
            TitlePW = FMath::Max(2.f, TitlePW * ScaleF);
            HdrPW   = FMath::Max(2.f, HdrPW   * ScaleF);
            ISz     = FMath::Max(8,   FMath::RoundToInt((float)ISz * ScaleF));
        }
    }

    const float HdrW      = PixelWordWidth(HdrTxt, TitlePW);
    const float HdrH      = 7.f * TitlePW;
    const float HdrX      = DX + (DW - HdrW) * 0.5f;
    const float HdrY      = DY + DH * 0.038f;
    DrawPixelWord(Geom, Out, Layer, HdrTxt, HdrX, HdrY, TitlePW, FadeAlpha);

    const float DivY = HdrY + HdrH + TitlePW * 2.f;
    SlateBox(Geom, Out, Layer, DX + Bdr, DivY, DW - Bdr * 2.f,
        FMath::Max(1.f, S * 0.0018f),
        FLinearColor(0.52f, 0.30f, 0.01f, FadeAlpha * 0.65f));
    ++Layer;

    const FSlateFontInfo IF = FCoreStyle::GetDefaultFontStyle("Regular", ISz);
    const float IH       = (float)MS->Measure(TEXT("A"), IF).Y;
    const float ILineGp  = IH * 1.40f;
    const float IPadX    = DW * 0.055f;
    const float IX       = DX + IPadX;
    const float HdrPH    = 7.f * HdrPW;
    const float MaxTextY = DY + DH - DH * 0.16f;
    float       IY       = DivY + DH * 0.032f;

    struct FInstrLine { const TCHAR* Text; bool bHeader; };
    static const FInstrLine Lines[] = {
        { TEXT(""),                                            false },
        { TEXT("THE MISSION"),                               true  },
        { TEXT("Keep the target vehicle in camera view."),     false },
        { TEXT("Score = longest continuous lock-on time."),    false },
        { TEXT("Stay within 1 m of the car to capture it."),   false },
        { TEXT(""),                                            false },
        { TEXT("KEYS"),                                       true  },
        { TEXT("W / S  - throttle up / down"),                 false },
        { TEXT("D / A  - roll right / left"),                  false },
        { TEXT("E / Q  - yaw right / left"),                   false },
        { TEXT("C      - change camera"),                      false },
    };

    Out.PushClip(FSlateClippingZone(Geom.ToPaintGeometry(
        FVector2f(DW - Bdr * 2.f, DH - Bdr * 2.f),
        FSlateLayoutTransform(FVector2f(DX + Bdr, DY + Bdr)))));
    for (const FInstrLine& L : Lines)
    {
        if (IY > MaxTextY) break;
        if (L.Text[0] != TEXT('\0'))
        {
            const FString Str(L.Text);
            if (L.bHeader)
            {
                DrawPixelWord(Geom, Out, Layer, Str, IX, IY, HdrPW, FadeAlpha);
            }
            else
            {
                const float LW = (float)MS->Measure(Str, IF).X;
                const float LH = (float)MS->Measure(Str, IF).Y;
                FSlateDrawElement::MakeText(Out, Layer,
                    Geom.ToPaintGeometry(FVector2f(LW + 4.f, LH + 4.f),
                        FSlateLayoutTransform(FVector2f(IX, IY))),
                    FText::FromString(Str), IF, ESlateDrawEffect::None,
                    FLinearColor(0.86f, 0.86f, 0.86f, FadeAlpha));
            }
        }
        IY += L.bHeader ? (HdrPH + HdrPW * 2.f) : ILineGp;
    }
    Out.PopClip();
    ++Layer;

    const float BtnW = DW * 0.36f;
    const float BtnH = DH * 0.088f;
    const float BtnX = DX + (DW - BtnW) * 0.5f;
    const float BtnY = DY + DH - BtnH - DH * 0.038f;
    CtaMin = FVector2D(BtnX, BtnY);
    CtaMax = FVector2D(BtnX + BtnW, BtnY + BtnH);

    const bool bHov = MousePos.X >= CtaMin.X && MousePos.X <= CtaMax.X &&
                      MousePos.Y >= CtaMin.Y && MousePos.Y <= CtaMax.Y;

    SlateBox(Geom, Out, Layer, BtnX, BtnY, BtnW, BtnH,
        bHov ? FLinearColor(0.04f, 0.06f, 0.20f, FadeAlpha)
             : FLinearColor(0.00f, 0.00f, 0.06f, FadeAlpha));
    const FLinearColor BtnBdr = bHov
        ? FLinearColor(1.00f, 0.90f, 0.25f, FadeAlpha)
        : FLinearColor(0.72f, 0.44f, 0.02f, FadeAlpha);
    SlateBox(Geom, Out, Layer, BtnX,              BtnY,               BtnW, Bdr,  BtnBdr);
    SlateBox(Geom, Out, Layer, BtnX,              BtnY + BtnH - Bdr,  BtnW, Bdr,  BtnBdr);
    SlateBox(Geom, Out, Layer, BtnX,              BtnY,               Bdr,  BtnH, BtnBdr);
    SlateBox(Geom, Out, Layer, BtnX + BtnW - Bdr, BtnY,               Bdr,  BtnH, BtnBdr);
    ++Layer;

    const FString BtnLabel = TEXT("CONTINUE");
    const float BtnPW  = FMath::Max(2.f, FMath::Floor(BtnW * 0.72f / 47.f));
    const float BLW    = PixelWordWidth(BtnLabel, BtnPW);
    const float BLH    = 7.f * BtnPW;
    const float BLX    = BtnX + (BtnW - BLW) * 0.5f;
    const float BLY    = BtnY + (BtnH - BLH) * 0.5f;
    DrawPixelWord(Geom, Out, Layer, BtnLabel, BLX, BLY, BtnPW, FadeAlpha);
}

void UDroneMainMenuWidget::NativeConstruct()
{
    Super::NativeConstruct();
    SetIsFocusable(true);
    SetVisibility(ESlateVisibility::Visible);

    UTextureRenderTarget2D* RT = LoadObject<UTextureRenderTarget2D>(nullptr, TEXT("/Game/RT_CarPreview"));
    if (RT)
    {
        RT->Filter = TF_Nearest;
        RT->UpdateResource();
    }

    CarMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/M_CarPreview"));
    if (CarMat)
    {
        CarBrush.SetResourceObject(CarMat);
        CarBrush.ImageSize = FVector2D(128.f, 72.f);
        bCarBrushReady = true;
    }

    UTextureRenderTarget2D* DroneRT = LoadObject<UTextureRenderTarget2D>(nullptr, TEXT("/Game/RT_DronePreview"));
    if (DroneRT)
    {
        DroneRT->Filter = TF_Nearest;
        DroneRT->UpdateResource();
    }

    DroneMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/M_DronePreview"));
    if (DroneMat)
    {
        DroneBrush.SetResourceObject(DroneMat);
        DroneBrush.ImageSize = FVector2D(128.f, 128.f);
        bDroneBrushReady = true;
    }
}

bool UDroneMainMenuWidget::NativeSupportsKeyboardFocus() const
{
    return true;
}

void UDroneMainMenuWidget::NativeOnFocusLost(const FFocusEvent& InFocusEvent)
{
    Super::NativeOnFocusLost(InFocusEvent);
    if (!bDismissing)
        SetFocus();
}

void UDroneMainMenuWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    ElapsedTime += InDeltaTime;

    if (bDismissing)
    {
        FadeAlpha -= InDeltaTime / 0.55f;
        if (FadeAlpha <= 0.f)
        {
            FadeAlpha = 0.f;
            if (!bContinueFired)
            {
                bContinueFired = true;
                OnContinue.ExecuteIfBound();
            }
        }
    }
}

int32 UDroneMainMenuWidget::NativePaint(
    const FPaintArgs& Args, const FGeometry& AllottedGeometry,
    const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
    int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
    LayerId = Super::NativePaint(Args, AllottedGeometry, MyCullingRect,
        OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

    const FVector2D Size = AllottedGeometry.GetLocalSize();

    {
        constexpr int32 NumBands = 28;
        const FLinearColor BgTop(0.01f, 0.01f, 0.18f);
        const FLinearColor BgBot(0.03f, 0.50f, 0.62f);
        const float BandH  = Size.Y / (float)NumBands;
        const float BgAlpha = bDismissing ? 1.f : FadeAlpha;
        for (int32 i = 0; i < NumBands; ++i)
        {
            const float T = (float)i / (float)(NumBands - 1);
            FLinearColor Col = FMath::Lerp(BgTop, BgBot, T);
            Col.A = BgAlpha;
            SlateBox(AllottedGeometry, OutDrawElements, LayerId,
                0.f, i * BandH, Size.X, FMath::Min(BandH + 1.f, Size.Y - i * BandH), Col);
        }
    }
    ++LayerId;

    DrawTitleScreen(AllottedGeometry, OutDrawElements, LayerId);

    if (State == EMenuState::Dialog)
        DrawDialogScreen(AllottedGeometry, OutDrawElements, LayerId);

    if (bDismissing)
    {
        SlateBox(AllottedGeometry, OutDrawElements, LayerId,
            0.f, 0.f, Size.X, Size.Y,
            FLinearColor(0.f, 0.f, 0.f, 1.f - FadeAlpha));
        ++LayerId;
    }

    return LayerId;
}

FReply UDroneMainMenuWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
    if (!bDismissing && State == EMenuState::Title)
        State = EMenuState::Dialog;
    return FReply::Handled();
}

FReply UDroneMainMenuWidget::NativeOnMouseButtonDown(
    const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (bDismissing) return FReply::Handled();

    if (State == EMenuState::Title)
    {
        State = EMenuState::Dialog;
        return FReply::Handled();
    }

    const FVector2D LocalPos = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
    if (LocalPos.X >= CtaMin.X && LocalPos.X <= CtaMax.X &&
        LocalPos.Y >= CtaMin.Y && LocalPos.Y <= CtaMax.Y)
        bDismissing = true;
    return FReply::Handled();
}

FReply UDroneMainMenuWidget::NativeOnMouseMove(
    const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    MousePos = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
    return FReply::Unhandled();
}

