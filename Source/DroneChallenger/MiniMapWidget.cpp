#include "MiniMapWidget.h"
#include "WebBrowser.h"
#include "Blueprint/WidgetTree.h"

bool UMiniMapWidget::Initialize()
{
    bool bResult = Super::Initialize();
    if (WidgetTree && !Browser)
    {
        Browser = WidgetTree->ConstructWidget<UWebBrowser>(UWebBrowser::StaticClass(), TEXT("Browser"));
        if (Browser)
        {
            if (FBoolProperty* Prop = CastField<FBoolProperty>(
                    UWebBrowser::StaticClass()->FindPropertyByName(FName(TEXT("bSupportsTransparency")))))
            {
                Prop->SetPropertyValue_InContainer(Browser, true);
            }
            WidgetTree->RootWidget = Browser;
        }
    }
    UE_LOG(LogTemp, Log, TEXT("MiniMapWidget: Initialize — Browser=%s"), Browser ? TEXT("OK") : TEXT("NULL"));
    return bResult;
}

void UMiniMapWidget::NativeConstruct()
{
    Super::NativeConstruct();
    bConstructed = true;
    if (Browser && !PendingURL.IsEmpty())
    {
        UE_LOG(LogTemp, Log, TEXT("MiniMapWidget: loading pending URL %s"), *PendingURL);
        Browser->LoadURL(PendingURL);
        PendingURL.Reset();
    }
}

void UMiniMapWidget::LoadURL(const FString& URL)
{
    if (bConstructed && Browser)
    {
        UE_LOG(LogTemp, Log, TEXT("MiniMapWidget: LoadURL %s"), *URL);
        Browser->LoadURL(URL);
    }
    else
        PendingURL = URL;
}
