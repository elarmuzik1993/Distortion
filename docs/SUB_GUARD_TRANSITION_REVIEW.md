# Sub Guard: transition artifacts and a clean sub

Status: sections 1-4 are a static read of `d924b6d`. Since then #73 has measured them: F1,
F2, F3, F5 and F8 fail as predicted; F6 passes and is now a regression check. The review
pass at `d924b6d` added F8 and corrected the trigger analysis for F2, F3 and F5. Section 2a
is different: it was measured from the start. It asks whether the sub really stays
untouched while Sub Guard is on.

Scope: `PluginProcessor::applySubGuardSplit` and its helpers in
`Source/PluginProcessor.cpp`, plus the state declared in `Source/PluginProcessor.h`.
Code is referenced by function name, checked against `main` at `d924b6d`. Line
numbers drift, so search for the function instead.

## 1. How Sub Guard works (context)

Sub Guard is a split-band distortion. Below the crossover frequency the signal
stays clean; above it the signal is distorted. The parameter is `subGuardFreq`
(0 = off, up to 200 Hz, default 60 Hz).

Per block, in the oversampled domain:

1. `applySubGuardSplit()` copies the input into `lowBandBuffer` and `highBandBuffer`,
   filters them into a Linkwitz-Riley low and high band, distorts only the high band
   (or runs the NAM profile stage on it), then writes `low + high` back to the main block.
2. `processBlock` then subtracts the low band from the main block again, so the waveshaper, tone filter,
   LA-2A and soft clipper see only the distorted high band.
3. `applyAutoGainAndISP` runs those stages, applies a mirror tone filter
   (`toneFilterLow`) to the low band, and adds the low band back after the soft clipper.

The crossover slope depends on frequency zone:

| Order | Filters | Chosen by `determineSubGuardFilterOrder` |
|---|---|---|
| LR24 | 2 cascaded biquads per band (`lowPassFilter1/2`, `highPassFilter1/2`) | `freq <= 80`, and `80 < freq < 92` |
| LR18 | 1st order + biquad per band (`subGuardLP18_*`, `subGuardHP18_*`) | `92 <= freq <= 125`, and `125 < freq < 142` |
| LR12 | 1 biquad per band (`subGuardLP12`, `subGuardHP12`), high band polarity-inverted | `freq >= 142` |

Slope changes crossfade over `SUBGUARD_CROSSFADE_TIME_S` (10 ms). Frequency changes are
smoothed over `SUBGUARD_FREQ_SMOOTH_TIME_S` (50 ms), with the smoother advanced once per
block (`prepareToPlay` re-rates it to the block rate).

Relevant state (`PluginProcessor.h`):
`currentSubGuardOrder`, `smoothedSubGuardFreq`, `lastSubGuardFreq`, `sgCrossfadeActive`,
`sgCrossfadePos`, `sgFromOrder`, `sgToOrder`, `sgWasActive`.

Existing test: `SubGuardOrderCrossfadeTest::runTest` in
`Source/Tests/DistortionTests.cpp`. See section 6 for why it is weak.

How the parameter actually moves matters for which findings a user can hit:

- `subGuardFreq` is continuous 0-200 Hz, so host automation can reach any value,
  including 1-25 Hz (active, very low crossover).
- The editor snaps it (`subGuardSlider.onValueChange` in `PluginEditor.cpp`): anything
  at or below 25 Hz snaps to 0 (off), 52-68 to 60, 92-108 to 100, and 142-158 to 150.
  A knob drag therefore cannot rest on either order boundary (92 and 142 Hz both snap
  away), and every downward drag ends in a jump from 26 Hz or more straight to off.
- Sub Guard is not an LFO destination. Presets, Randomize and session loads move it in a
  single jump.

## 2. Findings, in priority order

Severity means how likely a user is to hear it, not how hard the fix is.

### F1. Turning Sub Guard on resumes stale filter state (high)

Code: the OFF path and the OFF-to-ON snap in `applySubGuardSplit`.

While `pb_subGuardFreq <= 1`, the function takes the OFF path and returns. None of the
crossover banks are run or reset there. The mirror filter `toneFilterLow` is only
processed when `pb_subGuardActive` (`applyAutoGainAndISP`), so it also
holds stale state while Sub Guard is off.

On OFF to ON the code does:

```cpp
if (!sgWasActive)
{
    currentSubGuardOrder = targetOrder;
    sgCrossfadeActive = false;
    subGuardLowDelayLine.reset();   // only the delay line is cleared
}
```

The filter banks and `toneFilterLow` resume from whatever they held when Sub Guard last
ran, possibly minutes earlier. That leaks a burst of old signal into the low band and,
through the high-pass banks, into the distortion.

Related and unverified:

- `smoothedSubGuardFreq.setTargetValue` is only called on the ON path, so after a long
  OFF period the smoother glides from the last active frequency to the new one. The first
  ~50 ms may use the wrong crossover.
- Auto-gain switches its input reference from the full-range signal to the high band when
  Sub Guard turns on (`processBlock` and the end of `applySubGuardSplit`).
  The input envelope may jump.
- `resetDSPState` has the same snap-without-reset problem for two of the three banks.
  That is F8.

Test: loud sine (for example 80 Hz at amplitude 0.5) with Sub Guard on, then set
`subGuardFreq` to 0 for some blocks, then back to 60. Assert the peak inter-sample step
in the first blocks after re-enabling stays close to the steady-state slew.

### F8. A state load leaves the LR12 and LR18 banks stale (high)

