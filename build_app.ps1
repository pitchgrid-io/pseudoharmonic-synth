# Build PseudoHarmonic synth plugin for Windows
# Usage: .\build_app.ps1

$ErrorActionPreference = "Stop"

# Load environment variables from .env
if (Test-Path .env) {
    Get-Content .env | ForEach-Object {
        if ($_ -match "^([^#=]+)=(.*)$") {
            $name = $matches[1].Trim()
            $value = $matches[2].Trim().Trim("'`"")
            [Environment]::SetEnvironmentVariable($name, $value, "Process")
        }
    }
}

$AppName = if ($env:APP_NAME) { $env:APP_NAME } else { "PseudoHarmonic" }
$AppVersion = if ($env:APP_VERSION) { $env:APP_VERSION } else { "0.2.1" }

Write-Host "Building $AppName v$AppVersion for Windows..." -ForegroundColor Cyan

# Check for CMake
if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    Write-Host "Error: CMake not found. Install from https://cmake.org/" -ForegroundColor Red
    exit 1
}

# Check for Visual Studio / MSVC
$vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (Test-Path $vsWhere) {
    $vsPath = & $vsWhere -latest -property installationPath
    Write-Host "  Found Visual Studio: $vsPath"
} else {
    Write-Host "Warning: vswhere not found, CMake will attempt to find a generator" -ForegroundColor Yellow
}

$BuildDir = "build-release-win"

# Configure
Write-Host ""
Write-Host "Configuring CMake..." -ForegroundColor Yellow
cmake -B $BuildDir `
    -DCMAKE_BUILD_TYPE=Release `
    -A x64

# Build
Write-Host ""
Write-Host "Building..." -ForegroundColor Yellow
cmake --build $BuildDir --config Release -j

$ArtifactsDir = "$BuildDir\PseudoHarmonicSynth_artefacts\Release"

Write-Host ""
Write-Host "Build complete!" -ForegroundColor Green
Write-Host "Artifacts in: $ArtifactsDir"

if (Test-Path $ArtifactsDir) {
    Get-ChildItem $ArtifactsDir -Recurse -Depth 1 | ForEach-Object {
        Write-Host "  $($_.FullName.Replace((Get-Location).Path + '\', ''))"
    }
}
