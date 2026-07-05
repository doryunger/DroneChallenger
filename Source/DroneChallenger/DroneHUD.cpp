#include "DroneHUD.h"
#include "BTDisplayWidget.h"
#include "DroneTrackingWidget.h"
#include "DronePFDWidget.h"
#include "DroneCrosshairWidget.h"
#include "DroneLoadingWidget.h"
#include "DroneResultWidget.h"
#include "DroneCountdownWidget.h"
#include "DroneOptionsWidget.h"
#include "DroneGameMode.h"
#include "DroneActor.h"
#include "TargetPawn.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Canvas.h"
#include "MiniMapWidget.h"
#include "Blueprint/GameViewportSubsystem.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "WorldPartition/WorldPartitionSubsystem.h"

void ADroneHUD::BeginPlay()
{
    Super::BeginPlay();

    CachedDrone         = Cast<ADroneActor>(UGameplayStatics::GetActorOfClass(GetWorld(), ADroneActor::StaticClass()));
    ADroneActor* Drone  = CachedDrone.Get();
    ATargetPawn* Target = Cast<ATargetPawn>(UGameplayStatics::GetActorOfClass(GetWorld(), ATargetPawn::StaticClass()));
    CachedTarget        = Target;

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

    CountdownWidget = CreateWidget<UDroneCountdownWidget>(GetWorld(), UDroneCountdownWidget::StaticClass());
    if (CountdownWidget)
        CountdownWidget->AddToViewport(5);

    OptionsWidget = CreateWidget<UDroneOptionsWidget>(GetWorld(), UDroneOptionsWidget::StaticClass());
    if (OptionsWidget)
        OptionsWidget->AddToViewport(30);

    if (APlayerController* PC = GetOwningPlayerController())
        if (PC->InputComponent)
            PC->InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &ADroneHUD::ToggleOptions);

    if (CachedDrone)
        CachedDrone->DisableInput(GetOwningPlayerController());

    GetWorldTimerManager().SetTimer(
        TileStreamPollHandle, this, &ADroneHUD::PollTileStreaming, 0.5f, true);

    GetWorldTimerManager().SetTimer(
        MinimapTimeoutHandle, this, &ADroneHUD::OnMinimapTimeout, 45.f, false);

    GetWorldTimerManager().SetTimer(
        TileStreamTimeoutHandle, this, &ADroneHUD::OnTileStreamTimeout, 60.f, false);
}

void ADroneHUD::OnSceneReady()
{
    bAltitudeStable = true;
    TryDismissLoading();
}

void ADroneHUD::OnMinimapReady()
{
    GetWorldTimerManager().ClearTimer(MinimapTimeoutHandle);
    bMinimapReady = true;
    TryDismissLoading();
}

void ADroneHUD::OnMinimapTimeout()
{
    if (bMinimapReady) return;
    UE_LOG(LogTemp, Warning, TEXT("DroneHUD: minimap did not signal ready within 45s — proceeding without it"));
    bMinimapReady = true;
    TryDismissLoading();
}

void ADroneHUD::OnTileStreamTimeout()
{
    if (bTilesStreamed) return;
    UE_LOG(LogTemp, Warning, TEXT("DroneHUD: tile streaming did not settle within 60s — proceeding without it"));
    GetWorldTimerManager().ClearTimer(TileStreamPollHandle);
    bTilesStreamed = true;
    TryDismissLoading();
}

void ADroneHUD::TryDismissLoading()
{
    if (!bAltitudeStable || !bMinimapReady || !bTilesStreamed) return;

    GetWorldTimerManager().ClearTimer(TileStreamPollHandle);

    bLoadingActive = false;

    if (CachedDrone)
        CachedDrone->EnableInput(GetOwningPlayerController());

    if (UGameViewportClient* VP = GetWorld() ? GetWorld()->GetGameViewport() : nullptr)
        VP->bDisableWorldRendering = false;

    if (LoadingWidget)   LoadingWidget->Dismiss();
    if (BTDisplay)       BTDisplay->NotifyLoadingDismissed();
    if (CachedTarget)    CachedTarget->NotifyLoadingDismissed();
    if (MiniMapBrowser)  MiniMapBrowser->SetRenderOpacity(1.f);
    if (ADroneGameMode* GM = GetWorld()->GetAuthGameMode<ADroneGameMode>())
        GM->StartChaseTimer();
    if (CountdownWidget) CountdownWidget->StartCountdown();
}

