# Fullscreen Mode and the Minimap

## Actual bug: DPI scale, not CEF

`ADroneHUD::DrawHUD()` positions `MiniMapBrowser` by writing pixel values (from `Canvas->SizeX/SizeY`) directly into a `FGameViewportWidgetSlot::Offsets`. Those offsets are **not** raw device pixels — `UGameViewportSubsystem::SetWidgetSlotPosition`'s own doc comment confirms it: the position is in DPI-scaled logical units, and the engine-provided helper divides by `UWidgetLayoutLibrary::GetViewportScale()` before writing it. `DrawHUD` never did that division.

At DPI scale 1.0 (common for a small windowed test window) this is invisible — pixels and logical units are the same number. The moment the effective resolution changes enough to move up UE's DPI scale curve (e.g. F11 jumping to native monitor resolution), `Scale > 1`, and the un-divided pixel values become too large for the logical coordinate space they're placed in — pushing the widget's Y position past the bottom of the visible area. `DronePFDWidget` never hit this because it positions itself entirely inside its own `NativePaint` using `AllottedGeometry.GetLocalSize()`, which is already in the correct (post-DPI-scale) local space — it never touches `Canvas` pixels or `GameViewportSubsystem` slots directly.

Fix: `DrawHUD` now divides `VP` by `UWidgetLayoutLibrary::GetViewportScale(this)` before computing `mmMargin`/`mmSize`/the offset margin, so the minimap's position and size are computed in the same logical space the slot actually consumes, matching how `SetWidgetSlotPosition` does it internally.

## Unrelated, but worth keeping: exclusive fullscreen and CEF

Separately, `Config/DefaultGameUserSettings.ini` forces `FullscreenMode=1` (`EWindowMode::WindowedFullscreen`) since this project had no explicit fullscreen-mode override. This isn't the cause of the reported bug, but it avoids a real, unrelated failure mode: CEF's off-screen-rendering shared texture (used by every `UWebBrowser` widget — `MiniMapWidget`, `UBTDisplayWidget`) does not survive the swapchain change caused by **exclusive** fullscreen and goes blank. Worth keeping as defense-in-depth even though the DPI fix above is what actually resolves what was reported.
