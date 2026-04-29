# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**Monolit Distortion** is a professional JUCE audio plugin by Boris Miscenco (Monolit Beats) featuring multi-stage distortion processing, LA2A-style optical compression, and advanced signal processing. The plugin supports VST3, VST2, and Standalone formats.

## Build System

This is a JUCE project supporting both **CMake (Linux/Mac/Windows)** and **Projucer (Windows VS2022)**.

### GitHub Actions CI/CD (Recommended)

The easiest way to build for both Windows and Linux is using GitHub Actions. Push code to GitHub and it automatically builds release artifacts.

**Automatic builds trigger on:**
- Push to `main` or `develop` branches
- Pull requests to `main`
- Manual trigger via GitHub UI
- Tag pushes (creates a GitHub Release)

**Download built plugins:**
1. Go to repository → Actions tab
2. Click latest successful workflow run
3. Download artifacts: `Distortion-VST3-Windows` or `Distortion-VST3-Linux`

**Create a release:**
```bash
git tag v1.0.0
git push origin v1.0.0
```
This creates a GitHub Release with Windows and Linux ZIP files.

### CMake Build (Recommended for Linux/Cross-platform)

**Setup:**
```bash
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
```

**Build Commands:**
```bash
# Build plugin (VST3 + Standalone) — build BOTH targets to get fresh binaries
cmake --build . --target Distortion_Standalone --target Distortion_VST3 -j$(nproc)

# Build test runner
cmake --build . --target DistortionTests -j$(nproc)

# Run tests
./DistortionTests_artefacts/Debug/DistortionTests

# Launch standalone
./Distortion_artefacts/Debug/Standalone/Distortion
```

**IMPORTANT:** Do NOT use `--target Distortion` — that only builds the shared library and will NOT relink the standalone executable. Always use `--target Distortion_Standalone` to ensure the latest code is in the binary.

**Notes:**
- CMake will automatically fetch JUCE 7.0.12 from GitHub if not found locally
- To use local JUCE: `cmake .. -DJUCE_PATH=/path/to/JUCE`
- Requires: CMake 3.22+, C++17 compiler, Python 3, ALSA development libraries (Linux)
- Post-build automatically fixes moduleinfo.json (see below)

### Projucer Build (Windows Visual Studio 2022)

**Build Commands:**
```bash
# Build entire solution (VST3 + Standalone)
cd "Builds/VisualStudio2022"
"C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" Distortion.sln -p:Configuration=Debug -p:Platform=x64 -m -v:minimal

# Build only VST3
"C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" Distortion_VST3.vcxproj -p:Configuration=Debug -p:Platform=x64 -v:minimal

# Build test runner
"C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" DistortionTests_ConsoleApp.vcxproj -p:Configuration=Debug -p:Platform=x64
```

**Build Outputs:**
- Debug VST3: `C:\Program Files\Common Files\VST3\Distortion.vst3`
- Debug Standalone: `Builds\VisualStudio2022\x64\Debug\Standalone Plugin\Distortion.exe`
- Release VST3: `%CommonProgramW6432%\VST3`
- Test Runner: `Builds\VisualStudio2022_Tests\x64\Debug\ConsoleApp\DistortionTests.exe`

**Important**: If you get "Sharing violation" errors during build, the DAW still has the VST3 loaded. Close the DAW completely before rebuilding.

**Project Configuration:**
- Edit project settings via Projucer: Open `Distortion.jucer` in Projucer
- After making changes in Projucer, save and it will regenerate the Visual Studio project files
- JUCE modules path: `../../../Documents/JUCE/modules` (relative to project)

### VST3 moduleinfo.json Fix (Post-Build)

JUCE's VST3 manifest helper generates `moduleinfo.json` with **invalid JSON** (trailing commas). This causes some DAWs (Ableton, FL Studio, Cubase, etc.) to reject the plugin during validation.

**Solution**: A post-build script (`scripts/fix_moduleinfo_json.py`) automatically fixes the JSON after each build.

**Manual Usage:**
```bash
# Fix a specific VST3 bundle
python scripts/fix_moduleinfo_json.py "C:\Program Files\Common Files\VST3\Distortion.vst3"

# Fix all moduleinfo.json in a build directory
python scripts/fix_moduleinfo_json.py build --all
```

