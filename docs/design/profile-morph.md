# Profile Morph — design

Status: **proposal**, not implemented. Builds on PR #55 (NAM profiles as the
distortion stage, branch `feat/nam-profile-prototype`), which must land first.

## Goal

Give producers a reason to run NAM captures in Sledge rather than in a plain
NAM player: two profile slots, **A** and **B**, and a **Morph** control that
blends between them, automatable and LFO-drivable (tempo-synced). Example: an
808 that moves from a clean console-preamp capture to a fuzz capture once per bar.

Target audience is producers (bass, 808s, drums, synths, buses), not guitarists.
Sub Guard ahead of the model, the drawn EQ after it, and the LFO are what set
Sledge apart; the morph is the headline feature that uses them.

## What "morph" means

Morph is an **output blend of two models running in parallel**, not an
interpolation of network weights. Weight interpolation is undefined across
architectures and meaningless between unrelated captures even within one. At
the midpoint you hear both captures summed, the same as splitting a signal into
two amps, which producers already do. User-facing copy should say so honestly.

## Starting point in PR #55

`applyProfileChunk` already runs two models on the same input
(`activeProfile` and `incomingProfile`), applies each one's loudness gain, and
blends them linearly during a 30 ms swap after a silent warm-up. The morph turns
that transient pair into a persistent one:

| PR #55 today | Morph |
|---|---|
| `activeProfile` + transient `incomingProfile` | persistent slots A and B (a small slot array, sized for 2, written so 4 is possible later) |
| blend weight from a fade counter | blend weight from `profileMorph` (smoothed) plus LFO, per sample |
| swap requires equal island delay (`canCrossfadeTo`) | each slot padded to a common delay so A and B may be trained at different rates |
| session stores `namProfilePath` | slot A keeps `namProfilePath` (old sessions load unchanged); slot B adds `namProfilePathB` |

## Signal flow (profile mode)

```
input ─ Sub Guard split ─ input gain (Distortion Amount) ─┬─ slot A: island ─ align A ─ gain A ─┐
                                                          │                                    ├─ morph blend ─ Dist Mix ─ … EQ, comp, out
                                                          └─ slot B: island ─ align B ─ gain B ─┘
dry (for Dist Mix) ─ delayed by the common slot delay ─────────────────────────────────────────┘
```

