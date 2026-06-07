# Final Limiter Ceiling Clamp — Design

**Date:** 2026-06-07
**Status:** Approved (design), pending implementation plan
**Component:** `PluginProcessor::applyFinalLimiter` (Source/PluginProcessor.cpp)

## Problem

The final output limiter is placed correctly (last gain stage, after the global
dry/wet blend), but it cannot honor its advertised -0.5 dBFS ceiling on
transients.

`applyFinalLimiter` applies a gain-reduction envelope (`outputLimiterEnvelope`)
per sample with a 0.5 ms attack. On a transient, the envelope is still at its
previous, higher value for the first samples of the attack
(0.5 ms ≈ 22 samples @ 44.1 kHz). Those first samples are multiplied by a
near-unity gain and exceed -0.5 dBFS before gain reduction catches up. The
envelope is the right *musical* mechanism but provides no hard guarantee.

## Fix

Add a memoryless, stereo-linked **soft ceiling clamp** inside the existing
per-sample loop in `applyFinalLimiter`, **after** the envelope multiply. The
envelope continues to do the musical gain-reduction work; the clamp is a
backstop that only shapes the rare overshoot the envelope missed.

### Algorithm (per sample, after envelope is applied to both channels)

1. Re-measure the post-envelope stereo-linked peak: `peak = max(|L|, |R|)`.
2. If `peak > softPoint`, compute a single softened target and a stereo-linked
   gain factor, then multiply **both** channels by it:

   ```
   ceiling   = thresholdLinear                      // -0.5 dBFS, existing constant
   softPoint = ceiling * decibelsToGain(-softnessDB)// soft zone just under ceiling
   softened  = softPoint + (ceiling - softPoint)
                         * tanh((peak - softPoint) / (ceiling - softPoint))
   factor    = softened / peak                       // <= 1
   L *= factor;  R *= factor;
   ```

### Why not the literal `ceiling * tanh(x/ceiling)`

A global tanh attenuates the entire signal (adds THD even at -12 dBFS) and would
erode the limiter's role as a transparent safety net (and weaken the existing
"transparency below threshold" test). The transparent-below-`softPoint`
formulation above only touches signal above the soft zone.

### Properties

- **Hard guarantee:** output magnitude is at or below `-0.5 dBFS` on every sample,
  including the first transient sample. `softened` asymptotes to the ceiling in
  real arithmetic; in float32 `tanh` saturates to exactly `1.0f` for large
  arguments, so very hot transients land exactly on the ceiling — which is the
  intended `-0.5 dBFS` safety ceiling. The guarantee is therefore `≤ ceiling`
  (equals it only for very hot transients due to float `tanh` saturation).
- **No audible corner:** C¹-continuous at `softPoint` (tanh′(0) = 1), so the
  transition into the clamp has no kink.
- **Transparent below the soft zone:** below `softPoint` the clamp is a no-op, so
  existing sound and the transparency-below-threshold test are preserved.
- **Stereo image preserved:** the same `factor` is applied to both channels,
  consistent with the limiter's existing stereo-linking. The max channel becomes
  `softened` (≤ ceiling); the quieter channel scales by the same ratio.

### New constant

Add to the `DSPConstants` namespace in `PluginProcessor.h`:

```cpp
constexpr float OUTPUT_LIMITER_CLAMP_SOFTNESS_DB = 1.0f; // soft zone below ceiling
```

`softPoint` is derived from the existing `OUTPUT_LIMITER_THRESHOLD_DB` ceiling and
this softness; no new runtime state and no `prepareToPlay` changes are required
(the clamp is memoryless).

## Testing

- **New regression test (primary)** in `OutputLimiterTests`: the exact bug
  scenario. With the limiter freshly reset (`outputLimiterEnvelope == 1.0`),
  craft a stereo buffer whose **first sample is +6 dBFS (≈ 2.0 linear)** and call
  `processor.applyFinalLimiter(buffer)` directly (the test class is already a
  `friend` of `PluginProcessor`). Assert **every** sample — especially sample 0 —
  satisfies `|x| <= thresholdLinear + epsilon` (a real -0.5 dBFS guarantee, not
  the loose 0.5 dB the settled-envelope `testThresholdEnforcement` allows).
  Also assert no NaN/Inf.
- **Regression guard:** re-run the existing `OutputLimiterTests` suite (threshold
  enforcement, transparency below threshold, stereo linking, soft knee,
  attack/release, state reset, global-mix ceiling) to confirm no behavior change
  below the ceiling.

## Scope / non-goals

Purely an additive backstop in `applyFinalLimiter`. No change to attack/release
times, knee, threshold, placement (still last, post dry/wet blend), oversampling,
or any other stage. No dynamic allocation in `processBlock` (the clamp is
arithmetic on existing buffers).
