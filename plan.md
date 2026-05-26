# DroneChallenger — Project Plan

## Vision

A first-person drone simulator set in a georeferenced recreation of Munich, built on Unreal Engine 5.7.4 and Cesium for Unreal. The primary game mode is **Search & Destroy**: the player pilots an FPV drone across a 2 km² urban area, locates three hidden targets using the drone's camera, and eliminates them within a 10-minute window.

Target behaviour is driven by a **Behavior Tree** (Arborist framework) so that target state, visual reactions, and the capture sequence are all governed by a live, inspectable tree — not hardcoded C++ logic. The BT visualiser is displayed alongside the simulator as a collapsible in-game panel, showing in real time which nodes are active as the drone moves.

The simulation prioritises physical realism (cascade PID flight controller, thrust-based physics, angle-mode self-levelling) and environmental authenticity (Google Photorealistic 3D Tiles streamed live via Cesium ion).

---

## Game concept — Search & Destroy

- **Playspace**: 2 km radius around Marienplatz, Munich (48.1374 °N, 11.5755 °E)
- **Targets**: 3 ground objects placed at fixed geographic coordinates within the playspace
- **Time limit**: 10 minutes
- **Win condition**: all 3 targets captured before time expires
- **Lose condition**: timer reaches zero with at least one target remaining

### Detection & capture loop

1. **Search** — drone flies FPV; targets are not marked until found
2. **Detect** — when a target enters the drone camera's FOV cone within detection range (~300 m), it transitions to *Detected* and appears on the mini-map; the BT reacts by activating the beacon
3. **Capture** — drone flies within capture radius (~30 m) and holds position for 2 s; the BT drives the capture sequence and fires the captured delegate

Each target runs its own `bt::BehaviorTree` instance (Arborist). The blackboard is fed drone state each tick (distance, in-FOV flag, capture timer). The BT owns all state transitions and visual responses.

### BT visualiser

A collapsible UMG panel in the corner of the game screen embeds a `WebBrowserWidget` pointed at `http://localhost:8080`. Arborist's `MonitorServer` streams the live active-node path (colour-coded SUCCESS / FAILURE / RUNNING) to that port. As the drone approaches a target, the active BT path visibly changes — showing exactly what the target "knows" and what behaviour it is in.

### HUD elements (Phase 7)

| Element | Position | Shows |
|---|---|---|
| Timer | top-left | MM:SS countdown |
| Target counter | top-left | X / 3 captured |
| Target lock overlay | world-space rectangle | appears when target is Detected; shows distance in metres |
| Mini-map | top-right | drone position + heading cone; detected targets as dots |
| Telemetry bar | bottom | speed (m/s), altitude AGL (m), heading (°) |
| BT visualiser panel | collapsible corner | live Arborist MonitorServer via WebBrowserWidget |

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
| Build modules | Core, CoreUObject, Engine, InputCore, EnhancedInput, CesiumRuntime, UMG, Slate, SlateCore, WebBrowser |

---

## Arborist integration overview

Arborist is a standalone C++20 static library (CMake). It has no engine coupling by design. See `context/arborist-integration.md` for full API reference and UE5 wiring details.

**Key classes used:**

| Class | Role |
|---|---|
| `bt::BehaviorTree` | Owns root node; call `tree.tick()` each frame |
| `bt::Blackboard` | Typed key-value store; sources are lambdas polled each tick |
| `bt::RuntimeRegistry` | Maps action/condition names (from YAML) to C++ lambdas |
| `bt::SchemaLoader` | Parses YAML schema into node tree at startup |
| `bt::DecisionEmitter` | Records active path + blackboard snapshot each tick |
| `bt::MonitorServer` | Embedded HTTP server (port 8080); streams live state to browser |

**Per-target blackboard keys:**

| Key | Type | Source |
|---|---|---|
| `drone_distance` | double | distance from target to drone world position (cm) |
| `drone_in_fov` | bool | camera cone + range check (same logic as detection) |
| `drone_in_capture_range` | bool | distance < CaptureRadius |
| `capture_timer` | double | seconds drone has been in capture range continuously |
| `is_captured` | bool | latched true once capture completes |

**YAML tree (per target):**

