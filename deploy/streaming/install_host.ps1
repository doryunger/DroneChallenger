param(
    [switch]$SkipDriver,
    [string]$MinGitUrl = "https://github.com/git-for-windows/git/releases/download/v2.47.1.windows.1/MinGit-2.47.1-64-bit.zip"
)

$ErrorActionPreference = "Stop"

function Fail([string]$Message) {
    Write-Host "ERROR: $Message" -ForegroundColor Red
    exit 1
}

function Step([string]$Message) {
    Write-Host ""
    Write-Host "== $Message" -ForegroundColor Cyan
}

$Principal = New-Object Security.Principal.WindowsPrincipal([Security.Principal.WindowsIdentity]::GetCurrent())
if (-not $Principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    Fail "Run this script from an elevated (Administrator) PowerShell."
}
if (-not [Environment]::Is64BitProcess) {
    Fail "Run this script from 64-bit PowerShell (C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe)."
}

$RepoDir = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$HostRoot = Split-Path $RepoDir -Parent
$GameDir = Join-Path $RepoDir "Packaged\StreamShipping\Windows"
if (-not (Test-Path (Join-Path $GameDir "DroneChallenger.exe"))) { Fail "Game build not found in $GameDir" }
$RebootNeeded = $false

Step "Unblock kit files downloaded from the internet"
Get-ChildItem -Path $HostRoot -Recurse -File -ErrorAction SilentlyContinue | Unblock-File
Write-Host "  Done."

Step "Windows Firewall rules"
$Rules = @(
    @{ Name = "DroneChallenger Player Page";   Protocol = "TCP"; Port = "80" },
    @{ Name = "DroneChallenger TURN TCP";      Protocol = "TCP"; Port = "19303" },
    @{ Name = "DroneChallenger TURN UDP";      Protocol = "UDP"; Port = "19303" },
    @{ Name = "DroneChallenger WebRTC Media";  Protocol = "UDP"; Port = "49152-65535" }
)
foreach ($Rule in $Rules) {
    if (-not (Get-NetFirewallRule -DisplayName $Rule.Name -ErrorAction SilentlyContinue)) {
        New-NetFirewallRule -DisplayName $Rule.Name -Direction Inbound -Action Allow -Protocol $Rule.Protocol -LocalPort $Rule.Port | Out-Null
    }
    Write-Host "  $($Rule.Name): $($Rule.Protocol) $($Rule.Port)"
}

Step "Hardware GPU for Remote Desktop sessions"
$TsPolicy = "HKLM:\SOFTWARE\Policies\Microsoft\Windows NT\Terminal Services"
if (-not (Test-Path $TsPolicy)) { New-Item -Path $TsPolicy -Force | Out-Null }
$Current = (Get-ItemProperty -Path $TsPolicy -Name "bEnumerateHWBeforeSW" -ErrorAction SilentlyContinue).bEnumerateHWBeforeSW
if ($Current -ne 1) {
    New-ItemProperty -Path $TsPolicy -Name "bEnumerateHWBeforeSW" -PropertyType DWord -Value 1 -Force | Out-Null
    $RebootNeeded = $true
    Write-Host "  Enabled (takes effect after reboot)."
} else {
    Write-Host "  Already enabled."
}

Step "Trusted root certificates"
$RootSst = Join-Path $env:TEMP "windows-update-roots.sst"
certutil.exe -generateSSTFromWU $RootSst | Out-Null
if ($LASTEXITCODE -ne 0 -or -not (Test-Path $RootSst)) { Fail "Could not download the trusted root list from Windows Update." }
$Imported = Get-ChildItem -Path $RootSst | Import-Certificate -CertStoreLocation Cert:\LocalMachine\Root
Remove-Item $RootSst -Force
Write-Host "  Imported $(@($Imported).Count) root certificates."

Step "Media Foundation feature"
$Feature = Get-WindowsFeature -Name Server-Media-Foundation
if (-not $Feature.Installed) {
    $Result = Install-WindowsFeature -Name Server-Media-Foundation
    if ($Result.RestartNeeded -eq "Yes") { $RebootNeeded = $true }
    Write-Host "  Installed."
} else {
    Write-Host "  Already installed."
}

