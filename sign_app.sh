#!/bin/bash
# Code sign PseudoHarmonic plugin artifacts for macOS
# Usage: ./sign_app.sh [--arch arm64|x86_64|universal]

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

IDENTITY="${CODESIGN_IDENTITY}"
APP_NAME="${APP_NAME:-PseudoHarmonic}"
BUILD_DIR="build-release-${ARCH}"
ARTIFACTS="${BUILD_DIR}/PseudoHarmonicSynth_artefacts/Release"

if [ -z "$IDENTITY" ]; then
    echo "No CODESIGN_IDENTITY set in .env, skipping code signing"
    exit 0
fi

echo "Signing ${APP_NAME} artifacts for ${ARCH}..."

# Sign each artifact type
for artifact in \
    "${ARTIFACTS}/Standalone/${APP_NAME}.app" \
    "${ARTIFACTS}/VST3/${APP_NAME}.vst3" \
    "${ARTIFACTS}/AU/${APP_NAME}.component"; do

    if [ ! -e "$artifact" ]; then
        echo "  Skipping $(basename "$artifact") (not found)"
        continue
    fi

    echo "  Signing $(basename "$artifact")..."

    # Remove extended attributes
    xattr -rc "$artifact" 2>/dev/null || true

    # Remove existing signatures
    find "$artifact" -name "_CodeSignature" -type d -exec rm -rf {} + 2>/dev/null || true

    # Sign with hardened runtime
    codesign --deep --force --sign "$IDENTITY" --options runtime --timestamp "$artifact"

    # Verify
    if codesign --verify --deep --strict "$artifact" 2>&1; then
        echo "    Verified OK"
    else
        echo "    Verification FAILED"
        exit 1
    fi
done

echo ""
echo "All artifacts signed successfully!"
