#pragma once
#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "DroneGameInstance.generated.h"

UCLASS()
class DRONECHALLENGER_API UDroneGameInstance : public UGameInstance
{
	GENERATED_BODY()
public:
	virtual void Init() override;
	virtual void Shutdown() override;
private:
	// Called on-demand by the MoviePlayer (from PlayMovie) right before a level
	// load when no loading screen is otherwise prepared. Covers the synchronous
	// LoadMap blocking phase (packaged builds only).
	void OnPrepareLoadingScreen();
	FDelegateHandle PrepareLoadingScreenHandle;

	// Fires once the destination world is up (inside LoadMap, before the first
	// post-load frame renders). We use it to blackout the 3D scene for the
	// simulator level so its un-initialized sky/clouds never render before the
	// loading widget covers them. Re-enabled by ADroneHUD when the scene is ready.
	void OnPostLoadMap(UWorld* LoadedWorld);
	FDelegateHandle PostLoadMapHandle;
};
