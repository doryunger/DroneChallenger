#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PatrolPath.generated.h"

class USplineComponent;

UCLASS()
class DRONECHALLENGER_API APatrolPath : public AActor
{
	GENERATED_BODY()

public:
	APatrolPath();
	USplineComponent* GetSpline() const { return Spline; }

private:
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USplineComponent> Spline;
};
