---
date: 2026-04-23
status: active
tags: [project, plugin, audio, juce, dsp, contract, reference]
---

# Monolit Distortion — Architecture Contract

Updated on `ship/v2.1` after PR-10..PR-13. Use as a reference alongside handoff notes. Changes that break anything here are contract violations.

## Threading
- `processBlock` is the only audio-thread entry point. No locks held inside.
- Cross-thread communication is limited to: APVTS atomic params, named `std::atomic<T>` members, and `scopeFifo` (SPSC).
- Only legal audio-thread blocker: `getCallbackLock()` held during `handleAsyncUpdate → rebuildOversampling`. User-initiated (oversampling mode change), rare.
- DSP state reset from outside the audio thread: not allowed. Use `stateNeedsReset` atomic handoff.

## RT-safety
- No allocation in `processBlock`. Buffers are sized once in `prepareToPlay` / `rebuildOversampling` with `avoidReallocating = true`.
- Filter coefficient changes must never call `IIR::Coefficients::makeXxx(...)` from `processBlock`. Two RT-safe patterns are used:
  - **In-place write** (tone filter, PR-10): `writeXxxCoeffs(*filter.state, ...)`. The per-channel `IIR::Filter` was initialised in `prepare()` with a `CoefficientsPtr` that aliases `.state`, so writing through `.state` updates the coefficients the filter reads on its next `process()`.
  - **Standby-swap** (pre-HP, Sub Guard, PR-2/PR-3): `writeXxxCoeffs(*standby, ...)` then swap the `.state` / standby `Ptr` pair. CAVEAT: the per-channel `Filter.coefficients` held inside `ProcessorDuplicator.processors` is an independent `CoefficientsPtr` that is NOT reassigned by swapping `.state`. The legacy pre-HP / Sub Guard sites do not re-sync per-channel pointers, so updates land intermittently; prefer the in-place pattern for new filters. Flagged for follow-up.
- Sample-rate drift branch (non-`prepareToPlay` SR change) stays allocation-free: only `std::exp` math and `SmoothedValue::reset(double, double)` (noexcept, allocation-free in JUCE 7). Block-size changes without `prepareToPlay` are unsupported; the `dryBuffer` size jassert would trip.
- `juce::ScopedNoDenormals` wraps the block.
- Active smoothers: `smoothedOutputGain`, `smoothedGlobalMix`, `smoothedSubGuardFreq`. Other block-level parameters (`inputGain`, `distortionDrive`, `distMix`) use manual linear interpolation via `last*` endpoints. The previously-declared `smoothedLfoDepth` / `smoothedDistMix` / `smoothedToneParam` / `smoothedGainReduction` / `bypassRamp` were unused and removed in PR-11.
- RT verification is debug-only (`RT_ASSERT_SCOPE` + `operator new` override, gated by `DISTORTION_RT_GUARD`).

## Signal flow (preserve ordering)

Active path:
1. `stateNeedsReset` check → reset
2. SR-drift check → coefficient/smoother resync
3. Parameter load (+ NaN guard)
4. LFO: block-level dests update filter coeffs; per-sample dests (0/3/4) advance phase inside sample loops
5. Dry snapshot (only if global mix < 100% or smoothing)
6. Pre-HP filter (base rate, pre-upsample)
7. Upsample (4x default)
8. Pre-distortion transient tamer (always on, except Extreme)
9. Auto-gain input RMS
10. **Sub Guard**: split → distort high → recombine → measure output RMS on full signal → subtract low → post-process high only → re-add low at step 15
11. Auto-gain output RMS + compensation
12. Tone ↔ Waveshaper (order controlled by Clean Mode)
13. LA-2A compression (oversampled)
14. ISP soft clipper (-0.3 dBFS)
15. Sub Guard re-add: low band through `toneFilterLow` (phase-match), then sum
16. Downsample
17. Manual DC blocker (R=0.9995, one-pole, max 2 channels)
18. Output gain (+ per-sample LFO dest 4)
19. Output limiter (stereo-linked, -0.5 dBFS, always on)
20. Global mix with fractional dry-delay alignment via `dryDelayState`
21. Phase correlation + scope push

Bypass path (`distortion < 0.5% && !compEnabled`): output gain → limiter → global mix → scope. Everything else skipped.

## State ownership
- All DSP state is audio-thread-owned. Reset points: `prepareToPlay`, `rebuildOversampling` (under callback lock), `resetDSPState` (audio thread via flag).
- Per-channel state arrays are fixed `[2]`. Mono or stereo only; clamp at use site.
- Oversampling reconfiguration goes through `requestOversamplingRebuild` → async update. Never from the audio thread.

## Parameter flow
- APVTS is the single source of truth. UI binds via Attachments; programmatic writes use `setValueNotifyingHost`.
- Active smoothers: `smoothedOutputGain`, `smoothedGlobalMix`, `smoothedSubGuardFreq`. Oversampled-domain params (`inputGain`, `distortionDrive`, `distMix`) are linearly interpolated block-to-block via `last*` endpoints.
- State restore: `parameters.replaceState(...)` then set `stateNeedsReset`. Do not touch DSP state directly from the host thread.

## Hard invariants
- Oversampling type: `filterHalfBandPolyphaseIIR` (2x or 4x). Latency reported via `setLatencySamples`; fractional tail absorbed by `dryDelayState`.
- Tone cutoff clamped to `min(20000, oversampledSR × 0.45)`.
- Sub Guard slope zones: LR24 ≤80 Hz, LR18 92–125 Hz, LR12 ≥142 Hz, with hysteresis.
- Manual DC blocker R=0.9995 (~3.5 Hz). Do not revert to JUCE IIR DC blocking — it was unstable at low frequencies with the polyphase oversampler.
- Band-split auto-gain measures RMS on the **recombined** signal before the low band is subtracted. The sum→measure→subtract sequence is load-bearing.
- `toneFilterLow` must track `toneFilter` coefficients exactly; separate state, identical coeffs.

## Related
- [[Monolit Distortion]] — project note
- [[RT Safety Audit Plan]]
- [[Fractional Delay Compensation (DSP)]]
