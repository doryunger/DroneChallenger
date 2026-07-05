#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Rendering/DrawElements.h"
#include "BTDisplayWidget.generated.h"

class UBorder;
class UButton;
class UWebBrowser;

UCLASS()
class DRONECHALLENGER_API UBTDisplayWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, Category = "BT")
	int32 MonitorPort = 8080;

	UPROPERTY(BlueprintReadWrite, Category = "BT")
	bool bExpanded = false;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "BT")
	FString GetMonitorURL() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "BT")
	FText GetToggleLabel() const;

	UFUNCTION(BlueprintCallable, Category = "BT")
	void TogglePanel();

	void NotifyLoadingDismissed();

protected:
	UFUNCTION() void HandleConsoleMessage(const FString& Message, const FString& Source, int32 Line);
	virtual bool Initialize() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
		int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	static constexpr float MaxPanelOpenWait  = 15.f;
	static constexpr float ButtonRevealDelay = 5.f;

	float LoadingPhase  = 0.f;
	float StripePhase   = 0.f;
	bool  bURLLoaded    = false;
	bool  bPageLoaded   = false;
	int32 PanelOpenPolls = 0;
	mutable UBorder*     CachedPanel   = nullptr;
	mutable UButton*     CachedBtn     = nullptr;
	mutable UWebBrowser* CachedBrowser = nullptr;
	mutable UWidget*     CachedSpinner = nullptr;

	mutable FVector2D CachedPanelPos  = FVector2D::ZeroVector;
	mutable FVector2D CachedPanelSize = FVector2D::ZeroVector;

	void RevealBrowserPage();
	void RevealToggleButton();
	void OnAfterConstruct();
	void ReloadBrowser();
	void OnPanelOpenTimer();

	FTimerHandle PanelOpenTimerHandle;
	FTimerHandle ButtonRevealHandle;
};
