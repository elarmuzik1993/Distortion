# Profile Morph — design

Status: **proposal**, not implemented. Builds on PR #55 (NAM profiles as the
distortion stage, branch `feat/nam-profile-prototype`), which must land first.
Revised 2026-09-28 after a review against the #55 code: fixed target latency,
alignment to the input for every slot, per-slot load requests, defined
fill/clear transitions, level matching, CPU figures, a capture survey before
the DSP work.

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
| `activeProfile` + transient `incomingProfile` | persistent slots A and B (a small slot array, sized for 2, written so 4 is possible later), each with its own active + incoming pair |
| blend weight from a fade counter | the fade counter stays, inside each slot; Morph weights the two slots (see Blend) |
| swap requires equal island delay (`canCrossfadeTo`) | every profile is padded so its output lands at R; any profile that fits R crossfades |
| dry lines wait the island delay D | dry lines wait R |
| one `profileRequestId`, `profileStatus`, `clearRequested` | one of each per slot |
| session stores `namProfilePath` | slot A keeps `namProfilePath`; slot B adds `namProfilePathB` |

## Signal flow (profile mode)

```
input ─ Sub Guard split ─ input gain (Distortion Amount) ─┬─ slot A: island ─ pad to R ─ ±1 ─ gain A ─ ×wA ─┐
                                                          │                                                 ├─ Σ ─ Dist Mix ─ … EQ, comp, out
                                                          └─ slot B: island ─ pad to R ─ ±1 ─ gain B ─ ×wB ─┘
dry (for Dist Mix) ─ delayed by R ──────────────────────────────────────────────────────────────────────────┘
```

- Both slots take the **same** model input; Distortion Amount drives both.
- `gain A/B` is each profile's loudness normalisation to −18 dB (see Level).
- `pad to R` and `±1` come from the profile's alignment (see Alignment). They
  belong to the profile, not the slot, so an incoming capture brings its own.
- The dry half of Dist Mix, and the Sub Guard low band and global dry that join
  later, wait R, which never changes while profile mode is on.

## Latency

**The rule.** R = max(user oversampler latency, reserve + H), whatever runs, as
in #55 (one figure, reported always). `reserve` is #55's island delay for a
48 kHz model at the host rate. H is the alignment headroom: the latest a capture
may be and still be aligned, set as a time from the capture survey (phase 1a).

In profile mode every profile is padded so its output lands at exactly R:
`pad = R − D − o`, where D is its island delay and o its measured offset in host
samples. Consequences:

- Filling, swapping or clearing a slot never changes R, the dry lines, or the
  other slot's delay. No duck, no latency change, no click in the other slot.
- In profile mode the output pad is 0: the delay #55 added after the whole chain
  now sits inside the profile stage (see State and compatibility).
- A profile that doesn't fit (D + o > R) takes #55's existing path for a model
  whose island delay exceeds the reserve: a duck, and R rises for as long as it
  is loaded.
- A 44.1 kHz and a 48 kHz capture blend as long as both fit. Island delays from
  `ResamplingIsland::latencyFor`:

  | Host rate | reserve (48 kHz model) | 44.1 kHz-trained model |
  |---|---|---|
  | 44.1 kHz | 25 | 0 |
  | 48 kHz | 0 | 28 |
  | 96 kHz | 48 | 54 |

  With H at 1 ms (48 samples at 48 kHz), the 44.1 kHz model fits at every rate
  here with up to 20 samples of offset at 48 kHz.
- **Cost:** every instance reports H more than #55 unless its oversampling
  setting already covers reserve + H, whether or not a profile is loaded. That
  is why H comes from measurement and should be small (open decision 5).

The pad lines are allocated in `NamProfile::prepare` (loader or message thread,
never the audio thread) with room for R, so they travel with the profile
through the handoff. `updateLatencyPlan` sizes the dry lines for R.

## Alignment (the quality-critical part)

Captures can differ by a few samples of latency, and some are
**polarity-inverted** (many real amps invert). Blended at 50 %, that
comb-filters or partly cancels, and users will blame the plugin. The same
problem already exists in #55 for a single capture: an inverted one partly
cancels against the Sub Guard low band around the crossover and against the
Dist Mix dry, and a late one combs against the Dist Mix dry.

So every profile, a lone one included, is aligned **to the input**:

