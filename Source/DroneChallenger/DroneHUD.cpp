#include "DroneHUD.h"
#include "BTDisplayWidget.h"
#include "DroneTrackingWidget.h"
#include "DronePFDWidget.h"
#include "DroneCrosshairWidget.h"
#include "DroneActor.h"
#include "TargetPawn.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Canvas.h"
#include "MiniMapWidget.h"

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
            MiniMapBrowser->AddToViewport();

            FString HtmlPath = FPaths::ConvertRelativePathToFull(
                FPaths::ProjectDir() / TEXT("HUD/minimap/minimap.html"));
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
}

void ADroneHUD::DrawHUD()
{
    Super::DrawHUD();

    if (bMiniMapPositioned || !MiniMapBrowser || !Canvas) return;
    bMiniMapPositioned = true;

    const FVector2D VP(Canvas->SizeX, Canvas->SizeY);
    const float mmMargin = FMath::Min(VP.X, VP.Y) * 0.018f;
    const float mmSize   = FMath::Min(VP.X * 0.32f, VP.Y * 0.52f);
    const float R_pfd      = FMath::Min(VP.X * 0.1024f, VP.Y * 0.165f);
    const float mmCenterY  = VP.Y * 1.01f - R_pfd;

    MiniMapBrowser->SetDesiredSizeInViewport(FVector2D(mmSize, mmSize));
    MiniMapBrowser->SetPositionInViewport(FVector2D(mmMargin, mmCenterY - mmSize * 0.5f));
}

void ADroneHUD::NotifyHUDServerReady()
{
}
