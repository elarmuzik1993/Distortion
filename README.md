# Sledge Distortion v2.3 - Audio Plugin

A professional JUCE audio plugin featuring multi-stage distortion, LA2A-style compression, LFO modulation routing, and advanced signal processing.

## Project Overview

**Sledge Distortion** - A JUCE audio plugin by Monolit Beatz
- **Version**: 2.3
- **Type**: Audio Plugin — VST3 everywhere; + AU and a Standalone app on macOS
- **Framework**: JUCE 7.0.12
- **Platforms**: Windows, Linux, macOS (universal arm64 + x86_64)
- **Build Systems**: CMake (cross-platform), Visual Studio 2022

## What's New in v2.3

- **macOS support** — universal (arm64 + x86_64) **VST3 + AU + Standalone**, so Logic Pro and GarageBand are now covered; ships as a `.pkg` installer
- **Free-draw Graphic EQ** — draw a curve straight onto the oscilloscope to shape a 12-band peaking EQ (~30 Hz–16 kHz, ±12 dB) at the output stage; automatable, saved with presets, and bit-transparent while flat
- **One-click EQ bypass** — a power button on the overlay mutes the EQ click-free while keeping the drawn curve, for a true A/B
- **Scope overlay selector** — one Settings dropdown (Off / XY Morph / Graphic EQ) plus matching title-bar toggles decides which overlay owns the scope
- **UI texture** — cracked-glass and diamond-plate artwork across the editor, with an EXTREME mode that spreads the shatter over the whole canvas

