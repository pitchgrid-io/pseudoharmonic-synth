#!/bin/bash
# Icon generation script for PseudoHarmonic
# Generates all required macOS app icon sizes from icon_app.svg

set -e

# Load environment variables
if [ -f .env ]; then
    set -a
    source .env
    set +a
fi

APP_NAME="${APP_NAME:-PseudoHarmonic}"

echo "Generating ${APP_NAME} app icons..."

# Check if rsvg-convert is available
if ! command -v rsvg-convert &> /dev/null; then
    echo "rsvg-convert not found. Installing librsvg..."
    brew install librsvg
fi

# Check if source SVG exists
if [ ! -f "assets/icon_app.svg" ]; then
    echo "assets/icon_app.svg not found. Please create the icon first."
    exit 1
fi

# Create icons directory
mkdir -p icons
echo "Created icons directory"

# Generate all required sizes for macOS app bundle
echo "Generating PNG icons..."

rsvg-convert -h 16 -w 16 assets/icon_app.svg > icons/icon_16x16.png
rsvg-convert -h 32 -w 32 assets/icon_app.svg > icons/icon_16x16@2x.png
rsvg-convert -h 32 -w 32 assets/icon_app.svg > icons/icon_32x32.png
rsvg-convert -h 64 -w 64 assets/icon_app.svg > icons/icon_32x32@2x.png
rsvg-convert -h 128 -w 128 assets/icon_app.svg > icons/icon_128x128.png
rsvg-convert -h 256 -w 256 assets/icon_app.svg > icons/icon_128x128@2x.png
rsvg-convert -h 256 -w 256 assets/icon_app.svg > icons/icon_256x256.png
rsvg-convert -h 512 -w 512 assets/icon_app.svg > icons/icon_256x256@2x.png
rsvg-convert -h 512 -w 512 assets/icon_app.svg > icons/icon_512x512.png
rsvg-convert -h 1024 -w 1024 assets/icon_app.svg > icons/icon_512x512@2x.png

echo "Generated PNG icons:"
ls -la icons/

# Create .icns file for macOS
echo "Creating macOS .icns file..."
if command -v iconutil &> /dev/null; then
    mkdir -p "${APP_NAME}.iconset"

    cp icons/icon_16x16.png "${APP_NAME}.iconset/icon_16x16.png"
    cp icons/icon_16x16@2x.png "${APP_NAME}.iconset/icon_16x16@2x.png"
    cp icons/icon_32x32.png "${APP_NAME}.iconset/icon_32x32.png"
    cp icons/icon_32x32@2x.png "${APP_NAME}.iconset/icon_32x32@2x.png"
    cp icons/icon_128x128.png "${APP_NAME}.iconset/icon_128x128.png"
    cp icons/icon_128x128@2x.png "${APP_NAME}.iconset/icon_128x128@2x.png"
    cp icons/icon_256x256.png "${APP_NAME}.iconset/icon_256x256.png"
    cp icons/icon_256x256@2x.png "${APP_NAME}.iconset/icon_256x256@2x.png"
    cp icons/icon_512x512.png "${APP_NAME}.iconset/icon_512x512.png"
    cp icons/icon_512x512@2x.png "${APP_NAME}.iconset/icon_512x512@2x.png"

    iconutil -c icns "${APP_NAME}.iconset" -o "${APP_NAME}.icns"
    cp "${APP_NAME}.icns" assets/Icon.icns
    rm -rf "${APP_NAME}.iconset"

    echo "Created ${APP_NAME}.icns and assets/Icon.icns"
else
    echo "iconutil not found - .icns file not created"
fi

echo ""
echo "Icon generation complete!"
echo "PNG icons: icons/"
echo "macOS icon: ${APP_NAME}.icns"
