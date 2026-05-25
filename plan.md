# DroneChallenger — Project Plan

## Vision

A first-person drone simulator set in a georeferenced recreation of Munich, built on Unreal Engine 5.7.4 and Cesium for Unreal. The primary game mode is **Search & Destroy**: the player pilots an FPV drone across a 2 km² urban area, locates three hidden targets using the drone's camera, and eliminates them within a 10-minute window.

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
2. **Detect** — when a target enters the drone camera's FOV cone within detection range (~300 m), it transitions to *Detected* and appears on the mini-map
3. **Capture** — drone flies within capture radius (~30 m) and holds position for 2 s; target transitions to *Captured*

### HUD elements (Phase 7)

| Element | Position | Shows |
|---|---|---|
| Timer | top-left | MM:SS countdown |
| Target counter | top-left | X / 3 captured |
| Target lock overlay | on-screen world-space rectangle | appears when target is Detected; shows distance in metres |
| Mini-map | top-right | drone position + heading cone; detected targets as dots |
| Telemetry bar | bottom | speed (m/s), altitude AGL (m), heading (°) |

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
| UI | UMG + Slate |
| Build modules | Core, CoreUObject, Engine, InputCore, EnhancedInput, CesiumRuntime, UMG, Slate, SlateCore |

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

### Phase 6 — Target system

**Goal**: three targetable objects in the Munich playspace with camera-based detection and proximity capture.

#### 6a — `ATargetActor`

New file: `Source/DroneChallenger/TargetActor.h/.cpp`

- `UCesiumGlobeAnchorComponent` for georeferenced placement (latitude/longitude set in editor)
- `UStaticMeshComponent` visual mesh — assignable in editor; default placeholder until a vehicle asset is chosen
- `UPointLightComponent` beacon — off while Active, bright while Detected, off again when Captured
- `ETargetState` enum: `Active → Detected → Captured`
- **Detection**: checked at 10 Hz in Tick — if target is within `DetectionRange` (300 m) AND within the active camera's FOV cone angle, transition to Detected
- **Capture**: `USphereComponent` at `CaptureRadius` (30 m); when the drone pawn overlaps and holds for `CaptureHoldTime` (2 s), fire `OnCaptured` delegate and transition to Captured
- All radii and hold time are `EditAnywhere` UPROPERTY so designers can tune per-target

Three pre-placed geographic positions in the Munich level (within 2 km of Marienplatz, varied enough to require real searching).

#### 6b — `ASearchDestroyGameMode` + `ASearchDestroyGameState`

New files: `Source/DroneChallenger/SearchDestroyGameMode.h/.cpp`, `SearchDestroyGameState.h/.cpp`

- `ASearchDestroyGameState`: `RemainingTime`, `CapturedCount`, `TotalTargets`, `bSessionActive`, `bPlayerWon`
- `ASearchDestroyGameMode`: gathers all `ATargetActor` instances at BeginPlay via `UGameplayStatics::GetAllActorsOfClass`; starts a countdown timer (`SessionDuration` = 600 s); calls `OnTargetCaptured` when a target fires its delegate; triggers win/lose when all captured or timer expires
- Logs outcome to Output Log (HUD will pick it up in Phase 7)

**Done when**: three targets are placeable in the Munich level, camera detection transitions them to Detected state, flying within 30 m and holding for 2 s captures them, and the game mode correctly tracks all three to a win/lose outcome.

---

### Phase 7 — HUD

**Goal**: in-game UMG overlay that makes the game state legible during flight.

The FPV camera is the primary view. No picture-in-picture camera panel.

#### 7a — Game state widgets
- `UDroneChallengeHUD` (UUserWidget): timer (MM:SS), target counter (X/3)
- Wired to `ASearchDestroyGameState` — polls replicated values each tick

#### 7b — Target lock overlay
- When a target enters Detected state and is visible on screen, project its world position to screen space (`UGameplayStatics::ProjectWorldToScreen`)
- Draw a rectangle around the projected point with a distance label (metres)
- Disappears when target is Captured

#### 7c — Mini-map
- Fixed overhead view centred on drone position
- Drone icon with heading indicator
- Detected targets appear as coloured dots; Active targets are invisible; Captured targets shown as faded
- 2 km radius represented in the mini-map bounds

#### 7d — Telemetry bar
- Speed: magnitude of `PhysicsBody->GetPhysicsLinearVelocity()` converted to m/s
- Altitude AGL: drone ellipsoidal height minus latest Cesium terrain height sample (sampled every 2 s to avoid hammering the async API)
- Heading: yaw in degrees (0–360, north-up)

---

### Phase 8 — Settings menu

**Goal**: in-game pause menu that exposes flight controller parameters so players can tune the feel without recompiling.

- Accessible via Escape during a session
- Sliders for: `MaxTiltAngle`, `AngleGain`, `MaxYawRate`, `ThrottleRange`
- Sliders for: `LinearDamping`, `AngularDamping`
- Changes apply live to the active `ADroneActor` flight controller instance
- Settings persisted between sessions via `UGameUserSettings` subclass saved to `Saved/Config`
- C++ `UDroneSettingsWidget` (UUserWidget); no Blueprint logic

---

### Phase 9 — Packaging & release

**Goal**: a distributable build with all assets cooked.

- Main menu level: start session, open settings, quit
- `RealisticDroneV2` asset pack cooked into the build (excluded from source control via `.gitignore`; baked at package time)
- Packaging configuration: Windows 64-bit, shipping build, Cesium streaming budget tuned for the Munich 2 km playspace
- Frame-rate floor check: verify stable 60 FPS during active target detection and terrain streaming in the playspace before marking done

---

## Open items / known issues

| Item | Status |
|---|---|
| Yaw direction (CW vs CCW) unverified in-engine | Needs one PIE test — if pressing yaw-right turns left, negate `YawSpin[4]` in `ApplyRotorForces` |
| RealisticDroneV2 target vehicle mesh not yet chosen | Any static mesh can be assigned to `ATargetActor::VisualMesh` in editor |
| Drone spawn altitude needs terrain-sampled height | Currently spawned at editor-placed Z; Phase 6 terrain probe work can inform final spawn position |
| `Content/__ExternalActors__/ThirdPerson/` in working tree | Untracked ThirdPerson template artefact — confirm safe to gitignore before adding |

---

## File map

```
DroneChallenger/
├── plan.md                          ← this file
├── CLAUDE.md                        ← development rules
├── context/
│   ├── flight-controller.md         ← PID cascade, motor mixer, sign conventions
│   ├── actor-architecture.md        ← component hierarchy, blade system, cameras
│   └── coordinate-system.md        ← UE5 world frame, body angular velocity, Cesium coords
└── Source/DroneChallenger/
    ├── DroneActor.h/.cpp            ← APawn: physics, mesh, cameras, input, audio
    ├── DroneFlightController.h/.cpp ← cascade angle-mode PID
    ├── DroneMotorMixer.h            ← X-frame mixing table
    ├── DronePIDController.h         ← generic PID with derivative-on-measurement
    ├── TerrainProbe.h/.cpp          ← editor utility: Cesium height sampling
    └── DroneChallenger.Build.cs     ← module dependencies
```