**Troubleshooting DAW loading issues:**
1. Close the DAW completely
2. Run the fix script on the installed VST3
3. Clear DAW's plugin cache (varies by DAW)
4. Restart DAW and rescan plugins

## Architecture

### Signal Flow Overview

The plugin processes audio through a carefully ordered chain:

1. **Input Stage** → Pre Hi-Pass Filter (removes DC and low-frequency rumble)
2. **Oversampling** → 4x oversampling applied for distortion processing (polyphase IIR, 2 stages)
3. **Pre-Distortion Compression** → Hardcoded transient tamer (1ms attack, 50ms release, 2.5:1 ratio at -12dB threshold)
4. **Sub Guard Band-Split (Optional)** → Variable-slope crossover (50-200Hz) protects sub-bass from distortion
5. **Distortion Stage** → Applied to high band only (if Sub Guard enabled) or full signal
6. **Tone Filter & Waveshaper (Flexible Order)** → Order controlled by "Clean Mode" toggle:
   - **Clean Mode (ON)**: Waveshaper → Tone Filter (reduces aliasing, smoother analog character)
   - **Gritty Mode (OFF)**: Tone Filter → Waveshaper (original order, edgier digital character)
7. **Auto-Gain Compensation** → Maintains consistent perceived loudness (RMS-based, ±12dB range)
8. **Soft Clipper (ISP Protection)** → Prevents inter-sample peaks at -0.3dBFS before downsampling
9. **Downsampling** → Return to original sample rate
10. **LA2A Compression** → LA2A-style optical compressor (full-band processing)
11. **DC Blocking** → Manual one-pole DC blocker (~35Hz cutoff, R=0.995)
12. **Output Limiter** → Final stereo-linked safety limiter (-0.5dBFS, 0.5ms attack, 50ms release)
13. **Output Stage** → Final gain staging (±9dB; formula `(param-50)*0.18` dB at knob 0–100)

### Core Processing Components

**PluginProcessor** (`Source/PluginProcessor.cpp/h`)
- Central audio processing engine
- Manages all DSP state and parameter routing
- Key methods:
  - `processBlock()`: Main audio processing loop
  - `prepareToPlay()`: Initializes DSP components and buffers
  - `applyLA2ACompression()`: LA2A optical cell simulation
  - `fillScopeBuffer()`: Thread-safe scope data for oscilloscope
  - `pushSampleToScope()`: Adds samples to circular FIFO for visualization

**DSP Architecture:**
- Uses `juce::dsp::Oversampling<float>` for anti-aliasing during distortion
- All filters implemented via `juce::dsp::ProcessorDuplicator` with IIR coefficients
- Band-split processing uses parallel buffers: `lowBandBuffer`, `highBandBuffer`, `compLowBandBuffer`, `compHighBandBuffer`
- Smoothed parameters prevent zipper noise: `juce::SmoothedValue<float>`

**Parameter Management:**
- All parameters managed via `juce::AudioProcessorValueTreeState`
- Parameters accessed via atomic pointers (e.g., `inputGainParam`, `distortionAmountParam`)
- Parameter layout created in `createParameterLayout()` static method
- Important: All parameter pointers verified with `jassert` in constructor

### Pre-Distortion Transient Tamer

A hardcoded, always-on compression stage that precedes the distortion algorithm to tame transients and produce richer, more consistent distortion:

**Specifications**:
- **Location**: In oversampled domain, after pre-highpass filter but before band-split and distortion
- **Attack**: 1ms (catches transients rapidly)
- **Release**: 50ms (fast, preserves punch and dynamics)
- **Ratio**: 2.5:1 (light compression, not aggressive)
- **Threshold**: -12dB (catches peaks without over-compressing)
- **Knee**: 6dB soft knee (transparent, no pumping)
- **Per-channel**: Independent envelopes for stereo imaging preservation

**State Variables** (`PluginProcessor` private):
- `preCompEnvelope[2]`: Per-channel gain reduction state (1.0 = no compression)
- `preCompAttackCoeff`: Sample-rate-dependent attack coefficient (calculated in oversampled domain)
- `preCompReleaseCoeff`: Sample-rate-dependent release coefficient (calculated in oversampled domain)