Finding IDs are stable; F8 was added later but belongs here in priority order.

Code: `resetDSPState`, reached at runtime from `processBlock` when `stateNeedsReset` is set
(`setStateInformation`, so any preset or session load), and from the oversampler rebuild.

`resetDSPState` resets the LR24 bank (`lowPassFilter1/2`, `highPassFilter1/2`) and
`toneFilterLow`, then sets `sgWasActive = false`. It never resets the LR12 bank
(`subGuardLP12`, `subGuardHP12`) or the LR18 bank (`subGuardLP18_1/2`, `subGuardHP18_1/2`).
On the next active block the OFF-to-ON snap in `applySubGuardSplit` picks the target order
without resetting it either, so with Sub Guard at 100 Hz (LR18) or 150 Hz (LR12) the first
block after a state load runs a bank holding whatever it last processed. The same
stale-state pop as F1, on every preset change while playing.

The oversampler rebuild re-prepares and resets all three banks before it calls
`resetDSPState`, so that path is clean. `prepareToPlay` is also clean.

Fix: reset all three banks in `resetDSPState` (`resetSubGuardOrderFilters` for each order
already does the work). Cheap and independent of the larger F1/F2 design, so it can land
first. Better still, have the OFF-to-ON snap reset the target bank itself, which covers F8
and the filter-bank half of F1 in one place.

Test: Sub Guard at 150 Hz with a loud 80 Hz sine for some blocks, then call
`setStateInformation` with the same state (Sub Guard still 150 Hz) and process on. Assert
the peak step in the first block after the reset stays near baseline. Repeat at 100 Hz
(LR18) and 60 Hz (LR24, the control that should already pass).

### F2. Turning Sub Guard off is a hard switch (high)

Code: the OFF path at the top of `applySubGuardSplit`.

The output changes from `clean low + distorted high` to `distortion(full range)` at a
block boundary. There is no fade. With sub-frequency content present the two waveforms
differ, so the output has a step discontinuity at the switch. Downstream state also flips
at once: the subtract and add-back of the low band stop, and `toneFilterLow` stops being
processed.

This is the most common transition in practice: the editor snaps anything at or below
25 Hz to 0, so every time a user drags Sub Guard down to switch it off, the output jumps
from a crossover of 26 Hz or more straight to full-range distortion. Unchecking it by
automation or loading a preset with Sub Guard off does the same.

Constraint for any fix: `advanceSampleControls`, `inputGainRamp`, `driveRamp`,
`distMixRamp`, the LFO phase and the harmonic-density envelopes are consumed per sample
by the distortion stage. Running two distortion passes on the same block to crossfade
between them would double-advance that state. A fade has to be structured so the
distortion runs once per block. See section 4.

Test: sub tone plus a mid tone, Sub Guard on, then off. Assert the peak step at the switch
stays below a threshold derived from the steady-state slew.

### F3. Reversing the order change mid-crossfade corrupts filters (medium)

Code: the crossfade start and retarget in `applySubGuardSplit`.

```cpp
if (!sgCrossfadeActive) { ... start A->B ... }
else if (targetOrder != sgToOrder)
{
    sgFromOrder = currentSubGuardOrder;   // still the ORIGINAL order, not the blend
    sgToOrder   = targetOrder;
    sgCrossfadePos = 0;
    resetSubGuardOrderFilters(sgToOrder);
}
```

`currentSubGuardOrder` only updates when a fade completes. So while
fading A to B:

- If the target returns to A: `sgFromOrder == sgToOrder == A`. The code resets the A bank,
  which is the one carrying live audio in the "from" slot, so the main path gets a
  zero-state transient. Then `filterSubGuardBands(A, lowBandBuffer, highBandBuffer)` and
  `filterSubGuardBands(A, lowBandBufferB, highBandBufferB)` run the same bank over two
  copies of the same block back to back. The second pass starts from the state left by
  the first, so its output is wrong, and the state ends up advanced by two blocks.
- If the target becomes a third order C: the blend weights snap back to `wFrom = 1` on A,
  discarding the partial fade toward B, and the B bank state is thrown away.

Trigger: the smoothed frequency crossing 92 Hz or 142 Hz and coming back within one
10 ms crossfade. The knob cannot do this: it snaps across both boundaries (section 1),
and the 50 ms smoother turns a snap into one monotonic glide. Sub Guard is not an LFO
destination. That leaves host automation that wobbles around a boundary (a drawn curve,
a hardware controller mapped through the host). Real, but rarer than F1, F2 and F8, so
the severity is medium.

Test: hold `subGuardFreq` at 90 Hz, then alternate 90 and 94 Hz every block or two for
many blocks with a sine near 80 Hz. Assert no discontinuity and no NaN or blow-up.
This should also run under a sanitizer or with the `DISTORTION_RT_GUARD` build.

### F4. The incoming bank starts from zero state (medium)

Code: the two `resetSubGuardOrderFilters(sgToOrder)` calls in `applySubGuardSplit`.

The comment says this hides the stale-state pop. It does, but the incoming bank starts
with no history while the signal is already mid-flight:

- Its low band has not settled when the 10 ms fade ends. The argument is settling time,
  not group delay: an LR24 at 60 Hz has a low-frequency group delay of about
  2*sqrt(2)/(2*pi*60) = 7.5 ms, under the fade, but its step response rings for several
  cycles of 60 Hz (tens of ms). So the low band dips during and after the fade.
