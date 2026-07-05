#pragma once
#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "Engine/StreamableManager.h"
#include "UObject/WeakObjectPtr.h"
#include "DroneMainMenuHUD.generated.h"

class UDroneMainMenuWidget;
class USceneCaptureComponent2D;
class SWidget;

UCLASS()
class DRONECHALLENGER_API ADroneMainMenuHUD : public AHUD
{
    GENERATED_BODY()
protected:
    virtual void BeginPlay() override;

private:
    static constexpr float RecaptureInterval = 2.f;
    static constexpr float RevealDelay       = 8.f;
    static constexpr float MaxRevealDelay    = 15.f;

    UPROPERTY() TObjectPtr<UDroneMainMenuWidget> MenuWidget;

    TSharedPtr<FStreamableHandle> CarDisplayLoadHandle;
    TArray<TWeakObjectPtr<USceneCaptureComponent2D>> PendingCaptures;
    TSharedPtr<SWidget> BlackoutOverlay;
    FTimerHandle RecaptureHandle;
    FTimerHandle RevealDelayHandle;
    FTimerHandle FallbackRevealHandle;
    bool bRevealed = false;

    void LaunchGame();
    void OnCarDisplayClassLoaded();
    void SetupCarDisplay(UClass* CarDisplayClass);
    void RecaptureCarDisplay();
    void RevealCarDisplay();
};
