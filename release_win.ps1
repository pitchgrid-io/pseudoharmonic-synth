# Full Windows release script - builds, signs, creates installer, and uploads
# Usage: .\release_win.ps1

$ErrorActionPreference = "Stop"

Write-Host "==========================================" -ForegroundColor Cyan
Write-Host "  PseudoHarmonic - Windows Full Release" -ForegroundColor Cyan
Write-Host "==========================================" -ForegroundColor Cyan
Write-Host ""

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

Write-Host "App: $AppName"
Write-Host "Version: $AppVersion"
Write-Host ""

# Build
Write-Host "==========================================" -ForegroundColor Yellow
Write-Host "  Building" -ForegroundColor Yellow
Write-Host "==========================================" -ForegroundColor Yellow
.\build_app.ps1

# Sign artifacts
Write-Host ""
Write-Host "==========================================" -ForegroundColor Yellow
Write-Host "  Signing Artifacts" -ForegroundColor Yellow
Write-Host "==========================================" -ForegroundColor Yellow
.\sign_app.ps1

# Create installer
Write-Host ""
Write-Host "==========================================" -ForegroundColor Yellow
Write-Host "  Creating Installer" -ForegroundColor Yellow
Write-Host "==========================================" -ForegroundColor Yellow
.\create_installer.ps1

# Sign installer
Write-Host ""
Write-Host "==========================================" -ForegroundColor Yellow
Write-Host "  Signing Installer" -ForegroundColor Yellow
Write-Host "==========================================" -ForegroundColor Yellow
.\sign_installer.ps1

# Create release
Write-Host ""
Write-Host "==========================================" -ForegroundColor Yellow
Write-Host "  Creating GitHub Release" -ForegroundColor Yellow
Write-Host "==========================================" -ForegroundColor Yellow
.\create_release.ps1

Write-Host ""
Write-Host "==========================================" -ForegroundColor Green
Write-Host "  Release Complete!" -ForegroundColor Green
Write-Host "==========================================" -ForegroundColor Green
