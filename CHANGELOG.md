# Changelog — Monolit Distortion

Full development history, newest first.

---

## v2.1 — Oscilloscope Quality Audit

- **Removed SpinLock from scope pipeline**: `juce::AbstractFifo` is lock-free SPSC by design. The `scopeLock` SpinLock and `bufferLock` CriticalSection were redundant and risked audio-thread stalls. Both removed entirely.
- **Fixed FIFO drain logic in `fillScopeBuffer()`**: Previously read oldest 512 samples from a nearly-full 2048-sample FIFO, causing ~64ms display latency. Now drains excess samples first so the display always shows the most recent audio.
- **Zero-crossing trigger**: `Oscilloscope::timerCallback()` scans the first `SCOPE_TRIGGER_MARGIN` (256) samples of the buffer for a rising zero-crossing on channel 0, stores `triggerOffset`, and both draw functions offset all reads by it. Waveform is now phase-stable across frames.
- **Anti-alias decimation filter**: Both `pushSampleToScope` call sites now average `sample[n]` and `sample[n+1]` before pushing (2-point box filter). Prevents visual aliasing from naive 2x downsampling.
- **Removed `setBufferedToImage(true)`**: Backing bitmap was invalidated every 30Hz frame anyway — pure memory overhead with no benefit.
- **Removed `resized()` timer workaround**: The stop/restart-timer-after-50ms lambda (with unsafe `this` capture) existed only to avoid a race condition that never existed on the single JUCE message thread.
- **Refactored draw functions**: `drawChannelWithGlow` and `drawMonoWithGlow` shared ~130 lines of identical path-building code. Extracted into `buildWaveformPath(numSamples, readSample)` (takes a lambda) and `strokeWithGlow(g, path, colour, mainAlpha)`.
- **Reduced glow stroke passes**: 3 strokes per channel (3px + 2px + 1px) merged into 2 (3px glow + 1px main). Stereo: 6→4 strokes per frame.
- **`SCOPE_TRIGGER_MARGIN = 256`** constant added to `DSPConstants` in `PluginProcessor.h`.
- Tests: All 2033 assertions pass. New `testScopeDrainExcess` test added to `ThreadSafetyTests`.

---

## v1.8 — LFO Destination Routing

- **LFO Destination Parameter**: Added `"lfoDestination"` AudioParameterChoice with 5 destinations
  - 0: Distortion Amount (default, backward compatible)
  - 1: Tone Filter (logarithmic ±2 octave sweep, 2000-20000 Hz)
  - 2: Hi-Pass Filter (logarithmic ±1.5 octave sweep, 20-500 Hz)
  - 3: Dist Mix (linear ±50% swing)
  - 4: Output Gain (linear ±25% swing for tremolo)
- **SmoothedValue for Filter Modulation**: Added `smoothedModulatedToneFreq` and `smoothedModulatedHighPassFreq` (10ms smoothing) to prevent zipper noise during filter sweeps.
- **Bipolar Modulation**: LFO swings ±range around current parameter value for natural, musical modulation.
- **Visual Feedback**: Modulated knob shows pulsing cyan glow ring when LFO is active (30Hz timer with sine wave intensity).
- **GUI**: Added "Target" dropdown in LFO section (5 options: Distortion, Tone, Hi-Pass, Dist Mix, Out Gain) with lock icon support.
- **CustomKnob Modulation Indicator**: Added `setModulationIndicator(bool, float)` method with pulsing glow rendering.
- **Randomization Support**: Added `lfoDestination` to `randomizeAllParameters()` using existing `randomizeChoiceParam()` helper.
- **Unit Tests**: Added `LFODestinationTests` class with 142 new assertions covering all 5 destinations, extreme depth, fast rates, and NaN validation.

---

## v1.6 — Soft Drive (Reduced Internal Drive Scaling)

- **Reduced Main Drive Range**: Distortion drive formula changed from 1.0-8.0 to 1.0-4.0 range for more gradual response at low percentages.
  - `* 7.0f` → `* 3.0f` in main `processBlock` calculation and `prepareToPlay` initialization.
- **Reduced Per-Algorithm Multipliers (50% reduction)**: All 7 clip types now have halved internal drive multipliers.
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
- Tests: All 2000 assertions pass.

---

## v1.5 — Sub Guard & Output Limiter

- **Sub Guard Variable-Slope Crossover**: Replaces old 808-Safe toggle with continuous frequency control.
  - Range: 0 Hz (OFF) to 200 Hz with snap points at 60/100/150 Hz.
  - Variable filter slopes: LR24 (60Hz), LR18 (100Hz), LR12 (150Hz).
  - 10ms crossfade for order transitions, 50ms frequency smoothing.
  - Parameter: `"subGuardFreq"` (float, 0-200Hz, default 0=OFF).