void ADroneHUD::PollTileStreaming()
{
    UWorld* W = GetWorld();
    if (!W) return;

    if (UWorldPartitionSubsystem* WPS = W->GetSubsystem<UWorldPartitionSubsystem>())
    {
        if (!WPS->IsStreamingCompleted())
            return;
    }

    // Wait for Cesium tile activity to go quiet for the initial static frustum.
    // Since the camera doesn't move during load, the frustum is fixed. Once all
    // pending tile requests are fulfilled and no new ones are queued, GetLoadProgress()
    // stops changing. We detect this by requiring the minimum progress across all
    // tilesets to be identical for RequiredStablePolls consecutive polls — meaning
    // no new tile requests have been issued and the queue is drained.
    // This works at any network speed without a hardcoded threshold or timeout.
    if (UClass* CesiumClass = FindObject<UClass>(nullptr, TEXT("/Script/CesiumRuntime.Cesium3DTileset")))
    {
        TArray<AActor*> Tilesets;
        UGameplayStatics::GetAllActorsOfClass(W, CesiumClass, Tilesets);

        if (Tilesets.IsEmpty())
        {
            LastTileProgress = -1.f;
            StablePollCount  = 0;
            return;
        }

        float MinProgress = 100.f;
        for (AActor* A : Tilesets)
        {
            if (UFunction* Fn = A->FindFunction(FName("GetLoadProgress")))
            {
                struct { float ReturnValue; } Params = {};
                A->ProcessEvent(Fn, &Params);
                MinProgress = FMath::Min(MinProgress, Params.ReturnValue);
            }
        }

        if (FMath::IsNearlyEqual(MinProgress, LastTileProgress, 0.1f))
            ++StablePollCount;
        else
        {
            StablePollCount  = 0;
            LastTileProgress = MinProgress;
        }

        if (StablePollCount < RequiredStablePolls)
            return;
    }

    GetWorldTimerManager().ClearTimer(TileStreamPollHandle);
    GetWorldTimerManager().ClearTimer(TileStreamTimeoutHandle);
    bTilesStreamed = true;
    TryDismissLoading();
}

void ADroneHUD::DrawHUD()
{
    Super::DrawHUD();

    if (!MiniMapBrowser || !Canvas) return;

    FVector2D VP(Canvas->SizeX, Canvas->SizeY);
    if (UGameViewportClient* GVC = GetWorld() ? GetWorld()->GetGameViewport() : nullptr)
        GVC->GetViewportSize(VP);

    const float Scale = FMath::Max(UWidgetLayoutLibrary::GetViewportScale(this), KINDA_SMALL_NUMBER);
    VP /= Scale;

    const float mmMargin  = VP.Y * 0.00f;
    const float PFDBaseR  = FMath::Min(VP.X * 0.0627f, VP.Y * 0.1010f);
    const float ARBoost   = FMath::Sqrt(FMath::Max(1.f, (VP.X / VP.Y) / (16.f / 9.f)));
    const float PFDRadius = PFDBaseR * ARBoost;
    const float mmSize    = PFDRadius * 2.f * 1.15f;

    if (UGameViewportSubsystem* Sub = UGameViewportSubsystem::Get(GetWorld()))
    {
        FGameViewportWidgetSlot Slot = Sub->GetWidgetSlot(MiniMapBrowser);
        Slot.Anchors   = FAnchors(0.f, 0.f);
        Slot.Alignment = FVector2D(0.f, 0.f);
        Slot.Offsets   = FMargin(mmMargin, VP.Y - mmMargin - mmSize, mmSize, mmSize);
        Sub->SetWidgetSlot(MiniMapBrowser, Slot);
    }
}

void ADroneHUD::NotifyHUDServerReady()
{
}

void ADroneHUD::ToggleOptions()
{
    if (bLoadingActive) return;
    if (!OptionsWidget) return;
    if (ResultWidget && ResultWidget->GetVisibility() != ESlateVisibility::Collapsed) return;
    if (OptionsWidget->IsShowing())
        OptionsWidget->Hide();
    else
        OptionsWidget->Show();
}

void ADroneHUD::OnGameEnded(bool bWon)
{
    if (CountdownWidget) CountdownWidget->StopTimer();
    if (ResultWidget)    ResultWidget->ShowResult(bWon);
}
