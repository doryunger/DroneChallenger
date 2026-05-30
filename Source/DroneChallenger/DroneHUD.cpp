#include "DroneHUD.h"
#include "BTDisplayWidget.h"
#include "Kismet/GameplayStatics.h"

void ADroneHUD::BeginPlay()
{
	Super::BeginPlay();

	if (!BTDisplayWidgetClass) return;

	BTDisplay = CreateWidget<UBTDisplayWidget>(GetWorld(), BTDisplayWidgetClass);
	if (!BTDisplay) return;

	BTDisplay->AddToViewport();
}
