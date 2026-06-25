#include "DroneHUD.h"
#include "BTDisplayWidget.h"
#include "DroneTrackingWidget.h"
#include "DronePFDWidget.h"
#include "DroneCrosshairWidget.h"
#include "DroneLoadingWidget.h"
#include "DroneResultWidget.h"
#include "DroneOptionsWidget.h"
#include "DroneGameMode.h"
#include "DroneActor.h"
#include "TargetPawn.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Canvas.h"
#include "MiniMapWidget.h"
#include "Blueprint/GameViewportSubsystem.h"

void ADroneHUD::BeginPlay()
{
    Super::BeginPlay();

    ADroneActor* Drone  = Cast<ADroneActor>(UGameplayStatics::GetActorOfClass(GetWorld(), ADroneActor::StaticClass()));
    ATargetPawn* Target = Cast<ATargetPawn>(UGameplayStatics::GetActorOfClass(GetWorld(), ATargetPawn::StaticClass()));

    if (BTDisplayWidgetClass)
    {
        BTDisplay = CreateWidget<UBTDisplayWidget>(GetWorld(), BTDisplayWidgetClass);
        if (BTDisplay) BTDisplay->AddToViewport();
    }

    {
        TrackingWidget = CreateWidget<UDroneTrackingWidget>(GetWorld(), UDroneTrackingWidget::StaticClass());
        if (TrackingWidget)
        {
            TrackingWidget->Init(Drone, Target);
            TrackingWidget->AddToViewport();
        }
    }

    {
        MiniMapBrowser = CreateWidget<UMiniMapWidget>(GetWorld(), UMiniMapWidget::StaticClass());
        if (MiniMapBrowser)
        {
            MiniMapBrowser->OnReady.BindUObject(this, &ADroneHUD::OnMinimapReady);
            MiniMapBrowser->AddToViewport();
            MiniMapBrowser->SetRenderOpacity(0.f);

            FString HtmlPath = FPaths::ConvertRelativePathToFull(
                FPaths::ProjectContentDir() / TEXT("HUD/minimap/minimap.html"));
            HtmlPath.ReplaceInline(TEXT("\\"), TEXT("/"));
            MiniMapBrowser->LoadURL(TEXT("file:///") + HtmlPath);
        }
    }

    if (PFDWidgetClass)
    {
        PFDWidget = CreateWidget<UDronePFDWidget>(GetWorld(), PFDWidgetClass);
        if (PFDWidget)
        {
            PFDWidget->Init(Drone);
            PFDWidget->AddToViewport();
        }
    }

    if (CrosshairWidgetClass)
    {
        CrosshairWidget = CreateWidget<UDroneCrosshairWidget>(GetWorld(), CrosshairWidgetClass);
        if (CrosshairWidget)
        {
            CrosshairWidget->Init(Drone);
            CrosshairWidget->AddToViewport();
        }
    }

    if (Target)
        Target->OnAltitudeStable.AddUObject(this, &ADroneHUD::OnSceneReady);

    LoadingWidget = CreateWidget<UDroneLoadingWidget>(GetWorld(), UDroneLoadingWidget::StaticClass());
    if (LoadingWidget)
        LoadingWidget->AddToViewport(10);

    ResultWidget = CreateWidget<UDroneResultWidget>(GetWorld(), UDroneResultWidget::StaticClass());
    if (ResultWidget)
    {
        ResultWidget->AddToViewport(20);
        ResultWidget->SetVisibility(ESlateVisibility::Collapsed);
    }

    if (ADroneGameMode* GM = GetWorld()->GetAuthGameMode<ADroneGameMode>())
        GM->OnGameEnded.AddUObject(this, &ADroneHUD::OnGameEnded);

    OptionsWidget = CreateWidget<UDroneOptionsWidget>(GetWorld(), UDroneOptionsWidget::StaticClass());
    if (OptionsWidget)
        OptionsWidget->AddToViewport(30);

    if (APlayerController* PC = GetOwningPlayerController())
        if (PC->InputComponent)
            PC->InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &ADroneHUD::ToggleOptions);
}

void ADroneHUD::OnSceneReady()
{
    bAltitudeStable = true;
    TryDismissLoading();
}

void ADroneHUD::OnMinimapReady()
{
    bMinimapReady = true;
    TryDismissLoading();
}

void ADroneHUD::TryDismissLoading()
{
    if (!bAltitudeStable || !bMinimapReady) return;

    // Scene is ready (WP cells streamed, lighting settled). Re-enable world
    // rendering that DroneGameInstance suppressed on load, so the now-correct
    // sky is revealed as the loading widget fades out — no half-lit sky flash.
    if (UGameViewportClient* VP = GetWorld() ? GetWorld()->GetGameViewport() : nullptr)
        VP->bDisableWorldRendering = false;

    if (LoadingWidget)  LoadingWidget->Dismiss();
    if (MiniMapBrowser) MiniMapBrowser->SetRenderOpacity(1.f);
}

void ADroneHUD::DrawHUD()
{
    Super::DrawHUD();

    if (!MiniMapBrowser || !Canvas) return;

    FVector2D VP(Canvas->SizeX, Canvas->SizeY);
    if (UGameViewportClient* GVC = GetWorld() ? GetWorld()->GetGameViewport() : nullptr)
        GVC->GetViewportSize(VP);

    const float mmMargin = FMath::Min(VP.X, VP.Y) * 0.018f;
    const float mmSize   = FMath::Min(VP.X * 0.15f, VP.Y * 0.24f);

    if (UGameViewportSubsystem* Sub = UGameViewportSubsystem::Get(GetWorld()))
    {
        FGameViewportWidgetSlot Slot = Sub->GetWidgetSlot(MiniMapBrowser);
        Slot.Anchors   = FAnchors(0.f, 1.f);
        Slot.Alignment = FVector2D(0.f, 1.f);
        Slot.Offsets   = FMargin(mmMargin, mmMargin, mmSize, mmSize);
        Sub->SetWidgetSlot(MiniMapBrowser, Slot);
    }
}

void ADroneHUD::NotifyHUDServerReady()
{
}

void ADroneHUD::ToggleOptions()
{
    if (!OptionsWidget) return;
    // Don't open over the result screen
    if (ResultWidget && ResultWidget->GetVisibility() != ESlateVisibility::Collapsed) return;
    if (OptionsWidget->IsShowing())
        OptionsWidget->Hide();
    else
        OptionsWidget->Show();
}

void ADroneHUD::OnGameEnded(bool bWon)
{
    if (ResultWidget)
        ResultWidget->ShowResult(bWon);
}
