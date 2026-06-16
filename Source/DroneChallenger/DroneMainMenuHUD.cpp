#include "DroneMainMenuHUD.h"
#include "DroneMainMenuWidget.h"
#include "Kismet/GameplayStatics.h"
#include "Framework/Application/SlateApplication.h"

void ADroneMainMenuHUD::BeginPlay()
{
    Super::BeginPlay();

    MenuWidget = CreateWidget<UDroneMainMenuWidget>(GetWorld(), UDroneMainMenuWidget::StaticClass());
    if (!MenuWidget) return;

    MenuWidget->OnContinue.BindUObject(this, &ADroneMainMenuHUD::LaunchGame);
    MenuWidget->AddToViewport(0);

    APlayerController* PC = GetOwningPlayerController();
    if (PC)
    {
        FInputModeGameAndUI UIMode;
        UIMode.SetWidgetToFocus(MenuWidget->TakeWidget());
        UIMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        PC->SetInputMode(UIMode);
        PC->SetShowMouseCursor(true);
    }

    FSlateApplication::Get().SetKeyboardFocus(MenuWidget->TakeWidget(), EFocusCause::SetDirectly);
}

void ADroneMainMenuHUD::LaunchGame()
{
    UGameplayStatics::OpenLevel(GetWorld(), FName("/Game/Maps/Munich"));
}
