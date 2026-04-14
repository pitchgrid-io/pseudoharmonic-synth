#!/bin/bash
# Build macOS .pkg installer for PseudoHarmonic
# Creates a product archive with selectable components (VST3, AU, Standalone)
#
# Usage: ./Installers/Mac/make-installer.sh [--arch arm64|x86_64]
#
# Adapted from pitchgrid-plugin/Installers/Mac/make-installer.sh

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

APP_NAME="${APP_NAME:-PseudoHarmonic}"
APP_VERSION="${APP_VERSION:-0.3.0}"
BUNDLE_ID="${BUNDLE_ID:-io.pitchgrid.pseudoharmonic}"
IDENTITY="${CODESIGN_IDENTITY}"
INSTALLER_IDENTITY="${INSTALLER_IDENTITY:-Developer ID Installer: ${DEVELOPER_NAME} (${TEAM_ID})}"
BUILD_DIR="build-release-${ARCH}"
ARTIFACTS="${BUILD_DIR}/PseudoHarmonicSynth_artefacts/Release"

UNDERSCORE_NAME="${APP_NAME// /_}"
OUTPUT_FILENAME="${APP_NAME}-${APP_VERSION}-${ARCH}"
PKG_PATH="${OUTPUT_FILENAME}.pkg"

BASEDIR="$(cd "$(dirname "$0")" && pwd)"
TMPDIR_PKG="${BASEDIR}/installer-tmp"

if [ ! -d "$ARTIFACTS" ]; then
    echo "Build artifacts not found at ${ARTIFACTS}"
    echo "Run ./build_app.sh --arch ${ARCH} first"
    exit 1
fi

echo "Creating macOS installer for ${APP_NAME} v${APP_VERSION} (${ARCH})..."

# Clean up any previous tmp
rm -rf "$TMPDIR_PKG"
mkdir -p "$TMPDIR_PKG/unsigned"

# --- Helper: sign a binary ---
sign_binary() {
    local path="$1"
    if [ -n "$IDENTITY" ]; then
        echo "    Signing $(basename "$path")..."
        xattr -rc "$path" 2>/dev/null || true
        find "$path" -name "_CodeSignature" -type d -exec rm -rf {} + 2>/dev/null || true
        codesign --deep --force --sign "$IDENTITY" --timestamp --options runtime "$path"
        codesign --verify --deep --strict "$path"
    else
        echo "    Skipping signing (no CODESIGN_IDENTITY)"
    fi
}

# --- Helper: build a component .pkg ---
build_component() {
    local flavor="$1"
    local src_path="$2"
    local ident="$3"
    local install_location="$4"

    local pkg_file="${UNDERSCORE_NAME}_${flavor}.pkg"
    local work_dir="$TMPDIR_PKG/$flavor"

    echo "  Building ${pkg_file}..."
    mkdir -p "$work_dir"

    # Copy artifact to staging
    ditto "$src_path" "$work_dir/$(basename "$src_path")"

    # Sign the binary
    sign_binary "$work_dir/$(basename "$src_path")"

    # Build component package
    pkgbuild --root "$work_dir" \
             --identifier "$ident" \
             --version "$APP_VERSION" \
             --install-location "$install_location" \
             "$TMPDIR_PKG/unsigned/$pkg_file"

    # Sign the component package (productsign requires Installer identity)
    if [ -n "$IDENTITY" ]; then
        productsign --sign "$INSTALLER_IDENTITY" "$TMPDIR_PKG/unsigned/$pkg_file" "$TMPDIR_PKG/$pkg_file"
    else
        cp "$TMPDIR_PKG/unsigned/$pkg_file" "$TMPDIR_PKG/$pkg_file"
    fi

    rm -rf "$work_dir"
}

# --- Build component packages ---

VST3_PATH="${ARTIFACTS}/VST3/${APP_NAME}.vst3"
AU_PATH="${ARTIFACTS}/AU/${APP_NAME}.component"
STANDALONE_PATH="${ARTIFACTS}/Standalone/${APP_NAME}.app"

if [[ -d "$VST3_PATH" ]]; then
    build_component "VST3" "$VST3_PATH" "${BUNDLE_ID}.vst3.pkg" "/Library/Audio/Plug-Ins/VST3"
fi

if [[ -d "$AU_PATH" ]]; then
    build_component "AU" "$AU_PATH" "${BUNDLE_ID}.component.pkg" "/Library/Audio/Plug-Ins/Components"
fi

