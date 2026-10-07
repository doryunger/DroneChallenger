param(
    [string]$ArboristDir = (Join-Path $PSScriptRoot "..\..\arborist"),
    [string]$VcpkgDir = (Join-Path $env:USERPROFILE "vcpkg"),
    [string]$Triplet = "x64-windows-static-md"
)

$ErrorActionPreference = "Stop"

function Fail([string]$Message) {
    Write-Host "ERROR: $Message" -ForegroundColor Red
    exit 1
}

function Invoke-Checked([string]$Exe, [string[]]$Arguments) {
    & $Exe @Arguments
    if ($LASTEXITCODE -ne 0) { Fail "$Exe $($Arguments -join ' ') exited with $LASTEXITCODE" }
}

$ProjectDir = Resolve-Path (Join-Path $PSScriptRoot "..")
$ArboristDir = Resolve-Path $ArboristDir -ErrorAction SilentlyContinue
if (-not $ArboristDir -or -not (Test-Path (Join-Path $ArboristDir "CMakeLists.txt"))) {
    Fail "Arborist source not found. Pass -ArboristDir <path>."
}

$VsWhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $VsWhere)) { Fail "Visual Studio is not installed." }
$VsPath = & $VsWhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $VsPath) { Fail "Visual Studio is installed but the MSVC x64 toolset is missing." }

$CMake = Join-Path $VsPath "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
if (-not (Test-Path $CMake)) {
    $CMake = (Get-Command cmake -ErrorAction SilentlyContinue).Source
}
if (-not $CMake) { Fail "CMake not found. Add 'C++ CMake tools for Windows' in the Visual Studio Installer." }

Write-Host "Visual Studio: $VsPath"
Write-Host "CMake:         $CMake"
Write-Host "Arborist:      $ArboristDir"
Write-Host "vcpkg:         $VcpkgDir"

if (-not (Test-Path (Join-Path $VcpkgDir "vcpkg.exe"))) {
    if (-not (Test-Path $VcpkgDir)) {
        Invoke-Checked git @("clone", "https://github.com/microsoft/vcpkg.git", $VcpkgDir)
    }
    Invoke-Checked (Join-Path $VcpkgDir "bootstrap-vcpkg.bat") @("-disableMetrics")
}
$Vcpkg = Join-Path $VcpkgDir "vcpkg.exe"

Invoke-Checked $Vcpkg @("install", "ryml", "sqlite3", "cpp-httplib[brotli]", "--triplet", $Triplet, "--clean-after-build")

$BuildDir = Join-Path $ArboristDir "build-ue"
$Toolchain = Join-Path $VcpkgDir "scripts\buildsystems\vcpkg.cmake"

Invoke-Checked $CMake @(
    "-S", $ArboristDir,
    "-B", $BuildDir,
    "-A", "x64",
    "-DCMAKE_TOOLCHAIN_FILE=$Toolchain",
    "-DVCPKG_TARGET_TRIPLET=$Triplet",
    "-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL",
    "-DCMAKE_POLICY_DEFAULT_CMP0091=NEW",
    "-DENABLE_CLANG_TIDY=OFF",
    "-DARBORIST_BUILD_TESTS=OFF",
    "-DARBORIST_BUILD_EXAMPLES=OFF",
    "-DARBORIST_BUILD_BENCHMARKS=OFF",
    "-DARBORIST_BUILD_TOOLS=OFF"
)
Invoke-Checked $CMake @("--build", $BuildDir, "--config", "Release", "--target", "bt_framework", "--parallel")

$ModuleDir = Join-Path $ProjectDir "Source\ThirdParty\ArboristLib"
$IncludeOut = Join-Path $ModuleDir "include"
$LibOut = Join-Path $ModuleDir "lib\Win64"
$VcpkgInstalled = Join-Path $BuildDir "vcpkg_installed\$Triplet"
if (-not (Test-Path $VcpkgInstalled)) { $VcpkgInstalled = Join-Path $VcpkgDir "installed\$Triplet" }

New-Item -ItemType Directory -Force -Path $IncludeOut, (Join-Path $IncludeOut "httplib"), $LibOut | Out-Null
Copy-Item -Recurse -Force (Join-Path $ArboristDir "include\bt") $IncludeOut

$HttplibHeader = Get-ChildItem -Recurse -Filter "httplib.h" -Path (Join-Path $VcpkgInstalled "include"), (Join-Path $BuildDir "_deps") -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $HttplibHeader) { Fail "httplib.h not found." }
Copy-Item -Force $HttplibHeader.FullName (Join-Path $IncludeOut "httplib")

$BuiltLib = Join-Path $BuildDir "Release\bt_framework.lib"
if (-not (Test-Path $BuiltLib)) { Fail "Build output $BuiltLib not found." }
Copy-Item -Force $BuiltLib (Join-Path $LibOut "arborist.lib")

$VcpkgLibDir = Join-Path $VcpkgInstalled "lib"
$Expected = @("sqlite3", "ryml", "c4core", "brotlienc", "brotlidec", "brotlicommon")
foreach ($Name in $Expected) {
    $Candidate = Get-ChildItem -Path $VcpkgLibDir -Filter "*.lib" | Where-Object { $_.BaseName -match "^(lib)?$Name(-static)?$" } | Select-Object -First 1
    if (-not $Candidate) { Fail "$Name library not found in $VcpkgLibDir" }
    Copy-Item -Force $Candidate.FullName (Join-Path $LibOut "$Name.lib")
}

Write-Host ""
Write-Host "ArboristLib ready:" -ForegroundColor Green
Get-ChildItem $LibOut | ForEach-Object { Write-Host "  lib\Win64\$($_.Name)" }
Write-Host "  include\bt ($((Get-ChildItem (Join-Path $IncludeOut 'bt') -File).Count) headers), include\httplib\httplib.h"
