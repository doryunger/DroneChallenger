# UI Widgets

## Pixel font

The loading, main menu, options and result widgets draw text with a 5×7 pixel font painted as individual Slate boxes in `NativePaint`. Each glyph is seven `uint8` rows; within a row, bit 4 is the leftmost pixel and bit 0 the rightmost (`Rows[row] & (1u << (4 - col))`). Glyphs advance 6 pixels (5 wide + 1 gap).

## Gameplay input mode

During flight the player controller is in `FInputModeGameAndUI` with `EMouseLockMode::DoNotLock` and the cursor shown. This is required so on-screen UMG controls (the BT display toggle button, the minimap) receive mouse clicks while the drone is flown from the keyboard, both locally and through Pixel Streaming in hovering-mouse mode.

`ADroneActor::PossessedBy` sets this mode at the start of a flight. `UDroneOptionsWidget` switches to `FInputModeUIOnly` while the Esc menu is open and must restore the same gameplay mode when it closes; restoring `FInputModeGameOnly` with a hidden cursor leaves every on-screen button unclickable for the rest of the session.

## Hosted (streamed) sessions

When the game is launched with `-PixelStreamingConnectionURL=` (or the legacy `-PixelStreamingURL=`), it is running on a host machine and streamed to a remote player. In that case `UDroneOptionsWidget` hides the QUIT entry, re-centres the remaining two entries, and ignores clicks and hover in the former QUIT area, so a remote player cannot close the game process on the host. The same Shipping build launched without that argument keeps QUIT for local play. This is decided once in `NativeOnInitialized` from the command line; there is no separate build configuration.

Development builds still expose the in-game console, where `quit` would close the host process. Hosted deployments must run the Shipping build, which has no console.
