#include "DroneResultWidget.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "Kismet/GameplayStatics.h"
#include "Math/TransformCalculus2D.h"
#include "Rendering/SlateRenderTransform.h"

struct FRGlyph { uint8 Rows[7]; };

static const FRGlyph RG_A  = {{ 14, 17, 17, 31, 17, 17, 17 }};
static const FRGlyph RG_E  = {{ 31, 16, 16, 30, 16, 16, 31 }};
static const FRGlyph RG_G  = {{ 14, 17, 16, 19, 17, 17, 14 }};
static const FRGlyph RG_I  = {{ 31,  4,  4,  4,  4,  4, 31 }};
static const FRGlyph RG_L  = {{ 16, 16, 16, 16, 16, 16, 31 }};
static const FRGlyph RG_N  = {{ 17, 25, 21, 19, 17, 17, 17 }};
static const FRGlyph RG_O  = {{ 14, 17, 17, 17, 17, 17, 14 }};
static const FRGlyph RG_R  = {{ 30, 17, 17, 30, 20, 18, 17 }};
static const FRGlyph RG_S  = {{ 14, 17, 16, 14,  1, 17, 14 }};
static const FRGlyph RG_T  = {{ 31,  4,  4,  4,  4,  4,  4 }};
static const FRGlyph RG_U  = {{ 17, 17, 17, 17, 17, 17, 14 }};
static const FRGlyph RG_W  = {{ 17, 17, 17, 21, 21, 27, 10 }};
static const FRGlyph RG_Y  = {{ 17, 17, 10,  4,  4,  4,  4 }};

static const FRGlyph* GetResultGlyph(TCHAR C)
{
    switch (C)
    {
    case 'A': return &RG_A;
    case 'E': return &RG_E;
    case 'G': return &RG_G;
    case 'I': return &RG_I;
    case 'L': return &RG_L;
    case 'N': return &RG_N;
    case 'O': return &RG_O;
    case 'R': return &RG_R;
    case 'S': return &RG_S;
    case 'T': return &RG_T;
    case 'U': return &RG_U;
    case 'W': return &RG_W;
    case 'Y': return &RG_Y;
    default:  return nullptr;
    }
}

static float RG_WordWidth(const FString& Word, float PW)
{
    if (Word.IsEmpty()) return 0.f;
    return (float)(Word.Len() * 5 + (Word.Len() - 1)) * PW;
}

static const FLinearColor kGradient[7] = {
    FLinearColor(1.00f, 0.98f, 0.72f),
    FLinearColor(1.00f, 0.90f, 0.35f),
    FLinearColor(0.98f, 0.76f, 0.08f),
    FLinearColor(0.90f, 0.60f, 0.02f),
    FLinearColor(0.76f, 0.44f, 0.01f),
    FLinearColor(0.58f, 0.28f, 0.00f),
    FLinearColor(0.36f, 0.12f, 0.00f),
};

void UDroneResultWidget::SpawnConfetti(const FVector2D& ViewportSize)
{
    const float W = ViewportSize.X;
    const float H = ViewportSize.Y;
    constexpr int32 PiecesPerCorner = 70;

    static const FLinearColor GoldColors[] = {
        FLinearColor(1.00f, 0.84f, 0.20f),
        FLinearColor(1.00f, 0.70f, 0.10f),
        FLinearColor(0.95f, 0.55f, 0.03f),
        FLinearColor(1.00f, 0.95f, 0.60f),
    };

    // InwardSign biases the launch cone from straight up toward the opposite side of the
    // screen, so the left-corner burst fans up-and-right and the right-corner burst fans
    // up-and-left, meeting roughly in the middle.
    auto SpawnFromCorner = [&](float OriginX, float InwardSign)
    {
        for (int32 i = 0; i < PiecesPerCorner; ++i)
        {
            FConfettiPiece P;
            P.Pos = FVector2D(OriginX, H);

            const float AngleDeg = -90.f + InwardSign * FMath::FRandRange(15.f, 75.f);
            const float AngleRad = FMath::DegreesToRadians(AngleDeg);
            const float Speed    = FMath::FRandRange(H * 0.55f, H * 1.05f);
            P.Vel = FVector2D(FMath::Cos(AngleRad), FMath::Sin(AngleRad)) * Speed;

            P.Rotation      = FMath::FRandRange(0.f, 360.f);
            P.RotationSpeed = FMath::FRandRange(-360.f, 360.f);
            P.Size          = FMath::FRandRange(W * 0.006f, W * 0.014f);
            P.Color         = GoldColors[FMath::RandRange(0, UE_ARRAY_COUNT(GoldColors) - 1)];
            P.Color.A       = FMath::FRandRange(0.75f, 1.f);

            ConfettiPieces.Add(P);
        }
    };

    ConfettiPieces.Reset(PiecesPerCorner * 2);
    SpawnFromCorner(0.f, 1.f);
    SpawnFromCorner(W, -1.f);
}

