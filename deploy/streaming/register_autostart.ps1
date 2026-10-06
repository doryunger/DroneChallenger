param(
    [int]$IdleMinutes = 10,
    [int]$MaxUptimeMinutes = 20,
    [switch]$Remove
)

$ErrorActionPreference = "Stop"

$Principal = New-Object Security.Principal.WindowsPrincipal([Security.Principal.WindowsIdentity]::GetCurrent())
if (-not $Principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    Write-Host "ERROR: Run this script from an elevated (Administrator) PowerShell." -ForegroundColor Red
    exit 1
}

$Tasks = [ordered]@{
    "DroneChallengerStreaming"    = "-NoProfile -ExecutionPolicy Bypass -File `"$(Join-Path $PSScriptRoot 'start_streaming.ps1')`" -PublicIp auto -StartTurn"
    "DroneChallengerSessionGuard" = "-NoProfile -ExecutionPolicy Bypass -File `"$(Join-Path $PSScriptRoot 'session_guard.ps1')`" -IdleMinutes $IdleMinutes -MaxUptimeMinutes $MaxUptimeMinutes"
}

$WasRunning = @()
foreach ($Name in $Tasks.Keys) {
    $Existing = Get-ScheduledTask -TaskName $Name -ErrorAction SilentlyContinue
    if ($Existing) {
        if ($Existing.State -eq "Running") { $WasRunning += $Name }
        Unregister-ScheduledTask -TaskName $Name -Confirm:$false
    }
}
if ($Remove) {
    Write-Host "Autostart tasks removed."
    exit 0
}

$Trigger = New-ScheduledTaskTrigger -AtStartup
$TaskPrincipal = New-ScheduledTaskPrincipal -UserId "SYSTEM" -LogonType ServiceAccount -RunLevel Highest
$Settings = New-ScheduledTaskSettingsSet -ExecutionTimeLimit ([TimeSpan]::Zero) `
    -RestartCount 3 -RestartInterval (New-TimeSpan -Minutes 1) `
    -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries -StartWhenAvailable

foreach ($Name in $Tasks.Keys) {
    $Action = New-ScheduledTaskAction -Execute "powershell.exe" -Argument $Tasks[$Name] -WorkingDirectory $PSScriptRoot
    Register-ScheduledTask -TaskName $Name -Action $Action -Trigger $Trigger -Principal $TaskPrincipal -Settings $Settings | Out-Null
    Write-Host "Registered '$Name' (runs as SYSTEM at every boot)." -ForegroundColor Green
}
foreach ($Name in $WasRunning) {
    Start-ScheduledTask -TaskName $Name
    Write-Host "Restarted '$Name', which was running before re-registration."
}
Write-Host "Session limits: stop after $IdleMinutes min without a player, or $MaxUptimeMinutes min after boot."
