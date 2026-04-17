#!/bin/bash
# Build PseudoHarmonic synth plugin for macOS
# Usage: ./build_app.sh [--arch arm64|x86_64|universal]

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
            echo "Usage: ./build_app.sh [--arch arm64|x86_64|universal]"
            exit 1
            ;;
    esac
done

# Default to native architecture
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

echo "Building ${APP_NAME} v${APP_VERSION} for ${ARCH}..."

# Set CMake architecture flag
if [ "$ARCH" = "universal" ]; then
    CMAKE_ARCH_FLAG="-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64"
else
    CMAKE_ARCH_FLAG="-DCMAKE_OSX_ARCHITECTURES=${ARCH}"
fi

BUILD_DIR="build-release-${ARCH}"

# Configure
echo "Configuring CMake..."
cmake -B "${BUILD_DIR}" \
    -DCMAKE_BUILD_TYPE=Release \
    ${CMAKE_ARCH_FLAG}

# Build
echo "Building..."
cmake --build "${BUILD_DIR}" --config Release -j

echo ""
echo "Build complete!"
echo "Artifacts in: ${BUILD_DIR}/PseudoHarmonicSynth_artefacts/Release/"
ls -la "${BUILD_DIR}/PseudoHarmonicSynth_artefacts/Release/"