void UDroneResultWidget::ShowResult(bool bInWon)
{
    bWon        = bInWon;
    FadeAlpha   = 0.f;
    ElapsedTime = 0.f;
    bConfettiSpawned = false;
    ConfettiPieces.Reset();
    SetVisibility(ESlateVisibility::Visible);

    if (APlayerController* PC = GetOwningPlayer())
    {
        FInputModeUIOnly Mode;
        Mode.SetWidgetToFocus(TakeWidget());
        Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        PC->SetInputMode(Mode);
        PC->bShowMouseCursor = true;
    }
}

void UDroneResultWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    if (GetVisibility() == ESlateVisibility::Collapsed)
        return;
    ElapsedTime += InDeltaTime;
    FadeAlpha = FMath::Min(FadeAlpha + InDeltaTime * 1.5f, 1.f);

    // Deferred until the first tick because ShowResult() (called before this widget has ever
    // been laid out) doesn't know the viewport size yet, and the burst geometry (launch speed,
    // piece size) is scaled off of it.
    if (bWon && !bConfettiSpawned && MyGeometry.GetLocalSize().X > 0.f)
    {
        SpawnConfetti(MyGeometry.GetLocalSize());
        bConfettiSpawned = true;
    }

    const float Gravity = MyGeometry.GetLocalSize().Y * 0.9f;
    for (FConfettiPiece& P : ConfettiPieces)
    {
        P.Vel.Y    += Gravity * InDeltaTime;
        P.Pos      += P.Vel * InDeltaTime;
        P.Rotation += P.RotationSpeed * InDeltaTime;
    }
}

