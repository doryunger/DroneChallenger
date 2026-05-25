#include "TerrainProbe.h"
#include "Cesium3DTileset.h"

ATerrainProbe::ATerrainProbe()
{
	PrimaryActorTick.bCanEverTick = false;
}

void ATerrainProbe::SampleTestPositions()
{
	if (!Tileset)
	{
		UE_LOG(LogTemp, Warning, TEXT("TerrainProbe: Tileset is not assigned."));
		return;
	}

	TArray<FVector> Points = {
		FVector(11.5755, 48.1374, 0.0),
		FVector(11.5735, 48.1386, 0.0),
		FVector(11.6012, 48.1200, 0.0),
	};

	FCesiumSampleHeightMostDetailedCallback Callback;
	Callback.BindUObject(this, &ATerrainProbe::OnHeightsSampled);
	Tileset->SampleHeightMostDetailed(Points, Callback);

	UE_LOG(LogTemp, Log, TEXT("TerrainProbe: Height sampling started for %d positions..."), Points.Num());
}

void ATerrainProbe::OnHeightsSampled(
	ACesium3DTileset* SampledTileset,
	const TArray<FCesiumSampleHeightResult>& Results,
	const TArray<FString>& Warnings)
{
	for (const FString& Warning : Warnings)
	{
		UE_LOG(LogTemp, Warning, TEXT("TerrainProbe warning: %s"), *Warning);
	}

	static const TArray<FString> Labels = {
		TEXT("Marienplatz"),
		TEXT("Frauenkirche"),
		TEXT("Haidhausen"),
	};

	for (int32 i = 0; i < Results.Num(); ++i)
	{
		const FCesiumSampleHeightResult& R = Results[i];
		const FString& Label = Labels.IsValidIndex(i) ? Labels[i] : FString::Printf(TEXT("Point %d"), i);

		if (R.SampleSuccess)
		{
			UE_LOG(LogTemp, Log, TEXT("TerrainProbe [%s]: Lon=%.4f Lat=%.4f HeightAboveEllipsoid=%.2f m"),
				*Label, R.LongitudeLatitudeHeight.X, R.LongitudeLatitudeHeight.Y, R.LongitudeLatitudeHeight.Z);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("TerrainProbe [%s]: Height sampling failed."), *Label);
		}
	}
}
