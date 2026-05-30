#pragma once
#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "DroneHUD.generated.h"

class UBTDisplayWidget;
class UDroneTrackingWidget;
class UDroneMiniMapWidget;
class UDroneAttitudeWidget;
class ADroneActor;
class ATargetPawn;

UCLASS()
class DRONECHALLENGER_API ADroneHUD : public AHUD
{
    GENERATED_BODY()
public:
    UPROPERTY(EditDefaultsOnly, Category = "HUD")
    TSubclassOf<UBTDisplayWidget> BTDisplayWidgetClass;

    UPROPERTY(EditDefaultsOnly, Category = "HUD")
    TSubclassOf<UDroneTrackingWidget> TrackingWidgetClass;

    UPROPERTY(EditDefaultsOnly, Category = "HUD")
    TSubclassOf<UDroneMiniMapWidget> MiniMapWidgetClass;

    UPROPERTY(EditDefaultsOnly, Category = "HUD")
    TSubclassOf<UDroneAttitudeWidget> AttitudeWidgetClass;

protected:
    virtual void BeginPlay() override;

private:
    UPROPERTY() TObjectPtr<UBTDisplayWidget>      BTDisplay;
    UPROPERTY() TObjectPtr<UDroneTrackingWidget>  TrackingWidget;
    UPROPERTY() TObjectPtr<UDroneMiniMapWidget>   MiniMapWidget;
    UPROPERTY() TObjectPtr<UDroneAttitudeWidget>  AttitudeWidget;
};
