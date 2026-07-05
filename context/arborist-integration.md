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

## One loading signal, one loading indicator (`UBTDisplayWidget`)

Only the UE5-side wait is covered by an indicator: the browser widget itself hasn't loaded `viewer.html` yet. That's covered by the UE5-side spinner (`CachedSpinner`, the Blueprint's "LoadingSpinner", plus a hand-drawn backup dot spinner in `NativePaint`) — hidden as soon as `viewer.js` signals `console.log('VIEWER_PAGE_LOADED')` (the very first line executed once the script runs), with `MaxPanelOpenWait` (15s) as an absolute fallback for the rare case the panel is opened before that signal has arrived at all (e.g. clicking within the very first moment of level start).

There used to be an additional flat `MinPanelOpenDelay` (5s) floor here — reveal only after *both* `bPageLoaded` and 5 real seconds since the panel opened. Removed: since `bURLLoaded`/preload happens at level `BeginPlay`, `bPageLoaded` is essentially always already `true` long before the user ever opens the panel (confirmed — this is what made the artificial floor visible as "it always takes exactly 5-6s no matter how long I wait before opening," since that flat wait, not real load time, was what the user was actually seeing every single time). The floor was originally added to guard against an instant/invisible spinner transition back when content readiness was unpredictable; now that the page has effectively unlimited time to load in the background before the panel is ever touched, that protection was pure dead time with no benefit. `TogglePanel()` now reveals immediately if `bPageLoaded` is already true, and only falls back to the `OnPanelOpenTimer` poll loop (capped by `MaxPanelOpenWait`) for the case where it isn't yet.

There is deliberately no second, in-page loading indicator for the graph fetch/draw itself. One was attempted and removed — see "The in-page placeholder was abandoned" below for why.

## Poll rate follows panel open/closed state

`viewer.js` polls `/history` on a `setInterval`, but the interval now varies: 250ms while the BT panel is open (fast enough to feel live while someone is actually watching it), 3000ms while closed (still fresh if reopened, far less background HTTP/JS churn than polling 4x/sec indefinitely for a panel nobody's looking at). `pollIntervalMs`/`pollTimer`/`restartPollTimer()`/`window.setPollRate(ms)` in `viewer.js` own this entirely; `TogglePanel()`'s existing open and close branches each fire one `ExecuteJavascript("window.setPollRate(...)")` call — 250 on open, 3000 on close.

This is the same cross-process signal mechanism (`ExecuteJavascript`) that caused so much trouble for the placeholder, but it's safe to use here for a reason worth remembering: a poll-rate change has no correctness dependency on exact timing. If the message lands a few hundred ms late, or in some edge case doesn't land at all, the only consequence is the tree data is briefly staler than intended — nothing visibly breaks, nothing gets stuck waiting on it, there's no race between two things that both have to happen in the right order for the screen to look right. That's fundamentally different from the placeholder case, where `ExecuteJavascript`'s fire-and-forget nature directly caused wrong visual output. The lesson isn't "never use `ExecuteJavascript` for cross-process signaling" — it's "don't use it for anything where getting the timing wrong produces a visibly wrong result."

`HandleConsoleMessage` now only listens for `VIEWER_PAGE_LOADED` (drives the spinner-hide above). `viewer.js` still logs `BT_VIEWER_READY` when the graph is first drawn, but nothing on the C++ side reacts to it anymore — see below for why.

`CachedBrowser` also gets `bSupportsTransparency = true` set via reflection in `Initialize()`, not `NativeConstruct()` — `UUserWidget::RebuildWidget()` calls `WidgetTree->RootWidget->TakeWidget()`, which constructs every child's actual Slate widget (including `UWebBrowser::RebuildWidget()`, which reads `bSupportsTransparency` at that exact moment) *before* `OnWidgetRebuilt()` fires `NativeConstruct()`. Setting it in `NativeConstruct()`, as an earlier version did, updates the UObject's stored value after the real native browser was already built with the old default — too late to matter. This is now vestigial (the design doesn't rely on transparency) but left in place since it's correct and harmless.

An earlier attempt tried to let the page's own transparency-then-opaque background do the covering (browser always visible, spinner always drawn underneath, page covers it once rendered) instead of explicit show/hide. Abandoned after two problems: (1) `WebBrowser_47` sits inside a `Vertical Box`, sibling to `LoadingSpinner`, under what's almost certainly an `Overlay` — not a `Canvas Panel` — so `Cast<UCanvasPanelSlot>(...)->SetZOrder(...)` silently no-ops; Overlay stacking is pure child order, not a settable property. (2) Even with the browser drawn on top, the spinner never showed through — CEF's rendered output isn't genuinely alpha-transparent during load regardless of `bSupportsTransparency`. A stray `UFUNCTION(BlueprintCallable) NotifyContentReady()` was also found and removed during this (no C++ caller — only existed to be wired from a Blueprint event graph, likely bound to `WebBrowser_47`'s `OnLoadCompleted`, and the probable cause of the spinner disappearing too early). Its replacement, `VIEWER_PAGE_LOADED`, is an explicit console-message signal instead.

## The in-page placeholder was abandoned

A second loading indicator was attempted, inside `viewer.html` itself: a `#graph-loading` overlay div meant to cover the `/tree` fetch + `vis-network` draw, hidden only after `network.once('afterDrawing', ...)` fired *and* a fixed delay elapsed. It went through several redesigns — deferring `loadTree()` until a `window.onPanelOpened()` ping from C++, then adding a real time buffer between that ping and revealing the browser widget to survive the CEF IPC round-trip — but was dropped entirely rather than debugged further, for two reasons:

1. **It never worked reliably**, likely because of a genuine C++↔CEF timing/IPC issue (`ExecuteJavascript` is fire-and-forget across a browser-process→renderer-process boundary, while Slate `SetVisibility` takes effect same-process, same-frame — no ordering of the two calls actually makes one wait for the other) that couldn't be confirmed or fixed without runtime access to a Vagon session (no `UE_LOG` visibility in a cloud-streamed session, `AddOnScreenDebugMessage` compiled out in Shipping).
2. **It broke something that already worked, by accident.** The BT toggle button's text and its hand-drawn background/highlight-stripe (in `NativePaint`) were always two separate things: `CachedBtn->SetVisibility(Visible)` (in `OnAfterConstruct`) shows the button and its child text label unconditionally and early, but the button's own native Slate style is deliberately gutted to `NoDrawType`/fully transparent (in `NativeConstruct`) — by itself it has *no* visible background at all. The only thing that ever painted one was the hand-drawn box+stripe block in `NativePaint`, which used to be gated behind `bContentReady` (set by a now-removed `OnGraphReady()`, itself triggered by the `BT_VIEWER_READY` console message). Before the placeholder work, that message fired unconditionally and immediately once the graph first drew on page load, so the gate was invisible in practice — the background always appeared essentially as fast as the button did. Coupling `BT_VIEWER_READY` to the placeholder's "only after the panel is opened and the whole cross-process ping chain succeeds" logic meant the background silently inherited every fragility of that chain, while the text (gated on nothing) kept appearing fine — which is why the button was visible with text but no background.

Given the gate never served a clear purpose beyond a "content is ready" cosmetic flourish, and cost real reliability for that flourish, it was removed entirely rather than re-wired: `bContentReady`/`OnGraphReady()` are gone, `StripePhase` animates unconditionally, and the `NativePaint` background block draws whenever `CachedBtn` is valid — no readiness gate at all. The button, its text, and its background now all appear together, at the same moment, for the same reason.

Current state: `viewer.js` calls `loadTree()` unconditionally at script load, and `afterDrawing` logs `BT_VIEWER_READY` purely as a diagnostic (nothing in C++ reacts to it anymore) with no delay. A minimal, C++-free spinner was reinstated afterward for the fetch/draw window itself — see "A simple spinner, without the cross-process design" below.

## A simple spinner, without the cross-process design

A `#graph-loading` overlay (`viewer.html`) — a spinner, no text, no fancy styling — covers `#graph` from page load. `loadTree()`'s `network.once('afterDrawing', ...)` callback sets `loadingEl.style.display = 'none'` the moment it fires, immediately, with no delay. That's the entire mechanism: no `panelOpen`/`graphDrawn`/`started` state, no `window.onPanelOpened`/`onPanelClosed`, no `ExecuteJavascript` call from C++ at all.

This looks superficially like the abandoned placeholder (same `#graph-loading` div, same `afterDrawing` hook) but is a fundamentally different design, for one reason: `loadTree()` still runs unconditionally at page load, not deferred until the panel opens. The earlier placeholder failed because it tried to *re-show* a spinner over content that might have already finished rendering during preload, racing a cross-process signal to win that re-cover before the finished frame could flash through. This version never re-covers anything — it shows once, at the one moment there is guaranteed to be nothing drawn yet (immediately after the script starts), and hides once, the first time real content exists. There's nothing to race and nothing for C++ to signal, so most users will simply never see it at all (the graph typically draws well before the panel is ever opened) — it only becomes visible in the genuine edge case of opening the panel within the first moment of level start, which is exactly the "don't show a blank page" case it exists for.

## CEF has a persistent on-disk cache under `Saved/` that survives every relaunch

A temporary `TEMP_DEBUG_GRAPH_DRAW_DELAY_MS` (artificial delay before `loadTree()` builds the graph, added purely to force the spinner to be visible for testing) produced no visible change at all — no delay, no spinner — despite the source file being verified correct and current. That "a change that should be trivially, unmissably visible produced literally zero difference" is a strong signal the browser isn't re-reading the file at all.

Found the cause: `Saved/webcache_6613/Default/Code Cache` (and an older `webcache_6613_1`, dated back to June) is CEF's persistent disk cache — including a V8 "Code Cache" of compiled JS bytecode — and it lives under `Saved/`, not a per-process temp directory, so it survives across every PIE session and every relaunch of a packaged build. `LoadURL` was always called with the exact same `file:///.../viewer.html` URL every time, so Chromium's cache had every opportunity to treat repeated loads as the same resource and serve a stale compiled copy instead of re-reading the file — this plausibly explains a good deal of the earlier confusing back-and-forth in this whole BT viewer saga, not just the delay/spinner test, since it means the *running* script could have silently been out of sync with the source file for an unknown number of edits.

Fixed by cache-busting: both `LoadURL` call sites (`NativeConstruct`'s initial preload and `ReloadBrowser()`) now append `?cb=<FDateTime::Now().GetTicks()>` to the URL. Since Chromium's cache key includes the full URL (query string included), a value that's different on every single load guarantees a cache miss and a genuine re-read from disk every time — no manual cache-clearing required, and it can't regress silently in the future the way an unbusted URL could. One minor side effect worth knowing: because every load now looks like a "new" URL to Chromium, the disk cache will accumulate a distinct entry per load rather than reusing/evicting one entry — harmless for correctness, just something that grows `Saved/webcache_6613` over many test iterations; deleting that folder is always safe if it's ever worth reclaiming the space.

**A wrong theory, recorded so it doesn't get re-tried**: at one point the `Cast<UCanvasPanelSlot>(CachedBtn->Slot)` in `NativePaint` was blamed as dead code (by analogy with the `WebBrowser_47` slot mistake above) and replaced with `GetCachedGeometry()`-based positioning. That was backwards. The slot cast was always fine — `Button_0` *is* under a Canvas Panel, independently proven by `NativeConstruct`'s `SetAutoSize(true)` working through the identical cast, and by the user confirming the stripes had always rendered before this session's changes. The one packaged build that used the `GetCachedGeometry()` variant is the only build where the (ungated) drawing genuinely didn't render. So: slot-based positioning is the proven-working approach for this button; the cached-geometry variant is the one with an unexplained failure in packaged builds. (The cached-geometry technique is still used for the *panel* spinner backdrop, where there is no slot alternative and a full-area fallback exists.)

## Button background and delayed reveal

The button's visible background is the original hand-drawn block in `NativePaint`: dark box + animated diagonal highlight stripes + hover overlay, positioned from the button's `UCanvasPanelSlot` (anchors/position/alignment/desired-size). The button's own Slate style is deliberately gutted to `NoDrawType`/transparent in `NativeConstruct` — the hand-drawn block *is* the background, and it draws underneath the button (before `Super::NativePaint`), which only shows through because the button itself is transparent. A brief detour replaced this with real `FSlateColorBrush` style brushes (opaque style background, tint-pulse animation) on the mistaken theory that the hand-drawn path couldn't render in packaged builds; reverted once the history was established — the hand-drawn stripes had always worked, and the actual regression was the trigger gating (see above). Note the two halves are coupled: an opaque style background would cover the under-drawn stripes, so restoring the stripes required restoring the `NoDrawType` gutting too.

The draw is gated only on `CachedBtn->GetVisibility() == ESlateVisibility::Visible` — no readiness flag — so the background/stripes appear and disappear exactly with the button. `StripePhase` advances unconditionally in `NativeTick`.

The button is not shown at widget construction. `OnAfterConstruct` hides it along with everything else; `ADroneHUD::TryDismissLoading()` — the single moment the loading screen actually comes down — calls `UBTDisplayWidget::NotifyLoadingDismissed()`, which starts a one-shot `ButtonRevealDelay` (5s, initially 10s but shortened after testing) timer before `RevealToggleButton()` makes the button visible. So the sequence is: loading screen dismissed → 5s of clean gameplay view → BT button appears with its background and stripes as one unit (the visibility gate on the draw guarantees they can't be split). This replaces the old behavior where the button was technically visible from level start but hidden *under* the loading screen, making it pop in the instant the loading screen dismissed.
