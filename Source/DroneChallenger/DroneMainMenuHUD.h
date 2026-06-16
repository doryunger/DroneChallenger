#pragma once
#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "DroneMainMenuHUD.generated.h"

class UDroneMainMenuWidget;

UCLASS()
class DRONECHALLENGER_API ADroneMainMenuHUD : public AHUD
{
    GENERATED_BODY()
protected:
    virtual void BeginPlay() override;

private:
    UPROPERTY() TObjectPtr<UDroneMainMenuWidget> MenuWidget;

    void LaunchGame();
};
