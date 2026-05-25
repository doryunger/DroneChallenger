#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CesiumSampleHeightResult.h"
#include "TerrainProbe.generated.h"

class ACesium3DTileset;

UCLASS()
class DRONECHALLENGER_API ATerrainProbe : public AActor
{
	GENERATED_BODY()

public:
	ATerrainProbe();

	UPROPERTY(EditAnywhere, Category = "Terrain Probe")
	TObjectPtr<ACesium3DTileset> Tileset;

	UFUNCTION(CallInEditor, Category = "Terrain Probe")
	void SampleTestPositions();

private:
	void OnHeightsSampled(
		ACesium3DTileset* SampledTileset,
		const TArray<FCesiumSampleHeightResult>& Results,
		const TArray<FString>& Warnings);
};
