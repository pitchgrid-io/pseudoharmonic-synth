# Sign PseudoHarmonic Windows artifacts using Azure Trusted Signing
# Usage: .\sign_app.ps1
#
# Signs the Standalone exe and VST3 dll before installer creation.
#
# Requires:
# - Azure account with Trusted Signing configured
# - TrustedSigning PowerShell module
# - Environment variables: AZURE_ENDPOINT, AZURE_SIGNING_ACCOUNT, AZURE_CERT_PROFILE

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
$AppVersion = if ($env:APP_VERSION) { $env:APP_VERSION } else { "0.2.1" }
$BuildDir = "build-release-win"
$ArtifactsDir = "$BuildDir\PseudoHarmonicSynth_artefacts\Release"

Write-Host "Signing $AppName artifacts..." -ForegroundColor Cyan

# Check Azure Trusted Signing configuration
$AzureEndpoint = $env:AZURE_ENDPOINT
$SigningAccount = $env:AZURE_SIGNING_ACCOUNT
$CertProfile = $env:AZURE_CERT_PROFILE

if (-not $AzureEndpoint) {
    Write-Host "Error: AZURE_ENDPOINT not set in .env" -ForegroundColor Red
    exit 1
}

if (-not $SigningAccount) {
    $SigningAccount = "pitchgrid"
    Write-Host "  Using default signing account: $SigningAccount" -ForegroundColor Yellow
}

if (-not $CertProfile) {
    $CertProfile = "pitchgrid"
    Write-Host "  Using default certificate profile: $CertProfile" -ForegroundColor Yellow
}

# Ensure dotnet is in PATH
$dotnetPath = "C:\Program Files\dotnet"
if (Test-Path $dotnetPath) {
    if ($env:PATH -notlike "*$dotnetPath*") {
        $env:PATH = "$dotnetPath;$env:PATH"
    }
}

$dotnetCmd = Get-Command dotnet -ErrorAction SilentlyContinue
if (-not $dotnetCmd) {
    Write-Host "Error: .NET SDK not found. Install from https://dotnet.microsoft.com/download" -ForegroundColor Red
    exit 1
}

# Install TrustedSigning module if needed
Write-Host ""
Write-Host "Checking TrustedSigning module..." -ForegroundColor Yellow
if (-not (Get-InstalledModule TrustedSigning -ErrorAction SilentlyContinue)) {
    Write-Host "  Installing TrustedSigning module..."
    Install-Module TrustedSigning -Confirm:$False -Force -Scope CurrentUser
}
Write-Host "  TrustedSigning module ready"

# Collect files to sign
$FilesToSign = @()

# Standalone exe
$StandaloneExe = "$ArtifactsDir\Standalone\$AppName.exe"
if (Test-Path $StandaloneExe) {
    $FilesToSign += (Resolve-Path $StandaloneExe).Path
}

# VST3 dll
$Vst3Dll = "$ArtifactsDir\VST3\$AppName.vst3\Contents\x86_64-win\$AppName.vst3"
if (Test-Path $Vst3Dll) {
    $FilesToSign += (Resolve-Path $Vst3Dll).Path
}

if ($FilesToSign.Count -eq 0) {
    Write-Host "Error: No artifacts found to sign in $ArtifactsDir" -ForegroundColor Red
    Write-Host "Run .\build_app.ps1 first" -ForegroundColor Yellow
    exit 1
}

# Sign each artifact
foreach ($file in $FilesToSign) {
    Write-Host ""
    Write-Host "Signing $(Split-Path $file -Leaf)..." -ForegroundColor Yellow

    $params = @{
        Endpoint              = $AzureEndpoint
        CodeSigningAccountName = $SigningAccount
        CertificateProfileName = $CertProfile
        Files                 = $file
        FileDigest            = "SHA256"
        TimestampRfc3161      = "http://timestamp.acs.microsoft.com"
        TimestampDigest       = "SHA256"
    }

    try {
        Invoke-TrustedSigning @params
        Write-Host "  Signed successfully" -ForegroundColor Green
    }
    catch {
        Write-Host "  Error signing: $_" -ForegroundColor Red
        Write-Host "  Troubleshooting:" -ForegroundColor Yellow
        Write-Host "    1. Ensure you're logged in to Azure: az login" -ForegroundColor Yellow
        Write-Host "    2. Verify AZURE_ENDPOINT is correct" -ForegroundColor Yellow
        exit 1
    }

    # Verify signature
    $sig = Get-AuthenticodeSignature $file
    if ($sig.Status -eq "Valid") {
        Write-Host "  Signature verified" -ForegroundColor Green
    } else {
        Write-Host "  Warning: Signature status is $($sig.Status)" -ForegroundColor Yellow
    }
}

Write-Host ""
Write-Host "All artifacts signed!" -ForegroundColor Green
Write-Host ""
Write-Host "Next steps:" -ForegroundColor Yellow
Write-Host "  1. Create installer: .\create_installer.ps1"
Write-Host "  2. Sign installer: .\sign_installer.ps1"
