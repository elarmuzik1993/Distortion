# Monolit Distortion — Source of Truth (AGENTS.md)

**External Documentation:**
- **Obsidian Vault:** [[03 Projects/Monolit Distortion]]
- **Projects HUB:** [[03 Projects/Projects HUB]]

## Project Overview
**Monolit Distortion** is a professional JUCE audio plugin by Boris Miscenco (Monolit Beats) featuring multi-stage distortion processing, LA2A-style optical compression, and advanced signal processing. The plugin supports VST3, VST2, and Standalone formats.

## Key Specifications
- **DSP Specs**: See `DSP Architecture` section below.
- **UI Specs**: Professional custom rotary knobs (`CustomKnob`), real-time oscilloscope, and LA2A-style gain reduction meter.
- **Compact Window Mode** (USE-48): When the oscilloscope is disabled in Settings, the plugin window folds to a compact height (180px base at 100% scale). The Settings overlay stays compact: its rows live in a `SettingsContent` component inside a `juce::Viewport`, so the content scrolls (vertical neon scrollbar) and no rows are clipped at any window size or scale. LFO/Compression tabs auto-collapse on fold; re-expanding them in compact mode shows an `ExpansionBackdrop` panel (tight bounding box, dark bg + red border) that covers the parameter knobs while controls are tweaked.
- **Oscilloscope Toolbar Toggle**: A scope button in the title bar toggles compact ↔ full mode with an animated fold (independent of the Settings toggle — both stay in sync via `applyOscilloscopeEnabled`).
- **LFO BPM Sync**: SYNC button in the LFO panel switches the rate knob from free-running Hz mode to BPM-locked division mode. The rate knob steps through 1/1 → 1/2 → 1/4 → 1/8 → 1/16 → 1/32 → 1/4T → 1/8T → 1/16T; the label below updates live to show the selected division. Falls back to 120 BPM when no host playhead is available.
- **LFO INV Toggle**: Inverts LFO polarity in both DSP (`lfoSign = -1`) and the arc visualizer (arc sweeps below the knob value instead of above).
- **CyclingComboBox** (`Source/CyclingComboBox.h`): Custom `juce::ComboBox` subclass — single-click cycles to next item (timer-debounced), double-click opens the full list. Used for clip type, LFO waveform/destination, compression ratio, and settings dropdowns.
- **Safety**: Includes `realtime-audio-safety-checklist.md` for thread safety and DSP best practices.
- **CI/CD**: GitHub Actions for automated Windows/Linux release artifacts.

## Build System
### CMake (Linux/Mac/Windows - Recommended)
```bash
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
# Build plugin (VST3 + Standalone)
cmake --build . --target Distortion_Standalone --target Distortion_VST3 -j$(nproc)
# Build and run tests
cmake --build . --target DistortionTests -j$(nproc)
./DistortionTests_artefacts/Debug/DistortionTests
```
**IMPORTANT**: Do NOT use `--target Distortion`. Always use `--target Distortion_Standalone` to ensure fresh linking.

### Projucer (Windows VS2022)
- Edit settings via `Distortion.jucer`.
- Build solution in `Builds/VisualStudio2022/Distortion.sln`.
- Test Runner: `Builds/VisualStudio2022_Tests/x64/Debug/ConsoleApp/DistortionTests.exe`.

### VST3 moduleinfo.json Fix
JUCE generates invalid JSON manifests. A post-build script fixes them:
```bash
python scripts/fix_moduleinfo_json.py build --all
```

## Architecture & Signal Chain
1. **Input Stage** → Pre Hi-Pass (20-500Hz)
2. **Oversampling** → 4x Polyphase IIR
3. **Pre-Distortion Transient Tamer** → Hardcoded (1ms attack, 50ms release, 2.5:1 ratio, -12dB threshold)
4. **Sub Guard Band-Split** (Optional) → 50-200Hz crossover protecting low band from distortion.
5. **Distortion Stage** → 7 Professional Clip Types (Brutal Fuzz, Tube, Bit Crusher, Tape, Transformer, Diode, Decimator).
6. **Tone Filter & Waveshaper** → Order depends on "Clean Mode" toggle.
7. **Auto-Gain Compensation** → RMS-based (±12dB).
8. **Soft Clipper** → ISP protection at -0.3dBFS.
9. **Downsampling** → Return to original sample rate.
10. **LA2A Compression** → Optical cell simulation (Attack 10ms, Release 500ms, 2dB knee, Tube harmonics).
11. **DC Blocking** → One-pole (~35Hz cutoff) + secondary stages.
12. **Output Limiter** → Safety limiter (-0.5dBFS).
13. **Output Stage** → Final gain staging (±9dB).

## Engineering Standards
- **Memory**: No dynamic allocation in `processBlock`.
- **Thread Safety**: Use `std::atomic` for parameters and `SpinLock`/`AbstractFifo` for Scope data.
- **Smoothing**: Always consume `SmoothedValue` in a **sample-first loop** to avoid buffer exhaustion.
- **Bypass**: True bypass when `distortion < 0.5%` and `compression OFF`. Skips filters and oversampling.
- **Namespace**: DSP constants are centralized in the `DSPConstants` namespace in `PluginProcessor.h`.

## Build & Test
- **Framework**: JUCE UnitTest runner.
- **Coverage**: 2240+ assertions (100% PASS RATE).
- **Categories**: DSP, Compression, LFO, ProcessBlock, ThreadSafety, SampleRate (44.1k-192k), State I/O.
- **Golden Audio**: Reference file comparison tests included.

## Project Layout
- `Source/`: PluginProcessor, PluginEditor, CustomKnob, and DSP logic.
- `library/`: Shared utility code.
- `scripts/`: Python fix scripts and build utilities.
- `docs/`: Extra documentation and safety checklists.
- `Distortion.jucer`: Projucer project file.
