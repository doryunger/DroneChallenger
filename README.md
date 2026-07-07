# Drone Challenger

A first-person drone simulator built on Unreal Engine 5.7 and Cesium for Unreal, set in a georeferenced recreation of Munich. Pilot an FPV drone across city area and chase down a Behavior-Tree-driven patrol car, whose live AI state is visualized in real time right next to the gameplay.

![Drone Challenger demo](https://dcdemovid.s3.eu-central-1.amazonaws.com/dc_demo_compressed.gif)

[**▶ Play it live in your browser**](https://streams.vagon.io/streams/7d203400-fc2c-4b37-a084-cb42b4078521) — streamed via Vagon, no download or GPU required.

## Overview

The player flies a physically simulated FPV drone over real-world terrain streamed from Google Photorealistic 3D Tiles, anchored at Marienplatz, Munich (48.1374° N, 11.5755° E). A patrol car navigates the city's road graph on its own, reacting to the drone's presence: it goes dark and speeds up when spotted, and freezes once captured.

## Tech stack

| Layer | Technology |
|---|---|
| Engine | Unreal Engine 5.7 |
| Georeferencing | Cesium for Unreal + Cesium ion |
| Terrain data | Google Photorealistic 3D Tiles |
| Language | C++20 (no Blueprint logic) |
| Physics | Chaos physics — box-component root, per-rotor force application |
| Flight controller | Cascade angle-mode PID (outer angle loop → inner rate loop → X-frame motor mixer) |
| Input | Enhanced Input System |
| UI | UMG + Slate + WebBrowser plugin |
| Behavior trees | [Arborist](https://github.com/doryunger/arborist) |
| Road navigation | Custom graph loaded from `Content/Graph/nodes.csv` + `edges.csv` |


## Arborist integration

The patrol car's AI runs on [Arborist](https://github.com/doryunger/arborist), a standalone C++20 behavior tree framework with no UE5 coupling of its own. It's linked into Unreal as an `External`-type UBT module (`Source/ThirdParty/ArboristLib/`) — a single prebuilt static lib combining the BT framework, YAML schema parsing (`ryml`), and SQLite (decision-log persistence).

**The tree.** The car's behavior is described as data, not code — a YAML schema (`Content/BT/target.yaml`) defines a 4-priority selector, evaluated top-to-bottom every tick:

| Priority | Condition | Behavior |
|---|---|---|
| 1 | `is_captured` | Stop permanently |
| 2 | `drone_in_capture_range` (≤ 3 m) | Stop, flash beacon fast, accumulate capture timer |
| 3 | `drone_in_fov` | Go dark, accelerate, keep patrolling |
| 4 | *(default)* | Normal patrol speed, slow beacon pulse |

Conditions read from a `bt::Blackboard`, whose values are plain C++ lambdas capturing live actor state (`drone_distance`, `drone_in_fov`, `capture_timer`, etc.) — nothing is polled or copied by hand, the blackboard re-evaluates each source itself on every tick. Actions (`pulse_beacon_fast`, `deactivate_beacon`, ...) are C++ lambdas registered by name in a `bt::RuntimeRegistry`, matched against the action names in the YAML. `ATargetPawn::Tick` just calls `Tree->tick()` once a frame; everything else — which branch runs, what it does — is driven by the schema and the registry, not by hand-written control flow in the actor.

**Getting the state out.** A `bt::DecisionEmitter` records the active node path and a blackboard snapshot on every tick. `FDroneHUDServer` (`Source/DroneChallenger/DroneHUDServer.cpp`) wraps this in a tiny embedded HTTP server on port `8081`, exposing:
- `GET /tree` — the static schema (node names, types, hierarchy), fetched once
- `GET /history` — the live active-path + status samples, polled continuously

**Watching it live, in the sim.** A collapsible in-game panel ("BT DISPLAY" button) embeds a `WebBrowserWidget` pointing at a local `viewer.html` (`Content/HUD/arborist/viewer/`), which draws the tree with `vis-network` and colors each node by its current status:

- 🟢 **green** = SUCCESS (condition passed, subtree completed)
- 🔴 **red** = FAILURE (condition failed, branch short-circuited)
- 🟠 **orange** = RUNNING (actively executing leaf/subtree)

The viewer polls `/history` every 250 ms while the panel is open (fast enough to feel live) and drops to every 3 s while closed, so reopening it is never far out of date. The net effect: as the drone closes in on the car, you can watch the exact priority branch light up in real time — not just see the car react, but see *why*, node by node.

## How it was built

Development followed a strict plan → implement → verify loop per phase, roughly:

1. **Environment** — Cesium georeference anchored at Marienplatz, Google Photorealistic 3D Tiles streaming, terrain height probing.
2. **Flight** — a from-scratch cascade PID flight controller (angle outer loop → rate inner loop → X-frame motor mixer) driving Chaos physics via per-rotor `AddForceAtLocation`, wired to Enhanced Input.
3. **World + AI** — a Munich road graph (`tools/generate_graph.py`, baked to `Content/Graph/nodes.csv` / `edges.csv`) for the patrol car to navigate, then Arborist wired in as the car's decision-making layer.
4. **UI/UX** — UMG/Slate HUD (attitude indicator, minimap, tracking stats), the live BT viewer panel, and a hand-drawn pixel-font main menu with a captured in-engine preview behind it.
5. **Packaging polish** — loading-screen gating on scene-streaming readiness, mission/result flow, pause menu.


**Assets.** The drone mesh (RealisticDroneV2) and the patrol car mesh (a PS1-style hatchback) are both Unreal Marketplace/Fab asset packs, used as-is for visuals only — no gameplay logic depends on them beyond skeletal sockets for the poseable drone mesh.

## Demo mode

Launching with `-demo` on the command line pins the patrol car's starting position to the same predefined road node on every run, for reproducible demo recordings. Without the flag, the start node is chosen at random each session.

## Building this from source

Cloning this repo is **not enough on its own** to open or package the project — three things are intentionally excluded from version control (see `.gitignore`) and must be supplied separately:

| Missing piece | Why it's excluded | What you need to do |
|---|---|---|
| `Content/RealisticDroneV2/` | Marketplace asset pack, >400 MB | Own/download it via Fab and import into `Content/` |
| `Content/PS1_Style_Hatchback_Car/` | Marketplace asset pack | Same as above |
| `Source/ThirdParty/ArboristLib/{lib,include}/` | Prebuilt binaries, built from a separate repo | Build [Arborist](https://github.com/doryunger/arborist) yourself and copy its static lib + headers into place |

Everything else — code, the road graph, the BT schema, the HUD/viewer HTML/JS, Unreal project files — is version-controlled and builds as-is once those three are in place.
