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

### Phase 9 — Packaging
- Enable **Pixel Streaming** plugin in the project (`Edit → Plugins → Pixel Streaming`)
- Add launch flags to the packaged executable:
  ```
  DroneChallenger.exe -PixelStreamingIP=0.0.0.0 -PixelStreamingPort=8888 -AudioMixer -RenderOffScreen
  ```
- Disable MonitorServer in shipping (`#if !UE_BUILD_SHIPPING`) — BT visualiser is dev-only
- Verify Cesium ion API key is a project-scoped key (not personal) before packaging
- Windows 64-bit shipping build via `Project → Package Project → Windows`
- Smoke test the packaged build locally before upload

### Phase 10 — Cloud Deployment (Pixel Streaming on-demand)

**Goal**: a recruiter clicks a URL, waits ~3–4 minutes, and plays the game live in their browser — no install.

**Infrastructure**

```
Portfolio page (static, Vercel/Netlify — free)
       │
       ▼
Wake Controller (AWS Lambda + API Gateway — free tier)
       │
       ├─ instance running? ──► return stream URL
       └─ instance stopped? ──► call EC2 start_instances(), return "warming"
              │
              ▼
GPU Instance (AWS g4dn.xlarge, stopped when idle)
  ├── Epic Pixel Streaming Infrastructure (signaling server, TURN server)
  └── DroneChallenger.exe (auto-launches with Pixel Streaming flags on boot)
```

**Components to set up**

| Component | What it is | Cost |
|---|---|---|
| Elastic IP | Fixed public IP so the wake controller always knows the address | ~$3.65/month while stopped, free while running |
| AWS Lambda (×2) | `POST /wake` — starts instance; `GET /status` — checks readiness | Free tier |
| g4dn.xlarge | GPU instance; NVIDIA T4; runs the game + signaling server | ~$0.53/hr, only when running |
| Auto-shutdown watchdog | Script on the instance: stop after 15 min of no WebRTC connections | $0 |

**Instance startup sequence (auto, on boot)**
1. Windows boots
2. Startup task runs Epic signaling server (`node cirrus.js`)
3. Startup task launches `DroneChallenger.exe` with Pixel Streaming flags
4. Lambda `/status` polls `http://[elastic-ip]:80/` — returns `ready` when signaling server responds

**User experience**
```
[Launch DroneChallenger]  ← button on portfolio page
         │
         ▼
"Starting GPU session... (~3 minutes)"
[████████░░░░░░░░░░░░]  ← frontend polls /status every 5 s
         │  (signaling server responds)
         ▼
"Session ready. Connecting..."
         │
         ▼
Pixel Stream in the same browser tab
```

**Session limits**
- Max session: 20 minutes (watchdog stops the instance)
- If no connection after 5 minutes of starting: auto-stop (failed launch guard)

**Cesium note**: Cesium tile requests originate from the server during Pixel Streaming (the game runs server-side). Ensure the Cesium ion project-scoped key has sufficient tile request quota for server-side rendering.

**Estimated cost for portfolio use**: ~5 recruiter sessions × 20 min = ~$1/month in GPU time + ~$3.65/month Elastic IP = **under $5/month total**.

---

## Open items

| Item | Phase | Status |
|---|---|---|
| Pixel Streaming plugin enable + launch flags | 9 | Not started |
| Cesium ion project-scoped API key | 9 | Not started |
| Windows 64-bit shipping build | 9 | Not started |
| AWS infrastructure (Lambda, Elastic IP, g4dn) | 10 | Not started |
| Epic Pixel Streaming Infrastructure on instance | 10 | Not started |
| Auto-shutdown watchdog script | 10 | Not started |
| Portfolio page with wake UI + status polling | 10 | Not started |

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
