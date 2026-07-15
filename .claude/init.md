# Sledge Distortion Audio Plugin - Claude Initialization

> Professional JUCE audio plugin by Monolit Beatz featuring multi-stage distortion processing, LA2A-style optical compression, and advanced signal processing.

## Quick Reference

**Current Working Directory**: `C:\Users\boris\Desktop\Programming\Distortion`

**Primary Build Command**:
```bash
cd "Builds/VisualStudio2022"
"C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" Distortion.sln -p:Configuration=Debug -p:Platform=x64 -m -v:minimal
```

**Build Outputs**:
- Debug VST3: `C:\Program Files\Common Files\VST3\Distortion.vst3`
- Debug Standalone: `Builds\VisualStudio2022\x64\Debug\Standalone Plugin\Distortion.exe`

## Project Overview

### Tech Stack
- **Framework**: JUCE (C++ audio plugin framework)
- **IDE**: Visual Studio 2022
- **Platform**: Windows x64
- **Plugin Formats**: VST3, Standalone
- **DSP Library**: juce_dsp

### Core Features
1. **Multi-Stage Distortion Engine**
   - 7 professional clip types (Studio Tanh, Soft Knee, Dynamic Compress, Multi-Stage, Harmonic, Asymmetric, Hard Limit)
   - 4x oversampling with polyphase IIR filters
   - True bypass mode (zero phase distortion when inactive)
   - 808-Safe band-split mode (preserves sub-bass <150Hz)
   - Parallel distortion blending (Dist Mix 0-100%)

2. **LA2A-Style Optical Compressor**
   - T4 optical cell simulation
   - Program-dependent envelope follower
   - Adjustable band-split crossover (150-350Hz)
   - Tube harmonic saturation (15% blend)
   - Parallel compression with Wet/Dry mix

3. **Real-Time Visualization**
   - Dual-channel oscilloscope (30Hz refresh, thread-safe)
   - Gain reduction meter (0-20dB range with gradient)
   - Custom rotary knob controls

## Repository Structure

```
Distortion/
├── .claude/
│   └── init.md                    # This file
├── Source/                        # Plugin source code
│   ├── PluginProcessor.cpp/h      # Core DSP engine
│   ├── PluginEditor.cpp/h         # GUI implementation
│   └── CustomKnob.cpp/h           # Custom UI components
├── Resources/                     # Images and assets
│   ├── background.png             # GUI background
│   └── [design files]
├── Builds/
│   └── VisualStudio2022/          # Build files (gitignored)
├── JuceLibraryCode/               # JUCE generated code (gitignored)
├── Distortion.jucer               # Projucer project file
├── CLAUDE.md                      # Development guide (PRIMARY REFERENCE)
├── README.md                      # Project documentation
└── DISTORTION_44KHZ_TEST_PROCEDURE.txt  # Debug testing guide
```

## Key Files

### Core Implementation
- **PluginProcessor.cpp/h** - Audio processing engine, DSP algorithms, parameter management
- **PluginEditor.cpp/h** - GUI layout, custom components, real-time meters
- **CustomKnob.cpp/h** - Professional rotary knob controls

### Configuration
- **Distortion.jucer** - JUCE project configuration (edit in Projucer, regenerates VS project)
- **CLAUDE.md** - Comprehensive development guide with architecture, DSP details, patterns

