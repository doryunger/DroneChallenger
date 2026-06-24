#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "DroneGameMode.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnGameEnded, bool);

UCLASS()
class DRONECHALLENGER_API ADroneGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    ADroneGameMode();

    void NotifyCrash();
    void NotifyWin();

    [[nodiscard]] bool IsGameEnded() const { return bGameEnded; }

    FOnGameEnded OnGameEnded;

protected:
    virtual void BeginPlay() override;

private:
    bool         bGameEnded = false;
    FTimerHandle TimeoutHandle;

    void EndGame(bool bWon);
    void OnTimeout();
};
