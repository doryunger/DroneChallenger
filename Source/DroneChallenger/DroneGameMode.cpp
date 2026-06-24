#include "DroneGameMode.h"

ADroneGameMode::ADroneGameMode() {}

void ADroneGameMode::BeginPlay()
{
    Super::BeginPlay();
    GetWorldTimerManager().SetTimer(TimeoutHandle, this, &ADroneGameMode::OnTimeout, 15.f * 60.f, false);
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
    if (bGameEnded) return;
    bGameEnded = true;
    GetWorldTimerManager().ClearTimer(TimeoutHandle);
    OnGameEnded.Broadcast(bWon);
}