> Both platforms currently ship **unsigned** — see [Installation](#installation) for the one-time Windows SmartScreen / macOS Gatekeeper steps.

## What's New in v2.2

- **Multimode input filter** — switchable High-Pass / Low-Pass / Band-Pass ahead of the drive
- **LFO upgrades** — tempo **BPM Sync** (SYNC + note-division) and an **INV** polarity invert
- **Linear Phase Dry** — phase-coherent parallel path, no comb filtering at partial mix
- **Clean boost pre-emphasis** toggle in front of the distortion stage
- **XY Morph pad on/off toggle**
- **Anonymous bug reporting (opt-out)** — first-run notice, consent switch, and a "Report a Bug" button in Settings; nothing personal, off anytime
- **UI refresh** — layered-arc rotary knobs with value-driven colour, a 5-LED gain-reduction meter, scrollable settings panel, and compact window fold
- **16 factory presets** (up from 8)
- **Audio-quality fixes** — Sub Guard crossover phase + click-free slope changes, phase-aligned wet/dry mix, corrected LFO rate in the oversampled path, strict -0.5 dBFS output ceiling, latency-compensated true bypass
- **Platform** — VST3 on Windows + Linux (VST2 removed); validated with pluginval (strictness 10). *(macOS + AU arrived in v2.3, above.)*

## Features

### Distortion Engine
- **7 Professional Clip Types**: Brutal Fuzz, Tube Overdrive, Bit Crusher, Tape Saturation, Transformer Saturation, Diode Clipper, Decimator
- **Oversampling**: Polyphase IIR anti-aliasing — selectable Off / 2x / 4x in Settings (4x default)
- **Sub Guard**: Variable-slope crossover (50-200Hz) protects sub-bass from distortion
- **True Bypass**: Zero processing when distortion < 0.5%
- **Dist Mix**: Parallel distortion blending (0-100%)

### Multimode Input Filter
- **High-Pass / Low-Pass / Band-Pass** (SVF TPT) ahead of the drive
- Runs in the true-bypass path too, so it works as a standalone filter with distortion and compression off
- A filter left at its transparent default keeps bypass bit-clean

### Graphic EQ
- **Free-draw curve**: draw a magnitude response directly on the oscilloscope; double-click flattens
- **12 peaking bands**, log-spaced ~30 Hz–16 kHz, ±12 dB, at the output stage
- **Automatable** and saved with presets; bypasses entirely while flat, so a fresh instance is bit-transparent
- **One-click bypass** (power button) mutes it click-free while preserving the drawn curve

### Oscilloscope
- **Zero-crossing trigger**: Waveform locked to rising edge — no horizontal drift
- **Anti-alias decimation**: 2-point averaging filter before scope downsampling
- **Real-time display**: Always shows the most recent audio (FIFO drain on every frame)
- **Lock-free pipeline**: SpinLock removed — pure `AbstractFifo` SPSC, no audio-thread contention
- **Overlay selector**: Off / XY Morph / Graphic EQ decides which overlay owns the scope surface

### LFO Modulation System
- **5 Waveforms**: Sine, Triangle, Square, Saw, Random S&H
- **5 Modulation Destinations**: Distortion Amount, Tone Filter, Hi-Pass, Dist Mix, Output Gain
- **Rate**: 0.1-50Hz free-running, or **BPM Sync** to host tempo (1/1 → 1/32, incl. triplets)
- **INV**: polarity invert, reflected in both DSP and the arc visualiser
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
- DC blocking with manual one-pole filter
- Graphic EQ (post-DC-blocker, pre-output-gain)
- Output limiter (-0.5dBFS safety, always the last gain stage)
- Linear Phase Dry option — phase-coherent parallel path, no comb filtering at partial mix
- Latency reported to the host for PDC, re-imposed on the true-bypass path so toggling causes no timing jump

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

### macOS (CMake)
Builds a universal binary (arm64 + x86_64) with VST3, AU, and Standalone:
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target Distortion_VST3 Distortion_AU Distortion_Standalone -j
```
Add `-DCMAKE_OSX_ARCHITECTURES=arm64` for a faster single-arch dev build.

## Project Structure

```
Distortion/
├── Source/
│   ├── PluginProcessor.cpp/h    # Audio processing (3400+ lines)
│   ├── PluginEditor.cpp/h       # GUI implementation
│   ├── CustomKnob.cpp/h         # Custom rotary controls
│   ├── FactoryPresets.h         # Factory bank (single source of truth)
│   ├── Diagnostics/             # Anonymous bug reporting (opt-out)
│   ├── Tools/                   # Headless harnesses (render, soak, UI snapshot)
│   └── Tests/                   # Unit test suite (2580+ assertions)
├── Resources/                   # Images, textures and fonts
├── installer/                   # Inno Setup (Windows) + .pkg (macOS)
├── CMakeLists.txt              # Cross-platform build
├── Distortion.jucer            # Projucer project
├── AGENTS.md                   # Developer documentation / source of truth
└── .github/workflows/          # CI/CD pipeline
```

## Plugin Formats

- **VST3**: Primary format (all platforms)
- **AU (Audio Unit)**: macOS only — Logic Pro / GarageBand
- **Standalone**: **shipped on macOS only** (in the `.pkg`). The `Distortion_Standalone` target builds on every platform for local testing, but only the `build-macos` CI job packages it — Windows and Linux release artifacts contain the VST3 and nothing else.

## Installation

- **Windows**: run the installer (`SledgeDistortion-<version>-Windows.exe`) — installs to `%CommonProgramFiles%\VST3\` by default, replaces any previous version cleanly, and automatically installs the Microsoft VC++ runtime if the machine is missing it (required on clean Windows 10 installs). The installer is currently **unsigned**, so SmartScreen shows an "unrecognised app" warning on first run — click **More info → Run anyway**.
- **Linux**: unpack the tarball to `~/.vst3/`
- **macOS**: run the `.pkg` installer (VST3 → `/Library/Audio/Plug-Ins/VST3`, AU → `/Library/Audio/Plug-Ins/Components`, Standalone → `/Applications`). The current builds are **unsigned** until an Apple Developer ID is configured — on first launch right-click the `.pkg` → **Open** to bypass Gatekeeper.

### End-user requirements (Windows)

- 64-bit Windows 10 or later (ARM64 Windows works via x64 emulation)
- VC++ 2015–2022 x64 runtime — bundled with the installer; only needed manually for zip installs: https://aka.ms/vs/17/release/vc_redist.x64.exe

## Build requirements

- CMake 3.22+
- C++17 compiler
- JUCE 7.0.12 (auto-fetched by CMake)
- ALSA development libraries (Linux)
- Xcode command-line tools + macOS 11.0 SDK (macOS)

## Privacy & bug reporting

Sledge Distortion can send **anonymous** bug reports (version, OS, host, sample
rate/block size, anomaly counts, a random install ID — no audio, no presets, no
file paths, no personal data). It is **on by default and opt-out**: a one-time
notice explains it on first launch, and you can switch it off any time in
**Settings → "Send anonymous bug reports"**.

Full details — exactly what is collected, when it's sent, and how to turn it off
— are in **[docs/PRIVACY.md](docs/PRIVACY.md)**.

## License

This project is licensed under the [PolyForm Noncommercial License 1.0.0](LICENSE). You may view, fork, and modify the source for **non-commercial** purposes (research, personal study, hobby projects, educational use).

**Commercial use requires a paid license.** For commercial licensing inquiries, contact **elar.muzik@gmail.com**.

Copyright © 2026 Boris Miscenco. All rights reserved.
