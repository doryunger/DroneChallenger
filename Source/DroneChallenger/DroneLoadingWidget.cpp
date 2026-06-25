#include "DroneLoadingWidget.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/FileHelper.h"
#include "IImageWrapperModule.h"
#include "IImageWrapper.h"
#include "Modules/ModuleManager.h"
#include "Engine/Texture2D.h"

static constexpr float CrossDuration = 1.5f;
static constexpr float WaitDuration  = 1.5f;

struct FLGlyph { uint8 Rows[7]; };

static const FLGlyph LW_A   = {{ 14, 17, 17, 31, 17, 17, 17 }};
static const FLGlyph LW_D   = {{ 30, 17, 17, 17, 17, 17, 30 }};
static const FLGlyph LW_G   = {{ 14, 17, 16, 19, 17, 17, 14 }};
static const FLGlyph LW_I   = {{ 31,  4,  4,  4,  4,  4, 31 }};
static const FLGlyph LW_L   = {{ 16, 16, 16, 16, 16, 16, 31 }};
static const FLGlyph LW_N   = {{ 17, 25, 21, 19, 17, 17, 17 }};
static const FLGlyph LW_O   = {{ 14, 17, 17, 17, 17, 17, 14 }};
static const FLGlyph LW_Dot = {{  0,  0,  0,  0,  0,  6,  6 }};

static const FLGlyph* GetLoadingGlyph(TCHAR C)
{
    switch (C)
    {
    case 'A': return &LW_A;
    case 'D': return &LW_D;
    case 'G': return &LW_G;
    case 'I': return &LW_I;
    case 'L': return &LW_L;
    case 'N': return &LW_N;
    case 'O': return &LW_O;
    case '.': return &LW_Dot;
    default:  return nullptr;
    }
}

static float LW_WordWidth(const FString& Word, float PW)
{
    if (Word.IsEmpty()) return 0.f;
    return (float)(Word.Len() * 5 + (Word.Len() - 1)) * PW;
}

static const FLinearColor LW_Gradient[7] = {
    FLinearColor(1.00f, 0.98f, 0.72f),
    FLinearColor(1.00f, 0.90f, 0.35f),
    FLinearColor(0.98f, 0.76f, 0.08f),
    FLinearColor(0.90f, 0.60f, 0.02f),
    FLinearColor(0.76f, 0.44f, 0.01f),
    FLinearColor(0.58f, 0.28f, 0.00f),
    FLinearColor(0.36f, 0.12f, 0.00f),
};

UTexture2D* UDroneLoadingWidget::LoadPNGTexture(const FString& Path)
{
    TArray<uint8> FileData;
    if (!FFileHelper::LoadFileToArray(FileData, *Path))
        return nullptr;

    IImageWrapperModule& IWM = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
    TSharedPtr<IImageWrapper> IW = IWM.CreateImageWrapper(EImageFormat::PNG);
    if (!IW.IsValid() || !IW->SetCompressed(FileData.GetData(), FileData.Num()))
        return nullptr;

    TArray<uint8> Raw;
    if (!IW->GetRaw(ERGBFormat::BGRA, 8, Raw))
        return nullptr;

    UTexture2D* Tex = UTexture2D::CreateTransient(IW->GetWidth(), IW->GetHeight(), PF_B8G8R8A8);
    if (!Tex)
        return nullptr;

    FTexture2DMipMap& Mip = Tex->GetPlatformData()->Mips[0];
    void* Data = Mip.BulkData.Lock(LOCK_READ_WRITE);
    FMemory::Memcpy(Data, Raw.GetData(), Raw.Num());
    Mip.BulkData.Unlock();
    Tex->UpdateResource();
    return Tex;
}

void UDroneLoadingWidget::NativeConstruct()
{
    Super::NativeConstruct();

    const FString Base = FPaths::ProjectContentDir() / TEXT("HUD/");

    CarTex   = LoadPNGTexture(Base + TEXT("silhouette_car.png"));
    DroneTex = LoadPNGTexture(Base + TEXT("silhouette_drone.png"));

    if (CarTex)
    {
        CarTex->AddToRoot();
        CarBrush = MakeShared<FSlateDynamicImageBrush>(
            CarTex, FVector2D(CarTex->GetSizeX(), CarTex->GetSizeY()), FName("LoadingCarSil"));
    }
    if (DroneTex)
    {
        DroneTex->AddToRoot();
        DroneBrush = MakeShared<FSlateDynamicImageBrush>(
            DroneTex, FVector2D(DroneTex->GetSizeX(), DroneTex->GetSizeY()), FName("LoadingDroneSil"));
    }
}

void UDroneLoadingWidget::NativeDestruct()
{
    CarBrush.Reset();
    DroneBrush.Reset();
    if (CarTex)   { CarTex->RemoveFromRoot();   CarTex   = nullptr; }
    if (DroneTex) { DroneTex->RemoveFromRoot(); DroneTex = nullptr; }
    Super::NativeDestruct();
}

void UDroneLoadingWidget::Dismiss()
{
    bDismissed = true;
}

void UDroneLoadingWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    ElapsedTime += InDeltaTime;

    if (bDismissed)
    {
        FadeAlpha -= InDeltaTime / 0.8f;
        if (FadeAlpha <= 0.f)
        {
            FadeAlpha = 0.f;
            RemoveFromParent();
        }
        return;
    }

    if (ElapsedTime < 3.f) return;

    PhaseTimer += InDeltaTime;

    switch (Phase)
    {
    case EPhase::CarCross:
        if (PhaseTimer >= CrossDuration) { Phase = EPhase::CarWait;    PhaseTimer = 0.f; }
        break;
    case EPhase::CarWait:
        if (PhaseTimer >= WaitDuration)  { Phase = EPhase::DroneCross; PhaseTimer = 0.f; }
        break;
    case EPhase::DroneCross:
        if (PhaseTimer >= CrossDuration) { Phase = EPhase::DroneWait;  PhaseTimer = 0.f; }
        break;
    case EPhase::DroneWait:
        if (PhaseTimer >= WaitDuration)  { Phase = EPhase::CarCross;   PhaseTimer = 0.f; }
        break;
    }
}