- **Output Limiter**: Final stereo-linked safety limiter (always-on). Prevents digital overs at final output stage.
  - `OUTPUT_LIMITER_THRESHOLD_DB = -0.5f`, attack 0.5ms, release 50ms, knee 1dB.
- **Tone Filter**: Post-distortion lowpass for brightness/darkness control. Parameter: `"tone"` (2000-20000Hz, default 20kHz = bright/bypass).
- **Waveshaper Mix**: Parallel waveshaper blend control. Parameter: `"waveshaperMix"` (0-100%, default 0%).

---

## v1.4 — Auto-Gain Compensation & ISP Protection

- **Auto-Gain Compensation**: Maintains consistent perceived loudness as distortion amount changes.
  - Measures input/output RMS with envelope followers (5ms attack, 100ms release).
  - Compensation clamped to -20dB to +12dB range.
  - Applied in oversampled domain after distortion processing.
- **Soft Clipper (ISP Protection)**: Applied at -0.3dBFS with 0.5dB soft knee before downsampling. Prevents inter-sample peaks during sample rate conversion.
- **Input-Dependent Analog Noise (Brutal Fuzz)**: Noise scales with input level for realistic analog behavior. Formula: `noiseAmount = min(abs(x) * 0.01, 0.002)` — caps at 0.2% noise.

---

## v1.3 — Real-Time Audio Safety Audit

- **Removed all logging from processBlock()**: Eliminated 25+ blocking `juce::Logger::writeToLog()` calls. Replaced with atomic debug flags: `debugHadNaN`, `debugHadBufferOverflow`, `debugHadDistortionCorruption`.
- **Latency reporting**: Added `setLatencySamples()` in `prepareToPlay()` to report oversampling latency to host.
- **Tail time reporting**: Fixed `getTailLengthSeconds()` to return 0.5s (compression release time) instead of 0.0.
- **Bypass crossfade**: Added 10ms crossfade via `bypassRamp` to prevent clicks when toggling bypass on/off.
- **DSP state reset**: Thread-safe state reset system with `stateNeedsReset` atomic flag and `resetDSPState()` helper. Resets all envelopes, filters, DC blocker state on preset load.
- **Parameter smoothing**: Added `SmoothedValue` for automation-sensitive parameters (`lfoDepth`, `compWetDry`, `distMix`).
- **Thread safety**: `setStateInformation()` now uses atomic flag handoff instead of directly modifying DSP state from GUI thread.
- Tests: All 1991 assertions pass.

---

## v1.2 — Sub-Linear Harmonic Density Scaling

- **Harmonic Density Control**: Input-level-dependent harmonic scaling to prevent 2-5 kHz harshness at high input levels.
  - High input → fewer harmonics (prevents harshness); Low input → full harmonics (preserves richness).
  - Affected clip types: Tube Overdrive (1), Tape Saturation (3), Transformer Saturation (4).
  - Algorithm: envelope follower (2ms attack, 30ms release) with inverse sqrt scaling: `harmonicScale = 1 / (1 + sqrt(envelope))`.
  - Always-on when distortion is engaged (like transient tamer).
- Tests: Added `HarmonicDensityTests` class with 429 new assertions.

---

## v1.1 — Waveshaper Order Toggle & UI Layout

- **Waveshaper Order Toggle ("Clean Mode")**: Parameter `"waveshaperClean"` (AudioParameterBool, default false = Gritty mode).
  - **Gritty Mode (OFF)**: Tone Filter → Waveshaper (edgier character, potential aliasing).
  - **Clean Mode (ON)**: Waveshaper → Tone Filter (reduced aliasing, smoother analog character).
  - Implemented as conditional lambda-based signal flow in `processBlock()`.
  - GUI: "Anti-Alias" toggle at bottom-left.
- **UI Layout Optimization**: Anti-Alias toggle repositioned below Clean Sub toggle; both stack vertically in the same 60px column.

---

## v1.0 — Code Cleanup & Refactoring

- **Removed Deprecated Code**: Cleaned up 69 lines of unused code.
  - Removed `dcBlockingFilter` member (replaced by manual DC blocker).
  - Removed 4 deprecated normal-rate compression filters (`compLowPassFilter1/2`, `compHighPassFilter1/2`).
  - Removed 3 deprecated normal-rate compression buffers (`compLowBandBuffer`, `compHighBandBuffer`, `compDryBuffer`).
  - Removed unused `lastCompCrossoverFreq` cache variable and `#include <iostream>`.
  - All 1562 tests pass, no functional changes.

---

## Earlier Changes

**VST2 & LA2A Oversampling**:
- VST2 format support added to CMake build.
- LA2A band-split filters moved to oversampled domain for improved anti-aliasing.
- Added ISP protection constants (soft clipper at -0.3dBFS, 0.5dB knee).

