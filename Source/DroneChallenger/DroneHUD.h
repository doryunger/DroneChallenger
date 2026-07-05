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
class UDroneCountdownWidget;
class UDroneOptionsWidget;
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
    void ToggleOptions();

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
    UPROPERTY() TObjectPtr<UDroneCountdownWidget> CountdownWidget;
    UPROPERTY() TObjectPtr<UDroneOptionsWidget>   OptionsWidget;

    bool bAltitudeStable = false;
    bool bMinimapReady   = false;
    bool bTilesStreamed  = false;
    bool bLoadingActive  = true;

    UPROPERTY() TObjectPtr<ADroneActor> CachedDrone;
    UPROPERTY() TObjectPtr<ATargetPawn> CachedTarget;

    FTimerHandle TileStreamPollHandle;
    FTimerHandle MinimapTimeoutHandle;
    FTimerHandle TileStreamTimeoutHandle;

    float  LastTileProgress    = -1.f;
    int32  StablePollCount     = 0;
    static constexpr int32 RequiredStablePolls = 4;

    void OnSceneReady();
    void OnMinimapReady();
    void TryDismissLoading();
    void PollTileStreaming();
    void OnMinimapTimeout();
    void OnTileStreamTimeout();
    void OnGameEnded(bool bWon);
};
