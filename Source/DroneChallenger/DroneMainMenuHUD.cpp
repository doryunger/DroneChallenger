#include "DroneMainMenuHUD.h"
#include "DroneMainMenuWidget.h"
#include "Kismet/GameplayStatics.h"
#include "Framework/Application/SlateApplication.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Engine/GameViewportClient.h"
#include "Widgets/Colors/SColorBlock.h"

void ADroneMainMenuHUD::BeginPlay()
{
    Super::BeginPlay();

    MenuWidget = CreateWidget<UDroneMainMenuWidget>(GetWorld(), UDroneMainMenuWidget::StaticClass());
    if (!MenuWidget) return;

    MenuWidget->OnContinue.BindUObject(this, &ADroneMainMenuHUD::LaunchGame);
    MenuWidget->AddToViewport(0);

    if (APlayerController* PC = GetOwningPlayerController())
    {
        FInputModeGameAndUI UIMode;
        UIMode.SetWidgetToFocus(MenuWidget->TakeWidget());
        UIMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        PC->SetInputMode(UIMode);
        PC->SetShowMouseCursor(true);
    }
    FSlateApplication::Get().SetKeyboardFocus(MenuWidget->TakeWidget(), EFocusCause::SetDirectly);

    if (UGameViewportClient* VP = GetWorld() ? GetWorld()->GetGameViewport() : nullptr)
    {
        BlackoutOverlay = SNew(SColorBlock)
            .Color(FLinearColor::Black)
            .Visibility(EVisibility::Visible);
        VP->AddViewportWidgetContent(BlackoutOverlay.ToSharedRef(), 1000);
    }

    static const FSoftObjectPath CarDisplayPath(TEXT("/Game/BP_CarDisplay.BP_CarDisplay_C"));

    if (UClass* AlreadyLoaded = FindObject<UClass>(nullptr, TEXT("/Game/BP_CarDisplay.BP_CarDisplay_C")))
    {
        SetupCarDisplay(AlreadyLoaded);
    }
    else
    {
        FStreamableManager& Streamable = UAssetManager::GetStreamableManager();
        CarDisplayLoadHandle = Streamable.RequestAsyncLoad(
            CarDisplayPath,
            FStreamableDelegate::CreateUObject(this, &ADroneMainMenuHUD::OnCarDisplayClassLoaded));
    }

    GetWorldTimerManager().SetTimer(
        FallbackRevealHandle, this, &ADroneMainMenuHUD::RevealCarDisplay, MaxRevealDelay, false);
}

void ADroneMainMenuHUD::OnCarDisplayClassLoaded()
{
    UClass* CarDisplayClass = FindObject<UClass>(nullptr, TEXT("/Game/BP_CarDisplay.BP_CarDisplay_C"));
    if (!CarDisplayClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("DroneMainMenuHUD: async load completed but class not found"));
        return;
    }
    SetupCarDisplay(CarDisplayClass);
}

void ADroneMainMenuHUD::SetupCarDisplay(UClass* CarDisplayClass)
{
    if (!CarDisplayClass) return;

    TArray<AActor*> Existing;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), CarDisplayClass, Existing);

    AActor* CarDisplay = Existing.Num() > 0 ? Existing[0] : nullptr;
    if (!CarDisplay)
    {
        FActorSpawnParameters SP;
        SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        CarDisplay = GetWorld()->SpawnActor<AActor>(CarDisplayClass,
            FVector(0.f, 100000.f, 0.f), FRotator::ZeroRotator, SP);
    }

    if (!CarDisplay)
    {
        UE_LOG(LogTemp, Warning, TEXT("DroneMainMenuHUD: failed to find or spawn BP_CarDisplay"));
        return;
    }

    PendingCaptures.Reset();
    for (UActorComponent* Comp : CarDisplay->GetComponents())
    {
        if (USceneCaptureComponent2D* Capture = Cast<USceneCaptureComponent2D>(Comp))
        {
            Capture->bCaptureEveryFrame = false;
            PendingCaptures.Add(Capture);
        }
    }

    if (PendingCaptures.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("DroneMainMenuHUD: BP_CarDisplay has no SceneCaptureComponent2D"));
        return;
    }

    RecaptureCarDisplay();
    GetWorldTimerManager().SetTimer(
        RecaptureHandle, this, &ADroneMainMenuHUD::RecaptureCarDisplay, RecaptureInterval, true);

    GetWorldTimerManager().SetTimer(
        RevealDelayHandle, this, &ADroneMainMenuHUD::RevealCarDisplay, RevealDelay, false);
}

void ADroneMainMenuHUD::RecaptureCarDisplay()
{
    for (const TWeakObjectPtr<USceneCaptureComponent2D>& CapturePtr : PendingCaptures)
    {
        if (USceneCaptureComponent2D* Capture = CapturePtr.Get())
            Capture->CaptureScene();
    }
}

void ADroneMainMenuHUD::RevealCarDisplay()
{
    if (bRevealed) return;
    bRevealed = true;

    GetWorldTimerManager().ClearTimer(RevealDelayHandle);
    GetWorldTimerManager().ClearTimer(FallbackRevealHandle);

    if (MenuWidget)
        MenuWidget->NotifyPreviewCaptured();

    if (BlackoutOverlay.IsValid())
    {
        if (UGameViewportClient* VP = GetWorld() ? GetWorld()->GetGameViewport() : nullptr)
            VP->RemoveViewportWidgetContent(BlackoutOverlay.ToSharedRef());
        BlackoutOverlay.Reset();
    }
}

void ADroneMainMenuHUD::LaunchGame()
{
    UGameplayStatics::OpenLevel(GetWorld(), FName("/Game/Maps/Munich"));
}