- Its high-pass banks pass a low-frequency transient in the first samples. That energy
  enters the distortion as an audible thump, weighted by the ramping `wTo`.

Test: steady 50 Hz sine at moderate level, cross 92 Hz once, and compare the low-band
RMS in the 50 ms after the crossfade with steady state.

### F5. The order selection has no hysteresis (low-medium, amplifies F3)

Code: `determineSubGuardFilterOrder`.

The comment says "with hysteresis", but the function is stateless. The effective
boundaries are exactly 92 Hz (LR24 to LR18) and 142 Hz (LR18 to LR12). Any small motion
across a boundary retriggers a crossfade. The editor snaps 92-108 Hz to 100 and
142-158 Hz to 150 (section 1), so the knob jumps over both boundaries and cannot rest on
them. Only host automation can hold the frequency at a boundary.

### F6. Coefficients change once per block (low-medium)

Code: the coefficient update in `applySubGuardSplit` and `updateSubGuardCoefficients`.

The smoother is consumed once per block and coefficients are rewritten only if the
frequency moved more than 0.5 Hz. Small blocks give many small steps. Large host buffers
(1024 samples and up) turn a fast move into one coefficient step, which excites the
transposed-direct-form IIR state. Low priority, but easy to verify with a large-buffer
version of the sweep test.

### F7. Blending different orders is not exactly flat (low)

Code: the dual-bank blend in `applySubGuardSplit`.

LR12, LR18 and LR24 each sum flat on their own, but they differ in phase. A linear blend of
their band outputs can dip slightly near the crossover during the 10 ms fade. The code
comment justifies linear over equal-power because the bands are correlated. The dip is
probably inaudible. Note only.

## 2a. Is the sub untouched while Sub Guard is on? (measured)

The goal of Sub Guard is that everything below the crossover leaves the plugin exactly as
it came in, only delayed, while everything above it is distorted. These findings were
measured, not just read.

**Method.** `DistortionRender` (Release, Linux, 44.1 kHz, 512-sample blocks, default clip
type, mix 100) rendered two 4-second stereo inputs, analysed over the last 2 seconds:

- *sub*: a 45 Hz sine at −6 dBFS;
- *sub + mids*: the same sine plus 1000 Hz and 1037 Hz at −16.5 dBFS each. Their 37 Hz
  beat shows anything that couples the mids' envelope into the low end.

Sub Guard was set to off, 60, 100 and 150 Hz, with drive 0, 30 and 70. Drive 0 (true
bypass) is bit-transparent at every setting. With any drive on:

| Sub Guard, drive 70 | 45 Hz level | 3rd harmonic (135 Hz) | 8 Hz product (sub + mids) |
|---|---|---|---|
| 150 Hz, today | −3.2 dB (sub), −4.5 dB (sub + mids) | −45 dBFS | −41 dBFS |
| 150 Hz, transient tamer disabled (experiment) | −0.2 dB / −0.3 dB | −62 / −71 dBFS | −37 dBFS |
| 60 Hz, today | −3.2 / −4.7 dB | −37 dBFS | −37 dBFS |
| 60 Hz, transient tamer disabled | −0.2 / −0.5 dB | −34 dBFS | −30 dBFS |

### C1. The transient tamer compresses the sub (high, measured)

`applyPreCompression` (1 ms attack, 50 ms release, −12 dB threshold, 2.5:1) runs on the
full signal before the split whenever the drive is on. A loud sub is turned down by about
3 dB even at 150 Hz, where it is nowhere near the crossover. Its 1 ms attack follows the
waveform, so it also adds harmonics to the sub. And because sub and mids share one gain,
each bounces the other's level ("pumping"). With the tamer disabled the sub keeps its
level to within 0.2-0.5 dB, and at 150 Hz its added harmonic falls by 17-26 dB (at 60 Hz
the crossover leak, C3, dominates instead).

### C2. Distortion products land below the crossover (high, measured)

The high band is high-passed only *before* the drive. The drive then creates new low
frequencies (here 45 − (1037 − 1000) = 8 Hz, at −30 to −41 dBFS; also difference tones
and offsets from asymmetric clip types), and nothing removes them, so they sum straight
into the "clean" low end. This is the 8 Hz column above. The tamer doesn't explain it:
the product stays with the tamer disabled.

### C3. Sub near the crossover is shared, by design of any crossover (inherent)

At its own frequency a Linkwitz-Riley crossover sends half the signal (−6 dB) to each band.
Today's high-band filters let through (computed): at the 60 Hz setting, 50 Hz at −9.8 dB
and 40 Hz at −15.7 dB; at 150 Hz with the gentle LR12, 80 Hz at −13 dB. With drive, a
−10 dB leak is fully saturated. No filter makes the region just below the crossover
untouched. Steeper slopes narrow it, and the user has to set the crossover above the
sub's fundamental (an 808 at 50 Hz needs about 80 Hz or more, not 60).

### C4. Clean Boost changes the sub's level (medium, read)

`applyCleanBoostEmphasis` lifts the full signal by up to 6 dB and adds a treble boost
before the split. `applyCleanBoostDeEmphasis` then cuts the treble on low + high
*together*, but `processBlock` subtracts the low band as it was *before* the cut. So a
small leftover of the low band goes through the waveshaper and compressor. Not measured
(Clean Boost is off by default).

### C5. Smaller full-signal stages (low, computed)

- The input high-pass at 20 Hz runs whenever Legacy Filter is off (it is the legacy
  filter at its "transparent" default): −0.8 dB at 30 Hz, −0.2 dB at 45 Hz, plus phase.
  This is the remaining −0.2 dB in the table.
