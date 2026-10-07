param(
    [int]$IdleMinutes = 10,
    [int]$MaxUptimeMinutes = 20,
    [int]$PlayerPort = 80,
    [int]$CheckSeconds = 30,
    [string]$LogDir = (Join-Path $PSScriptRoot "logs"),
    [switch]$DryRun
)

$ErrorActionPreference = "Stop"
New-Item -ItemType Directory -Force -Path $LogDir | Out-Null
$LogFile = Join-Path $LogDir "session_guard.log"

function Write-Log([string]$Message) {
    $Line = "[{0:yyyy-MM-dd HH:mm:ss}] {1}" -f (Get-Date), $Message
    Write-Host $Line
    Add-Content -Path $LogFile -Value $Line
}

function Get-ConnectedPlayers {
    @(Get-NetTCPConnection -State Established -LocalPort $PlayerPort -ErrorAction SilentlyContinue |
        Where-Object { $_.RemoteAddress -notin @("127.0.0.1", "::1") } |
        Select-Object -ExpandProperty RemoteAddress -Unique).Count
}

function Stop-Instance([string]$Reason) {
    Write-Log "Stopping instance: $Reason"
    if ($DryRun) {
        Write-Log "Dry run: shutdown skipped."
        exit 0
    }
    Stop-Computer -Force
    exit 0
}

$BootTime = (Get-CimInstance Win32_OperatingSystem).LastBootUpTime
$LastActive = $BootTime
$LastReported = -1
Write-Log "Session guard started: idle limit $IdleMinutes min, uptime cap $MaxUptimeMinutes min, boot at $BootTime."

while ($true) {
    $Now = Get-Date
    $Uptime = $Now - $BootTime
    $Players = Get-ConnectedPlayers

    if ($Players -gt 0) { $LastActive = $Now }
    if ($Players -ne $LastReported) {
        Write-Log "Connected players: $Players (uptime $([int]$Uptime.TotalMinutes) min)."
        $LastReported = $Players
    }

    if ($Uptime.TotalMinutes -ge $MaxUptimeMinutes) {
        Stop-Instance "uptime cap of $MaxUptimeMinutes minutes reached."
    }
    $Idle = $Now - $LastActive
    if ($Idle.TotalMinutes -ge $IdleMinutes) {
        Stop-Instance "no player connected for $IdleMinutes minutes."
    }

    Start-Sleep -Seconds $CheckSeconds
}
