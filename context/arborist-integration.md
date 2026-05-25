# Arborist Integration

## Library overview

Arborist is a standalone C++20 static library (CMake). It has no UE5 coupling; it compiles and links as a pure C++ library that UBT treats as a ThirdParty module.

Source lives at `Source/ThirdParty/Arborist/` (git submodule: github.com/doryunger/arborist).

Third-party dependencies that Arborist requires:
- `yaml-cpp` — YAML schema parsing
- `SQLite3` — decision log persistence

Both must be vendored as static libs under `Source/ThirdParty/` and referenced in `Build.cs`.

## Key API classes

| Class | Header | Role |
|---|---|---|
| `bt::BehaviorTree` | `BehaviorTree.h` | Owns root node; call `tree.tick()` each frame |
| `bt::Blackboard` | `Blackboard.h` | Typed key-value store; sources are C++ lambdas polled each tick |
| `bt::RuntimeRegistry` | `RuntimeRegistry.h` | Maps action/condition names (strings from YAML) to C++ lambdas |
| `bt::SchemaLoader` | `SchemaLoader.h` | Parses YAML file into node tree at startup |
| `bt::DecisionEmitter` | `DecisionEmitter.h` | Records active path + blackboard snapshot each tick |
| `bt::MonitorServer` | `MonitorServer.h` | Embedded HTTP server (port 8080); streams live state to browser |

## Node types

| Type | YAML key | Behaviour |
|---|---|---|
| Sequence | `sequence` | Runs children left-to-right; fails on first FAILURE |
| Selector (Priority) | `priority` | Runs children left-to-right; succeeds on first SUCCESS |
| Parallel | `parallel` | Runs all children; configurable success/failure threshold |
| Action | `action` | Leaf; calls registered C++ lambda; returns its status |
| Condition | `condition` | Gate node; evaluates blackboard bool; prunes subtree on false |

Node statuses: `SUCCESS`, `FAILURE`, `RUNNING`.

## Blackboard

Sources are registered as lambdas before the first tick. The blackboard polls each source every tick so values are always fresh.

```cpp
bb.set<double>("drone_distance", [&]() { return ComputeDistance(); });
bb.set<bool>("drone_in_fov", [&]() { return CheckFOV(); });
```

Reading a value:
```cpp
double dist = bb.get<double>("drone_distance");
```

## RuntimeRegistry

Actions and conditions are registered by name. Names must match exactly what appears in the YAML schema.

```cpp
bt::RuntimeRegistry reg;
reg.registerAction("idle",                  [](bt::Blackboard&) { return bt::Status::SUCCESS; });
reg.registerAction("pulse_beacon_slow",     [this](bt::Blackboard&) { SetBeaconRate(SlowRate); return bt::Status::RUNNING; });
reg.registerAction("pulse_beacon_fast",     [this](bt::Blackboard&) { SetBeaconRate(FastRate); return bt::Status::RUNNING; });
reg.registerAction("deactivate_beacon",     [this](bt::Blackboard&) { BeaconLight->SetVisibility(false); return bt::Status::SUCCESS; });
reg.registerAction("increment_capture_timer",[this](bt::Blackboard& bb) {
    CaptureTimer += GetWorld()->GetDeltaSeconds();
    if (CaptureTimer >= CaptureHoldTime) { bCaptured = true; OnCaptured.Broadcast(this); }
    return bt::Status::RUNNING;
});
```

## SchemaLoader

Loads a YAML file into a node tree and binds it to a registry:

```cpp
bt::SchemaLoader loader;
auto root = loader.load("path/to/target.yaml", reg);
bt::BehaviorTree tree(std::move(root));
```

In UE5, the YAML path should resolve from the project's `Content/BT/` directory. Use `FPaths::ProjectContentDir()` to build the absolute path:

```cpp
FString YamlPath = FPaths::ProjectContentDir() / TEXT("BT/target.yaml");
auto Root = Loader.load(TCHAR_TO_UTF8(*YamlPath), Registry);
```

## MonitorServer

Owned by `ASearchDestroyGameMode` (one server, one attached tree at a time).

```cpp
bt::MonitorServer Monitor;
Monitor.start(8080);
Monitor.attachTree(&TargetTree);
```

Switch which target's tree is visible:
```cpp
Monitor.attachTree(&NewTarget->BehaviorTree);
```

Stop at session end:
```cpp
Monitor.stop();
```

The browser at `http://localhost:8080` shows the live active-node path colour-coded by status.

## UE5 per-target tick pattern

Each `ATargetActor::Tick`:

1. Update blackboard sources (already bound as lambdas — no explicit write needed if lambdas capture live state)
2. Call `Tree->tick()`
3. BT action lambdas call UE5 APIs directly on the actor; no polling needed

```cpp
void ATargetActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    if (Tree) Tree->tick();
}
```

## UBT build wiring (Build.cs)

Add Arborist sources and third-party include/lib paths to `DroneChallenger.Build.cs`:

```csharp
string ArboristRoot = Path.Combine(ModuleDirectory, "..", "ThirdParty", "Arborist");
PublicIncludePaths.Add(Path.Combine(ArboristRoot, "include"));

// Compile Arborist sources directly through UBT
string ArboristSrc = Path.Combine(ArboristRoot, "src");
PublicAdditionalLibraries.Add(/* yaml-cpp prebuilt lib path */);
PublicAdditionalLibraries.Add(/* sqlite3 prebuilt lib path */);
```

Alternative if Arborist ships a prebuilt static lib: reference the `.lib` directly.

## YAML schema (shared across all three targets)

`Content/BT/target.yaml`:

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

## Blackboard keys (per target)

| Key | Type | Updated by |
|---|---|---|
| `drone_distance` | double | Distance target → drone world position (cm) |
| `drone_in_fov` | bool | Camera cone + range check (same logic as detection) |
| `drone_in_capture_range` | bool | `drone_distance < CaptureRadius` |
| `capture_timer` | double | Seconds drone has been continuously in capture range |
| `is_captured` | bool | Latched true once capture completes; never reset |

## WebBrowserWidget (Phase 7e)

Add `WebBrowser` to the UE5 plugin list in `DroneChallenger.uproject`. Add `"WebBrowser"` to `PublicDependencyModuleNames` in `Build.cs`.

In UMG, create a `UWebBrowserWidget` and call `LoadURL("http://localhost:8080")` after the MonitorServer has started.

The panel is collapsible via a UMG `SExpandableArea` or a simple visibility toggle on a named widget slot.
