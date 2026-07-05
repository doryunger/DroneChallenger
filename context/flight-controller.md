# Flight Controller

## Architecture

Cascade (two-loop) angle-mode controller. Pitch and roll self-level; yaw is always rate mode.

```
Stick input → outer P loop (angle → desired rate) → inner PID loop (rate → motor output) → motor mixer
```

## FDroneControlInput sign conventions

| Field    | -1                        | 0     | +1                        |
|----------|---------------------------|-------|---------------------------|
| Throttle | full descent              | hover | full climb                |
| Pitch    | nose down / fly forward   | level | nose up / fly backward    |
| Roll     | roll left                 | level | roll right                |
| Yaw      | rotate left               | none  | rotate right              |

## AttitudeDeg packing

`FVector(ActorRot.Roll, ActorRot.Pitch, ActorRot.Yaw)` — X=roll, Y=pitch, Z=yaw in degrees.

UE5 FRotator sign conventions (positive = …):
- Pitch → nose up
- Roll → right side down (bank right)
- Yaw → right turn (CW from above)

## AngularVelocityBodyDeg

`GetPhysicsAngularVelocityInDegrees()` unrotated to body frame via `GetActorRotation().UnrotateVector(...)`.

Body-frame components: X = roll rate, Y = pitch rate, Z = yaw rate. Positive matches the FRotator conventions above (empirically confirmed: roll self-leveling works with positive X = rolling right).

**Yaw direction (needs in-game verification):** the motor mixer routes positive YawOut to CCW motors (FL, RR), which via `YawTorqueCoeff` adds positive `LocalUp` torque. If pressing the yaw-right key causes a left turn instead, negate `YawSpin` in `ApplyRotorForces`.

## PID derivative-on-measurement

`FDronePID::Update(Error, Measurement, DeltaTime)` computes the derivative as `(PrevMeasurement - Measurement) / dt` rather than `d(Error)/dt`. This avoids a derivative kick when the setpoint changes. The D term damps rate changes: as the measured rate increases toward the target, the negative derivative reduces the output, preventing overshoot.

## Motor mixer — X-frame layout (top view)

```
FL(0,CCW)   FR(1,CW)
    \           /
     [  body  ]
    /           \
RL(2,CW)    RR(3,CCW)
```

X = forward, Y = right.

| Motor     | Throttle | Pitch | Roll | Yaw |
|-----------|:--------:|:-----:|:----:|:---:|
| FL (0,CCW)| +        | +     | +    | +   |
| FR (1,CW) | +        | +     | -    | -   |
| RL (2,CW) | +        | -     | +    | -   |
| RR (3,CCW)| +        | -     | -    | +   |

- Pitch +1 → front rotors up, rear down → nose up
- Roll +1 → left rotors up, right down → bank right
- Yaw +1 → CCW rotors (FL, RR) up → yaw right (CW from above, pending verification)

## Yaw torque modeling

Rotor drag torque is applied as a separate `AddTorqueInRadians` call scaled by `YawTorqueCoeff` rather than being derived from simulated RPM. This decouples yaw authority from the thrust model, making it tunable independently. `YawSpin[4] = {+1, -1, -1, +1}` — index order matches motor indices 0–3 (FL, FR, RL, RR).

## Recovery near full inversion (±180° tilt)

`TiltPitchRad`/`TiltRollRad` (`Atan2(-ActorUp.X, ActorUp.Z)` / `Atan2(ActorUp.Y, ActorUp.Z)`) track true tilt angle exactly for a pure single-axis rotation, but `atan2` has a branch cut at ±180°: a tilt of -179° and its physical neighbor one degree further into the same rotation reads as +179°, not -180°+1°. The sign flips discontinuously exactly at full inversion.

`bPitchCorrects`/`bRollCorrects` gate full-rate recovery authority on `Input.Axis * TiltAxisRad < 0` (stick pushing to reduce tilt) once angular velocity has decayed below the 2°/s threshold that would otherwise satisfy the bypass on its own. Right at the ±180° pole this sign test can read backwards for whichever direction the stick is pushed — `LimitScale` is already pinned to 0 that far past `MaxTiltAngleDeg`, so the drone commands exactly zero rate and holds there regardless of input, indistinguishable from a stuck axis.

