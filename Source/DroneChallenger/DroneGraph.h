#pragma once
#include "CoreMinimal.h"

class ACesiumGeoreference;
class UWorld;

struct FDroneGraph
{
	TMap<int32, TArray<int32>> Adjacency;
	TMap<int32, FVector>       NodeWorldPos;
	TArray<int32>              NodeIds;

	bool Load(const FString& NodesPath, const FString& EdgesPath,
	          ACesiumGeoreference* Georef, UWorld* World);

	TArray<int32> GeneratePath(int32 StartNode, float TargetDistanceCm, int32 PrevNode = INDEX_NONE,
	                           const FVector* BiasTarget = nullptr, int32 BiasSteps = 0) const;
	int32 FindNearestNode(const FVector& WorldPos) const;
	bool IsEmpty() const;
};
