#include "DroneGameMode.h"

ADroneGameMode::ADroneGameMode() {}

void ADroneGameMode::BeginPlay()
{
    Super::BeginPlay();
}

void ADroneGameMode::StartChaseTimer()
{
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
    if (bGameEnded) return;
    bGameEnded = true;
    GetWorldTimerManager().ClearTimer(TimeoutHandle);
    OnGameEnded.Broadcast(bWon);
}