- Both slots take the **same** model input; Distortion Amount drives both.
- `gain A/B` is each profile's existing loudness normalisation to −18 dB.
- The dry half of Dist Mix and the Sub Guard low band wait the **common** delay
  (the larger slot's total delay), not one slot's island delay.

## Alignment (the quality-critical part)

Captures from different reamp rigs commonly differ by a few samples of latency,
and some are **polarity-inverted**. Blended at 50%, that comb-filters and sounds
hollow, and users will blame the plugin. So on load, on the loader thread:

1. Feed a short, fixed probe signal (e.g. band-limited noise burst at a moderate
   level) through the freshly prepared model.
2. Cross-correlate its output against the other slot's output for the same probe
   (or against the input, to get an absolute figure per slot that doesn't need the
   other slot loaded). Take the peak lag and its sign.
3. Store per slot: `alignDelaySamples` (integer, host rate) and `polarity` (±1).
4. The audio thread delays the earlier slot so both line up, and flips polarity
   where needed. Delay-line memory is allocated at prepare, sized for a bounded
   maximum offset (proposed: 64 samples at 48 kHz, scaled by rate). An offset
   beyond the bound is left uncorrected and flagged in the UI.

**Latency budget.** The reported latency R stays fixed, as #55 guarantees.
Alignment only ever delays the *earlier* slot up to the later one, and the later
one is bounded by the island delay plus the capture's own offset. If a slot's
total delay would exceed R, it takes the same path #55 uses for a model trained at
another rate: the host is told once, at load. Loading, swapping or clearing B
must never change R by itself.

Probe results must be deterministic (fixed probe, no `juce::Random` seeding) so
tests can assert on them.

## Parameters

| ID | Range | Default | Notes |
|---|---|---|---|
| `profileMorph` | 0–100 % | 0 % | automatable; ~20 ms smoothing; ignored unless both slots are loaded |
| `lfoDestination` | adds **"Morph"** at index 5 | — | appended, so saved sessions keep their meaning; per-sample like Dist / Dist Mix / Output Gain |

**Blend law:** start linear. Both slots are already loudness-normalised and, once
aligned, well correlated at low frequencies, where linear stays flat. Measure the
level at 50% with a set of real, dissimilar captures before choosing. If the dip
is audible, switch to a partial constant-power law (between linear and sin/cos)
chosen by measurement, not by guess.

## Threading and handoff

- Each slot gets its own staged → pending → retired handoff, the same pattern as
  #55's `stagedProfile` / `pendingProfile` / `retiredProfile`, and the same
  request-id supersession rules. Loading into a slot while the morph is anywhere
  in its range uses that slot's own swap crossfade (warm-up, then 30 ms), so it
  never clicks.
- Worst case in flight: 4 models (A, A-incoming, B, B-incoming). Scratch buffers
  are sized for that in `prepareToPlay`.
- Entering profile mode (the first slot loaded) still ducks, as in #55. Loading
  the *second* slot does not change mode and does not duck.
- Clearing a slot: if the other slot is loaded, fade the cleared slot's share out
  over 30 ms, then retire it; the morph control greys out. Clearing the last slot
  leaves profile mode with the existing duck.
- Offline renders never duck (unchanged).
- No allocation or locking on the audio thread, verified by the rt_guard counter.

## CPU

v1 runs **both models whenever both slots are loaded**: twice the model cost.
Mono Input sharing from #55 (channel 1 reuses channel 0's output) still applies
to each slot, halving it on mono sources, which covers most 808/bass use.

Deferred optimisation: rest the silent slot when Morph has sat at exactly 0 % or
100 % with no LFO on it for about 1 s, reusing the right-channel rest/warm-up
pattern. The catch is that an automation jump off the endpoint then needs a
warm-up (up to 100 ms) before the resting slot is audible, so it needs its own
design and listening test.

## UI

- PROFILE area: two slot rows, **A** and **B**, each with name, load, clear, and
  an alignment indicator (✓ aligned / ⟲ polarity flipped / ⚠ offset too large).
- **Morph** knob next to Distortion Amount, greyed until both slots are loaded.
- LFO destination list gains "Morph"; the LFO arc animates on the Morph knob.
- The scope can tint the trace by morph position (nice-to-have).

## State and compatibility

- Slot A path: `namProfilePath` (unchanged). Slot B path: `namProfilePathB`.
- An old session loads into slot A with Morph at 0 % and must render identically
  to the #55 build.
- Embedding the profiles themselves in presets remains the separate follow-up
  from #55; the morph doubles its value, since a morph preset is useless without
  both captures.

## Tests

With the identity and test models already used by the NAM Profile suite:

- Morph 0 % is bit-identical to slot A alone; 100 % is bit-identical to slot B alone.
- Same model in both slots: output level flat within 0.1 dB across the whole range.
- A 44.1 kHz-trained and a 48 kHz-trained slot line up at lag 0 at host rates
  44.1, 48 and 96 kHz.
- Alignment detection: a copy of a model delayed by N samples, and a
  polarity-inverted copy, are both detected and corrected; an offset beyond the
  bound is flagged and left alone.
- Reported latency R is unchanged across loading, swapping and clearing B
  (IIR and linear-phase oversampling, at 44.1/48/96 kHz).
- Zero audio-thread allocations while the LFO sweeps Morph, during a swap into B
  at 50 %, and while clearing B.
- Swap into B at 50 %: no step larger than the existing swap-crossfade tolerance.
- Mono Input on: two-slot outputs identical with channel 1 resting.
- State round trip of both paths; old-session compatibility as above.
- Block-size independence down to 1 sample, as for the island.

## Phases

0. **Land PR #55** (its listening checks are still open).
1. **DSP core**: slot array, `profileMorph`, common-delay padding, alignment probe,
   per-slot handoff, both models always running. Tests above.
2. **LFO → Morph**: destination index 5, per-sample.
3. **UI and state**: slot rows, Morph knob, `namProfilePathB`.
4. **Later**: slot resting (CPU), per-slot input trim (captures react
   differently to the same level), the built-in Sledge clip as a morph slot
   (hard: the built-in path runs oversampled, the profile path at 1x), profiles
   embedded in presets.

## Open decisions

1. **Two slots, or four with an XY pad?** Recommendation: ship two, and write the
   slot array so four is possible later (4× model CPU).
2. **Empty slot B:** Morph disabled (recommended), or "blend to dry" (duplicates
   Dist Mix).
3. **CPU:** accept 2× model cost in v1, or pull slot resting into phase 1.
4. **Alignment reference:** align each slot to the input (absolute, works with one
   slot loaded) or to each other (exact for the pair). Recommendation: to the
   input, since it also lets the UI show a per-capture offset.
