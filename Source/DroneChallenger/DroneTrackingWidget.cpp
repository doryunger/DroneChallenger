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

namespace
{
    FText FormatTime(float Seconds)
    {
        const int32 Mins = FMath::FloorToInt(Seconds / 60.f);
        const int32 Secs = FMath::FloorToInt(Seconds) % 60;
        return FText::FromString(FString::Printf(TEXT("%d:%02d"), Mins, Secs));
    }
}

FText UDroneTrackingWidget::GetCurrentTimeText() const
{
    return FormatTime(Target ? Target->CurrentTrackingTime : 0.f);
}

FText UDroneTrackingWidget::GetBestTimeText() const
{
    return FormatTime(Target ? Target->BestTrackingTime : 0.f);
}

FSlateColor UDroneTrackingWidget::GetTrackingColor() const
{
    const bool bTracking = Target && Target->IsDroneInFOV();
    return bTracking
        ? FSlateColor(FLinearColor(0.64f, 0.87f, 0.64f, 1.f))
        : FSlateColor(FLinearColor(0.60f, 0.62f, 0.70f, 1.f));
}
