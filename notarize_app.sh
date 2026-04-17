#!/bin/bash
# Notarize a PseudoHarmonic installer package (.pkg)
# Usage: ./notarize_app.sh [--arch arm64|x86_64]

set -e

# Parse arguments
ARCH=""
while [[ $# -gt 0 ]]; do
    case $1 in
        --arch)
            ARCH="$2"
            shift 2
            ;;
        *)
            echo "Unknown option: $1"
            echo "Usage: ./notarize_app.sh [--arch arm64|x86_64]"
            exit 1
            ;;
    esac
done

if [ -z "$ARCH" ]; then
    ARCH="$(uname -m)"
fi

# Load environment variables
if [ -f .env ]; then
    set -a
    source .env
    set +a
fi

APP_NAME="${APP_NAME:-PseudoHarmonic}"
APP_VERSION="${APP_VERSION:-0.3.1}"
PKG_NAME="${APP_NAME}-${APP_VERSION}-${ARCH}"
PKG_PATH="${PKG_NAME}.pkg"

APPLE_ID="${APPLE_ID:-your-apple-id@email.com}"
TEAM_ID="${TEAM_ID:-YOUR_TEAM_ID}"
APP_PASSWORD="${APP_PASSWORD:-your-app-specific-password}"

if [[ "$APPLE_ID" == "your-apple-id@email.com" ]] || [[ "$APP_PASSWORD" == "your-app-specific-password" ]]; then
    echo "Please set your Apple ID credentials in .env:"
    echo "   APPLE_ID='your@email.com'"
    echo "   APP_PASSWORD='your-app-specific-password'"
    echo ""
    echo "Create app-specific password at: https://appleid.apple.com/"
    exit 1
fi

if [ ! -f "$PKG_PATH" ]; then
    echo "Installer not found: ${PKG_PATH}"
    echo "Run ./Installers/Mac/make-installer.sh --arch ${ARCH} first"
    exit 1
fi

echo "Notarizing ${PKG_PATH}..."

# Submit for notarization
echo "Submitting for notarization..."
echo "This will take a few minutes..."

xcrun notarytool submit "$PKG_PATH" \
    --apple-id "$APPLE_ID" \
    --password "$APP_PASSWORD" \
    --team-id "$TEAM_ID" \
    --wait

if [ $? -eq 0 ]; then
    echo "Notarization successful!"

    # Staple the ticket to the pkg
    echo "Stapling notarization ticket..."
    xcrun stapler staple "$PKG_PATH"

    echo ""
    echo "Notarized installer ready: ${PKG_PATH}"
else
    echo "Notarization failed"
    echo "Check your Apple ID, password, and team ID"
    exit 1
fi