int32 UDroneLoadingWidget::NativePaint(
    const FPaintArgs& Args, const FGeometry& AllottedGeometry,
    const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
    int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
    LayerId = Super::NativePaint(Args, AllottedGeometry, MyCullingRect,
        OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

    const FVector2D Size = AllottedGeometry.GetLocalSize();
    const float W = Size.X;
    const float H = Size.Y;
    const FSlateBrush* White = FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));

    auto SlateBox = [&](float X, float Y, float BW, float BH, const FLinearColor& C)
    {
        FSlateDrawElement::MakeBox(OutDrawElements, LayerId,
            AllottedGeometry.ToPaintGeometry(FVector2f(BW, BH), FSlateLayoutTransform(FVector2f(X, Y))),
            White, ESlateDrawEffect::None, C);
    };

    SlateBox(0.f, 0.f, W, H, FLinearColor(0.f, 0.f, 0.f, FadeAlpha));
    ++LayerId;

    const FString WaveText  = TEXT("LOADING...");
    const float   PixelW    = FMath::Max(2.f, FMath::Floor(W * 0.55f / (WaveText.Len() * 6.f)));
    const float   TotalW    = LW_WordWidth(WaveText, PixelW);
    const float   CharH     = 7.f * PixelW;
    const float   BaseX     = (W - TotalW) * 0.5f;
    const float   BaseY     = (H - CharH)  * 0.5f;
    const float   Amplitude = PixelW * 1.5f;
    const float   WaveSpeed = 1.8f;
    const float   PhaseStep = 2.0f * UE_PI / (float)WaveText.Len();

    const float ShimmerCycle = (float)WaveText.Len() + 6.f;
    const float ShimmerPos   = FMath::Fmod(ElapsedTime * 3.f, ShimmerCycle) - 3.f;

    float CurX = BaseX;
    for (int32 i = 0; i < WaveText.Len(); ++i)
    {
        const float WaveY   = BaseY + FMath::Sin(ElapsedTime * WaveSpeed + i * PhaseStep) * Amplitude;
        const float T       = FMath::Clamp(FMath::Abs((float)i - ShimmerPos) / 2.5f, 0.f, 1.f);
        const float Bright  = 1.f + 0.35f * FMath::Max(0.f, 1.f - T * 1.5f);

        if (const FLGlyph* G = GetLoadingGlyph(WaveText[i]))
        {
            for (int32 row = 0; row < 7; ++row)
            {
                FLinearColor C(
                    LW_Gradient[row].R * Bright,
                    LW_Gradient[row].G * Bright,
                    LW_Gradient[row].B * Bright,
                    FadeAlpha);
                for (int32 col = 0; col < 5; ++col)
                {
                    if (G->Rows[row] & (1u << (4 - col)))
                        SlateBox(CurX + col * PixelW, WaveY + row * PixelW, PixelW, PixelW, C);
                }
            }
        }
        CurX += 6.f * PixelW;
    }
    ++LayerId;

    const bool bCrossing = (Phase == EPhase::CarCross || Phase == EPhase::DroneCross);
    if (bCrossing)
    {
        const TSharedPtr<FSlateDynamicImageBrush>& ActiveBrush =
            (Phase == EPhase::CarCross) ? CarBrush : DroneBrush;

        if (ActiveBrush.IsValid())
        {
            const float Progress = FMath::Clamp(PhaseTimer / CrossDuration, 0.f, 1.f);
            const float Scale    = 0.75f + 1.75f * FMath::Sin(Progress * UE_PI);

            const FVector2D NatSz = ActiveBrush->ImageSize;
            const float AspR  = NatSz.Y > 0.0 ? NatSz.X / NatSz.Y : 1.0f;
            const float BaseH = CharH * 0.35f;
            const float BaseW = BaseH * AspR;
            const float DrawH = BaseH * Scale;
            const float DrawW = BaseW * Scale;

            const float Buffer  = TotalW * 0.50f;
            const float StartCX = BaseX - Buffer - BaseW * 0.5f;
            const float EndCX   = BaseX + TotalW + Buffer + BaseW * 0.5f;
            const float CenterX = StartCX + Progress * (EndCX - StartCX);
            const float DrawX   = CenterX - DrawW * 0.5f;

            const float TextCenterY = BaseY + CharH * 0.5f;
            const float DrawY       = TextCenterY - DrawH * 0.5f;

            const float FadeIn    = FMath::Clamp(Progress / 0.2f, 0.f, 1.f);
            const float FadeOut   = FMath::Clamp((1.f - Progress) / 0.2f, 0.f, 1.f);
            const float IconAlpha = FMath::Min(FadeIn, FadeOut) * FadeAlpha;

            FSlateDrawElement::MakeBox(
                OutDrawElements, LayerId,
                AllottedGeometry.ToPaintGeometry(
                    FVector2f(DrawW, DrawH),
                    FSlateLayoutTransform(FVector2f(DrawX, DrawY))),
                ActiveBrush.Get(),
                ESlateDrawEffect::None,
                FLinearColor(1.f, 1.f, 1.f, IconAlpha));
            ++LayerId;
        }
    }

    return LayerId;
}
