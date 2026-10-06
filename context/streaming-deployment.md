# Streaming Deployment (Pixel Streaming 2)

## Components

- **Game**: the Shipping build packaged with the `PixelStreaming2` plugin enabled. Shipping is required for hosted sessions because it has no in-game console (`quit` would otherwise close the host process). The Esc menu hides QUIT when launched with `-PixelStreamingConnectionURL` (see `ui-widgets.md`).
- **Signalling and web server ("Wilbur")**: Epic's `PixelStreamingInfrastructure`, branch `UE5.8`, checked out next to this repo (`../PixelStreamingInfrastructure`). It serves the player page on port 80 and accepts the game's streamer connection on port 8888.
- **TURN relay (coturn)**: bundled with the infrastructure. Needed when the player's network blocks direct WebRTC peer connections. Started only with `start_streaming.ps1 -StartTurn`.

## Scripts (`deploy/streaming/`)

- `setup_streaming.ps1` clones the infrastructure if missing, patches the player page so `HoveringMouse` is the default control scheme, and runs Epic's `setup.bat --build`. Hovering mouse is required because the drone is flown from the keyboard while the cursor must stay visible for the Esc menu and the BT panel; the default locked-mouse scheme hides the cursor and the game's own cursor is not part of the video. The patch fails loudly if the upstream `player.ts` line it targets changes.
- `start_streaming.ps1` starts the signalling server through Epic's `start.bat`, waits for port 8888, then runs the game with `-PixelStreamingConnectionURL=ws://127.0.0.1:8888 -RenderOffScreen -ResX -ResY -ForceRes -Unattended` in a watchdog loop that relaunches it whenever it exits. On exit it stops the game, the server and coturn.

## TURN credentials

`start_streaming.ps1` never uses Epic's default demo credentials. On first run it generates `deploy/streaming/streaming.local.json` with `TurnUser` and a random `TurnPass`; the file is git-ignored. The signalling server's config log (`deploy/streaming/logs/signalling.log`) prints the resulting `peer_options`, including the credential, so the logs must be treated as sensitive on a shared host.

Locally the public IP is `127.0.0.1`. On EC2, pass the instance's public IP with `-PublicIp` and use `-StartTurn` so remote players behind restrictive NATs can connect.

## Ports

| Port | Protocol | Purpose |
|---|---|---|
| 80 | TCP | Player web page and player websocket |
| 8888 | TCP | Game (streamer) to signalling; local only, never expose publicly |
| 8889 | TCP | SFU connections; unused, local only |
| 19303 | TCP/UDP | TURN relay (when `-StartTurn`) |
| WebRTC media | UDP (ephemeral range) | Direct peer media between browser and game |

## Host setup (`install_host.ps1`) and known pitfalls

`install_host.ps1` runs once, elevated, on a fresh Windows Server 2022 GPU instance. It unblocks the kit files, opens the Windows Firewall ports, enables the hardware GPU for Remote Desktop sessions, imports the trusted root certificates from Windows Update, installs Media Foundation and the VC++ runtime, downloads and installs the NVIDIA GRID driver from the AWS driver bucket (requires an instance role with `AmazonS3ReadOnlyAccess`), fetches portable Git, and runs `setup_streaming.ps1`. A reboot is required afterwards.

Pitfalls found on the first EC2 deployment:

- **Unsigned executables from a downloaded zip.** Files extracted from an internet-downloaded zip carry the Mark of the Web. `Start-Process` (ShellExecute) on the unsigned game then shows "publisher could not be verified" and the game exits with code 1 every launch. Both scripts unblock the kit (`Unblock-File`).
- **Missing root certificates.** A fresh Windows Server only downloads root certificates on demand when SChannel needs them. The game's HTTP stack (OpenSSL via libcurl) never triggers that, so Cesium could not verify `tile.googleapis.com` and the loading screen never finished (tile cache stayed empty). The game now ships its own CA bundle at `Content/Certificates/cacert.pem` (copied from the engine, staged as non-UFS so it sits next to the game), which `FSslCertificateManager` loads before the platform store; the host script additionally imports the Windows Update root list.
- **Moving the infrastructure after setup.** npm workspaces in `PixelStreamingInfrastructure` store absolute paths. Renaming or moving the folder after `setup_streaming.ps1` breaks the signalling server with `MODULE_NOT_FOUND`. Re-run setup after any move.
- **NVIDIA installer and `Start-Process -Wait`.** In Windows PowerShell 5.1, `-Wait` also waits for every descendant process; the driver installer leaves a resident service, so the script hung. The scripts wait only on the installer process (`WaitForExit`).
