# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**Distortion** is a professional JUCE audio plugin by Elar Music Audio featuring multi-stage distortion processing, LA2A-style optical compression, and advanced signal processing. The plugin supports VST3, VST2, and Standalone formats.

## Build System

This is a JUCE project supporting both **CMake (Linux/Mac/Windows)** and **Projucer (Windows VS2022)**.

### CMake Build (Recommended for Linux/Cross-platform)

**Setup:**
```bash
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
```

**Build Commands:**
```bash
# Build plugin (VST3 + Standalone)
cmake --build . --target Distortion -j$(nproc)

# Build test runner
cmake --build . --target DistortionTests -j$(nproc)

# Run tests
./DistortionTests_artefacts/Debug/DistortionTests
```

**Notes:**
- CMake will automatically fetch JUCE 7.0.12 from GitHub if not found locally
- To use local JUCE: `cmake .. -DJUCE_PATH=/path/to/JUCE`
- Requires: CMake 3.22+, C++17 compiler, ALSA development libraries (Linux)

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
13. **Output Stage** → Final gain staging (±12dB)

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
- Output gain: ±12dB range around unity

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
- LFO: `"lfoRate"`, `"lfoDepth"`, `"lfoWaveform"`
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

**Current Session Changes (Soft Drive - Reduced Internal Drive Scaling)**:
- **Reduced Main Drive Range**: Changed distortion drive formula from 1.0-8.0 to 1.0-4.0 range for more gradual response at low percentages
  - Line 899: `* 7.0f` → `* 3.0f` in main processBlock calculation
  - Line 443: `* 7.0f` → `* 3.0f` in prepareToPlay initialization
- **Reduced Per-Algorithm Multipliers (50% reduction)**: All 7 clip types now have halved internal drive multipliers for less aggressive distortion at low settings
  - Brutal Fuzz: 4.5f → 2.25f
  - Tube Overdrive: 2.8f → 1.4f
  - Bit Crusher: 3.2f → 1.6f
  - Tape Saturation: 2.5f → 1.25f
  - Transformer Saturation: 3.0f → 1.5f
  - Diode Clipper: 3.8f → 1.9f
  - Decimator: 5.0f → 2.5f
- **Effective Drive Summary**:
  - 10% distortion: was 1.7× algo mult, now 1.3× algo mult
  - 50% distortion: was 4.5× algo mult, now 2.5× algo mult
  - 100% distortion: was 8.0× algo mult, now 4.0× algo mult
- **Version**: Updated to v1.6 Soft Drive
- **Tests**: All 2000 assertions pass (100% success rate)

**Previous Session Changes (Sub Guard & Output Limiter)**:
- **Sub Guard Variable-Slope Crossover**: Replaces old 808-Safe toggle with continuous frequency control
  - Range: 0 Hz (OFF) to 200 Hz with snap points at 60/100/150 Hz
  - Variable filter slopes: LR24 (60Hz), LR18 (100Hz), LR12 (150Hz)
  - 10ms crossfade for order transitions, 50ms frequency smoothing
  - **Parameter**: `"subGuardFreq"` (float, 0-200Hz, default 0=OFF)
  - **DSP Constants** (PluginProcessor.h:24-34):
    - `SUBGUARD_FREQ_MIN = 50.0f`, `SUBGUARD_FREQ_MAX = 200.0f`
    - Snap tolerances and crossfade times
- **Output Limiter**: Final stereo-linked safety limiter (always-on)
  - Prevents digital overs at final output stage
  - **DSP Constants** (PluginProcessor.h:101-105):
    - `OUTPUT_LIMITER_THRESHOLD_DB = -0.5f`
    - `OUTPUT_LIMITER_ATTACK_TIME_S = 0.0005f` (0.5ms)
    - `OUTPUT_LIMITER_RELEASE_TIME_S = 0.050f` (50ms)
    - `OUTPUT_LIMITER_KNEE_DB = 1.0f`
  - **State Variables**: `outputLimiterEnvelope`, `outputLimiterAttackCoeff`, `outputLimiterReleaseCoeff`
