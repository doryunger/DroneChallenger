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

## Default gains

| Parameter      | Value  | Notes                                          |
|----------------|--------|------------------------------------------------|
| AngleGain      | 10.0   | deg error → deg/s; higher = snappier leveling  |
| MaxTiltAngle   | 50°    | max commanded tilt; prevents unrecoverable flip|
| MaxYawRate     | 90°/s  | yaw rate at full stick deflection              |
| ThrottleRange  | 0.4    | stick ±1 maps to HoverThrottle ± 0.4          |
| PitchRate Kp   | 0.0008 |                                                |
| PitchRate Kd   | 0.00002|                                                |
| RollRate Kp    | 0.0008 |                                                |
| RollRate Kd    | 0.00002|                                                |
| YawRate Kp     | 0.001  |                                                |