**DSP Constants** (PluginProcessor.h:69-74):
- `PRE_COMP_ATTACK_TIME_S = 0.001f` (1ms)
- `PRE_COMP_RELEASE_TIME_S = 0.050f` (50ms)
- `PRE_COMP_THRESHOLD_DB = -12.0f`
- `PRE_COMP_RATIO = 2.5f`
- `PRE_COMP_KNEE_DB = 6.0f`

**Algorithm**:
1. Absolute value of input for level detection
2. Soft-knee gain reduction calculation (6dB transition zone)
3. One-pole filter envelope follower (asymmetric attack/release)
4. Multiply input sample by envelope state

**True Bypass**: Automatically bypassed when distortion < 0.5% (respects plugin's true bypass mode)

### LA2A Compression Simulation

The compressor simulates a Teletronix LA-2A optical cell (T4 cell) with:
- **Envelope follower**: Attack coefficient 0.9995 (~10ms), Release 0.99995 (~500ms)
- **Program-dependent behavior**: RMS history tracking for adaptive response
- **Soft knee**: 2dB knee width for smooth compression onset
- **Tube harmonics**: 15% blend with 1.5x drive for analog warmth
- **Full-band processing**: Simple compression without crossover (crossover was removed)

**State Variables** (`PluginProcessor` private):
- `compEnvelopeState`: Optical cell charge/discharge state
- `compRmsHistory`: Program-dependent RMS tracking
- `smoothedGainReduction`: Visual smoothing for gain reduction meter (500ms)
- `tubeWarmth`: Tube harmonic state accumulator

### GUI Architecture

**PluginEditor** (`Source/PluginEditor.cpp/h`)
- Custom rotary knobs via `CustomKnob` component
- Real-time oscilloscope with dual-channel display
- Gain reduction meter with visual feedback
- All controls use JUCE's attachment system for parameter binding

**Custom Components:**
- **Oscilloscope**: Timer-based (30Hz), thread-safe buffer access via `juce::SpinLock`, dual-channel waveform with glow effects (6px/4px/2px stroke widths)
- **GainReductionMeter**: Timer-based (30Hz), displays 0-20dB range with gradient (green→yellow→red), reads from `processor.currentGainReductionDB` atomic
- **CheckboxLookAndFeel**: Solid black boxes with green tick indicators
- **CustomKnob**: Professional rotary controls (`Source/CustomKnob.cpp/h`)

**GUI Layout:**
- Top section: Compression controls (Peak Reduction, Makeup Gain, Wet/Dry, Crossover, Mode dropdown, COMP toggle, Gain Reduction Meter)
- Middle section: Oscilloscope display (real-time dual-channel waveform)
- Bottom section: 6 main knobs (Input Gain, Hi-Pass Filter, Distortion Amount, Output Gain, LFO Rate, LFO Depth)
- Middle row controls: Sub Guard knob (crossover frequency), Clip Type dropdown, **Dist Mix knob** (50x50px, positioned between Distortion Amount and Output Gain)

**Thread Safety:**
- Scope buffer uses `juce::AbstractFifo` + `juce::SpinLock` for lock-free audio→GUI transfer
- Gain reduction uses `std::atomic<float>` for thread-safe reading
- Oscilloscope timer pauses during resize to prevent buffer access conflicts

### DSP Constants

All processing constants centralized in `DSPConstants` namespace (PluginProcessor.h:18-84):
- **Oversampling**: 4x factor, 2 polyphase IIR stages
- **Pre-Distortion Compression**: Attack 1ms, Release 50ms, 2.5:1 ratio at -12dB, 6dB knee
- **Harmonic Density Scaling**: Attack 2ms, Release 30ms, 10% minimum scale (prevents 2-5kHz harshness)
- **LA2A Compression**: Attack 10ms, Release 500ms, 3:1 or 12:1 ratio, 2dB knee (full-band, no crossover)
- **ISP Protection**: Soft clipper at -0.3dBFS with 0.5dB knee (prevents inter-sample peaks during downsampling)
- **Auto-Gain Compensation**: Attack 5ms, Release 100ms, range -20dB to +12dB (maintains perceived loudness)
- **Output Limiter**: -0.5dBFS threshold, 0.5ms attack, 50ms release, 1dB knee (final stereo-linked safety)
- **Sub Guard Crossover**: 50-200Hz adjustable (0=OFF), snap points at 60/100/150Hz with variable slopes (LR12/18/24)
- **Distortion Scaling**: Input scale 0.6×, Drive scale 1.2× (pre-algorithm gain staging)
- **DC Blocking**: 20Hz high-pass cutoff for stable, low-phase-shift DC removal
- **Scope/Meter Configuration**: Buffer sizes, refresh rates, decimation factors
- **Parameter Smoothing**: Gain (20ms), Distortion (150ms), Compression GR (500ms)

**Important**: When modifying DSP behavior, always update constants in this namespace rather than hardcoding values.

## Key Implementation Details

### Studio Distortion Processing

**7 Professional Clip Types** (implemented in `applyStudioDistortion()`):
- **Brutal Fuzz** (0): Aggressive hard clipping with input-dependent analog noise (silent on silence, warm with signal)
- **Tube Overdrive** (1): Asymmetric tube saturation with even/odd harmonics
- **Bit Crusher** (2): Digital bit reduction with aliasing character
- **Tape Saturation** (3): Analog tape with magnetic hysteresis simulation
- **Transformer Saturation** (4): Rich harmonic distortion with multi-stage waveshaping
- **Diode Clipper** (5): Asymmetric diode clipping with crossover distortion
- **Decimator** (6): Extreme digital destruction with sample foldback

**True Bypass Architecture**:
- When `distortion < 0.5%` AND `compression OFF`: **Complete bypass mode**
- Skips ALL processing: oversampling, filters, DC blocking
- Zero phase distortion, perfect level matching
- Only applies output gain for volume control
- **Oscilloscope continues updating** even in bypass mode (prevents waveform freeze)
- Critical for transparent operation and phase coherence testing

**Processing Chain (when active)**:
1. Pre-highpass filter (20Hz default, user adjustable 20-500Hz)
2. 4x oversampling (polyphase IIR)
3. Studio distortion algorithms (7 clip types)
4. DC blocking (custom first-order, R=0.999, ~1Hz cutoff)
5. **Wet/Dry mix** (`distMix` parameter 0-100% for parallel distortion blending)
6. Downsampling
7. Additional DC blocking stages (5Hz IIR filters)

**Sub Guard Mode** (Variable-Slope Band-Split):
- Replaces old 808-Safe toggle with continuous frequency control
- Crossover range: 0 Hz (OFF) to 200 Hz, default OFF
- **Variable filter slopes with snap points**:
  - 60 Hz ± 8 Hz → LR24 (steepest, maximum sub protection)
  - 100 Hz ± 8 Hz → LR18 (balanced)
  - 150 Hz ± 8 Hz → LR12 (gentle slope)
- 10ms crossfade when switching filter orders (prevents clicks)
- 50ms frequency smoothing for automation
- Low band: **Bypasses distortion entirely** (preserves kick/808 bass)
- High band: Full distortion processing
- Prevents bass frequency aliasing and maintains sub-bass integrity

**Gain Staging**:
- Input gain: Unity (1.0) at default 50, range 0-2.83 via `pow(param/50, 1.5)`
- Distortion drive: 1.0-4.0 range for controlled saturation (reduced from 1.0-8.0 for more gradual response at low percentages)
- Output gain: ±9dB range around unity (knob 0–100 mapped as `(param-50)*0.18` dB)

### Filter Coefficient Caching

The hi-pass filter caches the last frequency to avoid redundant coefficient recalculations:
```cpp
float lastHighPassFreq = -1.0f;  // PluginProcessor.h:199
```
When implementing new filters, follow this pattern to reduce CPU usage.

### Scope Buffer Management

The oscilloscope uses a two-buffer system:
1. **Audio thread**: Writes to `scopeBuffer` (circular FIFO, size 2048)
2. **GUI thread**: Reads from `cachedBuffer` (display buffer, 512 points) via `fillScopeBuffer()`

Decimation factor of 2 reduces CPU load. Update rate: 30Hz.

### Parameter IDs

When adding new parameters, use these existing patterns:
- Gains: `"inputGain"`, `"outputGain"`
- Distortion: `"distortionAmount"`, `"clipType"`, `"distMix"`, `"waveshaperClean"`, `"waveshaperMix"`
- Filters: `"highPassFreq"`, `"tone"` (2000-20000Hz post-distortion lowpass), `"subGuardFreq"` (0-200Hz crossover)
- LFO: `"lfoRate"`, `"lfoDepth"`, `"lfoWaveform"`, `"lfoEnabled"`, `"lfoDestination"`
- Compression: `"compPeakReduction"`, `"compMakeupGain"`, `"compRatio"`, `"compEnabled"`

### Critical Processing Loop Patterns

**Smoothed Parameter Consumption**:
```cpp
// CORRECT: Sample-first loop for proper smoothed value consumption
for (size_t sample = 0; sample < numSamples; ++sample)
{
    const float currentGain = smoothedGain.getNextValue();  // Once per sample
    for (size_t channel = 0; channel < numChannels; ++channel)
    {
        // Use currentGain for all channels
    }
}

// WRONG: Channel-first causes parameter buffer exhaustion
for (size_t channel = 0; channel < numChannels; ++channel)
{
    for (size_t sample = 0; sample < numSamples; ++sample)
    {
        smoothedGain.getNextValue();  // Called 2x per sample in stereo!
    }
}
```

**Sample Rate Context**:
- Distortion processing occurs in **oversampled domain** (4x sample rate)
- Filter cutoff calculations must use `currentSampleRate * oversamplingFactor`
- DC blocking and compression work at **normal sample rate** (after downsampling)
- One-pole filters were removed due to phase issues; use JUCE IIR filters instead

## Development Workflow

1. **Modifying Parameters**: Edit `createParameterLayout()` in PluginProcessor.cpp, add atomic pointer in header, initialize in constructor
2. **Modifying DSP**: Update constants in `DSPConstants` namespace first, then implement in `processBlock()`
3. **Adding UI Controls**: Create component in PluginEditor.h, initialize in constructor, add attachment, position in `resized()`
4. **Regenerating Build Files**: Open `Distortion.jucer` in Projucer, modify settings, save to regenerate Visual Studio project

## Plugin Formats

- **VST3**: Primary plugin format (builds to `C:\Program Files\Common Files\VST3`)
- **VST2**: Legacy plugin format for older DAW compatibility
- **Standalone**: Standalone application for testing without a DAW

## Recent Architectural Changes

See [CHANGELOG.md](CHANGELOG.md) for the full development history (v2.1 → initial release, including NaN investigation).

## Testing Framework

**Test Infrastructure**:
- **Framework**: JUCE UnitTest runner
- **Test Categories**: DSP, Compression, LFO, ProcessBlock, Parameters, SampleRate, ThreadSafety, StateIO, GoldenAudio
- **Run Tests** (CMake): `./DistortionTests_artefacts/Debug/DistortionTests`
- **Run Tests** (Visual Studio): `Builds/VisualStudio2022_Tests/x64/Debug/ConsoleApp/DistortionTests.exe`

**Test Coverage**:
- **Distortion DSP Tests**: All 7 clip types tested with various input levels (1,330+ assertions)
- **LA2A Compression Tests**: Threshold mapping, ratio modes, makeup gain, envelope follower, gain reduction meter
- **Pre-Distortion Compression Tests** (NEW): Transient reduction, bypass behavior, envelope attack/release, stereo independence, edge cases, gain reduction range (47 assertions)
- **LFO Tests**: All 5 waveform types with output range validation
- **ProcessBlock Integration**: True bypass, Sub Guard mode, oversampling, DC blocking, wet/dry mix
- **Parameter Tests**: Range validation, defaults, smoothing, pointer validity
- **Sample Rate Tests**: Multi-rate support (44.1k, 48k, 88.2k, 96k, 192k)
- **Thread Safety Tests**: Scope buffer access, atomic gain reduction, multi-instance independence
- **State I/O Tests**: Parameter serialization and deserialization
- **Golden Audio Tests**: Reference file comparison

**Current Test Status**: **2000 total assertions - 100% PASS RATE**

