param(
    [string]$InfraDir = (Join-Path $PSScriptRoot "..\..\..\PixelStreamingInfrastructure"),
    [string]$Branch = "UE5.8"
)

$ErrorActionPreference = "Stop"

function Fail([string]$Message) {
    Write-Host "ERROR: $Message" -ForegroundColor Red
    exit 1
}

if (-not (Test-Path (Join-Path $InfraDir "SignallingWebServer"))) {
    git clone --depth 1 --branch $Branch https://github.com/EpicGamesExt/PixelStreamingInfrastructure.git $InfraDir
    if ($LASTEXITCODE -ne 0) { Fail "Cloning PixelStreamingInfrastructure failed." }
}
$InfraDir = (Resolve-Path $InfraDir).Path

$PlayerTs = Join-Path $InfraDir "Frontend\implementations\typescript\src\player.ts"
if (-not (Test-Path $PlayerTs)) { Fail "player.ts not found at $PlayerTs" }

$Patched = "new Config({ useUrlParams: true, initialSettings: { HoveringMouse: true, AutoConnect: true, AutoPlayVideo: true, StartVideoMuted: true } })"
$KnownSources = @(
    "new Config({ useUrlParams: true })",
    "new Config({ useUrlParams: true, initialSettings: { HoveringMouse: true } })"
)
$Source = Get-Content $PlayerTs -Raw
if ($Source.Contains($Patched)) {
    Write-Host "player.ts already has the hosted defaults."
} else {
    $Match = $KnownSources | Where-Object { $Source.Contains($_) } | Select-Object -First 1
    if (-not $Match) { Fail "player.ts has no known Config line; the frontend changed upstream and the patch must be updated." }
    Set-Content -Path $PlayerTs -Value $Source.Replace($Match, $Patched) -NoNewline -Encoding utf8
    Write-Host "player.ts patched: hovering mouse, auto-connect, muted autoplay."
}

$SetupBat = Join-Path $InfraDir "SignallingWebServer\platform_scripts\cmd\setup.bat"
cmd.exe /c "`"$SetupBat`" --build < nul"
if ($LASTEXITCODE -ne 0) { Fail "setup.bat failed." }

Write-Host ""
Write-Host "Pixel Streaming infrastructure ready at $InfraDir" -ForegroundColor Green