Physically, direction-of-correction is undefined exactly at the pole (same as "which way is east" at the North Pole) — any rotation away from maximum tilt is an improvement, so there's nothing to gate. `bPitchNearInverted`/`bRollNearInverted` (`|TiltAxisRad| > π - π/12`, i.e. within 15° of full inversion — reusing the same 15° band width `LimitScale` already uses) bypass the sign check entirely inside that band, restoring full authority regardless of which side of the branch cut the current tilt reads on. This is symmetric between pitch and roll — both axes share the identical bypass shape, so both were equally exposed; pitch just reaches the pole far more often in practice (diving nose-first) than roll does.

## Input capture: raw key polling instead of Enhanced Input events

`ControlInput.Roll` and `ControlInput.Throttle` are recomputed from `PlayerController::IsInputKeyDown` every `Tick` (D/A for Roll, W/S for Throttle) instead of being set by Enhanced Input `Triggered`/`Completed` callbacks. Enhanced Input's `Completed` event is not guaranteed to fire on every key-up transition (overlapping opposite-key presses, focus loss during key-up, etc.); when it doesn't, the axis's control value is never reset and stays latched at whatever the last `Triggered` call set it to — the drone gets stuck mid-maneuver on that axis with no way to recover, since nothing else ever writes to it. Polling raw key state fresh every tick is self-healing: it can't latch because it never depends on an event being delivered.

Yaw and Pitch still use the Enhanced Input `Triggered`/`Completed` pattern (`IA_Yaw`, `IA_PitchRoll`) and have not shown this symptom. If they ever do, apply the same fix: read the bound physical keys directly via `IsInputKeyDown` in `Tick` instead of trusting the `Completed` event.

## Input pipeline

```
Raw stick → expo curve → target angle → setpoint ramp → outer P → rate clamp → rate PID → mixer desaturation → motors
```

**Expo curve** — `sign(x) * |x|^InputExpo` applied to pitch and roll before scaling by `MaxTiltAngle`. At `InputExpo = 2.5`, half-stick deflection produces ~18% of max angle instead of 50%, making centre-stick precise without reducing full-deflection authority.

**Setpoint ramp** — `FInterpConstantTo` moves `PitchSetpoint` / `RollSetpoint` toward the expo-shaped target at `SetpointRate` deg/s. With keyboard binary input (0 or 1) this converts the instantaneous step into a ~175 ms ramp to max tilt, eliminating the step-induced rate spike.

**Motor desaturation** — after mixing, a collective shift is applied so all four motors stay in [0, 1] while preserving the attitude differential exactly. Hard per-motor clamping was removed because it silently destroyed pitch/roll authority at saturation.

**Tilt compensation** — `ThrottleCmd` is scaled by `1 / cos(tilt)` (capped at 2×) so the drone holds altitude automatically during banked flight.

## Default gains

| Parameter      | Value  | Notes                                               |
|----------------|--------|-----------------------------------------------------|
| AngleGain      | 8.0    | deg error → deg/s; higher = snappier levelling      |
| MaxTiltAngle   | 35°    | max commanded tilt                                  |
| MaxAngularRate | 180°/s | outer loop rate cap; prevents saturation flip       |
| MaxYawRate     | 90°/s  | yaw rate at full stick deflection                   |
| ThrottleRange  | 0.4    | stick ±1 maps to HoverThrottle ± 0.4               |
| InputExpo      | 2.5    | stick expo exponent; 1.0 = linear                   |
| SetpointRate   | 200°/s | max angle setpoint change rate; smooths key presses |
| PitchRate Kp   | 0.0008 |                                                     |
| PitchRate Kd   | 0.00002|                                                     |
| RollRate Kp    | 0.0008 |                                                     |
| RollRate Kd    | 0.00002|                                                     |
| YawRate Kp     | 0.001  |                                                     |
