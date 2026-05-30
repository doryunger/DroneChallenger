#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DroneMiniMapWidget.generated.h"

class ADroneActor;
class ATargetPawn;

UCLASS()
class DRONECHALLENGER_API UDroneMiniMapWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    void Init(ADroneActor* InDrone, ATargetPawn* InTarget);

    UPROPERTY(EditDefaultsOnly, Category = "MiniMap")
    float MapRadiusCm = 50000.f;

    UPROPERTY(EditDefaultsOnly, Category = "MiniMap")
    float FPVHorizontalFOVDeg = 90.f;

    UPROPERTY(EditDefaultsOnly, Category = "MiniMap")
    float DetectionRangeCm = 30000.f;

protected:
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
    virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
        const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
        int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
    UPROPERTY()
    TObjectPtr<ADroneActor> Drone;

    UPROPERTY()
    TObjectPtr<ATargetPawn> Target;

    FVector  CachedDronePos    = FVector::ZeroVector;
    float    CachedDroneYaw    = 0.f;
    FVector  CachedTargetPos   = FVector::ZeroVector;
    bool     bCachedTargetInFOV = false;

    FVector2D WorldToMap(const FVector2D& Center, float Scale, const FVector& WorldPos) const;
};
