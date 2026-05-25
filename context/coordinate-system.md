# Coordinate System and Conventions

## UE5 world frame

X = forward, Y = right, Z = up. Right-handed: X × Y = Z.

All forces, torques, and offsets in this project use this frame unless stated otherwise.

## Rotor arm offsets (body frame, cm)

| Motor | Index | X         | Y         |
|-------|-------|-----------|-----------|
| FL    | 0     | +ArmLength| -ArmLength|
| FR    | 1     | +ArmLength| +ArmLength|
| RL    | 2     | -ArmLength| -ArmLength|
| RR    | 3     | -ArmLength| +ArmLength|

## Body-frame angular velocity

`GetPhysicsAngularVelocityInDegrees()` returns world-space angular velocity. `GetActorRotation().UnrotateVector(...)` converts it to the drone's body frame:

- X = roll rate (positive = rolling right)
- Y = pitch rate (positive = nose pitching up)
- Z = yaw rate (positive = turning right, pending yaw direction verification)

## FRotator sign conventions

| Axis  | Positive direction  |
|-------|---------------------|
| Pitch | nose up             |
| Roll  | right side down     |
| Yaw   | right turn (CW from above) |

`AttitudeDeg` in the flight controller is packed as `FVector(Roll, Pitch, Yaw)` from `GetActorRotation()`.

## Cesium / geographic coordinates

`FVector` used for geographic points: X = Longitude, Y = Latitude, Z = height above ellipsoid (meters).

`ACesiumGeoreference` and `UCesiumGlobeAnchorComponent` handle all world-to-geographic conversions. Never hard-code UE5 world-space offsets for geographic placement; always use the georeference actor.

Cesium `SampleHeightMostDetailed` returns height above the WGS84 ellipsoid, not above mean sea level. For altitude-above-terrain, subtract the sampled terrain height from the drone's current ellipsoidal height.

## Munich anchor

Marienplatz: 48.1374° N, 11.5755° E. All testing and positioning is done within 2 km of this point.
