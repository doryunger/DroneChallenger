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

### Phase 7e — BT visualiser panel ✅
- `ADroneHUD` + `UBTDisplayWidget` (C++) wired to `WBP_BTDisplay` Blueprint
- `WebBrowserWidget` loads `http://localhost:8080` — live BT tree alongside gameplay
- Toggleable panel via `TogglePanel()`

---

## Current phase — Phase 7: HUD

**Goal**: in-game UMG overlay providing flight data, spatial awareness, and tracking score.

### 7a — Tracking stats ✅ (C++ complete, Blueprint pending)

`UDroneTrackingWidget` — exposes three BlueprintPure functions:
- `GetCurrentTrackingTime()` — seconds target has been in FOV continuously this streak
- `GetBestTrackingTime()` — longest streak this session
- `IsCurrentlyTracking()` — true while drone_in_fov is active

Tracking logic lives in `ATargetPawn::Tick()` — increments `CurrentTrackingTime` while `bDroneInFOV`, resets on loss of contact, updates `BestTrackingTime` on new high.

**Remaining**: create `WBP_TrackingDisplay` UMG widget in editor; bind Blueprint text blocks to the three functions; anchor to top-left or top-right of screen.

### 7b — Mini map ✅ (C++ complete, Blueprint pending)

`UDroneMiniMapWidget` — NativePaint renders:
- Road network from `DroneGraph` (nodes/edges projected to 2D, 500 m radius)
- North-up map; heading indicator tick on compass ring
- N / E / S / W labels on ring edge
- 2D FOV frustum cone from drone icon (90° horizontal, detection range length)
- Drone arrow at center; target dot (yellow when in FOV, red otherwise)

**Remaining**: create `WBP_MiniMap` in editor; set widget size (e.g., 220 × 220); anchor to corner; assign `WBP_MiniMap` class to `ADroneHUD::MiniMapWidgetClass` in `BP_DroneHUD`.

### 7c — Attitude arc ✅ (C++ complete, Blueprint pending)

`UDroneAttitudeWidget` — NativePaint renders:
- Semicircular arc border with horizontal wings (∩ shape)
- Moving horizon line: tilts with roll, shifts vertically with pitch
- Fixed reference wings marking level flight
- Pitch scale ticks at ±10° and ±20°

Placed below the drone center crosshair.

**Remaining**: create `WBP_AttitudeArc` in editor; anchor to screen center + offset down; assign class in `BP_DroneHUD`.

---

## Planned phases

### Phase 8 — Telemetry bar
- Speed (m/s), altitude AGL (m), heading (°)
- Single horizontal bar at bottom of screen
- Data from `ADroneActor::GetVelocity()`, `GetActorLocation().Z`, `GetActorRotation().Yaw`

### Phase 9 — Hunting mode timer
- 30 s warmup indicator at session start (countdown before capture zone activates)
- Capture hold timer (counts up while within 1 m after warmup)
- Logic lives in `ATargetPawn` or a dedicated game mode

### Phase 10 — Settings menu
- Pause on Escape; sliders for PID gains and flight envelope parameters
- Persisted via `UGameUserSettings` subclass

### Phase 11 — Packaging
- Windows 64-bit shipping build
- Arborist setup script (`setup.ps1`) to rebuild the static lib on fresh machines
- MonitorServer disabled in shipping (`#if !UE_BUILD_SHIPPING`)

---

## Open items

| Item | Status |
|---|---|
| WBP_TrackingDisplay Blueprint | Not started |
| WBP_MiniMap Blueprint | Not started |
| WBP_AttitudeArc Blueprint | Not started |
| Hunting mode 30 s warmup logic | Not started |
| Telemetry bar | Not started |
| Arborist setup script | Deferred to Phase 11 |
| Multi-target switching in MonitorServer | Not needed for single-target mode |

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
        ├── DroneMiniMapWidget.h/.cpp
        ├── DroneAttitudeWidget.h/.cpp
        ├── DroneGameMode.h/.cpp
        └── DroneChallenger.Build.cs
```