- Pre-drive Shape, when it isn't flat: at most +0.3 / −0.5 dB at 60 Hz. It is the user's
  own tone choice, so leaving it full-band is reasonable.
- The output limiter (−0.5 dBFS) acts on low + high together, so a loud distorted top can
  duck the sub. It's a safety stage that only acts on overs. Note only.

Not measured: the other clip types (asymmetric ones should make C2 worse), NAM profiles,
and the effect of Clean Boost.

## 3. Why the fixes need care

- The crossfade and the state reset are entangled. Fixing F3 by making the retarget
  logic correct is easy. Fixing F4 changes the CPU budget or needs history buffers.
- The distortion stage is stateful and single-pass (see F2), which rules out the naive
  "run both paths and crossfade" fix for on/off.
- Filters are float biquads in transposed direct form running at the oversampled rate
  (Off, 2x or 4x; 4x is the default). Very low cutoffs are numerically delicate. A
  comment in `applyAutoGainAndISP` records that a 5 Hz DC blocker at 176 kHz was
  unstable. Any fix that moves the crossover toward a very low frequency must be checked
  for that.
- `pb_*` members are shared block-scope state written in `processBlock` and read in the
  stage helpers. New state should not add more coupling.

## 4. Fix options (to evaluate, not prescriptions)

### F3 and F5

- Only retarget when no crossfade is running. Orders change on a scale far slower than
  10 ms, so deferring one retarget by up to 10 ms is inaudible. This removes the
  reversal logic entirely.
- Add hysteresis by remembering the current order and requiring the frequency to move
  a few Hz past the boundary (for example 92 Hz going up, 88 Hz going down; 142 Hz up,
  136 Hz down) before changing.
- If a reversal must be supported, make it reverse the fade in place: swap
  `sgFromOrder`/`sgToOrder` and set `sgCrossfadePos = crossfadeLen - sgCrossfadePos`,
  with no bank resets.

### F4 (and part of F1)

- Run all three banks continuously whenever Sub Guard is active. Their coefficients are
  already written together in `updateSubGuardCoefficients`, so each bank tracks the same
  crossover. Then a slope change is just a weight ramp between two warm banks, and no
  resets are needed. Cost: about 10 biquads per channel at the oversampled rate instead
  of 4. Measure with `Source/Tools/SoakHarness.cpp`.
- Or keep a short input history and prime the incoming bank by running it over the last
  N samples before it starts fading in. More code, less steady CPU.
- Or lengthen the crossfade. This trades click safety for a longer window where the
  crossover is neither slope, and does not fix the stale-state cause.

### F1 and F2

- Treat "off" as a glide to the lowest usable crossover, then switch to the OFF path only
  once the low band is negligible, and treat "on" as the reverse. This reuses the
  existing 50 ms smoother and needs no second distortion pass. Risk: numerical stability
  at very low cutoff (section 3). The glide has to live in the processor, keyed off the
  parameter reaching 0, not the knob: the editor jumps straight from 26 Hz or more to 0,
  and automation can also park the parameter anywhere in 1-25 Hz, which is already an
  active, very low crossover today.
- Or ramp the low-band gain, and the amount removed and re-added around the nonlinear
  stages, over ~10 ms at the boundary instead of switching. Keep the distortion single-pass.
- For F1 specifically: on OFF to ON, reset `toneFilterLow` and prime or warm the target
  bank, and update `smoothedSubGuardFreq` even while off so it starts at the right value.

### Structure (after tests exist)

Move the Sub Guard state and its per-block processing into its own class with a small
interface (prepare, set target frequency, process block, reset). That contains the eight
state variables and the `sgWasActive` handshake with `resetDSPState`. Do this after the
regression tests are in and before the larger fixes; see section 5, step 3.

## 5. Implementation order

Two goals: the sub below the crossover leaves exactly as it came in (section 2a), and
nothing clicks when Sub Guard changes (section 2). The order follows dependencies, not
the severity ranking. A review of this plan (2026-10-19) added the compatibility decision,
moved the extraction ahead of the sound changes, and tightened steps 4, 6 and 7; the
reasons are given where they apply.

### The decision that shapes the rest: one crossover, split around the drive

C2 needs a high-pass *after* the drive as well as before it. Adding one on top of today's
filters would break the flat sum. A Linkwitz-Riley crossover is two identical filters in
series, so the pair can be split instead: one half before the drive, one half after.
With nothing nonlinear between them the sum is still exactly an LR crossover (flat), and
products below the crossover are cut by the second half.

That pairing works cleanly for one even order. **Decided (owner, 2026-10-19): LR48 (8th
order) at every frequency.** The high path becomes a 4th-order Butterworth high-pass before the
drive and another after it; the low path is the matching LR48 low-pass. Before the drive
it falls at 24 dB/oct like today's LR24, though it lets a little more through right at
the crossover (−3 dB there instead of −6). After it, products fall another 24 dB/oct.
Costs: about 8 biquads per channel instead of 4, and more low-end delay, about 14 ms at
60 Hz (7.5 ms today) and 5.5 ms at 150 Hz. Both bands are delayed alike, so the sum
stays flat, but the owner should judge a kick's punch by ear.

It also removes the slope switching (LR24/LR18/LR12 by zone), and with it F3, F4, F5, F7
and the crossfade code, plus F8's two extra banks. The price is the "AGGRO" character:
today 150 Hz uses a gentle slope that lets more low-mids into the drive on purpose. The
owner accepted losing it. If it is wanted back, it returns as its own control (a
soft/sharp switch), never as automatic zones: the zones are what cause the transition
bugs. The knob's snap points (60, 100, 150 Hz) stay.

