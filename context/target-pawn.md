# Target Pawn (patrol car)

## Game-phase gating

Nothing in the chase runs until `ADroneHUD::TryDismissLoading` calls `ADroneGameMode::StartChaseTimer`. That call sets the chase active, and `ATargetPawn::Tick` only ticks the behavior tree, the capture timer and the tracking-time win while `ADroneGameMode::IsChaseActive()` is true (started and not ended).

`ADroneGameMode::EndGame` also ignores win and crash notifications before the chase starts. During loading the drone and car are being placed and re-placed while Cesium tiles stream in, so any proximity, FOV or crash condition seen in that window is an artifact of placement, not gameplay.

## Win conditions require the player to fly

Neither win condition can be satisfied by where the drone happens to be, only by the player flying it there.

- `ADroneActor::HasPlayerGivenInput()` becomes true on the first non-zero control axis (throttle, pitch, roll or yaw) seen while the chase is active. Keys held during the loading screen do not count.
- `bDroneHasEverMoved` mirrors that flag and gates the 30 s tracking-time win. It is no longer derived from drone velocity, which physics settling or drift could satisfy without any player action.
- Capture is armed (`bCaptureArmed`) only after the first input **and** once the drone has been observed outside the capture zone. Until then `bDroneInCaptureRange` stays false, so the capture countdown, the BT `in_capture_range` branch and the capture timer never start. A drone that sits next to the car, or is inside the zone without ever having left it under player control, cannot capture.

## Capture range

Capture is proximity-based and independent of drone heading. It is computed before any FOV/heading pre-filter so hovering almost directly above the car always registers. Heading and FOV only matter for the separate sustained-tracking win (`bDroneInFOV`, 30 s).

Distance is measured to the nearest point on the car mesh's world-space bounding box, expanded by `CaptureBoxExpansionCm` in every direction, and compared against `CaptureRadius`. Measuring to the box rather than the actor origin accounts for the car's physical size. The expansion makes "close to the car" easier to trigger without changing the visual mesh.

The 2D pre-filter and the heading pre-filter only gate the expensive line-of-sight traces and the FOV test; they never affect capture range. Line of sight uses three traces at different car heights and stops at the first clear one.

## Drone spawn placement

`PlaceDroneNearCar` never places the drone inside the capture zone (`IsInCaptureZone`, the same expanded box plus `CaptureRadius`, with an extra `SpawnCaptureMarginCm`). Otherwise a spawn that fell back to a near candidate would satisfy capture the moment the chase starts. The zone is evaluated around the car's intended position, using the mesh bounds offset from the actor origin, because the car actor is moved to that position only after the drone is placed.

## Speeds

`PatrolSpeed` and `EvadeSpeed` are forced in `BeginPlay` regardless of per-instance overrides on the placed level actor; both properties are `EditAnywhere`, so a level-serialized value would otherwise take precedence over the class defaults.

## Wheel meshes

Each wheel `ConstructorHelpers::FObjectFinder` lives in its own scope so each static is a distinct variable. A shared static inside a lambda initialises only once, which would load wheel1's mesh for all four wheels.

## Placement candidate order

Candidate road nodes are either sorted by ascending distance from the drone's editor position (deterministic demo runs, same start every time) or Fisher-Yates shuffled (random start each run).

## Terrain altitude filtering

`ShouldAcceptAltitude` accepts small altitude changes directly. A sharp change is deferred while history is short, accepted when every sample in history moved in the same direction (sustained trend), and rejected as an isolated spike otherwise, keeping history stable.