**Pre-Distortion Compression & CMake**:
- Pre-distortion transient tamer: 1ms attack, 50ms release, 2.5:1 ratio, -12dB threshold.
- CMake build system with automatic JUCE 7.0.12 fetching.
- Unit tests for pre-compression (47 assertions covering all edge cases).

**Studio Distortion Implementation**:
- Replaced original 5 clip types with 7 professional algorithms from Python prototype.
- Implemented true bypass mode (skips all processing when distortion < 0.5%).
- Fixed input gain to unity at default (removed 0.7× multiplier causing level drop).
- Removed one-pole lowpass filters (12kHz pre, 10kHz post) to preserve high frequencies.
- Lowered DC blocking frequencies (20Hz → 5Hz) to preserve bass.
- Custom DC block filter (R=0.999, ~1Hz cutoff) for minimal phase impact.
- Added `distMix` parameter for parallel distortion blending (0-100%, default 100% wet).
- Fixed loop ordering (sample-first) for proper smoothed parameter consumption.

**Phase Coherence & Transparency**:
- True bypass eliminates all IIR filter phase shift when inactive.
- Pre-hipass default changed: 120Hz → 20Hz (preserves deep bass).
- Level matching: 40Hz sine wave maintains constant amplitude in bypass.

**Initial Changes**:
- Gain reduction meter added (visual compression feedback).
- Hi-pass filter initialization bug fixed (missing pointer dereference).
- Thread-safe scope buffer access improvements.
- Dual-stage DC blocking implementation.
- Dist Mix knob added to GUI (50x50px, between Distortion Amount and Output Gain).
- Clip Type dropdown updated from 5 to 7 clip types.
- Oscilloscope freeze fixed when distortion = 0.

---

## NaN Root Cause Investigation & Fixes

Comprehensive investigation identified and fixed 9 critical NaN sources. Initial test suite had 100+ failures; after all fixes: **100% pass rate**.

**Root Causes & Fixes**:

1. **Test Parameter Setting Race Condition** (`Source/Tests/TestUtilities.h:19-30`)
   - `setParameter()` used `getParameterAsValue()` / `juce::Value` proxy, creating a race where atomic pointers weren't updated before audio processing.
   - Fix: Direct `getParameter()` + `setValueNotifyingHost()`.

2. **Sample Rate Coefficient Calculation** (`Source/PluginProcessor.cpp:287-332`)
   - `exp(-1.0 / (timeConstant * sampleRate))` produced NaN for invalid sample rates.
   - Fix: Validate sample rate (1kHz-500kHz), fallback to 44.1kHz, validate coefficients (must be 0-1).

3. **Parameter Loading Without Validation** (`Source/PluginProcessor.cpp:756-793`)
   - Corrupted atomic pointers could inject NaN into the entire processing chain.
   - Fix: NaN checks for all parameters with safe default fallbacks.

4. **LFO Modulation Calculation** (`Source/PluginProcessor.cpp:810-852`)
   - LFO chain could produce NaN at phase increment division, modulation calc, and final drive.
   - Fix: Validate at each step; safe fallback of 1.0 for distortion drive.

5. **Filter State Management** (`Source/PluginProcessor.cpp:992-1015`)
   - `makeHighPass()` called without validating oversampledSR or highPassFreq.
   - Fix: Validate both inputs (1kHz-1MHz range; 1Hz to Nyquist); check state before dereferencing.

6. **Automatic NaN Recovery System** (`Source/PluginProcessor.cpp:1041-1047`)
   - NaN in filter state persisted forever.
   - Fix: Detect NaN after filter processing, reset filter state, zero the buffer.

7. **Pre-highpass Filter Instability** (`Source/PluginProcessor.cpp:469-472, 1008-1012`)
   - Second-order biquad at 20Hz / 176kHz oversampled rate was numerically unstable.
   - Fix: Replaced with `makeFirstOrderHighPass()`.

8. **DC Blocking Filter Instability** (`Source/PluginProcessor.cpp:479-481, 511-516, 750, 990`)
   - DC blocking at 5Hz produced extreme frequency ratios causing IIR NaN.
   - Fix: First-order filters; increased cutoff from 5Hz to 20Hz.

9. **Manual DC Blocker Implementation** (`Source/PluginProcessor.cpp:1269-1293`, `Source/PluginProcessor.h:163-166`)
   - Even first-order JUCE IIR produced NaN in `dcBlockingFilter2`.
   - Fix: Replaced with one-pole DC blocker: `y[n] = x[n] - x[n-1] + R * y[n-1]` (R=0.995, ~35Hz cutoff).

**Files Modified**: `Source/PluginProcessor.cpp` (~300 lines), `Source/PluginProcessor.h`, `Source/Tests/DistortionTests.cpp`, `DSPConstants::DC_BLOCKING_FREQ` (5Hz → 20Hz).
