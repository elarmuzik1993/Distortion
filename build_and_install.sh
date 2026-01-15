#!/bin/bash
# Build and install Distortion VST3 plugin
# Usage: ./build_and_install.sh

set -e  # Exit on error

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"
VST3_DEST="$HOME/.vst3"

echo "🔨 Building Distortion VST3..."

# Ensure build directory exists
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure if needed
if [ ! -f "Makefile" ]; then
    echo "📋 Configuring CMake..."
    cmake .. -DCMAKE_BUILD_TYPE=Release
fi

# Force rebuild of source files by touching them
touch "$SCRIPT_DIR/Source/PluginProcessor.cpp"
touch "$SCRIPT_DIR/Source/PluginEditor.cpp"

# Build the VST3 target specifically
cmake --build . --target Distortion_VST3 -j$(nproc)

# Copy to VST3 folder
echo "📦 Installing to $VST3_DEST..."
mkdir -p "$VST3_DEST"
rm -rf "$VST3_DEST/Distortion.vst3"
cp -r "$BUILD_DIR/Distortion_artefacts/Release/VST3/Distortion.vst3" "$VST3_DEST/"

echo "✅ Done! VST3 installed to $VST3_DEST/Distortion.vst3"
echo "   Timestamp: $(date)"
