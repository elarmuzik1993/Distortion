# Randomize Button — Realign with Current Parameter Architecture

**Date:** 2026-06-06
**Status:** Approved (design)
**Scope:** Editor-side only (`Source/PluginEditor.cpp` / `.h`). No DSP, parameter-layout, or test-signature changes.

## Problem

`PluginEditor::randomizeAllParameters()` and the Randomize button's right-click lock
menu have drifted from `PluginProcessor::createParameterLayout()`. The randomizer uses
two stale value ranges and ignores several parameters that were added to the plugin
since it was written.

### Stale ranges
- `highPassFreq` is now a full multimode **Filter Frequency** (20–20000 Hz, skewed),
  but randomize still uses the old hi-pass range `20–500`.
- `subGuardFreq` is now `0–200 Hz` with `0 = OFF`, but randomize uses `50–200`
  (never OFF, wrong floor).
- `lfoRate` layout range is `0–10`; randomize uses `0.1–10`.

### Parameters the randomizer ignores
`filterMode`, `lfoWaveform`, `lfoInvert`, `waveshaperMix`, `tone`, `cleanBoost`
(sound-shaping — should be randomized) and `waveshaperClean`, `linearPhaseDry`,
`lfoBpmSync`, `lfoBpmDivision`, `autoGainEnabled` (mode/quality/transport — stay fixed).

## Decision

Adopt a **curated sound subset**: randomize all sound-shaping parameters with musical
ranges; leave system/quality/transport toggles fixed so results stay usable.

### Randomized set (after change)

| Param | Type | Range / Choices | Change |
|-------|------|-----------------|--------|
| inputGain | float | 0–100 | keep |
| outputGain | float | 0–100 | keep |
| distortionAmount | float | 0–100 | keep |
| highPassFreq | float | **20–2000** | fix range |
| subGuardFreq | float | **0–200** | fix range (0 = OFF reachable) |
| filterMode | choice | 3 | **add** |
| clipType | choice | 7 | keep |
| distMix | float | 0–100 | keep |
| tone | float | **2000–20000** | **add** |
| waveshaperMix | float | **0–100** | **add** |
| lfoRate | float | **0–10** | fix range |
| lfoDepth | float | 0–100 | keep |
| lfoWaveform | choice | 5 | **add** |
| lfoDestination | choice | 5 | keep |
| lfoEnabled | bool | — | keep |
| lfoInvert | bool | — | **add** |
| compPeakReduction | float | 0–100 | keep |
| compMakeupGain | float | 0–100 | keep |
| compRatio | choice | 2 | keep |
| compEnabled | bool | — | keep |
| extremeEnabled | bool | — | keep |
| cleanBoost | bool | — | **add** |
| globalMix | float | 50–100 | keep |

### Left fixed (NOT randomized)
`autoGainEnabled` (**removed** from randomize), `waveshaperClean`, `linearPhaseDry`,
`lfoBpmSync`, `lfoBpmDivision`.

## Lock infrastructure

Keep locks in sync with the randomizable set:
- Add the new randomizable params to the `parameterLocks` init map.
- Add right-click lock-menu entries for the new params.
- Fix stale menu label `"Hi-Pass Filter"` → `"Filter Frequency"`.

**Out of scope:** new visual lock *icons* beside the new knobs (a layout task).
`isParameterLocked()` returns `false` for any unmapped ID, so existing icon wiring and
all randomization remain safe. Locking the new params works via the right-click menu.

## Verification

- Build `Distortion_Standalone` (Debug) — must compile clean.
- No unit-test signature changes; randomize lives in the editor and has no existing
  unit coverage. Existing `DistortionTests` suite must still pass.
