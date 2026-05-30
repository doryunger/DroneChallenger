#include "DroneHUD.h"
#include "BTDisplayWidget.h"
#include "DroneTrackingWidget.h"
#include "DroneMiniMapWidget.h"
#include "DronePFDWidget.h"
#include "DroneCrosshairWidget.h"
#include "DroneActor.h"
#include "TargetPawn.h"
#include "Kismet/GameplayStatics.h"

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

    if (TrackingWidgetClass)
    {
        TrackingWidget = CreateWidget<UDroneTrackingWidget>(GetWorld(), TrackingWidgetClass);
        if (TrackingWidget) { TrackingWidget->Init(Drone, Target); TrackingWidget->AddToViewport(); }
    }

    if (MiniMapWidgetClass)
    {
        MiniMapWidget = CreateWidget<UDroneMiniMapWidget>(GetWorld(), MiniMapWidgetClass);
        if (MiniMapWidget) { MiniMapWidget->Init(Drone, Target); MiniMapWidget->AddToViewport(); }
    }

    if (PFDWidgetClass)
    {
        PFDWidget = CreateWidget<UDronePFDWidget>(GetWorld(), PFDWidgetClass);
        if (PFDWidget) { PFDWidget->Init(Drone); PFDWidget->AddToViewport(); }
    }

    if (CrosshairWidgetClass)
    {
        CrosshairWidget = CreateWidget<UDroneCrosshairWidget>(GetWorld(), CrosshairWidgetClass);
        if (CrosshairWidget) { CrosshairWidget->Init(Drone); CrosshairWidget->AddToViewport(); }
    }
}
