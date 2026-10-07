#include "DroneGameMode.h"
#include "GameFramework/WorldSettings.h"

ADroneGameMode::ADroneGameMode() {}

void ADroneGameMode::BeginPlay()
{
    if (AWorldSettings* Settings = GetWorldSettings())
    {
        Settings->bEnableWorldBoundsChecks = false;
    }
    Super::BeginPlay();
}

void ADroneGameMode::StartChaseTimer()
{
    if (bChaseStarted) return;
    bChaseStarted = true;
    GetWorldTimerManager().SetTimer(TimeoutHandle, this, &ADroneGameMode::OnTimeout, 10.f * 60.f, false);
}

float ADroneGameMode::GetRemainingTime() const
{
    return GetWorldTimerManager().GetTimerRemaining(TimeoutHandle);
}

void ADroneGameMode::NotifyCrash()
{
    EndGame(false);
}

void ADroneGameMode::NotifyWin()
{
    EndGame(true);
}

void ADroneGameMode::OnTimeout()
{
    EndGame(false);
}

void ADroneGameMode::EndGame(bool bWon)
{
    if (bGameEnded || !bChaseStarted) return;
    bGameEnded = true;
    GetWorldTimerManager().ClearTimer(TimeoutHandle);
    OnGameEnded.Broadcast(bWon);
}