if [[ -d "$STANDALONE_PATH" ]]; then
    build_component "Standalone" "$STANDALONE_PATH" "${BUNDLE_ID}.app.pkg" "/Applications"
fi

# --- Generate distribution.xml ---

# Build XML fragments conditionally
VST3_PKG_REF="" VST3_CHOICE="" VST3_CHOICE_DEF=""
AU_PKG_REF="" AU_CHOICE="" AU_CHOICE_DEF=""
STANDALONE_PKG_REF="" STANDALONE_CHOICE="" STANDALONE_CHOICE_DEF=""

if [[ -d "$VST3_PATH" ]]; then
    VST3_PKG_REF="<pkg-ref id=\"${BUNDLE_ID}.vst3.pkg\"/>"
    VST3_CHOICE="<line choice=\"${BUNDLE_ID}.vst3.pkg\"/>"
    VST3_CHOICE_DEF="<choice id=\"${BUNDLE_ID}.vst3.pkg\" visible=\"true\" start_selected=\"true\" title=\"${APP_NAME} VST3\"><pkg-ref id=\"${BUNDLE_ID}.vst3.pkg\"/></choice><pkg-ref id=\"${BUNDLE_ID}.vst3.pkg\" version=\"${APP_VERSION}\" onConclusion=\"none\">${UNDERSCORE_NAME}_VST3.pkg</pkg-ref>"
fi
if [[ -d "$AU_PATH" ]]; then
    AU_PKG_REF="<pkg-ref id=\"${BUNDLE_ID}.component.pkg\"/>"
    AU_CHOICE="<line choice=\"${BUNDLE_ID}.component.pkg\"/>"
    AU_CHOICE_DEF="<choice id=\"${BUNDLE_ID}.component.pkg\" visible=\"true\" start_selected=\"true\" title=\"${APP_NAME} Audio Unit\"><pkg-ref id=\"${BUNDLE_ID}.component.pkg\"/></choice><pkg-ref id=\"${BUNDLE_ID}.component.pkg\" version=\"${APP_VERSION}\" onConclusion=\"none\">${UNDERSCORE_NAME}_AU.pkg</pkg-ref>"
fi
if [[ -d "$STANDALONE_PATH" ]]; then
    STANDALONE_PKG_REF="<pkg-ref id=\"${BUNDLE_ID}.app.pkg\"/>"
    STANDALONE_CHOICE="<line choice=\"${BUNDLE_ID}.app.pkg\"/>"
    STANDALONE_CHOICE_DEF="<choice id=\"${BUNDLE_ID}.app.pkg\" visible=\"true\" start_selected=\"true\" title=\"${APP_NAME} Standalone App\"><pkg-ref id=\"${BUNDLE_ID}.app.pkg\"/></choice><pkg-ref id=\"${BUNDLE_ID}.app.pkg\" version=\"${APP_VERSION}\" onConclusion=\"none\">${UNDERSCORE_NAME}_Standalone.pkg</pkg-ref>"
fi

cat >"$TMPDIR_PKG/distribution.xml" <<XMLEND
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="1">
    <title>${APP_NAME} ${APP_VERSION}</title>
    ${VST3_PKG_REF}
    ${AU_PKG_REF}
    ${STANDALONE_PKG_REF}
    <options require-scripts="false" rootVolumeOnly="true" customize="always" />
    <choices-outline>
        ${VST3_CHOICE}
        ${AU_CHOICE}
        ${STANDALONE_CHOICE}
    </choices-outline>
    ${VST3_CHOICE_DEF}
    ${AU_CHOICE_DEF}
    ${STANDALONE_CHOICE_DEF}
</installer-gui-script>
XMLEND

# --- Build final product archive ---

echo "  Assembling product archive..."
pushd "$TMPDIR_PKG" > /dev/null
productbuild --distribution "distribution.xml" --package-path "." "${OUTPUT_FILENAME}.pkg"
popd > /dev/null

# Sign the final product archive
if [ -n "$IDENTITY" ]; then
    echo "  Signing product archive..."
    productsign --sign "$INSTALLER_IDENTITY" "$TMPDIR_PKG/${OUTPUT_FILENAME}.pkg" "$PKG_PATH"
    pkgutil --check-signature "$PKG_PATH"
else
    mv "$TMPDIR_PKG/${OUTPUT_FILENAME}.pkg" "$PKG_PATH"
fi

# Clean up
rm -rf "$TMPDIR_PKG"

echo ""
echo "Installer created: ${PKG_PATH}"
