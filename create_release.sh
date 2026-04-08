#!/bin/bash
# Create GitHub release for PseudoHarmonic
# Uploads all available installer packages for the current version

set -e

# Load environment variables
if [ -f .env ]; then
    set -a
    source .env
    set +a
fi

APP_NAME="${APP_NAME:-PseudoHarmonic}"
VERSION="${APP_VERSION:-0.2.1}"

# Find all installer packages for this version
ASSETS=()
for arch in arm64 x86_64; do
    pkg="${APP_NAME}-${VERSION}-${arch}.pkg"
    if [ -f "$pkg" ]; then
        ASSETS+=("$pkg")
    fi
    # Also check for DMGs (legacy)
    dmg="${APP_NAME}-${VERSION}-${arch}.dmg"
    if [ -f "$dmg" ]; then
        ASSETS+=("$dmg")
    fi
done

# Check for Windows installer
WIN_INSTALLER="Installers/Windows/Output/PseudoHarmonic-${VERSION}-Setup.exe"
if [ -f "$WIN_INSTALLER" ]; then
    ASSETS+=("$WIN_INSTALLER")
fi

if [ ${#ASSETS[@]} -eq 0 ]; then
    echo "No installer files found for version ${VERSION}"
    echo "Expected files like: ${APP_NAME}-${VERSION}-arm64.pkg"
    echo "Run the build + installer scripts first"
    exit 1
fi

echo "Creating GitHub release for ${APP_NAME} v${VERSION}..."
echo "Found installer files:"
for asset in "${ASSETS[@]}"; do
    echo "   - $asset"
done

# Check if gh CLI is installed and authenticated
if ! command -v gh &> /dev/null; then
    echo "GitHub CLI not found. Install from https://cli.github.com/"
    exit 1
fi

if ! gh auth status &> /dev/null; then
    echo "Not authenticated with GitHub. Run 'gh auth login'"
    exit 1
fi

# Load release notes
if [ ! -f "RELEASE_NOTES.md" ]; then
    echo "RELEASE_NOTES.md not found"
    exit 1
fi
RELEASE_NOTES=$(cat RELEASE_NOTES.md)

# Check if release already exists
echo "Checking if release v${VERSION} exists..."

if gh release view "v${VERSION}" &> /dev/null; then
    echo "Release v${VERSION} already exists"
    echo "Uploading installers as additional assets..."

    for asset in "${ASSETS[@]}"; do
        echo "   Uploading $asset..."
        gh release upload "v${VERSION}" "$asset" --clobber
    done

    echo "Assets uploaded successfully!"

    echo "Updating release notes..."
    gh release edit "v${VERSION}" --notes "$RELEASE_NOTES"
else
    echo "Creating new release v${VERSION}..."

    gh release create "v${VERSION}" \
        --title "PseudoHarmonic v${VERSION}" \
        --notes "$RELEASE_NOTES" \
        "${ASSETS[@]}"

    echo "Release created successfully!"
fi

echo ""
echo "Uploaded files:"
for asset in "${ASSETS[@]}"; do
    echo "   - $asset"
done
