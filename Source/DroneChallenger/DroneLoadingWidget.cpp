#include "DroneLoadingWidget.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"
#include "Misc/FileHelper.h"
#include "IImageWrapperModule.h"
#include "IImageWrapper.h"
#include "Modules/ModuleManager.h"
#include "Engine/Texture2D.h"

static constexpr float CrossDuration = 1.5f;
static constexpr float WaitDuration  = 1.5f;

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

    const FString Base = FPaths::ProjectDir() / TEXT("HUD/");

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
    const float S = FMath::Min(W, H);

    FSlateDrawElement::MakeBox(OutDrawElements, LayerId,
        AllottedGeometry.ToPaintGeometry(
            FVector2f(W, H), FSlateLayoutTransform()),
        FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")),
        ESlateDrawEffect::None,
        FLinearColor(0.f, 0.f, 0.f, FadeAlpha));
    ++LayerId;

    const int32 FontSz = FMath::Clamp(FMath::RoundToInt(S * 0.114f), 42, 144);
    const FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Bold", FontSz);

    const auto& MeasureService = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
    auto MeasureChar = [&](TCHAR C) -> float
    {
        return (float)MeasureService->Measure(FString::Chr(C), Font).X;
    };

    const FString WaveText = TEXT("LOADING...");
    const float Amplitude  = FontSz * 0.22f;
    const float WaveSpeed  = 1.8f;
    const float PhaseStep  = 2.0f * UE_PI / (float)WaveText.Len();
    const float CharH      = (float)MeasureService->Measure(WaveText, Font).Y;

    float TotalW = 0.f;
    for (TCHAR C : WaveText) TotalW += MeasureChar(C);

    const float BaseX = (W - TotalW) * 0.5f;
    const float BaseY = (H - CharH)  * 0.5f;

    const FLinearColor kYellow(1.00f, 0.96f, 0.18f, FadeAlpha);
    const FLinearColor kGold  (0.78f, 0.38f, 0.00f, FadeAlpha);
    const FLinearColor kDepth3(0.15f, 0.08f, 0.00f, FadeAlpha);
    const FLinearColor kDepth2(0.30f, 0.16f, 0.00f, FadeAlpha);
    const FLinearColor kDepth1(0.50f, 0.28f, 0.01f, FadeAlpha);
    const FLinearColor kHilit (1.00f, 0.95f, 0.50f, FadeAlpha * 0.12f);

    float CurX = BaseX;
    for (int32 i = 0; i < WaveText.Len(); ++i)
    {
        const float CW    = MeasureChar(WaveText[i]);
        const float CharY = BaseY + FMath::Sin(ElapsedTime * WaveSpeed + i * PhaseStep) * Amplitude;
        const FString Glyph = FString::Chr(WaveText[i]);

        const float ShimmerCycle = (float)WaveText.Len() + 6.0f;
        const float ShimmerPos   = FMath::Fmod(ElapsedTime * 3.0f, ShimmerCycle) - 3.0f;
        const float T            = FMath::Clamp(FMath::Abs((float)i - ShimmerPos) / 2.5f, 0.f, 1.f);
        const FLinearColor FaceColor(
            FMath::Lerp(kYellow.R, kGold.R, T),
            FMath::Lerp(kYellow.G, kGold.G, T),
            FMath::Lerp(kYellow.B, kGold.B, T),
            FadeAlpha);

        auto DrawGlyph = [&](float OX, float OY, const FLinearColor& Col)
        {
            FSlateDrawElement::MakeText(OutDrawElements, LayerId,
                AllottedGeometry.ToPaintGeometry(
                    FVector2f(CW + 6.f, CharH + 6.f),
                    FSlateLayoutTransform(FVector2f(CurX + OX, CharY + OY))),
                FText::FromString(Glyph), Font, ESlateDrawEffect::None, Col);
        };

        DrawGlyph( 3.f,  3.f, kDepth3);
        DrawGlyph( 2.f,  2.f, kDepth2);
        DrawGlyph( 1.f,  1.f, kDepth1);
        DrawGlyph( 0.f,  0.f, FaceColor);
        DrawGlyph(-0.5f,-1.f, kHilit);

        CurX += CW;
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

            // Scale: 75% at edges, 250% at center
            const float Scale = 0.75f + 1.75f * FMath::Sin(Progress * UE_PI);

            const FVector2D NatSz = ActiveBrush->ImageSize;
            const float AspR  = NatSz.Y > 0.0 ? NatSz.X / NatSz.Y : 1.0f;
            const float BaseH = CharH * 0.35f;
            const float BaseW = BaseH * AspR;
            const float DrawH = BaseH * Scale;
            const float DrawW = BaseW * Scale;

            // X: 15% buffer beyond each end of the text
            const float Buffer  = TotalW * 0.50f;
            const float StartCX = BaseX - Buffer - BaseW * 0.5f;
            const float EndCX   = BaseX + TotalW + Buffer + BaseW * 0.5f;
            const float CenterX = StartCX + Progress * (EndCX - StartCX);
            const float DrawX   = CenterX - DrawW * 0.5f;

            // Y: vertically centered on the text — no vertical movement
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
