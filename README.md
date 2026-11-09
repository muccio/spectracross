# SpectraCross 🎛️
### Surgical Multi-Instance Spectral Analyzer & Masking Engine
*Built with JUCE 8 & modern C++20 for macOS (AU & VST3) and Windows (VST3)*

[![Build SpectraCross (macOS & Windows)](https://github.com/muccio/spectracross/actions/workflows/build.yml/badge.svg)](https://github.com/muccio/spectracross/actions/workflows/build.yml)
[![JUCE 8](https://img.shields.io/badge/JUCE-8.0.6-orange.svg)](https://juce.com/)
[![C++20](https://img.shields.io/badge/Standard-C%2B%2B20-blue.svg)](https://isocpp.org/)
[![Platform](https://img.shields.io/badge/Platform-macOS%20%7C%20Windows%20%7C%20Linux-lightgrey.svg)]()

SpectraCross is a high-performance, cross-platform audio plugin engineered for precision mix balance, frequency unmasking, and critical monitoring across multiple DAW tracks.

---

## 🌟 Key Features

### 1. DSP Engine & Mathematical Pipeline
- **Lock-Free Audio FIFO**: Single-Producer Single-Consumer (SPSC) circular FIFO via `juce::AbstractFifo` bridging real-time `processBlock()` and background DSP worker thread with **zero heap allocations, zero mutex locks, and zero syscalls**.
- **Summation Modes**: Stereo (L/R), Mono, Mid, Side, and Max-Peak stereo summation.
- **High-Resolution FFT**: 2048, 4096 (default), and 8192 points via `juce::dsp::FFT` with a 75% overlap factor.
- **Coherent Gain Normalization**: Blackman-Harris 7-term and Hann windows calibrated so 0 dBFS sine waves measure exact 0.0 dBFS.
- **Tilt EQ Slopes**: Flat (0 dB), +3.0 dB/oct, +4.5 dB/oct (psychoacoustic pinking slope), and +6.0 dB/oct.
- **Spectrum Ballistics**: Configurable Attack (0–100 ms) and Decay (50–3000 ms) in dB/s, plus true Peak & Hold buffers with selectable hold times (0.5s to infinite).

### 2. Psychoacoustic Masking Engine
- **Traunmüller Critical Bands**: 24 Bark critical band mapping:
  $$z = 26.81 \cdot \frac{f}{1960 + f} - 0.53$$
- **Excitation Spreading Function**: Steep +27 dB/Bark upward slope and level-dependent high-frequency slope.
- **Inter-Track SMR Collision Profiler**: Real-time Signal-to-Mask Ratio analysis against remote tracks with user-adjustable threshold (-12 dB to +6 dB) and quantified "Collision %" badges.

### 3. Inter-Instance Communication Hub
- **Lock-Free In-Process Registry**: Central `InstanceHub` using Read-Copy-Update (RCU) snapshots via `std::atomic<std::shared_ptr<const InstanceSnapshotMap>>`.
- Downsampled 512 log-spaced display points published at 60 FPS without memory bandwidth flooding.
- Automatic stale-instance pruning when tracks are removed or bypassed.

### 4. Modern FabFilter Pro-Q Style UI
- **Logarithmic Frequency Grid**: 10 Hz to 30 kHz with primary grid lines (20, 50, 100, 200, 500, 1k, 2k, 5k, 10k, 20k Hz) and configurable dB ranges (48, 72, 96 dB).
- **7 Problematic Frequency Band Overlays ("Mix Zones")**: Sub/Infrasound, Bottom/Low End, Warmth/Mud, Honk/Boxiness, Harshness/Bite, Sibilance, and Hiss/Air.
- **Piano Roll & Crosshair**: C0 to B8 mini piano roll with musical note name, cents deviation, and exact frequency readout.
- **Collision Visualizer**: Real-time glowing red-orange pulse alert fill in overlapping masked regions.

---

## 🚀 Building from Source

### macOS (AU & VST3 Universal Binary)
```bash
git clone --recurse-submodules https://github.com/muccio/spectracross.git
cd spectracross
chmod +x scripts/build_macos.sh
./scripts/build_macos.sh
```

### Windows (VST3 x64 via Visual Studio 2022)
Open PowerShell:
```powershell
git clone --recurse-submodules https://github.com/muccio/spectracross.git
cd spectracross
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass
.\scripts\build_windows.ps1
```

---

## 📦 Automated GitHub Actions Builds
Every commit to `main` triggers automated builds producing:
- `SpectraCross-macOS-Universal.zip` (AU `.component` and VST3 `.vst3` for Apple Silicon & Intel)
- `SpectraCross-Windows-x64.zip` (VST3 `.vst3` 64-bit)

Download the latest pre-compiled binaries from the **Actions** tab artifacts!
