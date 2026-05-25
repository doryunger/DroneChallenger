# Drone Actor Architecture

## Component hierarchy

```
ADroneActor (APawn)
└── PhysicsBody (UBoxComponent) — root, physics simulation owner
    ├── VisualMesh (UPoseableMeshComponent) — visual only, no physics
    │   ├── PropFL_A / PropFL_B  (UStaticMeshComponent) — front-left blades
    │   ├── PropFR_A / PropFR_B  (UStaticMeshComponent) — front-right blades
    │   ├── PropRL_A / PropRL_B  (UStaticMeshComponent) — rear-left blades
    │   ├── PropRR_A / PropRR_B  (UStaticMeshComponent) — rear-right blades
    │   └── FPVCamera (UCameraComponent)
    ├── MotorAudio (UAudioComponent)
    └── SpringArm (USpringArmComponent)
        └── ChaseCamera (UCameraComponent)
```

## Physics body

`UBoxComponent` extent: 35 × 40 × 11 cm. Collision profile: Pawn. Simulates physics; all forces and torques are applied to this component.

## Visual mesh

`UPoseableMeshComponent` — chosen over `USkeletalMeshComponent` because it exposes `GetBoneLocation` / `SetBoneTransformByName` for direct per-bone queries without triggering animation evaluation. The SK_Realistic_Drone skeleton is assigned here.

## Propeller blade architecture

The SK_Realistic_Drone skeleton has propeller bones (`LeftFrontPropeller1`, `RightFrontPropeller1`, `LeftBackPropeller1`, `RightBackPropeller1`) but those bones have no vertices weighted against them — they are transform-only markers.

Actual blade geometry is provided as separate static mesh assets:
- `SM_RealisticDronePropeller_Left` → PropellerMesh_CCW (FL + RR motors)
- `SM_RealisticDronePropeller_Right` → PropellerMesh_CW (FR + RL motors)

Blades are attached to `VisualMesh` **without a bone name** (root attachment). If attached with a bone name, UE inherits the cumulative scale of the parent bone chain, causing massively oversized meshes. Positions are set in `BeginPlay` by calling `GetBoneLocation(BoneName, EBoneSpaces::ComponentSpace)`.

Each motor has two blade components (A and B), offset 180° in yaw for the dual-blade appearance. Spin animation is purely visual — blade rotation does not affect physics.

## Motor spin visuals

`RotorCurrentSpinRate[4]` (deg/s per motor) interpolates toward `RotorThrottle[i] * MaxSpinRate` each frame using `FMath::FInterpTo` at speed `MotorSpoolRate`. This produces smooth spool-up/down and makes per-motor throttle differentials visible during yaw, pitch, and roll maneuvers.

SpinDir: FL and RR = +1 (CCW), FR and RL = -1 (CW). Blade B is always 180° ahead of blade A.

## Camera system

**Chase camera:** rigid spring arm (camera lag disabled), `TargetArmLength=180`, `SocketOffset=(0,0,20)`, `SetRelativeRotation(-15°,0,0)` to keep the drone vertically centered. Inherits yaw only; pitch and roll are locked so the arm stays level regardless of drone attitude. Building collision enabled.

**FPV camera:** attached to VisualMesh (moves with drone body). Positioned at `(18, 0, 9)` cm relative to VisualMesh, pitched -15°, FOV 90°. Toggle via `IA_SwitchCamera`.

## Audio

`MotorLoopSound` plays at start via `UAudioComponent`. Pitch multiplier mapped linearly from average rotor throttle: 0% throttle → 0.6×, 100% throttle → 1.8×.

## Hover throttle initialization

Computed in `InitHoverThrottle`: `HoverThrottle = (Mass * |GravityZ|) / (4 × MaxThrustPerRotor)`. If greater than 1.0, the drone cannot hover and a warning is logged. All four `RotorThrottle` values are seeded to `HoverThrottle` so the drone is stable from the first frame.