The alternative, keeping the zones, means a post-drive filter per order (LR18 has no
clean split), then warm banks plus the F3/F5 fixes from the earlier plan. It is more code
and still doesn't fix C2 at LR18.

**On/off as a weight, not a glide** (F1, F2): one smoothed weight `g` (0 = off, 1 = on)
applied everywhere the split acts:

- drive input: `(1 − g)·x + g·high`. The transient tamer (step 4) runs on this blended
  signal, so its input does not jump at the toggle;
- the post-drive high-pass (step 6): `(1 − g)·y + g·HP(y)`. Without this, at `g = 0` the
  high-pass would still cut the lows of the full-range distortion, so "off" would not be
  today's off path; switching it instead of blending it would bring the off click back;
- the low band's subtraction, `toneFilterLow` and add-back, scaled by `g`;
- the auto-gain reference (full-range vs high band), blended by `g`.

At `g = 0` this is the OFF path, at `g = 1` the ON path, continuous between, with one
drive pass per block. Today the low band is used in four places: the auto-gain reference
and the subtraction in `processBlock`, and `toneFilterLow` and the add-back in
`applyAutoGainAndISP`.

**The splitter stays warm.** It runs every active block, Sub Guard on or off, and its
frequency smoother tracks the parameter all the time. Resetting it when `g` leaves 0
would reintroduce F4: an LR48 at 60 Hz has about 14 ms of group delay and rings for tens
of ms, longer than the ramp, so a cold high-pass would leak a low thump into the drive
while `g` rises. Cost: about 8 biquads per channel, 16 in stereo, at the oversampled
rate (measure with `SoakHarness`). While off, the frequency it tracks is clamped to a safe minimum
(see numerics below); `g` alone decides whether the split is heard.

**Numerics.** Every Sub Guard filter today is `IIR::Filter<float>` in transposed direct
form at the oversampled rate. At 192 kHz × 4 a 60 Hz cutoff is fc/fs ≈ 8e-5, and LR48
adds a high-Q section (Q ≈ 1.31) that is more sensitive to coefficient and state
rounding than today's LR24. Section 3 records a 5 Hz DC blocker going unstable at
176 kHz. Step 6 therefore uses `double` state and coefficients, or a TPT/SVF topology,
from the start, and is tested at the highest rate and oversampling, including automation
parked at 1-25 Hz. Use one coefficient writer everywhere. Today `prepareToPlay` builds the
banks with JUCE's float `make*` helpers, and the runtime writers in `FilterCoeffs.h` use
double. At 60 Hz with 4x oversampling the two differ measurably, and the difference lasts
until the frequency moves more than 0.5 Hz. The new filters must not repeat that.

**Compatibility. Decided (owner, 2026-10-19): accept the change.** Sub Guard is on by
default (60 Hz), so steps 4, 5 and 6 change nearly every existing session, and Golden
Audio references change. Factory presets also sit on the slopes being deleted: in
`FactoryPresets.h`, two use 150 Hz (LR12) and five use 100-120 Hz (LR18). The repo has a
precedent for keeping old sessions identical (`legacyFilter`, state version 3). Options:

- *Accept the change* (recommended): no version gate, re-voice the seven presets by ear
  in step 6, regenerate Golden Audio, and say in CHANGELOG that Sub Guard now keeps the
  sub clean, so v2.3 sessions sound different with drive on.
- *Gate it*: state version 4 with a hidden `legacySubGuard` parameter that keeps today's
  path for older sessions. This keeps the zones, their crossfade and F3-F5 alive behind
  a flag forever, which is what the LR48 decision was meant to remove.

The owner chose the first: no version gate; step 6 re-voices the seven presets and
regenerates Golden Audio, with a CHANGELOG note.

### Steps

Each step is one PR that passes `bash scripts/verify.sh`, with `DistortionRender` renders
before and after. Steps 4-7 change the sound on purpose; the owner listens before merging.
Renders with drive on used to differ run to run: `distortionRandom` and `waveshaperRandom`
are clock-seeded, so step 3 proved bit-identity only with a temporary seeding patch. The
seeding hook is now in (section 8): `DistortionRender` seeds the noise (`--seed`, default 1),
so before/after renders compare byte for byte.

1. **Tests that fail today.** Done in #73. `SubGuardPendingTests` ("Sub Guard Pending")
   always logs its measurements and only asserts with `DISTORTION_PENDING_TESTS=1`. The
   clean-sub, off/on, on/off, reversal and chatter tests fail as expected. The
   large-buffer sweeps (F6) pass today, so they are ordinary always-on assertions, kept as
   a regression check. Each later step moves the tests it fixes into an always-on class.
2. **F8 alone.** Done in #74. `resetDSPState` resets all three banks. The state-load
   test moved to an always-on `SubGuardStateLoadTest` ("Sub Guard State Load"). It
   measures the gap to a reference, not the step (section 6).
3. **Extract `SubGuardSplitter`.** Done in #75. Interface: `prepare`, `startTracking`,
   `reset`, `setInactive`, `setTargetFrequency`, `process`, `justActivated`,
   `getLowBand`, `getHighBand`. There is no `g` yet. Renders at off, 60, 100 and 150 Hz,
   drive 0, 30 and 70, 44.1 and 96 kHz were byte-identical, with the drive noise seeded
   by a temporary patch. Steps 6 and 7 extend this interface (see there).
