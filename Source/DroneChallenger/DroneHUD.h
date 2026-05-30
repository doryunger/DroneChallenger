#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "DroneHUD.generated.h"

class UBTDisplayWidget;

UCLASS()
class DRONECHALLENGER_API ADroneHUD : public AHUD
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, Category = "HUD")
	TSubclassOf<UBTDisplayWidget> BTDisplayWidgetClass;

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY()
	TObjectPtr<UBTDisplayWidget> BTDisplay;
};
