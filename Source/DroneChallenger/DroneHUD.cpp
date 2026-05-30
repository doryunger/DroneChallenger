#include "DroneHUD.h"
#include "BTDisplayWidget.h"
#include "DroneTrackingWidget.h"
#include "DroneMiniMapWidget.h"
#include "DroneAttitudeWidget.h"
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

    if (AttitudeWidgetClass)
    {
        AttitudeWidget = CreateWidget<UDroneAttitudeWidget>(GetWorld(), AttitudeWidgetClass);
        if (AttitudeWidget) { AttitudeWidget->Init(Drone); AttitudeWidget->AddToViewport(); }
    }
}
