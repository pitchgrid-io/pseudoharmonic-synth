#!/bin/bash
# Full macOS release script - builds, creates installer, notarizes, and uploads
# Usage: ./release_mac.sh

set -e

echo "=========================================="
echo "  PseudoHarmonic - macOS Full Release"
echo "=========================================="
echo ""

# Load environment variables
if [ -f .env ]; then
    set -a
    source .env
    set +a
fi

VERSION="${APP_VERSION:-0.3.1}"
echo "Version: ${VERSION}"
echo ""

# Generate icons
echo "=========================================="
echo "  Generating Icons"
echo "=========================================="
./generate_icons.sh

# Build and package arm64
echo ""
echo "=========================================="
echo "  Building for Apple Silicon (arm64)"
echo "=========================================="
./build_app.sh --arch arm64

echo ""
echo "=========================================="
echo "  Creating arm64 Installer"
echo "=========================================="
./Installers/Mac/make-installer.sh --arch arm64

echo ""
echo "=========================================="
echo "  Notarizing arm64 Installer"
echo "=========================================="
./notarize_app.sh --arch arm64

# Build and package x86_64
echo ""
echo "=========================================="
echo "  Building for Intel (x86_64)"
echo "=========================================="
./build_app.sh --arch x86_64

echo ""
echo "=========================================="
echo "  Creating x86_64 Installer"
echo "=========================================="
./Installers/Mac/make-installer.sh --arch x86_64

echo ""
echo "=========================================="
echo "  Notarizing x86_64 Installer"
echo "=========================================="
./notarize_app.sh --arch x86_64

# Upload to GitHub
echo ""
echo "=========================================="
echo "  Creating GitHub Release"
echo "=========================================="
./create_release.sh

echo ""
echo "=========================================="
echo "  Release Complete!"
echo "=========================================="
echo ""
echo "Installer packages:"
ls -la PseudoHarmonic-${VERSION}-*.pkg 2>/dev/null || echo "  (none found)"
