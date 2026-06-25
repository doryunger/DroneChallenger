#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MiniMapWidget.generated.h"

class UWebBrowser;

DECLARE_DELEGATE(FOnMiniMapReady);

UCLASS()
class DRONECHALLENGER_API UMiniMapWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    void LoadURL(const FString& URL);
    FOnMiniMapReady OnReady;

protected:
    virtual bool Initialize() override;
    virtual void NativeConstruct() override;

private:
    UFUNCTION() void HandleConsoleMessage(const FString& Message, const FString& Source, int32 Line);

    UPROPERTY() TObjectPtr<UWebBrowser> Browser;
    FString  PendingURL;
    bool     bConstructed = false;
};