```yaml
priority:
  - name: Captured
    condition: is_captured
    sequence:
      - action: deactivate_beacon

  - name: InCaptureRange
    condition: drone_in_capture_range
    sequence:
      - action: pulse_beacon_fast
      - action: increment_capture_timer

  - name: Detected
    condition: drone_in_fov
    sequence:
      - action: pulse_beacon_slow

  - name: Idle
    sequence:
      - action: idle
```

**Integration pattern in UE5:**

Each `ATargetActor` owns a `bt::BehaviorTree` instance (via `TUniquePtr`). On each `Tick`, the actor:
1. Updates blackboard sources (distance, FOV flag, etc.)
2. Calls `tree.tick()`
3. Reads BT output via action lambdas (which directly call UE5 APIs on the actor)

A single `bt::MonitorServer` is started by the game mode at session begin and destroyed at session end. The monitor is attached to the selected target's tree (switchable via the HUD panel).

---

## Phase history

### Phase 1 — Munich environment ✅
Placed a `ACesiumGeoreference` actor anchored at Marienplatz and streamed Google Photorealistic 3D Tiles via Cesium ion. Confirmed terrain, buildings, and roads render correctly within the 2 km playspace.

### Phase 2 — Terrain elevation probe ✅
Implemented `ATerrainProbe` — an editor utility actor that calls `ACesium3DTileset::SampleHeightMostDetailed` asynchronously and logs ellipsoidal heights at three Munich test positions. Used to establish the ground altitude baseline for drone spawn placement.

### Phase 3 + 4 — Drone airframe, hover physics, and flight controls ✅
- `ADroneActor` (APawn) with `UBoxComponent` physics root (35 × 40 × 11 cm, Pawn collision)
- Hover throttle computed from mass and gravity at BeginPlay; all four rotors seeded to hover value
- Per-rotor `AddForceAtLocation` for pitch/roll torques; separate `AddTorqueInRadians` for yaw drag
- Cascade PID flight controller: angle outer loop (self-levelling) → rate inner loop → X-frame motor mixer
- Enhanced Input bindings: throttle (1D), pitch+roll (2D), yaw (1D), camera toggle

### Phase 5a — Dual camera system ✅
- Chase camera on a rigid spring arm (yaw-inherit only, −15° pitch, 180 cm arm, building collision)
- FPV camera nose-mounted on VisualMesh at (18, 0, 9) cm, −15° pitch, 90° FOV
- Toggle via `IA_SwitchCamera`

### Phase 5b — RealisticDroneV2 mesh, flight controller fixes, propeller visuals ✅
- `UPoseableMeshComponent` for the SK_Realistic_Drone skeleton (visual-only, no physics)
- 8 `UStaticMeshComponent` blade props positioned via `GetBoneLocation` at runtime; CCW mesh on FL+RR, CW mesh on FR+RL; two blades per motor offset 180°
- Fixed motor mixer pitch inversion (front rotors now correctly spin up for nose-up command)
- Propeller spin rate: 4320 deg/s max with per-motor `FInterpTo` spool inertia (MotorSpoolRate = 8)
- Motor audio pitch mapped from average throttle [0.6×–1.8×]

---

## Planned phases

### Phase 6 — Arborist integration + target system

**Goal**: Arborist running inside UE5, three BT-driven targets in the Munich playspace, live BT visualiser in-game.

#### 6a — Arborist in UE5 ✅

Arborist is integrated as a **prebuilt External UBT module** (`Source/ThirdParty/ArboristLib/`).

- `arborist.lib` (86 MB) combines `bt_framework + ryml + c4core + sqlite3 + brotli` into a single static lib via `lib.exe`
- Built from `github.com/doryunger/arborist` using **vcpkg overlay port** (`arborist/vcpkg-port/`) with the `x64-windows-static-md` triplet — static `.lib` output compiled with `/MD` to match UE5's CRT
- yaml-cpp replaced with **ryml** (rapidyaml) to eliminate `__std_find_last_not_ch_pos_1` linker errors caused by yaml-cpp's vectorised STL string intrinsics not being available in UE5's link environment
- `ArboristLib.Build.cs` registers the lib and public headers as `ModuleType.External`
- `lib/` and `include/` are gitignored — populated by a setup script (deferred to Phase 9)
- `TerrainProbe.cpp` fix: `#include "Windows/WindowsHWrapper.h"` + `#undef OPAQUE / TRANSPARENT` inserted before `#include "Cesium3DTileset.h"` to prevent Windows GDI macros from breaking CesiumGltf template instantiation under MSVC's conformant preprocessor (`/Zc:preprocessor`)
- `WebBrowserWidget` plugin added to uproject; `WebBrowser` module added to `Build.cs`
- Smoke test: `ATerrainProbe::BeginPlay` parses a one-action YAML schema, ticks the tree, and logs `ArboristLib smoke test: PASS`

