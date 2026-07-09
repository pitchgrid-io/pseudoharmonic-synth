#!/bin/bash
# Development startup script — Standalone debug build + webview hot reload.
#
# Starts the Svelte UI (vite) on an EPHEMERAL port and tells the plugin which
# port to load via PH_DEV_UI_PORT (see Source/PluginEditor.cpp). Debug builds
# load the UI from the vite dev server, so frontend changes hot-reload without
# rebuilding the plugin. The WebSocket backend port is separate and discovered
# by the UI at runtime via the `uiMounted` native function.
#
# Usage: ./run_dev.sh            (native arch Debug build)
#        ARCH=x86_64 ./run_dev.sh

set -e
cd "$(dirname "$0")"

echo "=== PseudoHarmonic Synth — Development Mode ==="
echo ""

ARCH="${ARCH:-$(uname -m)}"
BUILD_DIR="build-debug"

command -v cmake >/dev/null || { echo "Error: cmake not installed (brew install cmake ninja)"; exit 1; }
command -v npm   >/dev/null || { echo "Error: npm not installed"; exit 1; }
command -v python3 >/dev/null || { echo "Error: python3 not installed (needed to pick a free port)"; exit 1; }

# Install UI dependencies if needed
if [ ! -d "ui/node_modules" ]; then
    echo "Installing UI dependencies..."
    (cd ui && npm install)
fi

VITE_PID=""
UI_PORT=""
cleanup() {
    echo ""
    echo "Stopping services..."
    # npm spawns a child vite/node process, so kill children too, then free the
    # port to be sure nothing is left listening.
    if [ -n "$VITE_PID" ]; then
        pkill -P "$VITE_PID" 2>/dev/null || true
        kill "$VITE_PID" 2>/dev/null || true
        wait "$VITE_PID" 2>/dev/null || true
    fi
    if [ -n "$UI_PORT" ]; then
        local pids
        pids=$(lsof -ti ":$UI_PORT" 2>/dev/null || true)
        [ -n "$pids" ] && kill $pids 2>/dev/null || true
    fi
    echo "All services stopped"
}
trap cleanup SIGINT SIGTERM EXIT

# Pick an ephemeral free port for the vite dev server and tell the plugin.
UI_PORT=$(python3 -c "import socket; s=socket.socket(); s.bind(('127.0.0.1',0)); print(s.getsockname()[1]); s.close()")
export PH_DEV_UI_PORT="$UI_PORT"

# Start the vite dev server on the chosen port.
echo "Starting UI dev server on ephemeral port $UI_PORT..."
(cd ui && npm run dev -- --port "$UI_PORT" --strictPort) &
VITE_PID=$!

# Configure + build the Standalone Debug target.
echo ""
echo "Building plugin (Debug, $ARCH)..."
cmake -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Debug -DCMAKE_OSX_ARCHITECTURES="$ARCH" >/dev/null
cmake --build "$BUILD_DIR" --target PseudoHarmonicSynth_Standalone -j

APP="$BUILD_DIR/PseudoHarmonicSynth_artefacts/Debug/Standalone/PseudoHarmonic.app"
[ -d "$APP" ] || { echo "Error: Standalone app not found at $APP"; exit 1; }

# Wait for the dev server to be listening before launching the app.
echo "Waiting for UI dev server..."
for _ in $(seq 1 50); do
    if curl -s "http://localhost:$UI_PORT" >/dev/null 2>&1; then break; fi
    sleep 0.2
done

echo ""
echo "====================================="
echo "  UI Dev Server: http://localhost:$UI_PORT  (PH_DEV_UI_PORT)"
echo "  Standalone:    $APP"
echo "====================================="
echo ""
echo "Launching standalone app (close the app or Ctrl+C to stop everything)..."

# Foreground so Ctrl+C / closing the app triggers cleanup of the dev server.
"$APP/Contents/MacOS/PseudoHarmonic"
