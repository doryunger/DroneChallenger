#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Rendering/DrawElements.h"
#include "BTDisplayWidget.generated.h"

class UButton;

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

	UFUNCTION(BlueprintCallable, Category = "BT")
	void NotifyContentReady();

protected:
	UFUNCTION() void HandleConsoleMessage(const FString& Message, const FString& Source, int32 Line);
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
		int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	float StripePhase    = 0.f;
	float LoadingElapsed = 0.f;
	bool  bContentReady  = false;
	mutable UButton* CachedBtn = nullptr;
};
