#include "DroneHUD.h"
#include "BTDisplayWidget.h"
#include "DroneTrackingWidget.h"
#include "DroneMiniMapWidget.h"
#include "DronePFDWidget.h"
#include "DroneCrosshairWidget.h"
#include "DroneActor.h"
#include "TargetPawn.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/GameViewportClient.h"

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

    if (MiniMapWidgetClass)
    {
        MiniMapWidget = CreateWidget<UDroneMiniMapWidget>(GetWorld(), MiniMapWidgetClass);
        if (MiniMapWidget)
        {
            MiniMapWidget->Init(Drone, Target);
            MiniMapWidget->AddToViewport();
            MiniMapWidget->SetDesiredSizeInViewport(FVector2D(240.f, 240.f));
            MiniMapWidget->SetPositionInViewport(FVector2D(VP.X - 260.f, 20.f));
        }
    }

    if (PFDWidgetClass)
    {
        PFDWidget = CreateWidget<UDronePFDWidget>(GetWorld(), PFDWidgetClass);
        if (PFDWidget)
        {
            PFDWidget->Init(Drone);
            PFDWidget->AddToViewport();
            PFDWidget->SetDesiredSizeInViewport(FVector2D(VP.X * 0.22f, VP.Y * 0.28f));
            PFDWidget->SetPositionInViewport(
                FVector2D((VP.X - VP.X * 0.22f) * 0.5f, VP.Y * 0.62f));
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
