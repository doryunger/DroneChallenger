#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CesiumSampleHeightResult.h"
#include "TerrainProbe.generated.h"

class ACesium3DTileset;

/**
 * Editor utility actor for querying terrain elevation via ACesium3DTileset::SampleHeightMostDetailed.
 * Place one in the Munich level, assign the Google Photorealistic 3D Tiles tileset,
 * then use the CallInEditor button to sample heights at test positions.
 * Results are printed to the Output Log.
 */
UCLASS()
class DRONECHALLENGER_API ATerrainProbe : public AActor
{
	GENERATED_BODY()

public:
	ATerrainProbe();

	// The Cesium tileset to query. Assign the Google Photorealistic 3D Tiles actor here.
	UPROPERTY(EditAnywhere, Category = "Terrain Probe")
	TObjectPtr<ACesium3DTileset> Tileset;

	// Samples height at three fixed positions around Marienplatz and logs the results.
	UFUNCTION(CallInEditor, Category = "Terrain Probe")
	void SampleTestPositions();

private:
	void OnHeightsSampled(
		ACesium3DTileset* SampledTileset,
		const TArray<FCesiumSampleHeightResult>& Results,
		const TArray<FString>& Warnings);
};