1. On load, on the loader thread, before `prepare()` prewarms it: run a fixed
   probe through channel 0's model at its trained rate, without the island.
   The probe is band-limited noise in the low-mids, where the two captures will
   actually correlate, generated from a fixed seed (never time-seeded) so tests
   can assert on the result. Run it at two input levels Distortion Amount
   produces, e.g. −17 dB (the 20 % default) and −6 dB (50 %).
2. Cross-correlate each output against the probe over lags 0 to H. Take the
   peak lag and its sign.
3. Accept the result only if the normalised peak clears a confidence threshold
   (set by the survey) and both levels agree (lag within one sample, same sign).
   Otherwise the profile is "not measured": offset 0, polarity as captured,
   which is exactly #55's behaviour.
4. Store on the profile, as time: `alignOffsetSeconds`, `polarity` (±1) and
   whether it was measured. Host samples are derived in `prepare()`, so a
   host rate change needs no re-probe. (`prepareProfilesForHost` re-prepares
   under the callback lock; a probe must never run there.)
5. An offset beyond H is left uncorrected and flagged.

Each slot also gets a **manual override**: flip polarity, and nudge ±N samples.
It is saved with the session next to the slot's path and wins over the probe.

Captures are nonlinear and dispersive (tone stacks, cab sims), so the peak lag
is an energy-weighted estimate, not an exact delay. The survey (phase 1a) shows
how well it works on real captures and whether automatic alignment is worth
building at all; if offsets and inversions turn out to be rare, v1 ships with
the manual override only.

## Level

- Profiles without loudness metadata get `outputGain` 1 in #55. For a morph
  that means a level jump between slots. Estimate their loudness from the probe
  pass and normalise to the same −18 dB target, marked as estimated.
- Loudness metadata is measured at one input level. Distortion Amount moves
  the model input from −24 to +12 dB, and captures respond differently: a clean
  preamp scales with the input, a fuzz stays compressed. At low amounts, the
  goal's clean → fuzz example risks being mostly a volume swell. Options, to be
  chosen by listening (open decision 6):
  - per-slot input trim in v1 (moved up from phase 4);
  - a slow relative level match: both slots run anyway, so track each one's
    short-term level (~300 ms) and scale B toward A. Cheap, but it acts on the
    difference like a slow compressor, so it can pump where the captures'
    dynamics differ;
  - leave it to Auto Gain, which evens the overall level as Morph moves but not
    the balance between the slots in between.

## Parameters

| ID | Range | Default | Notes |
|---|---|---|---|
| `profileMorph` | 0–100 % | 0 % | automatable; ~20 ms smoothing; no effect unless both slots are loaded |
| `lfoDestination` | adds **"Morph"** at index 5 | — | see below |

**LFO destination.** `lfoDestination` is an `AudioParameterChoice`. Saved
sessions store the index, so they keep their meaning. Host automation that
stores the normalised value does not: with six entries instead of five, an
automated Dist Mix destination (0.75) plays as Output Gain and Output Gain (1.0)
plays as Morph. Either say so in the release notes, or give Morph its own LFO
amount instead of a destination entry (open decision 7).

Morph modulation is per sample: 5 joins `perSampleLFO` and the condition in
`advanceSampleControls`, which returns the modulated morph (same scaling as
Dist: ±50 points at 100 % depth, clamped). The phase advances there and nowhere
else, in built-in mode too (the arc keeps moving, nothing audible changes).
The code already warns against advancing it twice.

## Blend

Each slot has a presence `p` (0 or 1 when settled). It ramps 0 → 1 over 30 ms
after the warm-up when an empty slot is filled, and 1 → 0 over 30 ms when a slot
is cleared while the other is loaded. With `m` the smoothed Morph plus LFO,
clamped to 0–1:

```
wA = pA · (1 − pB · m)
wB = pB · (1 − pA · (1 − m))
```

- Both loaded: `wA = 1 − m`, `wB = m`.
- One loaded: it plays at full share, whatever Morph says.
- Filling a slot: it fades in to its share while the other fades to its own.
- Clearing B at Morph 100 %: a 30 ms crossfade to A, not a fade toward silence.

Inside each slot, #55's swap blend is unchanged: its active and incoming
profiles, each with its own pad, polarity and gain, crossfade after the warm-up.

**Blend law:** start linear. Both slots are loudness-normalised and, once
aligned, well correlated at low frequencies, where linear stays flat. The survey
measures the level at 50 % on real, dissimilar pairs. If the dip is audible,
switch to a partial constant-power law (between linear and sin/cos) chosen by
measurement, not by guess.

