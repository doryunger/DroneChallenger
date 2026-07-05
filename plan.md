# DroneChallenger — Project Plan

## Vision

A first-person drone simulator set in a georeferenced recreation of Munich, built on Unreal Engine 5.7.4 and Cesium for Unreal. The player pilots an FPV drone across a 2 km² urban area and engages with a BT-driven target vehicle (a patrol car).

Two game modes:

**Tracking** — keep the target car in your camera's FOV for as long as possible. The car reacts by speeding up when spotted. Score = longest continuous FOV hold this session.

**Hunting** — get within 1 m of the moving car and hold for the required duration. Only counts after 30 s from session start (the car needs time to move away from spawn). The car does not evade at close range — getting within 1 m is the skill challenge; the car's speed increase when spotted makes the approach harder.

The target's behavior is driven by a live **Behavior Tree** (Arborist framework). A collapsible in-game panel shows the BT visualization in real time alongside the gameplay, demonstrating live AI state inspection.

---

## Game concept

- **Playspace**: 2 km radius around Marienplatz, Munich (48.1374 °N, 11.5755 °E)
- **Target**: one BT-driven patrol car navigating the Munich road graph
- **Win condition** (Hunting): maintain capture range (≤ 1 m) for `CaptureRequiredTime` seconds after the 30 s warmup

### Target behavior tree (4 priorities)

| Priority | Condition | Behavior | In-game effect |
|---|---|---|---|
| 1 | `is_captured` | stop | Car freezes permanently |
| 2 | `drone_in_capture_range` (1 m) | stop + pulse_beacon_fast + increment_capture_timer | Car stops, red beacon flashes |
| 3 | `drone_in_fov` | manage_patrol_path + deactivate_beacon + set_speed_fast + advance | Car goes dark and accelerates |
| 4 | *(always)* | manage_patrol_path + set_speed_normal + pulse_beacon_slow + advance | Normal patrol, slow yellow beacon |

### BT visualiser

A collapsible UMG panel embeds a `WebBrowserWidget` pointed at `http://localhost:8080`. Arborist's `MonitorServer` streams the live active-node path (colour-coded SUCCESS / FAILURE / RUNNING). As the drone approaches the target, the active BT path changes in real time — showing exactly what the car "knows" and what state it is in.

---

## Technical stack

| Layer | Technology |
|---|---|
| Engine | Unreal Engine 5.7.4 |
| Georeferencing | Cesium for Unreal + Cesium ion |
| Terrain data | Google Photorealistic 3D Tiles |
| Language | C++20 (no Blueprint logic) |
| Physics | Chaos physics — UBoxComponent root, per-rotor AddForceAtLocation |
| Flight controller | Cascade angle-mode PID (see `context/flight-controller.md`) |
| Input | Enhanced Input System |
| UI | UMG + Slate + WebBrowser plugin |
| Behavior trees | Arborist (C++20 static lib, prebuilt External module at `Source/ThirdParty/ArboristLib/`) |
| Road navigation | DroneGraph — nodes/edges loaded from `Content/Graph/nodes.csv` + `edges.csv` |

---

## Phase history

### Phase 1 — Munich environment ✅
Placed a `ACesiumGeoreference` actor anchored at Marienplatz and streamed Google Photorealistic 3D Tiles via Cesium ion.

### Phase 2 — Terrain elevation probe ✅
`ATerrainProbe` — async Cesium height sampling, used to establish ground altitude baseline.

### Phase 3 + 4 — Drone airframe, hover physics, flight controls ✅
- `ADroneActor` (APawn) with `UBoxComponent` physics root
- Cascade PID: angle outer loop → rate inner loop → X-frame motor mixer
- Enhanced Input: throttle, pitch+roll, yaw, camera toggle

### Phase 5 — Dual camera system + mesh ✅
- Chase camera (spring arm, yaw-inherit) and FPV camera (nose-mounted, 90° FOV)
- `UPoseableMeshComponent` for RealisticDroneV2 skeleton; 8 blade props with spin visuals

### Phase 6a — Arborist in UE5 ✅
Prebuilt External UBT module (`Source/ThirdParty/ArboristLib/`). Single static lib combining bt_framework + ryml + sqlite3. WebBrowserWidget plugin added.

### Phase 6b — ATargetPawn with Arborist BT ✅
- Moving patrol car NPC navigating the Munich road graph (`DroneGraph`)
- 4-priority behavior tree: captured → in_capture_range → spotted → patrol
- 1 m capture radius; beacon state driven by BT actions
- `MonitorServer` on port 8080 serving `HUD/arborist/viewer/viewer.html`

### Phase 7 — HUD ✅ (C++ complete, Blueprint wiring pending)

