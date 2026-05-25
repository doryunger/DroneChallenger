# DroneChallenger — Development Rules

## Project overview

A small drone simulator built with **Unreal Engine 5.7.4** and the **Cesium for Unreal** plugin. The simulator places a controllable drone in a georeferenced world, with realistic flight physics and terrain streaming from Cesium ion via Google Photorealistic 3D Tiles.

## Playground definition

- **Anchor**: Marienplatz, Munich — 48.1374° N, 11.5755° E
- **Radius**: 2 km around the anchor
- **Data source**: Google Photorealistic 3D Tiles streamed via Cesium ion
- All development and testing is done within this boundary unless explicitly stated otherwise

## Workflow

Every step follows this sequence — no exceptions:

1. **Plan** — describe what we're building and why it comes next
2. **Implement** — write the code or configure the asset/Blueprint
3. **Verify** — run the editor (PIE or standalone), confirm behaviour before moving on

Never move to the next step until the current step is confirmed working in-engine.

## Step sizing

Steps are small and focused — one concept at a time. If a step feels large, split it.
Good step size: something that can be planned and verified in a single session.

## Architecture

- **C++ for logic** — flight physics, input handling, telemetry, and any system with non-trivial state live in C++ classes
- **Blueprints for wiring** — use Blueprints only to expose C++ properties to the editor or to connect components; keep logic out of Blueprints
- **Cesium for georeferencing** — all world placement uses `ACesiumGeoreference` and `UCesiumGlobeAnchorComponent`; never hard-code world-space offsets

## C++ standard and style

This project targets **C++20** via UE5's build system. Always use the most modern and safest syntax available within UE's constraints.

- Prefix UE classes with `A` (Actor) or `U` (UObject/Component) per Unreal convention
- Prefer `TUniquePtr` / `TSharedPtr` over raw owning pointers outside of UObject graphs
- Prefer `FString` for UE-facing strings; prefer `std::string_view` for pure C++ internals
- Use `UPROPERTY` and `UFUNCTION` macros only where engine reflection is genuinely needed
- Use `static_assert` for compile-time invariants
- Never use C-style casts — use `static_cast`, `Cast<>()`, or `CastChecked<>()`
- Never use `NULL` — use `nullptr`
- Never use `#define` for constants — use `constexpr` or `inline const`
- Avoid deprecated UE APIs — check the UE5 release notes and deprecation warnings

## Design decisions

Key rules for the simulation:

- Flight physics run on the server (or authority) and replicate state to clients — never compute authoritative physics client-side
- Drone input is processed through UE's Enhanced Input system; no legacy `BindAxis` / `BindAction`
- Cesium tile streaming budget and LOD settings are configured in C++, not left at editor defaults
- All geographic coordinates are stored as `FVector` latitude/longitude/altitude and converted at the georeferenced actor boundary — never mix coordinate spaces

## Quality standard

**Do what is right, not what is easy.**

A feature is not done when it compiles and PIE launches without crashing. It is done when:

- The drone behaves correctly at the boundaries of its flight envelope (hover, max speed, sudden stop)
- Cesium terrain streams correctly at the operating location without pop-in or stalls
- It integrates correctly with every system that depends on it
- It does not leave a known gap that would cause a crash or incorrect behaviour in a real session

Before marking any phase complete, explicitly ask: *what would break this in a real flight session?* If the answer is non-trivial, the phase is not complete.

## Comments and documentation

Source files contain no comments of any kind — no inline `//`, no block `/* */`, no docstrings.

If design context, sign conventions, or non-obvious constraints need to be recorded, they go in a Markdown file under `context/`. Each file in `context/` covers one system. The source files are the implementation; `context/` is the explanation.

When editing an existing file, remove any comments you encounter. When creating a new file, write none.

## What not to do

- Do not add features beyond what the current step requires
- Do not implement logic in Blueprints that belongs in C++
- Do not hard-code world coordinates — use Cesium georeferencing
- Do not declare a feature "done" when it only works in an empty level with no terrain streaming
- Do not build the drone controller and the physics model as if they are independent — they must be designed together
- Do not write comments in source files — use `context/` instead