## Threading and handoff

- Each slot gets its own staged → pending → retired handoff, the same pattern as
  #55's `stagedProfile` / `pendingProfile` / `retiredProfile`, and **its own**
  request id, `clearRequested` and status. In #55 every load bumps the single
  `profileRequestId` and `loadProfileNow` drops a load whose id is stale, so with
  a shared id, loading B would cancel an in-flight load into A. Restoring a
  session with both paths does exactly that.
- Shared: profile mode, the duck, the loader thread (one thread, so loads run
  one after the other), and the Mono Input right-channel state (its warm-up
  covers every running profile's settle time).
- Filling or swapping a slot whose profile fits R crossfades (warm-up, then
  30 ms); the check replaces #55's equal-island-delay test in
  `routeStagedProfile` and `canCrossfadeTo`.
- Entering profile mode (the first slot loaded) still ducks, as in #55. Filling
  the second slot does not change mode and does not duck.
- Clearing a slot while the other is loaded: its presence ramps out (see Blend),
  then it is retired; the Morph control greys out. Clearing the last slot leaves
  profile mode with the existing duck.
- **Session restore:** `setStateInformation` requests both loads, and the timer
  holds routing until both have finished or failed, so one rebuild installs the
  pair. A session never starts, or bounces, with half a pair.
- Worst case in flight: 4 models (A, A-incoming, B, B-incoming). The scratch grows
  from 9 channels to 14 (model input 2, four outputs × 2, mix, morph, dry 2),
  sized in `prepareToPlay`.
- `feedProfileWhileBypassed` feeds every running profile, both slots included.
- Offline renders never duck (unchanged). A bounce that starts before an async
  load has finished still renders without it, as in #55.
- No allocation or locking on the audio thread, verified by the rt_guard counter.

## CPU

v1 runs **both models whenever both slots are loaded**. #55 measured a WaveNet
profile at about 9 % of a core per channel with NAM's fast tanh. Per instance:

| Case | Stereo | Mono Input |
|---|---|---|
| one slot | ~18 % | ~9 % |
| two slots | ~36 % | ~18 % |
| swapping both slots at once | ~72 % | ~36 % |

Mono Input sharing from #55 (channel 1 reuses channel 0's output) applies to
each slot, which covers most 808 and bass use.

Deferred optimisation: rest the silent slot when Morph has sat at exactly 0 % or
100 % with no LFO on it for about 1 s, reusing the right-channel rest/warm-up
pattern. The catch is that an automation jump off the endpoint then needs a
warm-up (up to 100 ms) before the resting slot is audible, so it needs its own
design and listening test.

## UI

- PROFILE area: two slot rows, **A** and **B**, each with name, load, clear, an
  alignment indicator (✓ aligned / ⟲ polarity flipped / ⚠ offset beyond H, not
  corrected / ? not measured) and the manual override (flip, ±N samples).
- **Morph** knob next to Distortion Amount, greyed until both slots are loaded.
- LFO destination list gains "Morph"; the LFO arc animates on the Morph knob.
- The scope can tint the trace by morph position (nice-to-have).

## State and compatibility

- Slot A path: `namProfilePath` (unchanged). Slot B path: `namProfilePathB`.
  Each slot's manual alignment override is saved next to its path.
- An old session (no `namProfilePathB`, no `profileMorph`) loads into slot A
  with B empty and Morph at 0 %, even into an instance where Morph was elsewhere.
- It is not sample-identical to the #55 build. The pad now sits inside the
  profile stage, where #55 delayed after the whole chain, so Auto Gain's
  envelopes, automation and LFO on the stages after the profile meet the audio
  up to R − D samples later. And a capture measured late or inverted plays
  corrected: earlier by its offset, or flipped. With Auto Gain off, parameters
  static and a capture at offset 0 and normal polarity (or not measured), it is
  identical when R is unchanged. All of this only matters if #55 ships in a
  release before the morph (open decision 8).
- A session saved with B loaded, opened in the #55 build, plays slot A only.
- Embedding the profiles themselves in presets remains the separate follow-up
  from #55; the morph doubles its value, since a morph preset is useless without
  both captures.

## Tests

With the identity and test models already used by the NAM Profile suite:

**Blend**
- Morph 0 % is bit-identical to slot A alone; 100 % is bit-identical to slot B
  alone (same R, so no shift).
- Same model in both slots: output level flat within 0.1 dB across the whole
  range.
- Two different test models: level across the range stays within the bound the
  chosen law promises.
- Filling B at Morph 70 %, clearing B at 100 % (crossfades to A), swapping A at
  50 %: no step larger than the existing swap-crossfade tolerance.

**Latency and alignment**
- A 44.1 kHz-trained and a 48 kHz-trained slot line up at lag 0 at host rates
  44.1, 48 and 96 kHz.
- R is unchanged across filling, swapping and clearing either slot, mixed rates
  included, for IIR and linear-phase oversampling at 44.1/48/96 kHz. A profile
  that doesn't fit R takes the duck path and raises R, once.
- Alignment detection: a copy of a model delayed by N samples and a
  polarity-inverted copy are both detected and corrected; an offset beyond H is
  flagged and left alone; a model whose output doesn't correlate with its input
  is "not measured" and left as captured. Results are the same on every run.
- A host rate change from 48 to 96 kHz keeps the pair at lag 0 without a
  re-probe.
- A file without loudness metadata gets an estimated gain.

**Handoff and state**
- Restoring a session with both paths loads both, installed in one rebuild.
- A load into B while A's load is in flight doesn't cancel A.
- Zero audio-thread allocations while the LFO sweeps Morph, while filling B,
  during a swap into B at 50 %, and while clearing B.
- True bypass at Morph 50 %: both slots keep running, and coming back replays
  nothing stale.
- Mono Input on: two-slot outputs identical with channel 1 resting.
- LFO → Morph: the phase advances once per sample, in built-in and profile mode,
  at the same rate as the Dist destination.
- State round trip of both paths and overrides; an old session sets Morph to 0 %
  and leaves B empty; with Auto Gain off and static parameters, the test models
  (offset 0, normal polarity) render identically to #55.
- Block-size independence down to 1 sample, as for the island.

## Phases

0. **Land PR #55** (its listening checks are still open).
1. **Survey, then DSP core.**
   - **1a. Capture survey.** An offline tool next to `RenderHarness` runs the
     probe over 20–30 real captures from different rigs and reports offset,
     polarity, confidence, estimated loudness, and the 50 % level dip for
     pairs. It sets H, the confidence threshold and the blend law, and decides
     whether automatic alignment is worth building.
   - **1b. DSP core**: slot array, per-slot handoff and request ids, R-based
     padding, presence and Morph weights, level handling, both models always
     running, bypass feeding. Alignment from the probe, or manual only if the
     survey says so. Tests above.
2. **LFO → Morph**: destination index 5, per-sample.
3. **UI and state**: slot rows, alignment indicator and override, Morph knob,
   `namProfilePathB`.
4. **Later**: slot resting (CPU), per-slot input trim (unless it moved to v1),
   the built-in Sledge clip as a morph slot (hard: the built-in path runs
   oversampled, the profile path at 1x), profiles embedded in presets.

## Decisions

1. **Two slots**, with the slot array written so four (and an XY pad) is
   possible later, at 4× model CPU.
2. **Empty slot:** Morph disabled, not "blend to dry" (that duplicates Dist Mix).
3. **CPU:** accept 2× model cost in v1 (figures above); slot resting stays
   deferred.
4. **Alignment reference:** the input, for every slot including a lone one. It
   works with one slot loaded, lets the UI show a per-capture offset, never
   depends on load order, and fixes #55's cancellation against the Sub Guard low
   band and the Dist Mix dry.

## Open decisions

5. **H:** every instance pays it in reported latency, profile or not, unless the
   oversampling setting covers it. Set from the survey; the smallest value that
   covers most captures. The alternative, adding H only in profile mode, breaks
   #55's rule that R never changes.
6. **Level between slots:** input trim in v1, slow relative level match, or
   Auto Gain alone. Decided by listening to clean → fuzz pairs across
   Distortion Amount.
7. **LFO → Morph:** append "Morph" to `lfoDestination` with a release note
   (recommended: automating the destination is rare), or a separate LFO → Morph
   amount, which keeps the choice list and lets one LFO move Dist and Morph
   together, at the cost of another control.
8. **If #55 ships first:** a lone capture with a measured offset or inversion
   plays corrected, not as #55 played it. Recommended: accept, since it is a
   fix. Keeping #55's timing for a lone slot would change its delay when a
   second slot arrives.
