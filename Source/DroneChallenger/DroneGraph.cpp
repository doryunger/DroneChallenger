#include "DroneGraph.h"
#include "CesiumGeoreference.h"
#include "Misc/FileHelper.h"
#include "Engine/World.h"

bool FDroneGraph::Load(const FString& NodesPath, const FString& EdgesPath,
                       ACesiumGeoreference* Georef, UWorld* World)
{
	if (!Georef || !World) return false;

	FString NodesText;
	if (!FFileHelper::LoadFileToString(NodesText, *NodesPath))
	{
		UE_LOG(LogTemp, Error, TEXT("FDroneGraph: failed to read %s"), *NodesPath);
		return false;
	}

	TMap<int32, FVector2D> RawNodes;
	{
		TArray<FString> Lines;
		NodesText.ParseIntoArrayLines(Lines);
		if (Lines.Num() < 2)
		{
			UE_LOG(LogTemp, Error, TEXT("FDroneGraph: nodes file has no data rows"));
			return false;
		}
		const TCHAR* Delim = Lines[0].Contains(TEXT("\t")) ? TEXT("\t") : TEXT(",");
		UE_LOG(LogTemp, Log, TEXT("FDroneGraph: nodes delimiter=%s, lines=%d"), Delim, Lines.Num());
		for (int32 i = 1; i < Lines.Num(); ++i)
		{
			TArray<FString> Parts;
			Lines[i].ParseIntoArray(Parts, Delim);
			if (Parts.Num() < 3) continue;
			const int32  Id  = FCString::Atoi(*Parts[0].TrimStartAndEnd());
			const double Lon = FCString::Atod(*Parts[1].TrimStartAndEnd());
			const double Lat = FCString::Atod(*Parts[2].TrimStartAndEnd());
			RawNodes.Add(Id, FVector2D(Lon, Lat));
		}
	}

	for (auto& Pair : RawNodes)
	{
		FVector WorldPos = Georef->TransformLongitudeLatitudeHeightPositionToUnreal(
			FVector(static_cast<float>(Pair.Value.X), static_cast<float>(Pair.Value.Y), 1000.0f));

		FHitResult Hit;
		const FVector TraceStart(WorldPos.X, WorldPos.Y, 10000000.0f);
		const FVector TraceEnd  (WorldPos.X, WorldPos.Y, -1000000.0f);
		if (World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_WorldStatic))
			WorldPos.Z = Hit.ImpactPoint.Z + 50.0f;

		NodeWorldPos.Add(Pair.Key, WorldPos);
		NodeIds.Add(Pair.Key);
	}

	FString EdgesText;
	if (!FFileHelper::LoadFileToString(EdgesText, *EdgesPath))
	{
		UE_LOG(LogTemp, Error, TEXT("FDroneGraph: failed to read %s"), *EdgesPath);
		return false;
	}

	{
		TArray<FString> Lines;
		EdgesText.ParseIntoArrayLines(Lines);
		const TCHAR* Delim = Lines.Num() > 0 && Lines[0].Contains(TEXT("\t")) ? TEXT("\t") : TEXT(",");
		UE_LOG(LogTemp, Log, TEXT("FDroneGraph: edges delimiter=%s, lines=%d"), Delim, Lines.Num());
		for (int32 i = 1; i < Lines.Num(); ++i)
		{
			TArray<FString> Parts;
			Lines[i].ParseIntoArray(Parts, Delim);
			if (Parts.Num() < 2) continue;
			const int32 From = FCString::Atoi(*Parts[0].TrimStartAndEnd());
			const int32 To   = FCString::Atoi(*Parts[1].TrimStartAndEnd());
			if (!NodeWorldPos.Contains(From) || !NodeWorldPos.Contains(To)) continue;
			Adjacency.FindOrAdd(From).AddUnique(To);
			Adjacency.FindOrAdd(To).AddUnique(From);
		}
	}

	{
		TSet<int32> Largest;
		TSet<int32> Seen;
		for (int32 Node : NodeIds)
		{
			if (Seen.Contains(Node)) continue;

			TSet<int32>   Component;
			TArray<int32> Queue;
			Queue.Add(Node);
			Seen.Add(Node);
			Component.Add(Node);

			int32 Head = 0;
			while (Head < Queue.Num())
			{
				const int32 Cur = Queue[Head++];
				if (const TArray<int32>* Neighbours = Adjacency.Find(Cur))
				{
					for (int32 N : *Neighbours)
					{
						if (!Seen.Contains(N))
						{
							Seen.Add(N);
							Component.Add(N);
							Queue.Add(N);
						}
					}
				}
			}

			if (Component.Num() > Largest.Num())
				Largest = MoveTemp(Component);
		}

		if (Largest.Num() < NodeIds.Num())
		{
			const int32 PrunedCount = NodeIds.Num() - Largest.Num();
			NodeIds.RemoveAll([&Largest](int32 Id) { return !Largest.Contains(Id); });
			for (auto It = NodeWorldPos.CreateIterator(); It; ++It)
				if (!Largest.Contains(It->Key)) It.RemoveCurrent();
			for (auto It = Adjacency.CreateIterator(); It; ++It)
				if (!Largest.Contains(It->Key)) It.RemoveCurrent();

			UE_LOG(LogTemp, Warning, TEXT("FDroneGraph: pruned %d node(s) in disconnected component(s), keeping the largest connected component (%d nodes)"),
				PrunedCount, Largest.Num());
		}
	}

	UE_LOG(LogTemp, Log, TEXT("FDroneGraph: loaded %d nodes, %d connected nodes, %d adjacency entries"),
		NodeIds.Num(), Adjacency.Num(), [&]{ int32 t=0; for (auto& p : Adjacency) t+=p.Value.Num(); return t; }());

	return !NodeIds.IsEmpty();
}

