#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MiniMapWidget.generated.h"

class UWebBrowser;

UCLASS()
class DRONECHALLENGER_API UMiniMapWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    void LoadURL(const FString& URL);
protected:
    virtual bool Initialize() override;
    virtual void NativeConstruct() override;
private:
    UPROPERTY() TObjectPtr<UWebBrowser> Browser;
    FString  PendingURL;
    bool     bConstructed = false;
};