**Done when**: project compiles cleanly with Arborist linked; a minimal tree ticks without crash in PIE.

#### 6b — `ATargetPawn` with Arborist BT ✅

New files: `Source/DroneChallenger/TargetPawn.h/.cpp`, `TargetAIController.h/.cpp`, `PatrolPath.h/.cpp`

Targets are moving ground vehicle NPCs (`APawn`), not static actors.

- `ATargetPawn : APawn` — possessed by `ATargetAIController` (`AutoPossessAI = PlacedInWorldOrSpawned`); owns `bt::BehaviorTree*`
- `APatrolPath` — `AActor` with `USplineComponent` root; placed in the level along Munich road segments; referenced by `ATargetPawn::PatrolPath`
- Movement: BT sets `CurrentSpeed` (`set_speed_normal` / `set_speed_fast` / `stop`), then `advance_along_path` advances `SplineDistance` along the spline. Ground clamping via synchronous downward line trace against Cesium tile collision geometry.
- BT behaviors (priority order):

| Behavior | Condition | Actions |
|---|---|---|
| `captured` | `is_captured` | stop, deactivate_beacon |
| `in_capture_range` | `drone_in_capture_range` | stop, pulse_beacon_fast, increment_capture_timer |
| `evading` | `drone_in_fov` | set_speed_fast, pulse_beacon_fast, advance_along_path |
| `patrol` | *(always)* | set_speed_normal, pulse_beacon_slow, advance_along_path |

- FOV detection: `PlayerController::GetControlRotation().Vector()` dot product ≥ 0.707 within `DetectionRange` (300 m)
- `FOnTargetCaptured` multicast delegate fires after 2 s continuous hold in capture radius (30 m)
- YAML schema: `Content/BT/target.yaml`

Three instances placed in the Munich level at fixed geographic positions:

| Target | Latitude | Longitude | Notes |
|---|---|---|---|
| T1 | 48.1503 | 11.5754 | Englischer Garten north edge (~1.4 km N) |
| T2 | 48.1348 | 11.5764 | Viktualienmarkt area (~300 m S) |
| T3 | 48.1368 | 11.5944 | Isar east bank (~1.4 km E) |

**Done when**: pawns patrol their splines; beacon reacts correctly to drone proximity and detection; vehicle stops and capture completes after 2 s hold; delegate fires.

#### 6c — `ASearchDestroyGameMode` + `ASearchDestroyGameState`

New files: `SearchDestroyGameMode.h/.cpp`, `SearchDestroyGameState.h/.cpp`

- `ASearchDestroyGameState`: `RemainingTime`, `CapturedCount`, `TotalTargets`, `bSessionActive`, `bPlayerWon`
- `ASearchDestroyGameMode`: gathers all `ATargetPawn` at BeginPlay; starts 600 s countdown; owns the `bt::MonitorServer` (port 8080); handles win/lose
- MonitorServer attached to T1's tree by default; switchable from HUD

**Done when**: session starts, timer counts down, all three captures trigger win, timeout triggers lose, MonitorServer serves live data at `localhost:8080`.

---

### Phase 7 — HUD

**Goal**: in-game UMG overlay making game state legible; BT visualiser panel alongside the main view.

#### 7a — Game state bar
- Timer (MM:SS), target counter (X/3), wired to `ASearchDestroyGameState`

#### 7b — Target lock overlay
- World-space projection of detected target position (`UGameplayStatics::ProjectWorldToScreen`)
- Rectangle + distance label; disappears on capture

#### 7c — Mini-map
- Overhead drone icon + heading cone
- Detected targets as coloured dots; active targets hidden; captured targets faded
- 2 km radius in mini-map bounds