**Goal**: in-game UMG overlay providing flight data, spatial awareness, and tracking score. All five widgets are implemented in C++ and instantiated by `ADroneHUD::BeginPlay()`. Blueprint wiring for the two class-assigned widgets (`PFD`, `Crosshair`) is deferred to Phase 8.

#### ADroneHUD
`ADroneHUD` is the single aggregator. In `BeginPlay()` it resolves `ADroneActor` and `ATargetPawn` via `GetActorOfClass`, creates all five widgets, and adds them to the viewport. A loading screen (`UDroneLoadingWidget`) is shown at z-order 10 and dismissed when `ATargetPawn::OnAltitudeStable` fires — guaranteeing Cesium terrain is streamed before the player sees the world. `DrawHUD()` runs once to compute viewport-relative positions for the mini map and PFD, then sets the `bMiniMapPositioned` flag so it never runs again.

#### 7a — Tracking stats
`UDroneTrackingWidget` — NativePaint widget. Instantiated directly via `StaticClass()` (no Blueprint class needed). Holds a `TObjectPtr` to both `ADroneActor` and `ATargetPawn`; reads tracking state from `ATargetPawn` each tick. Exposes `GetCurrentTimeText()`, `GetBestTimeText()`, and `GetTrackingColor()` as `BlueprintPure` for optional Blueprint text binding. Color shifts when actively tracking.

#### 7b — Mini map
`UMiniMapWidget` — wraps `UWebBrowser`. Instantiated directly via `StaticClass()`. On `NativeConstruct` it binds the browser and loads a local HTML file: `HUD/minimap/minimap.html` converted to an absolute `file:///` URL. The widget is added to the viewport at opacity 0 and made visible only after `OnAltitudeStable` fires. Position and size are set in `ADroneHUD::DrawHUD()` relative to viewport dimensions and PFD radius, keeping the mini map flush with the PFD disc.

#### 7c — PFD (Primary Flight Display)
`UDronePFDWidget` — NativePaint widget. Wired via `PFDWidgetClass` `UPROPERTY` in `BP_DroneHUD`. Caches pitch, roll, speed (m/s), altitude (m), and vertical speed (m/s) from `ADroneActor` each tick. Renders a moving horizon line (tilts with roll, shifts vertically with pitch), speed tape, altitude tape, and vertical speed indicator. `MaxDisplayPitchDeg` is `EditDefaultsOnly` for designer tuning.

#### 7d — Crosshair
`UDroneCrosshairWidget` — NativePaint widget. Wired via `CrosshairWidgetClass` `UPROPERTY` in `BP_DroneHUD`. Caches `bCachedFPVMode` from `ADroneActor` each tick and adapts the crosshair style between chase-camera and FPV-camera modes.

#### 7e — BT display panel ✅
`UBTDisplayWidget` — wraps `WebBrowserWidget` pointed at `http://localhost:[MonitorPort]` (default 8080). `TogglePanel()` flips `bExpanded` (BlueprintReadWrite) so the Blueprint can animate open/close. `GetMonitorURL()` is BlueprintPure. Wired via `BTDisplayWidgetClass` `UPROPERTY`; Blueprint `WBP_BTDisplay` already created and assigned in `BP_DroneHUD`.

---

## Planned phases

### Phase 8 — UI Polish + Flight Feel ✅

**Result screen**
- `UDroneResultWidget` (new) — YOU WIN / YOU LOSE screen with 5×7 pixel art title, three-layer depth shadow, shimmer sweep, and gold gradient; ellipse-shaped TRY AGAIN button with scanline fill, gold ring, hover color change (black → bright gray fill, gold → bright-gold ring), and mouse cursor change; clicking reloads the level

**Main menu layout**
- All elements shifted up ~10% of screen height; title, subtitle, and car/drone previews now correctly distributed with the car fully visible (was clipped 22 px below the bottom at 1080p)
- BT toggle button text centered with symmetric padding (`NormalPadding = FMargin(12, 4)`, slot changed from `HAlign_Fill` to `HAlign_Center`)
- Minimap widget reduced to 75% of previous size while keeping bottom-left anchor

**Minimap ↔ detection sync**
- `FHUDState` extended with `FovDeg` and `DetRangeCm`; both broadcast from `ATargetPawn::Tick` every frame
- `minimap.js` reads `fovDeg` / `detRangeCm` from the WebSocket state; cone now matches actual detection zone exactly (was 500 m / ~145°, now 200 m / 90°)
- `DetectionFovDeg` UPROPERTY on `ATargetPawn` replaces hardcoded `0.3f` dot threshold in `ComputeDroneInFOV`

**LOS 2D pre-filter**
- `UpdateDroneState()` gates the three `LineTraceSingleByChannel` calls behind a cheap 2D (XY-plane) distance and angular check; all three traces are skipped when the drone is clearly outside the detection cone
- Angular filter is bypassed when drone forward vector has negligible horizontal component (steep pitch), letting the 3D check decide