### Build System
- **Builds/VisualStudio2022/** - Visual Studio solution and projects (auto-generated)
  - `Distortion.sln` - Main solution file
  - `Distortion_VST3.vcxproj` - VST3 plugin project
  - `Distortion_StandalonePlugin.vcxproj` - Standalone app project

## Architecture Overview

### Signal Flow
1. **Input Stage** → Pre Hi-Pass Filter (20-500Hz adjustable, default 20Hz)
2. **Band Splitting (Optional)** → 808-Safe mode splits at 150Hz
3. **Oversampling** → 4x for distortion processing
4. **Distortion Stage** → 7 clip types, high band only if split
5. **Downsampling** → Return to original sample rate
6. **Compression Stage** → LA2A optical cell with optional band-split (150-350Hz)
7. **Parallel Blending** → Wet/Dry mix for compression
8. **DC Blocking** → Dual-stage filters at 20Hz
9. **Output Stage** → Final gain staging

### DSP Constants Location
All processing constants centralized in `DSPConstants` namespace (`PluginProcessor.h:18-66`):
- Oversampling configuration
- Crossover frequencies
- Compression coefficients
- Scope/meter settings
- Parameter smoothing times

### Thread Safety
- **Oscilloscope**: Uses `juce::AbstractFifo` + `juce::SpinLock` for lock-free audio→GUI transfer
- **Gain Reduction Meter**: Uses `std::atomic<float>` for thread-safe reading
- **Parameter Management**: All via `juce::AudioProcessorValueTreeState` with atomic pointers

## Development Workflows

### Building the Plugin

**Full Build** (VST3 + Standalone):
```bash
cd "Builds/VisualStudio2022"
"C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" Distortion.sln -p:Configuration=Debug -p:Platform=x64 -m -v:minimal
```

**VST3 Only** (faster when DAW has plugin unloaded):
```bash
cd "Builds/VisualStudio2022"
"C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" Distortion_VST3.vcxproj -p:Configuration=Debug -p:Platform=x64 -v:minimal
```

**Standalone Only** (for testing without DAW):
```bash
cd "Builds/VisualStudio2022"
"C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" Distortion_StandalonePlugin.vcxproj -p:Configuration=Debug -p:Platform=x64 -v:minimal
```

**Important**: If you get "Sharing violation" errors, close your DAW completely before rebuilding.

### Modifying Project Settings

1. Open `Distortion.jucer` in Projucer
2. Make changes to modules, settings, build configurations
3. Save project (Ctrl+S) - this regenerates Visual Studio project files
4. Return to Visual Studio and reload solution if prompted

JUCE modules path: `../../../Documents/JUCE/modules` (relative to project)

### Adding New Parameters

1. Edit `createParameterLayout()` in `PluginProcessor.cpp`
2. Add atomic pointer in `PluginProcessor.h` private section
3. Initialize pointer in `PluginProcessor` constructor with `jassert` verification
4. Add GUI control in `PluginEditor.h` and initialize in constructor
5. Create parameter attachment for automatic binding
6. Position control in `PluginEditor::resized()`

### Adding DSP Features

1. Update constants in `DSPConstants` namespace first (avoid hardcoding)
2. Implement in `processBlock()` following sample-first loop pattern
3. Use `juce::SmoothedValue<float>` for parameter smoothing
4. Add thread-safe state variables if GUI needs real-time feedback

## Critical Implementation Patterns

### Smoothed Parameter Consumption
```cpp
// ✓ CORRECT: Sample-first loop
for (size_t sample = 0; sample < numSamples; ++sample)
{
    const float currentGain = smoothedGain.getNextValue();  // Once per sample
    for (size_t channel = 0; channel < numChannels; ++channel)
    {
        // Use currentGain for all channels
    }
}

// ✗ WRONG: Channel-first causes parameter buffer exhaustion
for (size_t channel = 0; channel < numChannels; ++channel)
{
    for (size_t sample = 0; sample < numSamples; ++sample)
    {
        smoothedGain.getNextValue();  // Called 2x per sample in stereo!
    }
}
```

### True Bypass Mode
When `distortion < 0.5%` AND `compression OFF`:
- Complete bypass (skips ALL processing)
- Zero phase distortion, perfect level matching
- Only applies output gain
- Oscilloscope continues updating (prevents freeze)

### Filter Coefficient Caching
Cache last frequency to avoid redundant recalculations:
```cpp
float lastHighPassFreq = -1.0f;  // Check before recalculating
```

## Parameter Reference

### Main Controls
- `inputGain` - Input gain staging (0-2.83, unity at 50 via `pow(param/50, 1.5)`)
- `outputGain` - Output gain (±12dB range)
- `distortionAmount` - Distortion drive (1.0-4.5)
- `clipType` - 7 clip algorithms (0-6)
- `bandSplitEnabled` - 808-Safe mode toggle
- `distMix` - Parallel distortion blend (0-100%, default 100%)
- `highPassFreq` - Pre-distortion filter (20-500Hz)
- `lfoRate` - LFO modulation rate
- `lfoDepth` - LFO modulation depth

### Compression Controls
- `compEnabled` - Compressor on/off
- `compPeakReduction` - Compression amount
- `compMakeupGain` - Output makeup gain
- `compRatio` - Compression ratio selection
- `compWetDry` - Parallel compression mix
- `compCrossover` - Band-split frequency (150-350Hz)

## Recent Architectural Changes

### Latest Fixes (Commit 5ddb180)
- Fixed critical 44.1kHz sample rate instability issues
- Improved debug logging for tracking DSP issues

### Studio Distortion Refactor
- Implemented 7 professional clip types from Python prototype
- True bypass mode eliminates IIR phase shift when inactive
- Fixed input gain to unity at default (removed 0.7x multiplier)
- Removed one-pole lowpass filters (preserved high frequencies)
- Lowered DC blocking: 20Hz → 5Hz (preserved bass)
- Custom DC block filter: R=0.999, ~1Hz cutoff

### Preset Management System
- Save/load/delete functionality
- Improved UI organization

### GUI Improvements
- Dist Mix knob added (positioned between Distortion Amount and Output Gain)
- Clip Type dropdown updated (5 → 7 types)
- Hard Limit made more aggressive (brick-wall clipping)
- Oscilloscope freeze fix (updates even in bypass mode)

## Testing & Debugging

### Sample Rate Testing
See `DISTORTION_44KHZ_TEST_PROCEDURE.txt` for comprehensive debug logging guide:
1. Use DebugView++ to capture OutputDebugString logs
2. Test at 44.1kHz sample rate (critical for stability)
3. Monitor peak levels at each processing stage
4. Look for corrupted values, NaN, Inf, or excessive peaks

### Standalone Testing
```bash
Builds\VisualStudio2022\x64\Debug\Standalone Plugin\Distortion.exe
```
Set audio settings to 44100 Hz and test distortion/compression independently.

## Git Workflow

**Main Branch**: `main`

**Ignored Files** (`.gitignore`):
- `**/Builds` - Build outputs
- `**/JuceLibraryCode` - Auto-generated JUCE code
- `**/.DS_Store` - macOS metadata

**Recent Commits**:
- 5ddb180 - 44.1kHz stability fixes
- fda6f63 - Preset management system
- 88be8d7 - Randomize button with locking
- 4293478 - Oscilloscope freeze fix
- 47c414d - Aggressive Hard Limit clip type

## Common Tasks

### Rebuild After DAW Crash
```bash
# Close DAW completely, then:
cd "Builds/VisualStudio2022"
"C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" Distortion_VST3.vcxproj -p:Configuration=Debug -p:Platform=x64 -v:minimal
```

### Add New GUI Control
1. Add component member to `PluginEditor.h`
2. Initialize in `PluginEditor` constructor
3. Create attachment: `std::make_unique<AudioProcessorValueTreeState::SliderAttachment>`
4. Position in `resized()` method
5. Set look and feel if needed

### Modify Distortion Algorithm
1. Locate `applyStudioDistortion()` in `PluginProcessor.cpp`
2. Update clip type case in switch statement
3. Test at 44.1kHz and 48kHz sample rates
4. Verify peak levels don't exceed safe thresholds
5. Check bypass mode still works (distortion < 0.5%)

### Add Compression Feature
1. Update `applyLA2ACompression()` in `PluginProcessor.cpp`
2. Modify state variables: `compEnvelopeState`, `compRmsHistory`, `smoothedGainReduction`
3. Ensure thread-safe access for `currentGainReductionDB` (atomic)
4. Update gain reduction meter if adding visual feedback

## References

- **CLAUDE.md** - Primary development reference (architecture, patterns, DSP details)
- **README.md** - Project overview and feature list
- **Projucer** - JUCE project configuration tool
- **JUCE Documentation** - https://docs.juce.com/

## Notes for Claude Code

- Always read `CLAUDE.md` before making architectural changes
- Follow sample-first loop pattern for smoothed parameters
- Update `DSPConstants` namespace instead of hardcoding values
- Use thread-safe patterns for audio→GUI communication
- Test at both 44.1kHz and 48kHz sample rates
- Preserve true bypass behavior when distortion/compression off
- Avoid over-engineering - keep solutions focused and simple
- Only create files when absolutely necessary
