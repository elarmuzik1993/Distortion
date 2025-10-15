# Distortion - Audio Plugin

A professional JUCE audio plugin featuring multi-stage distortion, LA2A-style compression, and advanced signal processing.

## Project Overview

**Distortion** - A JUCE audio plugin by Elar Music Audio
- **Type**: Audio Plugin (VST3 + Standalone)
- **Framework**: JUCE
- **Platform**: Windows (Visual Studio 2022)
- **Architecture**: Multi-stage audio processor with parallel compression

## Recent Features
- Gain reduction meter with visual feedback
- Parallel compression (LA2A-style compressor)
- Thread-safe scope buffer access
- Hi-pass filter
- LFO with rate/depth controls
- Multiple clip types
- Dual-stage DC blocking

## Project Structure

```
Distortion/
├── Source/
│   ├── PluginProcessor.cpp/h    # Audio processing logic
│   ├── PluginEditor.cpp/h       # GUI implementation
│   └── CustomKnob.cpp/h         # Custom UI components
├── Builds/                       # Build outputs
├── JuceLibraryCode/             # JUCE generated code
├── Distortion.jucer             # JUCE project file
└── Distortion.filtergraph       # Audio graph configuration
```

## Plugin Architecture

### Core Features

**Distortion Engine**
- Multi-stage distortion with tanh and exponential clipping types
- 4x oversampling for high-quality processing
- Band-split processing for frequency-specific distortion

**LA2A-Style Compressor**
- Optical cell envelope follower simulation
- Program-dependent compression behavior
- Adjustable peak reduction and makeup gain
- Multiple ratio modes
- Parallel compression with wet/dry mix control

**Signal Processing**
- Band-split processing: Separate low and high frequency paths
- Adjustable crossover frequency (150-350Hz)
- Dual-stage DC blocking filters
- Pre-distortion hi-pass filter
- LFO modulation system

### Current Parameters

**Main Controls:**
- Input Gain
- Output Gain
- Distortion Amount
- Hi-Pass Frequency
- Clip Type (combo box selection)
- Band Split Enable/Disable
- LFO Rate
- LFO Depth

**Compressor Section:**
- Peak Reduction
- Makeup Gain
- Compression Ratio (combo box selection)
- Enable/Disable
- Wet/Dry Mix (parallel compression)
- Crossover Frequency

### GUI Components

- **Custom Rotary Knobs**: Professional-looking parameter controls
- **Oscilloscope Display**: Dual-channel waveform visualization with glow effects
- **Gain Reduction Meter**: Visual compression feedback (0-20dB range)
- **Custom Checkbox Styling**: Solid black with green tick indicators

### Signal Flow

1. **Input Stage**: Audio input → Pre Hi-Pass Filter
2. **Oversampling**: 4x oversampling for distortion processing
3. **Band Splitting**: Split into low and high frequency bands
4. **Distortion**: Parallel distortion applied to high band
5. **Compression**: LA2A-style compression with optional band-split
6. **Mixing**: Blend dry/wet signals for parallel compression
7. **Output Stage**: DC blocking → Final output

## Tech Stack

- **JUCE Framework**: Core audio plugin framework
- **C++**: Primary programming language
- **DSP Modules**: juce_dsp for audio processing
- **VST3**: Plugin format
- **Visual Studio 2022**: Build system

## Development

The project uses JUCE's Projucer for project management. Open `Distortion.jucer` in Projucer to modify project settings and regenerate build files.
