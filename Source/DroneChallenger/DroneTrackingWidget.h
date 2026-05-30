#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DroneTrackingWidget.generated.h"

class ADroneActor;
class ATargetPawn;

UCLASS()
class DRONECHALLENGER_API UDroneTrackingWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    void Init(ADroneActor* InDrone, ATargetPawn* InTarget);

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "HUD|Tracking")
    float GetCurrentTrackingTime() const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "HUD|Tracking")
    float GetBestTrackingTime() const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "HUD|Tracking")
    bool IsCurrentlyTracking() const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "HUD|Tracking")
    FText GetCurrentTimeText() const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "HUD|Tracking")
    FText GetBestTimeText() const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "HUD|Tracking")
    FSlateColor GetTrackingColor() const;

private:
    UPROPERTY()
    TObjectPtr<ADroneActor> Drone;

    UPROPERTY()
    TObjectPtr<ATargetPawn> Target;
};