#### 7d — Telemetry bar
- Speed (m/s), altitude AGL (m, periodic Cesium height sample every 2 s), heading (°)

#### 7e — BT visualiser panel
- Collapsible UMG panel; `WebBrowserWidget` loading `http://localhost:8080`
- Toggle key (e.g. Tab) shows/hides panel
- Dropdown to switch which target's tree is displayed (T1 / T2 / T3)
- MonitorServer reattached to selected tree on switch

---

### Phase 8 — Settings menu

**Goal**: in-game pause menu exposing flight controller parameters.

- Accessible via Escape
- Sliders: `MaxTiltAngle`, `AngleGain`, `MaxYawRate`, `ThrottleRange`, `LinearDamping`, `AngularDamping`
- Changes apply live to the active `ADroneActor`
- Persisted via `UGameUserSettings` subclass to `Saved/Config`
- `UDroneSettingsWidget` (UUserWidget, C++ only)

---

### Phase 9 — Packaging & release

**Goal**: distributable Windows build.

- Main menu level (start, settings, quit)
- `RealisticDroneV2` cooked into build (gitignored, baked at package time)
- **Arborist setup script** (`setup.ps1`): installs vcpkg if absent; runs `vcpkg install arborist-poc:x64-windows-static-md --overlay-ports=<arborist-repo>/vcpkg-port`; combines output static libs with `lib.exe` into `arborist.lib`; copies `bt/` headers — must run once before opening the project on a fresh machine
- MonitorServer and EditorServer disabled in shipping build (`#if !UE_BUILD_SHIPPING`)
- Windows 64-bit shipping configuration; Cesium streaming budget tuned for 2 km playspace
- Frame-rate floor check: stable 60 FPS during active BT ticking + terrain streaming

---

## Open items / known issues

| Item | Status |
|---|---|
| Yaw direction (CW vs CCW) unverified in-engine | Needs one PIE test — if yaw-right turns left, negate `YawSpin[4]` in `ApplyRotorForces` |
| Target vehicle mesh not chosen | Any static mesh assignable to `ATargetActor` in editor |
| Drone spawn altitude | Currently editor-placed Z; should be terrain-sampled via Cesium before Phase 6b |
| Arborist setup script | Written in Phase 9 — clones arborist repo, builds lib, populates ThirdParty dirs |
| MonitorServer multi-target switching | One server, one tree at a time — switching requires `monitor.attachTree(&newTree)` |

---

## File map

```
DroneChallenger/
├── plan.md                               ← this file
├── CLAUDE.md                             ← development rules
├── context/
│   ├── flight-controller.md              ← PID cascade, motor mixer, sign conventions
│   ├── actor-architecture.md             ← component hierarchy, blade system, cameras
│   ├── coordinate-system.md             ← UE5 world frame, Cesium coords, Munich anchor
│   └── arborist-integration.md          ← Arborist API reference, UE5 wiring pattern
├── Content/
│   └── BT/
│       └── target.yaml                   ← shared BT schema for all three targets
└── Source/
    ├── ThirdParty/
    │   └── ArboristLib/                  ← External UBT module; lib/ and include/ gitignored, populated by setup.ps1
    └── DroneChallenger/
        ├── DroneActor.h/.cpp             ← APawn: physics, mesh, cameras, input, audio
        ├── DroneFlightController.h/.cpp  ← cascade angle-mode PID
        ├── DroneMotorMixer.h             ← X-frame mixing table
        ├── DronePIDController.h          ← generic PID with derivative-on-measurement
        ├── TargetPawn.h/.cpp             ← APawn: BT-driven moving NPC target (Phase 6b)
        ├── TargetAIController.h/.cpp     ← minimal AAIController for TargetPawn (Phase 6b)
        ├── PatrolPath.h/.cpp             ← AActor + USplineComponent road path (Phase 6b)
        ├── SearchDestroyGameMode.h/.cpp  ← session timer, MonitorServer (Phase 6c)
        ├── SearchDestroyGameState.h/.cpp ← replicated session state (Phase 6c)
        ├── TerrainProbe.h/.cpp           ← editor utility: Cesium height sampling
        └── DroneChallenger.Build.cs      ← module + Arborist + AIModule dependencies
```