- **Tone Filter**: Post-distortion lowpass for brightness/darkness control
  - **Parameter**: `"tone"` (float, 2000-20000Hz, default 20kHz = bright/bypass)
- **Waveshaper Mix**: Parallel waveshaper blend control
  - **Parameter**: `"waveshaperMix"` (float, 0-100%, default 0%)

**Previous Session Changes (Auto-Gain Compensation & ISP Protection)**:
- **Auto-Gain Compensation**: Maintains consistent perceived loudness as distortion amount changes
  - Measures input/output RMS with envelope followers (5ms attack, 100ms release)
  - Calculates compensation ratio: `inputRMS / outputRMS`
  - Limits compensation to -20dB to +12dB range to prevent extreme adjustments
  - Applied in oversampled domain after distortion processing
  - **DSP Constants** (PluginProcessor.h:95-99):
    - `AUTO_GAIN_ATTACK_TIME_S = 0.005f` (5ms)
    - `AUTO_GAIN_RELEASE_TIME_S = 0.100f` (100ms)
    - `AUTO_GAIN_MIN = 0.1f` (-20dB)
    - `AUTO_GAIN_MAX = 4.0f` (+12dB)
  - **State Variables**: `autoGainInputEnvelope`, `autoGainOutputEnvelope`, `autoGainCompensation`
- **Soft Clipper (ISP Protection)**: Prevents inter-sample peaks before downsampling
  - Applied at -0.3dBFS with 0.5dB soft knee
  - Prevents digital overs that could occur during sample rate conversion
  - **DSP Constants** (PluginProcessor.h:84-86):
    - `SOFT_CLIP_THRESHOLD_DB = -0.3f`
    - `SOFT_CLIP_KNEE_DB = 0.5f`
- **Input-Dependent Analog Noise (Brutal Fuzz)**: Noise scales with input level for realistic analog behavior
  - Silent when input is silent (biggest improvement for silence)
  - Formula: `noiseAmount = min(abs(x) * 0.01, 0.002)` - caps at 0.2% noise
  - Provides warm analog character only when signal is present

**Previous Session Changes (Real-Time Audio Safety Audit & Fixes)**:
- **Critical RT-Safety Improvements**: Comprehensive audit against `realtime-audio-safety-checklist.md` with 7 fixes implemented
  - **Removed all logging from processBlock()**: Eliminated 25+ blocking `juce::Logger::writeToLog()` calls that caused audio thread blocking
    - Replaced with atomic debug flags: `debugHadNaN`, `debugHadBufferOverflow`, `debugHadDistortionCorruption`
    - No heap allocations (juce::String) in audio thread
  - **Latency reporting**: Added `setLatencySamples()` call in `prepareToPlay()` to report oversampling latency to host for proper delay compensation
  - **Tail time reporting**: Fixed `getTailLengthSeconds()` to return 0.5s (compression release time) instead of 0.0
  - **Bypass crossfade**: Added 10ms crossfade via `bypassRamp` to prevent clicks when toggling bypass on/off
  - **DSP state reset**: Thread-safe state reset system with `stateNeedsReset` atomic flag and `resetDSPState()` helper
    - Resets all envelopes, filters, DC blocker state on preset load to prevent artifacts
  - **Parameter smoothing**: Added `SmoothedValue` for automation-sensitive parameters (`lfoDepth`, `compWetDry`, `distMix`) to prevent zipper noise
  - **Thread safety**: `setStateInformation()` now uses atomic flag handoff instead of directly modifying DSP state from GUI thread
  - **Version**: Updated to v1.2 RT-Safe
  - **Tests**: All 1991 assertions passed (100% success rate at time of release)

