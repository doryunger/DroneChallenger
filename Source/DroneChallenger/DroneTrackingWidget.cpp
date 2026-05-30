#include "DroneTrackingWidget.h"
#include "DroneActor.h"
#include "TargetPawn.h"

void UDroneTrackingWidget::Init(ADroneActor* InDrone, ATargetPawn* InTarget)
{
    Drone  = InDrone;
    Target = InTarget;
}

float UDroneTrackingWidget::GetCurrentTrackingTime() const
{
    return Target ? Target->CurrentTrackingTime : 0.f;
}

float UDroneTrackingWidget::GetBestTrackingTime() const
{
    return Target ? Target->BestTrackingTime : 0.f;
}

bool UDroneTrackingWidget::IsCurrentlyTracking() const
{
    return Target && Target->IsDroneInFOV();
}
