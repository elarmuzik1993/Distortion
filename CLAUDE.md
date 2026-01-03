# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**Distortion** is a professional JUCE audio plugin by Elar Music Audio featuring multi-stage distortion processing, LA2A-style optical compression, and advanced signal processing. The plugin supports VST3 and Standalone formats.

## Build System

This is a JUCE project managed via Projucer.

**Build Commands:**
```bash
# Build entire solution (VST3 + Standalone)
cd "Builds/VisualStudio2022"
"C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" Distortion.sln -p:Configuration=Debug -p:Platform=x64 -m -v:minimal

# Build only VST3 (faster when DAW has plugin unloaded)
"C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" Distortion_VST3.vcxproj -p:Configuration=Debug -p:Platform=x64 -v:minimal

# Build only Standalone (for testing without DAW)
"C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" Distortion_StandalonePlugin.vcxproj -p:Configuration=Debug -p:Platform=x64 -v:minimal
```

**Build Outputs:**
- Debug VST3: `C:\Program Files\Common Files\VST3\Distortion.vst3`
- Debug Standalone: `Builds\VisualStudio2022\x64\Debug\Standalone Plugin\Distortion.exe`
- Release VST3: `%CommonProgramW6432%\VST3`

**Important**: If you get "Sharing violation" errors during build, the DAW still has the VST3 loaded. Close the DAW completely before rebuilding.

**Project Configuration:**
- Edit project settings via Projucer: Open `Distortion.jucer` in Projucer
- After making changes in Projucer, save and it will regenerate the Visual Studio project files
- JUCE modules path: `../../../Documents/JUCE/modules` (relative to project)

## Architecture

### Signal Flow Overview

The plugin processes audio through a carefully ordered chain:

1. **Input Stage** → Pre Hi-Pass Filter (removes DC and low-frequency rumble)
2. **Band Splitting (Optional)** → Splits signal into low (<150Hz) and high (>150Hz) bands for 808-safe processing
3. **Oversampling** → 4x oversampling applied for distortion processing (polyphase IIR, 2 stages)
4. **Distortion Stage** → Applied to high band only (if band-split enabled) or full signal
5. **Downsampling** → Return to original sample rate
6. **Compression Stage** → LA2A-style optical compressor with optional band-split (150-350Hz adjustable)
7. **Parallel Blending** → Wet/Dry mix for parallel compression
8. **DC Blocking** → Dual-stage DC blocking filters at 20Hz
9. **Output Stage** → Final gain staging

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

### LA2A Compression Simulation

The compressor simulates a Teletronix LA-2A optical cell (T4 cell) with:
- **Envelope follower**: Attack coefficient 0.9995 (~10ms), Release 0.99995 (~500ms)
- **Program-dependent behavior**: RMS history tracking for adaptive response
- **Soft knee**: 2dB knee width for smooth compression onset
- **Tube harmonics**: 15% blend with 1.5x drive for analog warmth
- **Parallel compression**: Wet/Dry mix control (0-100%)
- **Band-split mode**: Independent crossover (150-350Hz) from distortion crossover

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
- Middle row controls: 808-Safe toggle, Clip Type dropdown, **Dist Mix knob** (50x50px, positioned between Distortion Amount and Output Gain)

**Thread Safety:**
- Scope buffer uses `juce::AbstractFifo` + `juce::SpinLock` for lock-free audio→GUI transfer
- Gain reduction uses `std::atomic<float>` for thread-safe reading
- Oscilloscope timer pauses during resize to prevent buffer access conflicts

### DSP Constants

All processing constants centralized in `DSPConstants` namespace (PluginProcessor.h:18-66):
- Oversampling: 4x factor, 2 polyphase IIR stages
- Crossover frequencies: Distortion (150Hz fixed), Compression (150-350Hz adjustable)
- Compression optical cell coefficients
- Scope/meter configuration (buffer sizes, refresh rates)
- Parameter smoothing times

**Important**: When modifying DSP behavior, always update constants in this namespace rather than hardcoding values.

## Key Implementation Details

### Studio Distortion Processing

**7 Professional Clip Types** (implemented in `applyStudioDistortion()`):
- **Studio Tanh** (0): Enhanced tanh with asymmetric bias and 2nd-order shaping
- **Soft Knee** (1): Soft knee compression above 0.5 threshold
- **Dynamic Compress** (2): Dynamic ratio compression with program-dependent behavior
- **Multi-Stage** (3): Multi-stage hard clipping with progressive limiting
- **Harmonic** (4): Tanh with 2nd harmonic boost for tube-like character
- **Asymmetric** (5): Asymmetric clipping with different positive/negative thresholds
- **Hard Limit** (6): Aggressive brick-wall clipping (3.5x drive, 0.65 threshold, only 5% overshoot allowed, ±0.85 final limit)

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

**808-Safe Mode** (Band-Split):
- Crossover: 150Hz (Linkwitz-Riley 4th order, dual cascaded filters)
- Low band (<150Hz): **Bypasses distortion entirely** (preserves kick/808 bass)
- High band (>150Hz): Full distortion processing
- Prevents bass frequency aliasing and maintains sub-bass integrity

**Gain Staging**:
- Input gain: Unity (1.0) at default 50, range 0-2.83 via `pow(param/50, 1.5)`
- Distortion drive: 1.0-4.5 range for controlled saturation
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
- Distortion: `"distortionAmount"`, `"clipType"`, `"bandSplitEnabled"`, `"distMix"`
- Filters: `"highPassFreq"`
- LFO: `"lfoRate"`, `"lfoDepth"`
- Compression: `"compPeakReduction"`, `"compMakeupGain"`, `"compRatio"`, `"compEnabled"`, `"compWetDry"`, `"compCrossover"`

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
- **Standalone**: Standalone application for testing without a DAW

## Recent Architectural Changes

**Latest Session Changes**:
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

**Remaining 28 Failures**:
All related to pre-highpass filter creating NaN from valid input in specific edge cases. The automatic NaN recovery system prevents propagation, but the underlying JUCE IIR filter behavior in edge cases needs further investigation (likely related to coefficient stability at extreme filter frequencies).

**Files Modified**:
- `Source/PluginProcessor.cpp`: 240 lines added/modified (6 major safety check locations)
- `Source/Tests/TestUtilities.h`: 15 lines modified (parameter setting fix)
