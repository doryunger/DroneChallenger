# Main Menu Car/Drone Preview Capture

## Architecture

`BP_CarDisplay` is a single placed actor in `MainMenu.umap` holding the car mesh, the drone skeletal mesh, and two `USceneCaptureComponent2D`s — one per model. Each capture renders into its own render target (`RT_CarPreview`, `RT_DronePreview`), sampled by a material (`M_CarPreview`, `M_DronePreview`) that `UDroneMainMenuWidget` paints as a `FSlateBrush` in `DrawTitleScreen`. This used to be two separate Blueprints; they were merged into one actor because running two continuously-capturing actors was too expensive on CPU.

`ADroneMainMenuHUD` owns the capture lifecycle in C++ (`SetupCarDisplay`, `RevealCarDisplay`) — the Blueprint only wires components, it does not drive the capture timing.

## Confirmed on Vagon: captures must stay continuous, never freeze

An earlier version froze each capture (`bCaptureEveryFrame = false` + a single manual `CaptureScene()`) once the scene looked ready, to avoid the CPU cost of two permanently-rendering scene captures — the same CPU concern that caused the two-Blueprint-to-one merge in the first place. **This does not work on Vagon** — confirmed empirically: leaving both captures at `bCaptureEveryFrame = true` forever displays correctly; freezing them (even after waiting for shaders/textures to be ready) results in nothing rendering. Most likely mechanism: once a capture stops rendering every frame, nothing keeps "touching" its source meshes' textures every frame, and on a more memory-constrained/virtualized GPU those resources can get evicted from residency — the frozen render target then has nothing valid backing it. This never reproduced locally because a local dev GPU has enough headroom that eviction never happens.

See "Periodic re-capture" below for what `SetupCarDisplay` actually does now (it does not simply set `bCaptureEveryFrame = true` and leave it — that was an intermediate step, superseded once the flicker-vs-blank tradeoff below was worked out).

## Kept deliberately simple

A more elaborate version of this also gated the widget reveal behind polling `GShaderCompilingManager->IsCompiling()` and per-texture `IsFullyStreamedIn()` checks, plus an on-screen Slate-drawn diagnostic readout. That version caused a crash in a packaged Shipping build — `GShaderCompilingManager` (`RenderCore`'s `ShaderCompiler.h`) is an editor/cook-time-oriented API, not something a Runtime game module should be calling from a shipped executable, and its behavior there is undefined. It was removed along with the `RenderCore`/`RHI` module dependencies it required.

## Periodic re-capture instead of continuous or one-shot

Leaving both captures at `bCaptureEveryFrame = true` forever (the Vagon-safe fix above) is visually live — every frame re-renders the scene, which is visible as flicker on a menu screen that's supposed to look like a static product shot. A hard freeze (`bCaptureEveryFrame = false` + a single `CaptureScene()`) removes the flicker but is the exact thing that goes blank on Vagon.

The compromise: `bCaptureEveryFrame` stays `false` (no per-frame cost, no flicker), but `RecaptureCarDisplay` calls `CaptureScene()` manually on a **repeating** timer (`RecaptureHandle`, `RecaptureInterval`), forever — not once. This keeps the source textures "touched" regularly, which should prevent whatever residency eviction caused the hard-freeze failure on Vagon, while looking static frame-to-frame locally since nothing changes in between recaptures. Whether this fully solves the Vagon blank-render issue is still unconfirmed as of the last test round (which also surfaced unrelated regressions — drone spawn-in-building, missing BT button — so a "still broken" report doesn't necessarily mean this specific fix failed). If it turns out the eviction theory is wrong, continuous capture (accepting the flicker) remains the fallback that's actually proven to work.

## The menu is rendered from the start, hidden behind a real black overlay — not delayed by omission

`ADroneMainMenuHUD::BeginPlay` now calls `MenuWidget->AddToViewport(0)`, sets input mode, and sets keyboard focus immediately — the widget is genuinely rendering (and, importantly, `DrawTitleScreen` is running every frame, so `bCarBrushReady`/`bDroneBrushReady` resolve and the widget is fully warmed up) from the first frame. What actually blocks the player from seeing it is a separate opaque `SColorBlock` (`BlackoutOverlay`) added directly to the game viewport via `UGameViewportClient::AddViewportWidgetContent` at a Z-order (1000) above the menu. `RevealCarDisplay` removes that overlay via `RemoveViewportWidgetContent` once ready — the menu itself never gets added or re-added, it was there the whole time.

This replaces an earlier design where `AddToViewport`/input mode/focus were themselves deferred until `RevealCarDisplay` — which meant every extra second of delay was genuinely wasted (nothing was rendering, nothing was warming up, the widget didn't exist yet) rather than being used productively. It also replaces an even earlier, briefly-tried version that showed the menu immediately and gated only the car/drone *images* (`bCarBrushReady && bPreviewCaptured` / `bDroneBrushReady && bPreviewCaptured` in `DrawTitleScreen`) with no cover at all — that was reverted for showing a menu-without-car/drone "pop-in" moment, which is exactly what a real opaque cover avoids while still letting the rest of the delay do useful work.

`RecaptureCarDisplay` and the reveal are now fully decoupled — recapturing is no longer what triggers the reveal, it's purely a periodic "keep the render target fresh" tick (`RecaptureInterval`, lowered to 2s now that it has no bearing on when anything becomes visible), called once immediately in `SetupCarDisplay` and then repeating. The reveal itself is a separate one-shot `RevealDelayHandle` timer (`RevealDelay`, 8s), started alongside it. Splitting them means the covered period can be extended for a genuine safety margin (multiple recapture cycles happen behind the cover, each one a real chance for the render target to have valid pixels) instead of the old design where the *only* recapture that mattered was the one that also triggered the reveal on the same tick.

A `FallbackRevealHandle` (`MaxRevealDelay`, 15s), started unconditionally in `BeginPlay`, guards a latent bug in the old design: if `SetupCarDisplay` ever failed to find any `USceneCaptureComponent2D` (early `return` before the recapture/reveal timers were ever set), nothing would ever call `RevealCarDisplay` and the black overlay would stay up forever. `RevealCarDisplay` is idempotent (`bRevealed` guard, clears both timers), so whichever of `RevealDelayHandle`/`FallbackRevealHandle` fires first wins and the other is a safe no-op.

## Why the widget waits for a signal instead of just loading the material

`UDroneMainMenuWidget::NativeConstruct` loads `RT_CarPreview` / `RT_DronePreview` only to confirm they exist — it never calls `UpdateResource()` on them, because that recreates the GPU texture and would wipe out whatever `BP_CarDisplay`'s scene captures have already rendered into it.

Loading the *material* (`M_CarPreview`/`M_DronePreview`) succeeds almost immediately, long before the scene capture actually has valid pixels in the render target. `DrawTitleScreen` therefore checks `bCarBrushReady && bPreviewCaptured` (and the drone equivalent) rather than the brush-loaded flag alone — `bPreviewCaptured` only becomes true once `ADroneMainMenuHUD::RevealCarDisplay` has actually run, so the title screen never paints a guaranteed-blank render target during the fixed wait.

## GPU tier dependency (Vagon)

`plan.md`'s Phase 10 notes flag that this project enables `r.RayTracing=True` and `r.Lumen.HardwareRayTracing=True`, which require DXR hardware support, and that a non-RTX Vagon tier (e.g. Tesla T4 / OptiX-CUDA) "causes rendering failures including broken SceneCapture output." The continuous-capture fix above resolved the reported failure, so this wasn't the root cause this time — but it's worth checking first if a future Vagon regression looks like a rendering failure rather than a timing one.
