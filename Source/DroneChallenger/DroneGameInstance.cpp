#include "DroneGameInstance.h"
#include "MoviePlayer.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Images/SImage.h"
#include "Engine/GameViewportClient.h"

void UDroneGameInstance::Init()
{
	Super::Init();

	UE_LOG(LogTemp, Log, TEXT("DroneGameInstance: Init — transition blackout active"));

	// Hook the MoviePlayer's on-demand "prepare" delegate rather than PreLoadMap
	// directly: the MoviePlayer registers its OWN PreLoadMap handler during engine
	// startup (before this GameInstance exists), so a PreLoadMap handler here would
	// run AFTER PlayMovie() and be too late. OnPrepareLoadingScreen is broadcast
	// from inside PlayMovie() at exactly the right moment.
	PrepareLoadingScreenHandle = GetMoviePlayer()->OnPrepareLoadingScreen().AddUObject(
		this, &UDroneGameInstance::OnPrepareLoadingScreen);

	PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(
		this, &UDroneGameInstance::OnPostLoadMap);
}

void UDroneGameInstance::Shutdown()
{
	if (PrepareLoadingScreenHandle.IsValid())
		GetMoviePlayer()->OnPrepareLoadingScreen().Remove(PrepareLoadingScreenHandle);
	if (PostLoadMapHandle.IsValid())
		FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
	Super::Shutdown();
}

void UDroneGameInstance::OnPostLoadMap(UWorld* LoadedWorld)
{
	if (!LoadedWorld) return;
	UGameViewportClient* VP = GetGameViewportClient();
	if (!VP) return;

	// Munich is a World Partition level: its sky (SkyAtmosphere + VolumetricCloud +
	// DirectionalLight) and Cesium tiles stream in as WP cells AFTER LoadMap returns.
	// In a packaged build those cells stream cold from the pak, so for ~1-2s the
	// viewport would render the half-initialised sky before it settles. Suppress all
	// world rendering now (we are still inside LoadMap, before the first post-load
	// frame); ADroneHUD re-enables it once the scene is actually ready. Ticking,
	// streaming and physics are unaffected — only rendering is held back.
	const FString MapName = LoadedWorld->GetMapName(); // PIE prefix stripped is fine; Contains still matches
	const bool bBlackout = MapName.Contains(TEXT("Munich"));
	VP->bDisableWorldRendering = bBlackout;
	UE_LOG(LogTemp, Log, TEXT("DroneGameInstance: OnPostLoadMap %s — bDisableWorldRendering=%d"),
		*MapName, bBlackout ? 1 : 0);
}

void UDroneGameInstance::OnPrepareLoadingScreen()
{
	// A solid black full-screen cover for every level transition. The MoviePlayer
	// holds this from the start of LoadMap until OnPostLoadMap (which fires after
	// World->BeginPlay), so the destination level — e.g. Munich's sky/atmosphere —
	// never renders an uncovered frame before the in-game HUD/loading widget is up.
	//
	// NOTE: the MoviePlayer is disabled in PIE (IsMoviePlayerEnabled() == !GIsEditor),
	// so this only takes effect in a packaged/standalone build.
	FLoadingScreenAttributes Attrs;
	Attrs.bAutoCompleteWhenLoadingCompletes = true;
	Attrs.MinimumLoadingScreenDisplayTime   = -1.f;
	Attrs.bMoviesAreSkippable               = false;

	// Solid black (white brush tinted black) stretched to fill the screen.
	Attrs.WidgetLoadingScreen = SNew(SImage)
		.Image(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
		.ColorAndOpacity(FSlateColor(FLinearColor::Black));

	GetMoviePlayer()->SetupLoadingScreen(Attrs);
}
