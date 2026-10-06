param(
    [string]$GameExe = (Join-Path $PSScriptRoot "..\..\Packaged\StreamShipping\Windows\DroneChallenger.exe"),
    [string]$InfraDir = (Join-Path $PSScriptRoot "..\..\..\PixelStreamingInfrastructure"),
    [string]$ConfigFile = (Join-Path $PSScriptRoot "streaming.local.json"),
    [string]$LogDir = (Join-Path $PSScriptRoot "logs"),
    [string]$PublicIp = "",
    [switch]$StartTurn,
    [int]$ResX = 1920,
    [int]$ResY = 1080,
    [int]$StreamerPort = 8888,
    [int]$PlayerPort = 80
)

$ErrorActionPreference = "Stop"

function Fail([string]$Message) {
    Write-Host "ERROR: $Message" -ForegroundColor Red
    exit 1
}

function Write-Log([string]$Message) {
    $Line = "[{0:yyyy-MM-dd HH:mm:ss}] {1}" -f (Get-Date), $Message
    Write-Host $Line
    Add-Content -Path (Join-Path $LogDir "watchdog.log") -Value $Line
}

function New-Secret {
    $Bytes = New-Object byte[] 24
    [System.Security.Cryptography.RandomNumberGenerator]::Create().GetBytes($Bytes)
    return ([Convert]::ToBase64String($Bytes) -replace '[^A-Za-z0-9]', '')
}

function Test-PortListening([int]$Port) {
    return [bool](Get-NetTCPConnection -State Listen -LocalPort $Port -ErrorAction SilentlyContinue)
}

function Stop-PortOwners([int[]]$Ports) {
    foreach ($Port in $Ports) {
        Get-NetTCPConnection -State Listen -LocalPort $Port -ErrorAction SilentlyContinue |
            ForEach-Object { Stop-Process -Id $_.OwningProcess -Force -ErrorAction SilentlyContinue }
    }
}

if (-not (Test-Path $GameExe)) { Fail "Game executable not found: $GameExe" }
$GameExe = (Resolve-Path $GameExe).Path
Get-ChildItem -Path (Split-Path $GameExe -Parent) -Recurse -File | Unblock-File
$StartBat = Join-Path $InfraDir "SignallingWebServer\platform_scripts\cmd\start.bat"
if (-not (Test-Path $StartBat)) { Fail "Signalling server not set up. Run setup_streaming.ps1 first." }
$StartBat = (Resolve-Path $StartBat).Path
New-Item -ItemType Directory -Force -Path $LogDir | Out-Null

if (Test-Path $ConfigFile) {
    $Config = Get-Content $ConfigFile -Raw | ConvertFrom-Json
} else {
    $Config = [pscustomobject]@{ TurnUser = "dronechallenger"; TurnPass = (New-Secret) }
    $Config | ConvertTo-Json | Set-Content -Path $ConfigFile -Encoding utf8
    Write-Log "Created $ConfigFile with new TURN credentials."
}
if (-not $Config.TurnUser -or -not $Config.TurnPass) { Fail "$ConfigFile must define TurnUser and TurnPass." }

if ((Test-PortListening $StreamerPort) -or (Test-PortListening $PlayerPort)) {
    Fail "Port $StreamerPort or $PlayerPort is already in use. Stop the running signalling server first."
}

if ($PublicIp -eq "auto") {
    $Deadline = (Get-Date).AddMinutes(3)
    $PublicIp = ""
    while (-not $PublicIp) {
        try {
            $Token = Invoke-RestMethod -Method Put -Uri "http://169.254.169.254/latest/api/token" -Headers @{ "X-aws-ec2-metadata-token-ttl-seconds" = "300" } -TimeoutSec 5
            $PublicIp = Invoke-RestMethod -Uri "http://169.254.169.254/latest/meta-data/public-ipv4" -Headers @{ "X-aws-ec2-metadata-token" = $Token } -TimeoutSec 5
        } catch {
            if ((Get-Date) -gt $Deadline) { Fail "Could not read the public IPv4 address from instance metadata: $($_.Exception.Message)" }
            Start-Sleep -Seconds 5
        }
    }
    Write-Log "Public IP from instance metadata: $PublicIp"
}

$ServerArgs = @("--turn-user", $Config.TurnUser, "--turn-pass", $Config.TurnPass)
if ($PublicIp) { $ServerArgs += @("--publicip", $PublicIp) } else { $ServerArgs += @("--publicip", "127.0.0.1") }
if ($StartTurn) { $ServerArgs += "--start-turn" }
$ServerArgs += @("--", "--streamer_port", $StreamerPort, "--player_port", $PlayerPort)

$ServerLog = Join-Path $LogDir "signalling.log"
$ServerCmd = "/s /c `"`"$StartBat`" $($ServerArgs -join ' ') < nul > `"$ServerLog`" 2>&1`""
$Server = Start-Process -FilePath "cmd.exe" -ArgumentList $ServerCmd -WindowStyle Hidden -PassThru
Write-Log "Signalling server starting (pid $($Server.Id)), log: $ServerLog"

$Deadline = (Get-Date).AddMinutes(3)
while (-not (Test-PortListening $StreamerPort)) {
    if ((Get-Date) -gt $Deadline) { Fail "Signalling server did not open port $StreamerPort within 3 minutes. See $ServerLog" }
    if ($Server.HasExited) { Fail "Signalling server exited. See $ServerLog" }
    Start-Sleep -Seconds 2
}
Write-Log "Signalling server listening: players on port $PlayerPort, streamer on port $StreamerPort."

$GameArgs = @(
    "-PixelStreamingConnectionURL=ws://127.0.0.1:$StreamerPort",
    "-RenderOffScreen",
    "-ResX=$ResX", "-ResY=$ResY", "-ForceRes",
    "-Unattended"
)

try {
    while ($true) {
        if ($Server.HasExited -and -not (Test-PortListening $StreamerPort)) {
            Fail "Signalling server stopped. See $ServerLog"
        }
        $Game = Start-Process -FilePath $GameExe -ArgumentList $GameArgs -PassThru
        Write-Log "Game started (pid $($Game.Id))."
        $Game.WaitForExit()
        Write-Log "Game exited with code $($Game.ExitCode); restarting in 5 seconds."
        Start-Sleep -Seconds 5
    }
} finally {
    Get-Process -Name "DroneChallenger*" -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
    Stop-PortOwners @($StreamerPort, $PlayerPort, 8889)
    Get-Process -Name "turnserver" -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
    Write-Log "Streaming stopped."
}