int32 UDroneResultWidget::NativePaint(
    const FPaintArgs& Args, const FGeometry& AllottedGeometry,
    const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
    int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
    LayerId = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements,
        LayerId, InWidgetStyle, bParentEnabled);

    const FVector2D Size = AllottedGeometry.GetLocalSize();
    const float W = Size.X;
    const float H = Size.Y;
    const float S = FMath::Min(W, H);
    const FSlateBrush* White = FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));

    auto SlateBox = [&](float X, float Y, float BW, float BH, const FLinearColor& C)
    {
        FSlateDrawElement::MakeBox(OutDrawElements, LayerId,
            AllottedGeometry.ToPaintGeometry(FVector2f(BW, BH), FSlateLayoutTransform(FVector2f(X, Y))),
            White, ESlateDrawEffect::None, C);
    };

    SlateBox(0.f, 0.f, W, H, FLinearColor(0.f, 0.f, 0.f, FadeAlpha * 0.75f));
    ++LayerId;

    const FString Title = bWon ? TEXT("YOU WIN") : TEXT("YOU LOSE");
    const float TitlePW  = FMath::Max(2.f, FMath::Floor(W * 0.55f / (Title.Len() * 6.f)));
    const float TitleW   = RG_WordWidth(Title, TitlePW);
    const float TitleH   = 7.f * TitlePW;
    const float TitleX   = (W - TitleW) * 0.5f;
    const float TitleY   = H * 0.25f - TitleH * 0.5f;

    const float ShimmerCycle = (float)Title.Len() + 6.f;
    const float ShimmerPos   = FMath::Fmod(ElapsedTime * 3.f, ShimmerCycle) - 3.f;

    float CurX = TitleX;
    for (int32 ci = 0; ci < Title.Len(); ++ci)
    {
        const float T = FMath::Clamp(FMath::Abs((float)ci - ShimmerPos) / 2.5f, 0.f, 1.f);
        const float Bright = 1.f + 0.35f * FMath::Max(0.f, 1.f - T * 1.5f);

        auto DrawCharAt = [&](float OX, float OY, const FLinearColor& FlatCol, bool bGrad)
        {
            if (const FRGlyph* G = GetResultGlyph(Title[ci]))
            {
                for (int32 row = 0; row < 7; ++row)
                {
                    FLinearColor C = bGrad
                        ? FLinearColor(kGradient[row].R * Bright, kGradient[row].G * Bright, kGradient[row].B * Bright, FadeAlpha)
                        : FLinearColor(FlatCol.R, FlatCol.G, FlatCol.B, FadeAlpha);
                    for (int32 col = 0; col < 5; ++col)
                    {
                        if (G->Rows[row] & (1u << (4 - col)))
                            SlateBox(CurX + OX + col * TitlePW, TitleY + OY + row * TitlePW,
                                TitlePW, TitlePW, C);
                    }
                }
            }
        };

        DrawCharAt(3.f, 3.f, FLinearColor(0.15f, 0.08f, 0.00f), false);
        DrawCharAt(2.f, 2.f, FLinearColor(0.30f, 0.16f, 0.00f), false);
        DrawCharAt(1.f, 1.f, FLinearColor(0.50f, 0.28f, 0.01f), false);
        DrawCharAt(0.f, 0.f, FLinearColor(), true);

        CurX += 6.f * TitlePW;
    }
    ++LayerId;

    const float BtnPixelW = FMath::Max(2.f, FMath::Floor(S * 0.033f / 7.f));
    const FString BtnLabel = TEXT("TRY AGAIN");
    const float BtnLblW = RG_WordWidth(BtnLabel, BtnPixelW);
    const float BtnLblH = 7.f * BtnPixelW;
    const float BtnRX   = BtnLblW * 0.5f + BtnPixelW * 18.f;
    const float BtnRY   = BtnLblH * 0.5f + BtnPixelW * 9.f;
    const float BtnCX   = W * 0.5f;
    const float BtnCY   = H * 0.72f;

    BtnX = BtnCX - BtnRX;
    BtnY = BtnCY - BtnRY;
    BtnW = BtnRX * 2.f;
    BtnH = BtnRY * 2.f;
    bBtnValid = true;

    {
        const float dx = (MousePos.X - BtnCX) / BtnRX;
        const float dy = (MousePos.Y - BtnCY) / BtnRY;
        bBtnHovered = (BtnRX > 0.f) && (dx * dx + dy * dy <= 1.f);
    }

    const FLinearColor FillColor = bBtnHovered
        ? FLinearColor(0.75f, 0.75f, 0.75f, FadeAlpha)
        : FLinearColor(0.00f, 0.00f, 0.00f, FadeAlpha);

    const int32 FillRows = (int32)(BtnRY * 2.f + 1.f);
    for (int32 ry = 0; ry < FillRows; ++ry)
    {
        const float dy    = (float)ry - BtnRY;
        const float ratio = 1.f - (dy / BtnRY) * (dy / BtnRY);
        if (ratio < 0.f) continue;
        const float rx = BtnRX * FMath::Sqrt(ratio);
        SlateBox(BtnCX - rx, BtnCY - BtnRY + ry, rx * 2.f, 1.f, FillColor);
    }
    ++LayerId;

    TArray<FVector2D> Ring;
    Ring.Reserve(49);
    for (int32 k = 0; k <= 48; ++k)
    {
        const float A = 2.f * UE_PI * k / 48.f;
        Ring.Add(FVector2D(BtnCX + BtnRX * FMath::Cos(A), BtnCY + BtnRY * FMath::Sin(A)));
    }
    const FLinearColor RingCol = bBtnHovered
        ? FLinearColor(1.00f, 0.90f, 0.25f, FadeAlpha)
        : FLinearColor(0.85f, 0.60f, 0.04f, FadeAlpha);
    FSlateDrawElement::MakeLines(OutDrawElements, LayerId,
        AllottedGeometry.ToPaintGeometry(), Ring, ESlateDrawEffect::None, RingCol, true, 2.f);
    ++LayerId;

    float BtnCurX = BtnCX - BtnLblW * 0.5f;
    const float BtnTextY = BtnCY - BtnLblH * 0.5f;
    for (int32 ci = 0; ci < BtnLabel.Len(); ++ci)
    {
        if (const FRGlyph* G = GetResultGlyph(BtnLabel[ci]))
        {
            for (int32 row = 0; row < 7; ++row)
            {
                const FLinearColor C = bBtnHovered
                    ? FLinearColor(0.12f, 0.07f, 0.00f, FadeAlpha)
                    : FLinearColor(kGradient[row].R, kGradient[row].G, kGradient[row].B, FadeAlpha);
                for (int32 col = 0; col < 5; ++col)
                {
                    if (G->Rows[row] & (1u << (4 - col)))
                        SlateBox(BtnCurX + col * BtnPixelW, BtnTextY + row * BtnPixelW,
                            BtnPixelW, BtnPixelW, C);
                }
            }
        }
        BtnCurX += 6.f * BtnPixelW;
    }
    ++LayerId;

    if (bWon)
    {
        for (const FConfettiPiece& P : ConfettiPieces)
        {
            const float Alpha = P.Color.A * FadeAlpha;
            if (Alpha <= 0.f) continue;

            const FVector2D PieceSize(P.Size, P.Size * 0.42f);
            const FSlateRenderTransform RotXform(FQuat2D(FMath::DegreesToRadians(P.Rotation)));

            FSlateDrawElement::MakeBox(OutDrawElements, LayerId,
                AllottedGeometry.ToPaintGeometry(
                    FVector2f((float)PieceSize.X, (float)PieceSize.Y),
                    FSlateLayoutTransform(FVector2f(
                        (float)(P.Pos.X - PieceSize.X * 0.5f),
                        (float)(P.Pos.Y - PieceSize.Y * 0.5f))),
                    RotXform),
                White, ESlateDrawEffect::None,
                FLinearColor(P.Color.R, P.Color.G, P.Color.B, Alpha));
        }
        ++LayerId;
    }

    return LayerId;
}

