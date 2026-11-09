#!/usr/bin/env bash
set -e

# ==============================================================================
# SpectraCross - Native macOS Build Script
# Compiles Universal Binary (Apple Silicon M1/M2/M3/M4 + Intel x86_64)
# Formats: AudioUnit (AU v2 .component) and VST3 (.vst3)
# ==============================================================================

echo "=========================================================="
echo " Building SpectraCross for macOS (AU & VST3 Universal)    "
echo "=========================================================="

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

cd "${ROOT_DIR}"

# 1. Check for CMake
if ! command -v cmake &> /dev/null; then
    echo "Error: CMake is not installed. Install via: brew install cmake"
    exit 1
fi

# 2. Check for JUCE
if [ ! -d "JUCE" ] && [ ! -d "../JUCE" ]; then
    echo "Cloning JUCE 8.0.6..."
    git clone --depth 1 --branch 8.0.6 https://github.com/juce-framework/JUCE.git JUCE
fi

# 3. Configure CMake with Universal Binary support
echo "Configuring CMake for macOS Universal (arm64 + x86_64)..."
cmake -B build_mac -S . \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
    -DCMAKE_OSX_DEPLOYMENT_TARGET="11.0" \
    -G Ninja

# 4. Build Targets
echo "Compiling AU and VST3 plugins..."
cmake --build build_mac --config Release --parallel

echo "Build complete!"
echo "Artefacts generated:"
ls -ld build_mac/SpectraCross_artefacts/AU/SpectraCross.component || true
ls -ld build_mac/SpectraCross_artefacts/VST3/SpectraCross.vst3 || true

# 5. Optional Installation into system plugin directories
echo ""
read -p "Do you want to install plugins into ~/Library/Audio/Plug-Ins? [y/N] " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    mkdir -p ~/Library/Audio/Plug-Ins/Components
    mkdir -p ~/Library/Audio/Plug-Ins/VST3
    
    rm -rf ~/Library/Audio/Plug-Ins/Components/SpectraCross.component
    rm -rf ~/Library/Audio/Plug-Ins/VST3/SpectraCross.vst3
    
    cp -R build_mac/SpectraCross_artefacts/AU/SpectraCross.component ~/Library/Audio/Plug-Ins/Components/
    cp -R build_mac/SpectraCross_artefacts/VST3/SpectraCross.vst3 ~/Library/Audio/Plug-Ins/VST3/
    
    echo "Installed to:"
    echo "  - ~/Library/Audio/Plug-Ins/Components/SpectraCross.component"
    echo "  - ~/Library/Audio/Plug-Ins/VST3/SpectraCross.vst3"
    
    echo "Validating AU with auval..."
    killall -9 AudioComponentRegistrar 2>/dev/null || true
    auval -v aufx SpCr Acro || echo "Run auval manually after restart."
fi

echo "Done!"