TArray<int32> FDroneGraph::GeneratePath(int32 StartNode, float TargetDistanceCm, int32 PrevNode,
                                         const FVector* BiasTarget, int32 BiasSteps) const
{
	TArray<int32> Path;
	if (!NodeWorldPos.Contains(StartNode)) return Path;

	Path.Add(StartNode);
	TSet<int32> Visited;
	Visited.Add(StartNode);
	if (PrevNode != INDEX_NONE)
		Visited.Add(PrevNode);

	float Accumulated = 0.0f;
	int32 StepCount   = 0;

	while (Accumulated < TargetDistanceCm)
	{
		const int32 Current = Path.Last();
		const TArray<int32>* Neighbours = Adjacency.Find(Current);
		if (!Neighbours)
			break;

		TArray<int32> Candidates;
		for (int32 N : *Neighbours)
			if (!Visited.Contains(N))
				Candidates.Add(N);

		if (Candidates.IsEmpty())
		{
			if (Path.Num() <= 1)
			{
				if (PrevNode != INDEX_NONE && Neighbours->Contains(PrevNode) && NodeWorldPos.Contains(PrevNode))
				{
					Accumulated += FVector::Dist(NodeWorldPos[Current], NodeWorldPos[PrevNode]);
					Path.Add(PrevNode);
				}
				break;
			}
			Path.RemoveAt(Path.Num() - 1);
			continue;
		}

		int32 Next;
		if (BiasTarget && StepCount < BiasSteps)
		{
			Next = Candidates[0];
			float BestDistSq = FVector::DistSquared(NodeWorldPos[Next], *BiasTarget);
			for (int32 i = 1; i < Candidates.Num(); ++i)
			{
				const float DistSq = FVector::DistSquared(NodeWorldPos[Candidates[i]], *BiasTarget);
				if (DistSq < BestDistSq)
				{
					BestDistSq = DistSq;
					Next       = Candidates[i];
				}
			}
		}
		else
		{
			Next = Candidates[FMath::RandRange(0, Candidates.Num() - 1)];
		}

		Accumulated += FVector::Dist(NodeWorldPos[Current], NodeWorldPos[Next]);
		Path.Add(Next);
		Visited.Add(Next);
		++StepCount;
	}

	return Path;
}

int32 FDroneGraph::FindNearestNode(const FVector& WorldPos) const
{
	int32 NearestId = INDEX_NONE;
	float MinDistSq = FLT_MAX;
	for (int32 Id : NodeIds)
	{
		const FVector& NodePos = NodeWorldPos[Id];
		const float DistSq = FMath::Square(NodePos.X - WorldPos.X) + FMath::Square(NodePos.Y - WorldPos.Y);
		if (DistSq < MinDistSq)
		{
			MinDistSq = DistSq;
			NearestId = Id;
		}
	}
	return NearestId;
}

bool FDroneGraph::IsEmpty() const
{
	return NodeIds.IsEmpty();
}
