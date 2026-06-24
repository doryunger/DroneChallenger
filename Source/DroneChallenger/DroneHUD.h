#pragma once
#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "DroneHUD.generated.h"

class UBTDisplayWidget;
class UDroneTrackingWidget;
class UDronePFDWidget;
class UDroneCrosshairWidget;
class UMiniMapWidget;
class UDroneLoadingWidget;
class UDroneResultWidget;
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
    TSubclassOf<UDronePFDWidget> PFDWidgetClass;

    UPROPERTY(EditDefaultsOnly, Category = "HUD")
    TSubclassOf<UDroneCrosshairWidget> CrosshairWidgetClass;

    void NotifyHUDServerReady();

protected:
    virtual void BeginPlay() override;
    virtual void DrawHUD() override;

private:
    UPROPERTY() TObjectPtr<UBTDisplayWidget>      BTDisplay;
    UPROPERTY() TObjectPtr<UDroneTrackingWidget>  TrackingWidget;
    UPROPERTY() TObjectPtr<UMiniMapWidget>        MiniMapBrowser;
    UPROPERTY() TObjectPtr<UDronePFDWidget>       PFDWidget;
    UPROPERTY() TObjectPtr<UDroneCrosshairWidget> CrosshairWidget;
    UPROPERTY() TObjectPtr<UDroneLoadingWidget>   LoadingWidget;
    UPROPERTY() TObjectPtr<UDroneResultWidget>    ResultWidget;

    void OnSceneReady();
    void OnGameEnded(bool bWon);
};