Step "Visual C++ runtime"
$VcRedist = Join-Path $GameDir "Engine\Extras\Redist\en-us\vc_redist.x64.exe"
if (Test-Path $VcRedist) {
    $Proc = Start-Process -FilePath $VcRedist -ArgumentList "/install", "/quiet", "/norestart" -PassThru
    $Proc.WaitForExit()
    if ($Proc.ExitCode -notin 0, 1638, 3010) { Fail "vc_redist failed with exit code $($Proc.ExitCode)" }
    if ($Proc.ExitCode -eq 3010) { $RebootNeeded = $true }
    Write-Host "  Done (exit code $($Proc.ExitCode))."
} else {
    Fail "vc_redist.x64.exe not found at $VcRedist"
}

Step "NVIDIA GRID driver"
if ($SkipDriver) {
    Write-Host "  Skipped."
} elseif (Get-Command nvidia-smi -ErrorAction SilentlyContinue) {
    Write-Host "  Already installed:"
    nvidia-smi --query-gpu=name,driver_version --format=csv,noheader
} else {
    Import-Module AWSPowerShell -ErrorAction Stop
    $DriverDir = Join-Path $HostRoot "nvidia-driver"
    New-Item -ItemType Directory -Force -Path $DriverDir | Out-Null
    $Objects = Get-S3Object -BucketName "ec2-windows-nvidia-drivers" -KeyPrefix "latest" -Region us-east-1
    if (-not $Objects) { Fail "No driver files found. Is an IAM role with AmazonS3ReadOnlyAccess attached to this instance?" }
    foreach ($Object in $Objects) {
        if ($Object.Key -and $Object.Size -gt 0) {
            Copy-S3Object -BucketName "ec2-windows-nvidia-drivers" -Key $Object.Key -LocalFile (Join-Path $DriverDir $Object.Key) -Region us-east-1 | Out-Null
            Write-Host "  Downloaded $($Object.Key)"
        }
    }
    $Installer = Get-ChildItem -Path $DriverDir -Recurse -Filter "*server2022*.exe" | Select-Object -First 1
    if (-not $Installer) { $Installer = Get-ChildItem -Path $DriverDir -Recurse -Filter "*.exe" | Select-Object -First 1 }
    if (-not $Installer) { Fail "No driver installer found in $DriverDir" }
    Write-Host "  Installing $($Installer.Name) (several minutes; the screen may flicker)..."
    $Proc = Start-Process -FilePath $Installer.FullName -ArgumentList "-s", "-noreboot" -PassThru
    $Proc.WaitForExit()
    if ($Proc.ExitCode -ne 0) { Fail "Driver installer exited with code $($Proc.ExitCode)" }
    New-Item -Path "HKLM:\SOFTWARE\NVIDIA Corporation\Global" -Name GridLicensing -Force | Out-Null
    New-ItemProperty -Path "HKLM:\SOFTWARE\NVIDIA Corporation\Global\GridLicensing" -Name "NvCplDisableManageLicensePage" -PropertyType DWord -Value 1 -Force | Out-Null
    $RebootNeeded = $true
    Write-Host "  Installed."
}

Step "Git (portable) for the streaming server setup"
$GitDir = Join-Path $HostRoot "MinGit"
if (-not (Test-Path (Join-Path $GitDir "cmd\git.exe"))) {
    $GitZip = Join-Path $HostRoot "MinGit.zip"
    Invoke-WebRequest -Uri $MinGitUrl -OutFile $GitZip -UseBasicParsing
    Expand-Archive -Path $GitZip -DestinationPath $GitDir -Force
    Remove-Item $GitZip
}
$env:Path = "$(Join-Path $GitDir 'cmd');$env:Path"
Write-Host "  $(git --version)"

Step "Pixel Streaming server"
& (Join-Path $PSScriptRoot "setup_streaming.ps1") -InfraDir (Join-Path $HostRoot "PixelStreamingInfrastructure")
if ($LASTEXITCODE -ne 0) { Fail "setup_streaming.ps1 failed." }

Write-Host ""
if ($RebootNeeded) {
    Write-Host "Host setup complete. REBOOT NOW (Restart-Computer), then run start_streaming.ps1 -PublicIp auto -StartTurn" -ForegroundColor Yellow
} else {
    Write-Host "Host setup complete. Run start_streaming.ps1 -PublicIp auto -StartTurn" -ForegroundColor Green
}
