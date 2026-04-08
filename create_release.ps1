# Create GitHub release for PseudoHarmonic Windows
# Usage: .\create_release.ps1 [version]

param(
    [string]$Version
)

$ErrorActionPreference = "Stop"

# Load environment variables from .env file
if (Test-Path ".env") {
    Get-Content ".env" | ForEach-Object {
        if ($_ -match "^\s*([^#][^=]+)=(.*)$") {
            $name = $matches[1].Trim()
            $value = $matches[2].Trim().Trim('"').Trim("'")
            [Environment]::SetEnvironmentVariable($name, $value, "Process")
        }
    }
}

$AppName = if ($env:APP_NAME) { $env:APP_NAME } else { "PseudoHarmonic" }

if (-not $Version) {
    $Version = $env:APP_VERSION
    if (-not $Version) { $Version = "0.2.1" }
}

Write-Host "Creating GitHub release for $AppName v$Version..." -ForegroundColor Cyan

# Find the installer
$OutputDir = "Installers\Windows\Output"
$InstallerFile = Get-ChildItem "$OutputDir\*.exe" -ErrorAction SilentlyContinue |
                 Sort-Object LastWriteTime -Descending |
                 Select-Object -First 1

if (-not $InstallerFile) {
    Write-Host "Error: Installer not found in $OutputDir" -ForegroundColor Red
    Write-Host "Run the following first:" -ForegroundColor Yellow
    Write-Host "  1. .\build_app.ps1" -ForegroundColor Yellow
    Write-Host "  2. .\sign_app.ps1" -ForegroundColor Yellow
    Write-Host "  3. .\create_installer.ps1" -ForegroundColor Yellow
    Write-Host "  4. .\sign_installer.ps1" -ForegroundColor Yellow
    exit 1
}

$InstallerPath = $InstallerFile.FullName
Write-Host "  Found installer: $InstallerPath"

# Verify installer is signed
Write-Host ""
Write-Host "Verifying installer signature..." -ForegroundColor Yellow
$sig = Get-AuthenticodeSignature $InstallerPath

if ($sig.Status -eq "Valid") {
    Write-Host "  Signature: Valid" -ForegroundColor Green
}
elseif ($sig.Status -eq "UnknownError") {
    Write-Host "  Signature: Self-signed (testing only)" -ForegroundColor Yellow
    $continue = Read-Host "Continue with self-signed installer? (y/N)"
    if ($continue -ne "y" -and $continue -ne "Y") {
        Write-Host "Release cancelled" -ForegroundColor Yellow
        exit 0
    }
}
else {
    Write-Host "  Warning: Installer is not signed ($($sig.Status))" -ForegroundColor Red
    $continue = Read-Host "Continue anyway? (y/N)"
    if ($continue -ne "y" -and $continue -ne "Y") {
        Write-Host "Release cancelled" -ForegroundColor Yellow
        exit 0
    }
}

# Check GitHub CLI
if (-not (Get-Command gh -ErrorAction SilentlyContinue)) {
    Write-Host "Error: GitHub CLI (gh) not found" -ForegroundColor Red
    Write-Host "Install with: winget install GitHub.cli" -ForegroundColor Yellow
    exit 1
}

$authStatus = gh auth status 2>&1
if ($LASTEXITCODE -ne 0) {
    Write-Host "Error: Not authenticated with GitHub. Run: gh auth login" -ForegroundColor Red
    exit 1
}

# Load release notes
if (-not (Test-Path "RELEASE_NOTES.md")) {
    Write-Host "Error: RELEASE_NOTES.md not found" -ForegroundColor Red
    exit 1
}
$releaseNotes = Get-Content "RELEASE_NOTES.md" -Raw

# Check if release already exists
Write-Host ""
Write-Host "Checking if release v$Version exists..." -ForegroundColor Yellow

$releaseExists = gh release view "v$Version" 2>$null
$releaseExistsCode = $LASTEXITCODE

if ($releaseExistsCode -eq 0) {
    Write-Host "  Release v$Version already exists, uploading installer..." -ForegroundColor Cyan

    try {
        gh release upload "v$Version" "$InstallerPath" --clobber
        Write-Host "Installer uploaded successfully!" -ForegroundColor Green

        gh release edit "v$Version" --notes $releaseNotes

        Write-Host ""
        Write-Host "View release at:" -ForegroundColor Yellow
        Write-Host "  https://github.com/$(gh repo view --json nameWithOwner -q .nameWithOwner)/releases/tag/v$Version"
    }
    catch {
        Write-Host "Error uploading installer: $_" -ForegroundColor Red
        exit 1
    }
}
else {
    Write-Host "  Creating new release v$Version..." -ForegroundColor Cyan

    try {
        gh release create "v$Version" `
            --title "PseudoHarmonic v$Version" `
            --notes $releaseNotes `
            "$InstallerPath"

        Write-Host "Release created successfully!" -ForegroundColor Green

        Write-Host ""
        Write-Host "View release at:" -ForegroundColor Yellow
        Write-Host "  https://github.com/$(gh repo view --json nameWithOwner -q .nameWithOwner)/releases/tag/v$Version"
    }
    catch {
        Write-Host "Error creating release: $_" -ForegroundColor Red
        exit 1
    }
}