**Roll input (D / A keys)**
- `ControlInput.Roll` now set directly in `ADroneActor::Tick` via `IsInputKeyDown(EKeys::D/A)`; self-clears when released; unaffected by IMC asset configuration
- Roll axis removed from `OnPitchRoll` / `OnPitchRollCompleted` (arrow-key left/right no longer controls roll)

**Tilt recovery**
- `bRollCorrects` / `bPitchCorrects` extended with a tilt-angle term: `Atan2(ActorUp.Y, ActorUp.Z)` for roll, `Atan2(-ActorUp.X, ActorUp.Z)` for pitch; corrective input now bypasses `LimitScale` even when the drone is statically tilted beyond `MaxTiltAngleDeg` with near-zero angular velocity

**Floor collision**
- CCD enabled on `PhysicsBody` (`SetUseCCD(true)`) to prevent tunneling through thin Cesium terrain geometry
- Crash threshold for upward-normal impacts (`ImpactNormal.Z > 0.5`) lowered from 400 cm/s to 150 cm/s

### Phase 9 — Packaging + transition/UX polish ✅

**Goal**: a working standalone `.exe` that runs correctly on a fresh Windows machine without the editor.

**The key problem**: several runtime files are loaded via `FPaths::ProjectDir()` (standard file I/O, not through UE's asset system). In a packaged build `ProjectDir()` resolves to the folder containing the `.exe`. These files must be explicitly bundled:

| File(s) | Loaded by |
|---|---|
| `HUD/minimap/minimap.html`, `minimap.js` | `UMiniMapWidget` — `file:///` URL |
| `HUD/arborist/viewer/viewer.html` | `UBTDisplayWidget` — `file:///` URL |
| `Content/Graph/nodes.csv`, `edges.csv` | `DroneGraph` — raw file I/O |
| `Content/BT/target.yaml` | Arborist — raw file I/O |

**Steps**
1. Add `HUD/` and `Content/Graph/` and `Content/BT/` to **Project Settings → Packaging → Additional Non-Asset Directories to Copy** so the packager copies them next to the `.exe`
2. Windows 64-bit Shipping build — `Project → Package Project → Windows`
3. Smoke test the packaged build locally:
   - Cesium tiles stream correctly
   - Drone flies, win/lose condition triggers
   - BT visualiser loads and shows live BT state
   - Minimap renders with correct cone

**What shipped**
- **HUD assets relocated** from project-root `HUD/` into `Content/HUD/`; runtime loaders switched `FPaths::ProjectDir()` → `FPaths::ProjectContentDir()` (`MiniMapWidget`, `BTDisplayWidget`, `DroneGraph`, loading widget). `HUD/`, `BT/`, `Graph/` staged as non-UFS so the `file:///` and raw-IO paths resolve next to the `.exe`.
- **Cook fixes**: Munich added to `MapsToCook`; `/Game` added to `DirectoriesToAlwaysCook` (the cooker doesn't follow `FName`/`LoadObject` string references, which was dropping the Munich level and `BP_CarDisplay`).
- **Level-transition blackout** — root cause of the "dusk sky with clouds" flash entering Munich: Munich is a World Partition level, so its `SkyAtmosphere`/`VolumetricCloud`/lighting stream in cold from the pak *after* `LoadMap`, rendering a half-lit sky before the loading widget covered it (packaged-only; in PIE the cells are already resident). Fixed with a `UDroneGameInstance` that sets `UGameViewportClient::bDisableWorldRendering` on entering Munich (via `PostLoadMapWithWorld`) and clears it from `ADroneHUD` once the scene is ready. *Gotcha:* `GameInstanceClass` must live under `[/Script/EngineSettings.GameMapsSettings]`, not `[/Script/Engine.Engine]`, or the engine silently falls back to the default GameInstance.
- **Loading screen gating**: dismiss now waits for both altitude-stable **and** the minimap actually being ready (`MINIMAP_READY` console sentinel from `minimap.js` → `UMiniMapWidget::OnReady`), so the HUD no longer appears before the minimap canvas.
- **ESC Options/Pause screen** (`UDroneOptionsWidget`): fade in/out, pauses the sim, "Back to Main Menu" / "Exit", hover-highlighted entries, pixel-art gold styling matching the result screen. Requires in-game mouse cursor — `DroneActor` input mode switched to `GameAndUI` with cursor shown.
- **BT viewer loading spinner**: animated gold spinner on `UBTDisplayWidget` while `viewer.html` loads, dismissed by a `BT_VIEWER_READY` console sentinel (15s safety fallback).
- **Minimap size** reduced and decoupled from its bottom margin; frame rate capped (`t.MaxFPS`).

### Phase 10 — Cloud Deployment (Vagon Streaming) ✅

**Goal**: showcase the simulator on-demand via Vagon's cloud streaming platform — no install required for the viewer, no infrastructure to manage.

**Why Vagon instead of Pixel Streaming / AWS**
The Pixel Streaming approach was replaced because it requires maintaining AWS infrastructure (signaling server, TURN server, EC2 instance, Lambda wake controller). Vagon provides the entire streaming stack out of the box and integrates directly with UE5 packaged builds.

**What was done**

- **Build config** — `DefaultGame.ini` switched to `BuildConfiguration=PPBC_Shipping`
- **Cook coverage** — `DirectoriesToAlwaysCook=(Path="/Game")` ensures the entire Content directory is cooked, including Blueprint assets (`BP_CarDisplay`) referenced only by string at runtime
- **Correct executable** — Vagon must be pointed at `DroneChallenger.exe` (root launcher), not `Binaries/Win64/DroneChallenger-Win64-Shipping.exe`. The root launcher sets the correct working directory so `FPaths::ProjectContentDir()` resolves properly
- **Machine type** — Must use an **RTX-capable tier** (G2 or G3). The project enables `r.RayTracing=True` and `r.Lumen.HardwareRayTracing=True`, which require DXR hardware support. The Tesla T4 (OptiX & CUDA tier) does not support DXR and causes rendering failures including broken SceneCapture output
- **BP_CarDisplay spawning** — `ADroneMainMenuHUD::BeginPlay()` now spawns `BP_CarDisplay` (car mesh + SceneCapture2D) at `Z = -100000` (underground) before creating the menu widget. Underground spawn keeps the mesh out of the main camera view while the SceneCapture child component still captures the car correctly (child transforms are relative to the actor)
- **RT resource clearing fixed** — `DroneMainMenuWidget::NativeConstruct` previously called `RT->UpdateResource()` on both `RT_CarPreview` and `RT_DronePreview`, which recreates the GPU texture from scratch and discards SceneCapture content. Both calls removed; the widget only loads the asset reference and reads from it via the material
- **Loading screen gating — Cesium tiles** — `ADroneHUD::PollTileStreaming()` now checks Cesium tile load progress via the UE5 reflection API (`FindObject<UClass>` + `ProcessEvent("GetLoadProgress")`), avoiding `#include "Cesium3DTileset.h"` which triggers Windows min/max macro conflicts. Loading screen is held until every `ACesium3DTileset` actor in the level reports `GetLoadProgress() == 100`
- **Input blocking during load** — `ADroneHUD` disables drone input (`CachedDrone->DisableInput`) in `BeginPlay` and re-enables it (`EnableInput`) only when all three readiness conditions are met. `ToggleOptions` (ESC) is also gated by `bLoadingActive`

**Cost model**

| Cost | Amount |
|---|---|
| Base (storage/availability) | $0.67/day → ~$20/month |
| Per session (10-min cap) | ~$0.42/session |
| Example: 1 session/day | ~$33/month total |

**Remaining**
- Validate the full build on Vagon (G2 or G3 tier) — confirm BP car display, loading screen timing, and input blocking all behave correctly

---

## Open items

| Item | Phase | Status |
|---|---|---|
| Validate Vagon deployment on G2 or G3 tier (car display, loading screen, input blocking) | 10 | In progress |

---

## File map

```
DroneChallenger/
├── plan.md
├── CLAUDE.md
├── context/
│   ├── flight-controller.md
│   ├── actor-architecture.md
│   ├── coordinate-system.md
│   └── arborist-integration.md
├── Content/
│   ├── BT/target.yaml
│   ├── Graph/nodes.csv + edges.csv
│   ├── BP_DroneGameMode.uasset
│   ├── BP_DroneHUD.uasset
│   └── WBP_BTDisplay.uasset
└── Source/
    ├── ThirdParty/ArboristLib/
    └── DroneChallenger/
        ├── DroneActor.h/.cpp
        ├── DroneFlightController.h/.cpp
        ├── DroneMotorMixer.h
        ├── DronePIDController.h
        ├── DroneGraph.h/.cpp
        ├── TargetPawn.h/.cpp
        ├── TargetAIController.h/.cpp
        ├── PatrolPath.h/.cpp
        ├── DroneHUD.h/.cpp
        ├── BTDisplayWidget.h/.cpp
        ├── DroneTrackingWidget.h/.cpp
        ├── MiniMapWidget.h/.cpp
        ├── DronePFDWidget.h/.cpp
        ├── DroneCrosshairWidget.h/.cpp
        ├── DroneLoadingWidget.h/.cpp
        ├── DroneResultWidget.h/.cpp
        ├── DroneMainMenuWidget.h/.cpp
        ├── DroneGameMode.h/.cpp
        └── DroneChallenger.Build.cs
```
