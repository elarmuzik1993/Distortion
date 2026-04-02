# Distortion v1.9 - Audio Plugin

A professional JUCE audio plugin featuring multi-stage distortion, LA2A-style compression, LFO modulation routing, and advanced signal processing.

## Project Overview

**Distortion** - A JUCE audio plugin by Elar Music Audio
- **Version**: 1.9 Oscilloscope Quality
- **Type**: Audio Plugin (VST3, VST2, Standalone)
- **Framework**: JUCE 7.0.12
- **Platforms**: Windows, Linux, macOS
- **Build Systems**: CMake (cross-platform), Visual Studio 2022

## Features

### Distortion Engine
- **7 Professional Clip Types**: Brutal Fuzz, Tube Overdrive, Bit Crusher, Tape Saturation, Transformer Saturation, Diode Clipper, Decimator
- **4x Oversampling**: Polyphase IIR anti-aliasing
- **Sub Guard**: Variable-slope crossover (50-200Hz) protects sub-bass from distortion
- **True Bypass**: Zero processing when distortion < 0.5%
- **Dist Mix**: Parallel distortion blending (0-100%)

### Oscilloscope (v1.9)
- **Zero-crossing trigger**: Waveform locked to rising edge — no horizontal drift
- **Anti-alias decimation**: 2-point averaging filter before scope downsampling
- **Real-time display**: Always shows the most recent audio (FIFO drain on every frame)
- **Lock-free pipeline**: SpinLock removed — pure `AbstractFifo` SPSC, no audio-thread contention

### LFO Modulation System (v1.8)
- **5 Waveforms**: Sine, Triangle, Square, Saw, Random S&H
- **5 Modulation Destinations**: Distortion Amount, Tone Filter, Hi-Pass, Dist Mix, Output Gain
- **Rate**: 0.1-50Hz
- **Visual Feedback**: Pulsing cyan glow on modulated knobs

### LA2A-Style Compression
- Optical cell envelope simulation
- Program-dependent behavior with RMS tracking
- 3:1 or 12:1 ratio modes
- 15% tube harmonics for analog warmth

### Signal Processing
- Pre-distortion transient tamer (hardcoded compression)
- Harmonic density scaling (prevents 2-5kHz harshness)
- Auto-gain compensation (RMS-based loudness maintenance)
- ISP protection (soft clipper at -0.3dBFS)
- Output limiter (-0.5dBFS safety)
- DC blocking with manual one-pole filter

## Build Instructions

### CMake (Recommended)
```bash
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)
```

### Run Tests
```bash
./DistortionTests_artefacts/Debug/DistortionTests
```

### Visual Studio 2022 (Windows)
```bash
cd Builds/VisualStudio2022
MSBuild Distortion.sln -p:Configuration=Release -p:Platform=x64
```

## Project Structure

```
Distortion/
├── Source/
│   ├── PluginProcessor.cpp/h    # Audio processing (2200+ lines)
│   ├── PluginEditor.cpp/h       # GUI implementation
│   ├── CustomKnob.cpp/h         # Custom rotary controls
│   └── Tests/                   # Unit test suite (2000+ assertions)
├── Resources/                   # Images and assets
├── CMakeLists.txt              # Cross-platform build
├── Distortion.jucer            # Projucer project
├── CLAUDE.md                   # Developer documentation
└── .github/workflows/          # CI/CD pipeline
```

## Plugin Formats

- **VST3**: Primary format
- **VST2**: Legacy DAW support
- **Standalone**: Testing without DAW

## Installation

- **Linux**: `~/.vst3/`
- **macOS**: `~/Library/Audio/Plug-Ins/VST3/`
- **Windows**: `%CommonProgramFiles%\VST3\`

## Requirements

- CMake 3.22+
- C++17 compiler
- JUCE 7.0.12 (auto-fetched by CMake)
- ALSA development libraries (Linux)

## License

Elar Music Audio
