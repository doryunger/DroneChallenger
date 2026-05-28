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

	TArray<int32> GeneratePath(int32 StartNode, float TargetDistanceCm, int32 PrevNode = INDEX_NONE) const;
	int32 FindNearestNode(const FVector& WorldPos) const;
	bool IsEmpty() const;

private:
	int32 PickNextNode(int32 From, int32 CameFrom) const;
};
