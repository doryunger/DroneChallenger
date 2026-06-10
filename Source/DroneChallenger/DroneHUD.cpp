#include "DroneHUD.h"
#include "BTDisplayWidget.h"
#include "DroneTrackingWidget.h"
#include "DronePFDWidget.h"
#include "DroneCrosshairWidget.h"
#include "DroneActor.h"
#include "TargetPawn.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/GameViewportClient.h"
#include "MiniMapWidget.h"

void ADroneHUD::BeginPlay()
{
    Super::BeginPlay();

    ADroneActor* Drone  = Cast<ADroneActor>(UGameplayStatics::GetActorOfClass(GetWorld(), ADroneActor::StaticClass()));
    ATargetPawn* Target = Cast<ATargetPawn>(UGameplayStatics::GetActorOfClass(GetWorld(), ATargetPawn::StaticClass()));

    FVector2D VP(1920.f, 1080.f);
    if (GetWorld() && GetWorld()->GetGameViewport())
        GetWorld()->GetGameViewport()->GetViewportSize(VP);

    if (BTDisplayWidgetClass)
    {
        BTDisplay = CreateWidget<UBTDisplayWidget>(GetWorld(), BTDisplayWidgetClass);
        if (BTDisplay) BTDisplay->AddToViewport();
    }

    if (TrackingWidgetClass)
    {
        TrackingWidget = CreateWidget<UDroneTrackingWidget>(GetWorld(), TrackingWidgetClass);
        if (TrackingWidget)
        {
            TrackingWidget->Init(Drone, Target);
            TrackingWidget->AddToViewport();
            TrackingWidget->SetPositionInViewport(FVector2D(20.f, 20.f));
        }
    }

    const float mmMargin   = FMath::Min(VP.X, VP.Y) * 0.018f;
    const float mmSize     = FMath::Min(VP.X * 0.32f, VP.Y * 0.52f);
    const float bottomEdge = VP.Y - mmMargin;

    {
        MiniMapBrowser = CreateWidget<UMiniMapWidget>(GetWorld(), UMiniMapWidget::StaticClass());
        if (MiniMapBrowser)
        {
            MiniMapBrowser->AddToViewport();
            MiniMapBrowser->SetDesiredSizeInViewport(FVector2D(mmSize, mmSize));
            const float R_mm = (mmSize / 2.f) * 0.78f - 2.f;
            const float mmY  = bottomEdge - (mmSize / 2.f + R_mm) + VP.Y * 0.02f;
            MiniMapBrowser->SetPositionInViewport(FVector2D(mmMargin, mmY));

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
            const float pfdW         = VP.X * 0.32f;
            const float pfdH         = VP.Y * 0.42f;
            const float R_pfd        = FMath::Min(pfdW * 0.32f, pfdH * 0.44f);
            const float pfdCircleBot = pfdH / 2.f + R_pfd;
            PFDWidget->SetDesiredSizeInViewport(FVector2D(pfdW, pfdH));
            PFDWidget->SetPositionInViewport(FVector2D(
                (VP.X - pfdW) * 0.5f,
                bottomEdge - pfdCircleBot));
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

void ADroneHUD::NotifyHUDServerReady()
{
}