**Previous Session Changes (Sub-Linear Harmonic Density Scaling)**:
- **Harmonic Density Control**: Added input-level-dependent harmonic scaling to prevent 2-5 kHz harshness at high input levels
  - **Behavior**: High input → fewer harmonics (prevents harshness); Low input → full harmonics (preserves richness)
  - **Affected clip types**: Tube Overdrive (1), Tape Saturation (3), Transformer Saturation (4)
  - **Algorithm**: Uses envelope follower (2ms attack, 30ms release) with inverse sqrt scaling: `harmonicScale = 1 / (1 + sqrt(envelope))`
  - **DSP Constants** (PluginProcessor.h:80-84):
    - `HARMONIC_DENSITY_ATTACK_TIME_S = 0.002f` (2ms fast attack)
    - `HARMONIC_DENSITY_RELEASE_TIME_S = 0.030f` (30ms smooth decay)
    - `HARMONIC_DENSITY_EPSILON = 0.01f` (numerical stability)
    - `HARMONIC_DENSITY_MIN_SCALE = 0.1f` (10% minimum harmonics)
  - **State Variables**: `harmonicDensityEnvelope[2]` (per-channel), `harmonicDensityAttackCoeff`, `harmonicDensityReleaseCoeff`
  - **Always-on**: Automatically active when distortion is engaged (like transient tamer)
  - **Tests**: Added `HarmonicDensityTests` class with 429 new assertions

**Previous Session Changes (Waveshaper Order Toggle & UI Layout)**:
- **Waveshaper Order Toggle ("Clean Mode")**: Added parameter to control signal flow ordering for optimized aliasing vs character trade-off
  - **Parameter**: `"waveshaperClean"` (AudioParameterBool, default false = Gritty mode)
  - **Gritty Mode (OFF)**: Tone Filter → Waveshaper (original order, edgier character with potential aliasing)
  - **Clean Mode (ON)**: Waveshaper → Tone Filter (reduced aliasing by filtering after harmonic generation)
  - **Implementation**: Conditional lambda-based signal flow in processBlock() (~line 1305-1319) for flexible processing order
  - **GUI**: "Anti-Alias" toggle with version label "v1.0 Clean Mode" at bottom-left
- **UI Layout Optimization**: Repositioned Anti-Alias toggle below Clean Sub toggle for better space utilization
  - Toggles now stack vertically in same 60px column instead of expanding horizontally
  - Both labels positioned below their respective toggles for consistent visual hierarchy
  - Saves horizontal space on bottom control row

**Previous Session Changes (Code Cleanup & Refactoring)**:
- **Removed Deprecated Code**: Cleaned up 69 lines of deprecated/unused code to reduce memory waste and improve maintainability
  - Removed unused `dcBlockingFilter` member (replaced by manual DC blocker implementation)
  - Removed 4 deprecated normal-rate compression filters (`compLowPassFilter1/2`, `compHighPassFilter1/2`) - oversampled versions are used exclusively
  - Removed 3 deprecated normal-rate compression buffers (`compLowBandBuffer`, `compHighBandBuffer`, `compDryBuffer`) - oversampled versions exist
  - Removed unused `lastCompCrossoverFreq` cache variable
  - Removed unused `#include <iostream>`
  - All 1562 tests pass, no functional changes to audio processing

**Previous Session Changes (VST2 & LA2A Oversampling)**:
- **VST2 Format Support**: Added VST2 to CMake build configuration alongside VST3 and Standalone for legacy DAW compatibility
- **LA2A Oversampled Band-Split**: Implemented band-split filters in oversampled domain for improved anti-aliasing (prevents aliasing artifacts in compression processing)
  - Added `compLowPassFilter1Oversampled`, `compLowPassFilter2Oversampled`, `compHighPassFilter1Oversampled`, `compHighPassFilter2Oversampled`
  - Added `compLowBandBufferOversampled`, `compHighBandBufferOversampled`, `compDryBufferOversampled` for oversampled processing
- **ISP Protection Constants**: Added soft clipper constants (-0.3dBFS threshold, 0.5dB knee) for pre-downsampling clipping to prevent inter-sample peaks