4. **Transient tamer on the high band only (C1).** With Sub Guard on, run
   `applyPreCompression` on the high band, after the split and before the drive. With Sub
   Guard off, behaviour is unchanged. Expected effect: sub level from −3.2 dB toward
   −0.2 dB. That figure comes from disabling the tamer entirely, not from running it on
   the high band, which still carries half the signal at the crossover, so this PR
   measures the real number. Side effect, likely welcome: a loud sub no longer ducks the
   mids going into the drive.
   *Done (`fix/subguard-tamer-high-band`).* Clean-sub measurements, drive 70, before → after:

   | Case | Sub level | 3rd harmonic | Product below the crossover |
   |---|---|---|---|
   | 150 Hz, 45 Hz sub | −3.18 → −0.17 dB | −44.9 → −61.6 dBFS | |
   | 150 Hz, 45 Hz sub + mids | −4.50 → −0.25 dB | −47.4 → −71.1 dBFS | 8 Hz: −41.3 → −37.2 dBFS |
   | 100 Hz, 45 Hz sub | −3.19 → −0.17 dB | −45.6 → −57.5 dBFS | |
   | 100 Hz, 45 Hz sub + mids | −4.47 → −0.18 dB | −48.9 → −69.3 dBFS | 8 Hz: −47.9 → −36.4 dBFS |
   | 60 Hz, 30 Hz sub | −3.15 → −0.78 dB | −43.8 → −75.2 dBFS | |
   | 60 Hz, 30 Hz sub + mids | −4.87 → −0.82 dB | −45.1 → −96.1 dBFS | 7 Hz: −47.7 → −40.8 dBFS |

   Expected levels are −0.17 dB (45 Hz) and −0.78 dB (30 Hz), the 20 Hz input high-pass
   (C5). The sub-level check is now asserted always. The product below the crossover rose
   4-11 dB: the mids now reach the drive unducked, so they intermodulate harder with the
   sub leaking past the first half of the crossover. Step 6's post-drive high-pass is what
   removes that product; it stays pending. With Sub Guard off, renders are byte-identical
   (seeded, drive 0 and 70). Interim order, until step 5: the tamer ran after Clean
   Boost's lift and pre-emphasis with Sub Guard on, since both sat before the split.
5. **Clean Boost on the high band only (C4).** Lift, emphasis and de-emphasis all on the
   high band with Sub Guard on, so the sub's level stays put and the de-emphasis
   leftover disappears.
   *Done (`fix/subguard-boost-high-band`).* With Sub Guard on the high band now runs
   tamer → auto-gain reference → lift and emphasis → drive → de-emphasis, the full-range
   path's order, so step 4's interim order is gone. Clean-sub cases with Clean Boost, drive
   70, before → after: 150 Hz, 45 Hz sub +5.67 → −0.21 dB (expected −0.17); with mids
   +3.75 → −0.21 dB; 60 Hz, 30 Hz sub +5.18 → −0.41 dB (expected −0.78); with mids +3.53 →
   −0.78 dB. The 60 Hz sub-only case misses the 0.3 dB limit: the 30 Hz sub leaks into the
   high band at about −24 dB and the 6 dB lift raises that leak. It stays pending for step
   6, whose post-drive high-pass cuts the leak. Seeded renders are byte-identical except
   with Sub Guard and Clean Boost both on.