FReply UDroneResultWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    MousePos = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
    return FReply::Unhandled();
}

FReply UDroneResultWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    const FVector2D P = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
    if (bBtnValid)
    {
        const float CX = BtnX + BtnW * 0.5f;
        const float CY = BtnY + BtnH * 0.5f;
        const float RX = BtnW * 0.5f;
        const float RY = BtnH * 0.5f;
        const float DX = (P.X - CX) / RX;
        const float DY = (P.Y - CY) / RY;
        if (DX * DX + DY * DY <= 1.f)
        {
            const FString LevelName = UGameplayStatics::GetCurrentLevelName(this, true);
            UGameplayStatics::OpenLevel(this, FName(*LevelName));
        }
    }
    return FReply::Handled();
}

FCursorReply UDroneResultWidget::NativeOnCursorQuery(const FGeometry& InGeometry, const FPointerEvent& InCursorEvent)
{
    const FVector2D P = InGeometry.AbsoluteToLocal(InCursorEvent.GetScreenSpacePosition());
    if (bBtnValid)
    {
        const float CX = BtnX + BtnW * 0.5f;
        const float CY = BtnY + BtnH * 0.5f;
        const float RX = BtnW * 0.5f;
        const float RY = BtnH * 0.5f;
        const float DX = (P.X - CX) / RX;
        const float DY = (P.Y - CY) / RY;
        if (DX * DX + DY * DY <= 1.f)
            return FCursorReply::Cursor(EMouseCursor::Hand);
    }
    return FCursorReply::Cursor(EMouseCursor::Default);
}