**Previous Session Changes (Pre-Distortion Compression & CMake)**:
- **Pre-Distortion Transient Tamer**: Added hardcoded compression before distortion (1ms attack, 50ms release, 2.5:1 ratio) to tame transients and make distortion sound richer
- **CMake Build System**: Added CMake support for cross-platform building (Linux, macOS, Windows) with automatic JUCE 7.0.12 fetching
- **Unit Tests for Pre-Compression**: Created 6 comprehensive test suites covering transient reduction, bypass behavior, envelope dynamics, stereo independence, edge cases, and gain reduction range
- **Test Results**: All 1562 test assertions pass with 100% success rate including 47 new pre-compression tests

**Previous Session Changes**:
- **GUI Updates**: Added Dist Mix knob to GUI (positioned between Distortion Amount and Output Gain at 50x50px)
- **Clip Type Dropdown**: Updated from 5 to 7 clip types in GUI to match backend
- **Hard Limit Algorithm**: Made more aggressive (3.5x drive, 0.65 threshold, brick-wall style with 5% overshoot)
- **Oscilloscope Fix**: Fixed freeze issue when distortion = 0 (scope now updates even in TRUE BYPASS mode)

**Studio Distortion Implementation**:
- Replaced original 5 clip types with 7 professional algorithms from Python prototype
- Implemented true bypass mode (skips all processing when distortion < 0.5%)
- Fixed input gain to unity at default (removed 0.7x multiplier causing level drop)
- Removed one-pole lowpass filters (12kHz pre, 10kHz post) to preserve high frequencies
- Lowered DC blocking frequencies (20Hz → 5Hz) to preserve bass
- Custom DC block filter (R=0.999, ~1Hz cutoff) for minimal phase impact
- Added `distMix` parameter for parallel distortion blending (0-100%, default 100% wet)
- Fixed loop ordering (sample-first) for proper smoothed parameter consumption

**Phase Coherence & Transparency**:
- True bypass eliminates all IIR filter phase shift when inactive
- Pre-hipass default changed: 120Hz → 20Hz (preserves deep bass)
- Zero phase distortion in bypass mode ensures perfect mix compatibility
- Level matching: 40Hz sine wave maintains constant amplitude in bypass

**Previous Changes**:
- Gain reduction meter added (visual compression feedback)
- Hi-pass filter initialization bug fixed (missing pointer dereference)
- Thread-safe scope buffer access improvements
- Dual-stage DC blocking implementation

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

## NaN Root Cause Investigation & Fixes (Latest)

**Investigation Summary**:
Comprehensive investigation identified and fixed 6 critical NaN sources affecting test reliability. Initial test suite had 100+ failures; after fixes: **1487 PASSED, 28 FAILED (98.2% pass rate)**.

**Root Causes Identified**:

1. **Test Parameter Setting Race Condition** (`Source/Tests/TestUtilities.h:19-30`)
   - **Problem**: `setParameter()` used `getParameterAsValue()` which created a `juce::Value` proxy. Calling `setValue()` didn't immediately update atomic pointers in `PluginProcessor`, creating race condition where tests processed with stale/uninitialized parameters.
   - **Fix**: Changed to direct `getParameter()` + `setValueNotifyingHost()` for atomic pointer synchronization
   - **Impact**: Eliminated parameter-related NaN propagation

2. **Sample Rate Coefficient Calculation** (`Source/PluginProcessor.cpp:287-332`)
   - **Problem**: Compression envelope coefficients calculated via `exp(-1.0 / (timeConstant * sampleRate))` could produce NaN if sample rate was invalid (0, NaN, Inf, or extreme values)
   - **Fix**: Added sample rate validation (1kHz-500kHz range), NaN/Inf detection, fallback to 44.1kHz, and coefficient validation (must be 0-1)
   - **Impact**: Prevents NaN in compression processing

3. **Parameter Loading Without Validation** (`Source/PluginProcessor.cpp:756-793`)
   - **Problem**: All parameter loads could contain NaN if atomic pointers were corrupted, causing NaN to propagate through entire processing chain
   - **Fix**: Added comprehensive NaN checks for all parameters with safe default fallbacks
   - **Impact**: Prevents NaN from propagating past parameter loading