6. **The new crossover (C2).** LR48 split around the drive (decided), in `double` or TPT
   (numerics above). The second half goes after all the high band's nonlinear stages
   (drive, waveshaper, compressor, soft clipper), right before the add-back. Delete the
   order zones and the order crossfade. Latency is unchanged (it's an IIR crossover).
   Checklist:
   - *Soft clipper:* the high-pass after it can push peaks back over its −0.3 dBFS
     ceiling. The output limiter still bounds the output; note it in `AGENTS.md`'s
     signal chain.
   - *Compressor:* the LA-2A sees sub products that the second half later removes. Accepted
     (putting the second half earlier lets the later stages create new ones); say so in a
     comment.
   - *NAM profile mode:* the high band goes through the `ResamplingIsland` (delay D). The
     second half runs on the island's output, in the same domain as the add-back, and
     `subGuardLowDelayLine` keeps the low band matched to D. Test a profile with Sub Guard
     on at 44.1 and 96 kHz.
   - *Presets:* re-voice the seven factory presets at 100-150 Hz by ear (per the
     compatibility decision) and regenerate Golden Audio.
   - *Interface:* add a post-drive method that takes any `AudioBlock` (the drive output,
     or the island's output in NAM mode). Store the sample rate in `prepare`; today
     `process` takes it on every call, and a second method should not need it passed in.
   - *Coefficients:* `prepare` and the runtime use the same double writer (numerics
     above).
   - Tests: clean-sub, crossover flatness (on the splitter: true bypass skips Sub Guard
     at drive 0, so `processBlock` cannot show it), numerics, Golden Audio.
7. **On/off weight `g` (F1, F2).** Ramp over 10-20 ms, applied at every point listed under
   "On/off as a weight". No reset when `g` leaves 0: the splitter is warm. Replace
   `setInactive` and `justActivated` with `g`. Today the OFF path calls `setInactive` and
   skips `process`, so the splitter must start running on off blocks too. The pending
   on/off tests allow 2.0x the baseline step (`kSubGuardOnOffRatio`), assuming a ramp of
   at least 10 ms. Reset
   `toneFilterLow` only when `g` leaves 0 after it has been fully off (it is not run while
   off), or run it warm as well. `resetDSPState` sets `g` straight to its target and
   resets the splitter (a state load is a discontinuity anyway).
8. **Remaining, each only if measured:** coefficient updates per sub-block (F6); the
   20 Hz input high-pass (C5), whose removal would make the bass below 40 Hz slightly
   louder in every session, so the owner decides.

What no step can fix: content just below the crossover is shared (C3). Make that clear
to users, for example in the manual or a hint on the knob: "protects what sits well below
the setting, so set it above your 808's note."

Listening pass after steps 4, 5, 6 and 7, with a real 808 and bass line: hold Sub Guard on
and compare the sub with the plugin bypassed; change presets while playing; drag Sub
Guard to off and back.

## 6. Testing

The existing test (`SubGuardOrderCrossfadeTest`, `DistortionTests.cpp`):

- sweeps 60 to 200 Hz to 60 Hz monotonically in 80-block steps;
- uses an 80 Hz sine at amplitude 0.1, 256-sample blocks, 44.1 kHz;
- sets `distMix = 0`, so the distortion is an identity and the stress on the distortion
  path is not exercised;
- asserts `sweepStep < 0.02`, and computes `baselineStep` but ignores it
  (`juce::ignoreUnused(baselineStep)`), so the threshold is an absolute number rather than
  relative to the tone's own slew.

It cannot catch F1, F2 or F3. Add:

| Test | What it drives | Assertion |
|---|---|---|
| Clean sub | 45 Hz sine at −6 dBFS, then plus 1000 + 1037 Hz; Sub Guard 100 and 150 Hz (also 60 Hz with a 30 Hz sine); drive 70 | 45 Hz level within 0.3 dB of the input; nothing below the crossover except the input tone above −80 dBFS (the 8 Hz product, C2) |
| State load | Sub Guard 150, 100 and 60 Hz with a loud 80 Hz sine, then `setStateInformation` with the same state; a second processor with the same history is re-prepared, then loads the same state | largest gap to that reference over 8 blocks below an absolute limit (1e-4 clean, 5e-3 distorted; 60 Hz is the control). A load restarts the whole chain, so the step is never near baseline (F8) |
| Boundary chatter | alternate `subGuardFreq` around 92 Hz and 142 Hz every 1-2 blocks | peak step within a small multiple of baseline; all samples finite |
| Off then on | loud 80 Hz sine, Sub Guard 60, then 0, then 60 | step in the first blocks after re-enable near baseline |
| On then off | sub tone plus mid tone, Sub Guard on then off | step at the switch near baseline |
| Reverse mid-fade | move the frequency across 92 Hz and back within one crossfade | no discontinuity, filter state finite afterwards |
| Large buffer sweep | same sweep with 2048-sample blocks | peak step near baseline. Passes today, so it is an always-on regression check (F6) |
| With distortion | repeat the above with real distortion (`distMix = 100`) | no bursts beyond expected level |
| Crossover flatness (step 6) | impulse and swept sine through `SubGuardSplitter` alone, low + high with the post-drive half applied and no stage between | magnitude flat within 0.01 dB, 20 Hz-20 kHz, at 60, 100, 150 and 200 Hz |
| Numerics (step 6) | 192 kHz, 4x oversampling; Sub Guard 1, 5, 25, 60 and 200 Hz; also automation parked at 1-25 Hz for 10 s | all samples finite, low band settles to the input level, no growth |
| On/off with `g` (step 7) | sub + mid tones, toggle Sub Guard every 30 ms, at 64 and 2048-sample blocks | peak step near baseline; no low thump in the high band when switching on |
| NAM profile (step 6) | a test profile with Sub Guard on, 44.1 and 96 kHz | low and high bands aligned (sum flat with the profile's own response removed) |

Use the relative form (`step <= k * baselineStep`) and print both numbers in the failure
message. Where possible, also compare the high-band and low-band RMS before and after a
transition to catch the dip in F4. The state-load test is the exception: it compares with
a reference processor. The rig seeds the drive's noise, so with the drive on the two match
to about 1e-5 and the distorted limit is the same 1e-4 as the clean one (it was 5e-3
while the noise was clock-seeded).

Running the tests: `bash scripts/verify.sh` configures, builds `DistortionTests` and runs
it; it is the definition of done (`AGENTS.md`). `DistortionTests <name filter>` runs one
group. On Linux it needs the apt packages from `.github/workflows/build.yml`.

## 7. Not checked

- No transition was rendered or reproduced; only the static settings in section 2a were.
  `Source/Tools/RenderHarness.cpp` can render a static Sub Guard setting but cannot
  automate the parameter, so it cannot exercise transitions without a small extension.
- Whether the JUCE `IIR::Filter` in use reacts to a coefficient change with a click at
  these magnitudes (F6) is unmeasured.
- The interaction between Sub Guard transitions and the NAM profile path
  (`profileMode`, `islandDelay`, `subGuardLowDelayLine`) is not analysed beyond noting
  that the low band is delayed by the same amount as the high band.
- Auto-gain envelope behaviour across a Sub Guard toggle is unmeasured.

## 8. Implementation hand-off

For the session that does the work. Each step in section 5 is its own `fix/` or
`refactor/` branch and PR off `main`. Steps 1-3 are done (#73, #74, #75); the notes for
them below record what was learned. Do the rest in order; do not start step 4 until the
owner has answered the compatibility question in section 5. The owner has answered it
(2026-10-19): accept the change. No version gate; step 6 re-voices the seven presets and
regenerates Golden Audio, and a CHANGELOG note says Sub Guard now keeps the sub clean.

**Before step 1**

- Read sections 1, 2a, 5 and 6. Section 4 is background; section 5 supersedes it where
  they differ.
- On Linux, install the apt packages from `.github/workflows/build.yml` (the `Install
  Dependencies` step of the Linux job), then `bash scripts/verify.sh` must pass before
  any change. A fresh cloud container lacks them.
- `DistortionRender` (`Source/Tools/RenderHarness.cpp`) renders static settings:
  `--in <wav> --subguard <Hz|off> --drive <0-100> --mix <0-100> --rate <Hz>`. It has no
  automation flags and no direct clip-type flag; `--preset <name>` loads a factory preset,
  which sets a clip type indirectly. Section 2a's inputs were generated WAVs passed with
  `--in`.
  Keep the before/after renders for each PR in a scratch folder, not the repo.

**Step 1: where the tests go**

- Tests live in `Source/Tests/DistortionTests.{h,cpp}`: declare a `juce::UnitTest`
  subclass in the header, implement `runTest` in the .cpp, and add a `static` instance to
  `registerAllTests()` at the end of the header (Debug builds only; `verify.sh` builds
  Debug). Helpers: `TestUtilities::setParameter`, `generateSineWave`, `calculateRMS`,
  `containsInvalidSamples` (`Source/Tests/TestUtilities.h`).
- Copy the processor set-up from `SubGuardOrderCrossfadeTest::runTest` (it pins tone,
  comp, auto-gain and Clean Boost) but set `distMix` to 100 where the test needs real
  distortion.
- Existing Sub Guard tests: `RTCleanSubGuardTest`, `SubGuardCrossoverFlatnessTest`,
  `SubGuardFlatnessTest` ("Sub Guard Spectral Flatness", renamed in #73),
  `SubGuardOrderCrossfadeTest`, `SubGuardPendingTests` and `SubGuardStateLoadTest`. Step 6
  deletes or rewrites the order-crossfade test.
- `verify.sh` must stay green, so a test that fails today cannot assert the fixed
  behaviour yet. Put the new tests in one class (for example `SubGuardPendingTests`,
  "Sub Guard Pending") that always prints its measurements but only `expect`s when
  `DISTORTION_PENDING_TESTS=1` is set, and run it that way in the PR to show each one
  failing. Each later step moves the tests it fixes into an always-on class.
- For the clean-sub test, measure the 45 Hz level and the 8 Hz product with a single-bin
  DFT (Goertzel) over the last 2 s, after the warm-up; there is no FFT helper yet. Match
  section 2a's set-up: 44.1 kHz, 512-sample blocks, default clip type, mix 100. The
  numbers should land near the table in section 2a; if they don't, stop and find out
  why before going on.

**Step 2 (F8), done in #74**: `resetDSPState` resets all three orders. The state-load
test as first written could not pass even after the fix. A load resets the whole chain, so
the step after it is never near baseline. It now measures the gap to a reference that was
re-prepared and then loaded the same state. Loading matters: it rewrites the coefficients
through the runtime path, and a reference left with `prepareToPlay`'s float coefficients
keeps a gap of its own.

**Step 3 (extract), done in #75**: `Source/SubGuardSplitter.{h,cpp}`; the coefficient
writers moved to `FilterCoeffs.h`, the `SUBGUARD_*` constants to `SubGuardSplitter.h`. The
low band's island delay, `toneFilterLow` and the auto-gain reference stay in the
processor. Renders with drive on matched byte for byte only after a temporary patch that
seeded `distortionRandom` and `waveshaperRandom`.

**Before steps 4-7**

- Seeding hook: done. `PluginProcessor::seedNoiseForTesting(seed)` seeds both
  `juce::Random` members. `DistortionRender` always calls it (`--seed <n>`, default 1),
  `SubGuardToneRig` seeds every rig, and `ProcessBlockDecompTest` now asserts seeded peers
  are bit-exact. Steps 4 and 5 promise no change with Sub Guard off: prove it by comparing
  `sha1sum` of renders before and after, Sub Guard off, drive on.
- Names moved in #75; sections 1-4 use the old ones. `determineSubGuardFilterOrder` is
  now `SubGuardSplitter::orderForFrequency`. `updateSubGuardCoefficients`,
  `filterSubGuardBands` and `resetSubGuardOrderFilters` are now `updateCoefficients`,
  `filterBands` and `resetOrderFilters`. The banks are now `lr24*`, `lr18*` and `lr12*`.
  The `sg*` state is now the splitter's own (`wasActive`, `crossfadeActive`, ...).
- Step 6 adds a post-drive method on any `AudioBlock` and stores the sample rate in
  `prepare`. Step 7 replaces `setInactive` and `justActivated` with `g` (section 5).

**Rules that apply to every step**

- No allocation or locks in `processBlock`; size new buffers in `prepareToPlay` and
  `rebuildOversampling` from the wider of input and output channel counts (`AGENTS.md`).
- Update the signal chain in `AGENTS.md` whenever a stage moves (steps 4, 5 and 6 move
  stages). `AGENTS.md` also points to `docs/Architecture Contract.md`, which is not in
  the repo; don't go looking for it.
- Add a CHANGELOG `Unreleased` line for each step that changes the sound.
- Owner listens before merging steps 4-7 (section 5's listening pass).
