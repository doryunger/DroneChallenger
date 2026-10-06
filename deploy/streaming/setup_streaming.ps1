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

$Original = "new Config({ useUrlParams: true })"
$Patched  = "new Config({ useUrlParams: true, initialSettings: { HoveringMouse: true } })"
$Source = Get-Content $PlayerTs -Raw
if ($Source.Contains($Patched)) {
    Write-Host "player.ts already defaults to hovering mouse."
} elseif ($Source.Contains($Original)) {
    Set-Content -Path $PlayerTs -Value $Source.Replace($Original, $Patched) -NoNewline -Encoding utf8
    Write-Host "player.ts patched: hovering mouse is the default control scheme."
} else {
    Fail "player.ts no longer contains '$Original'; the frontend changed upstream and the patch must be updated."
}

$SetupBat = Join-Path $InfraDir "SignallingWebServer\platform_scripts\cmd\setup.bat"
cmd.exe /c "`"$SetupBat`" --build < nul"
if ($LASTEXITCODE -ne 0) { Fail "setup.bat failed." }

Write-Host ""
Write-Host "Pixel Streaming infrastructure ready at $InfraDir" -ForegroundColor Green