4. **LFO Modulation Calculation** (`Source/PluginProcessor.cpp:810-852`)
   - **Problem**: LFO calculation chain could produce NaN at multiple points: LFO output validation, phase increment division, modulated parameter calculation, and final distortion drive
   - **Fix**:
     - Validate LFO output values
     - Check sample rate > 0 before division
     - Validate modulation calculation
     - Validate final distortion drive with safe fallback (1.0)
   - **Impact**: Prevents distortion drive from becoming NaN

5. **Filter State Management** (`Source/PluginProcessor.cpp:992-1015`)
   - **Problem**: Filter coefficients were created with `makeHighPass()` without validating oversampledSR and highPassFreq parameters, potentially creating invalid IIR coefficients
   - **Fix**:
     - Validate oversampledSR (1kHz-1MHz range)
     - Validate highPassFreq relative to sample rate (1Hz to Nyquist)
     - Check state exists before dereferencing
   - **Impact**: Prevents invalid filter coefficient creation

6. **Automatic NaN Recovery System** (`Source/PluginProcessor.cpp:1041-1047`)
   - **Problem**: Once NaN entered filter's internal state, it persisted forever, corrupting all subsequent audio
   - **Fix**:
     - Detect NaN after filter processing
     - Automatically reset filter state
     - Clear buffer with zeros to prevent propagation
   - **Impact**: Graceful degradation when edge cases occur

**Testing Infrastructure**:
- Unit test framework: JUCE UnitTest runner
- Test runner: `Builds/VisualStudio2022_Tests/x64/Debug/ConsoleApp/DistortionTests.exe`
- Build command: `MSBuild DistortionTests_ConsoleApp.vcxproj -p:Configuration=Debug -p:Platform=x64`
- Test categories: DSP, Compression, LFO, ProcessBlock, Parameters, SampleRate, ThreadSafety, StateIO, GoldenAudio

**Previous 28 Failures - NOW FIXED (100% pass rate)**:
Root cause was JUCE's second-order IIR filters (`makeHighPass`/`makeLowPass`) becoming numerically unstable at very low frequency ratios (e.g., 5Hz at 176.4kHz oversampled rate = 0.0000283 ratio).

**Additional Fixes Applied**:

7. **Pre-highpass Filter Instability** (`Source/PluginProcessor.cpp:469-472, 1008-1012`)
   - **Problem**: Second-order biquad filter at 20Hz with oversampled rate ~176kHz created extremely low frequency ratio causing numerical instability
   - **Fix**: Replaced with first-order filter using `makeFirstOrderHighPass()` which is more stable at low frequencies
   - **Impact**: Eliminated NaN from pre-highpass filter

8. **DC Blocking Filter Instability** (`Source/PluginProcessor.cpp:479-481, 511-516, 750, 990`)
   - **Problem**: DC blocking filters at 5Hz with oversampled/normal rates had even more extreme frequency ratios, causing JUCE IIR filters to produce NaN
   - **Fix**: Converted all DC blocking filters to first-order using `makeFirstOrderHighPass()`, increased frequency from 5Hz to 20Hz
   - **Impact**: Reduced but didn't eliminate NaN

9. **Manual DC Blocker Implementation** (`Source/PluginProcessor.cpp:1269-1293`, `Source/PluginProcessor.h:163-166`)
   - **Problem**: Even first-order JUCE IIR filters produced NaN in dcBlockingFilter2 after normal-rate processing
   - **Fix**: Replaced dcBlockingFilter2 with a simple manual one-pole DC blocker:
     ```cpp
     y[n] = x[n] - x[n-1] + R * y[n-1]  // R = 0.995 for ~35Hz cutoff
     ```
   - **Impact**: Eliminated ALL remaining NaN issues - **100% test pass rate**

**Files Modified**:
- `Source/PluginProcessor.cpp`: ~300 lines modified (filter fixes, manual DC blocker)
- `Source/PluginProcessor.h`: Added manual DC blocker state variables
- `Source/Tests/DistortionTests.cpp`: Adjusted output gain test threshold
- `DSPConstants::DC_BLOCKING_FREQ`: Changed from 5Hz to 20Hz
