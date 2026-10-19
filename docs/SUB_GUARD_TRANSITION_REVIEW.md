# Sub Guard crossover: transition artifact review

Status: static read of the code. Nothing here has been built, run, or reproduced.
Every finding is a hypothesis with a concrete mechanism and a proposed test. The
first job is to turn each one into a failing test, or to disprove it. A review pass
re-checked the claims against `main` at `d924b6d`; that pass added F8 and corrected the
trigger analysis for F2, F3 and F5.

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

The order follows dependencies, not the severity ranking. Two observations drive it:

- **Warm banks (F4) remove most of the reset logic.** If all three banks run whenever Sub
  Guard is active, nothing resets a bank mid-signal. F3's "reset a live bank, run one bank
  twice" goes away, and a reversal becomes reversing the weight ramp. The cost is small:
  10 biquads per channel instead of 4, at most 192 kHz at 4x, a few percent of one
  core at worst. So F4 goes before F3 rather than last, and the F3 fix gets simpler.
- **On/off should be a weight, not a glide.** Introduce one smoothed weight `g` (0 = off,
  1 = on) and apply it at every place the low band is used: the distortion input becomes
  `(1 - g) * x + g * high`, and the low band is subtracted, tone-filtered and added back
  scaled by `g`. At `g = 0` this is the OFF path, at `g = 1` today's ON path, and between
  them it is continuous with one distortion pass per block (the F2 constraint). Unlike the
  glide, it never pushes the crossover toward a very low cutoff, so the stability risk in
  section 3 doesn't arise. The UI snap to 0 stops mattering, because the processor ramps
  whatever the parameter does. Today the low band is used in four places: the auto-gain
  reference and the low-band subtraction in `processBlock`, and `toneFilterLow` and the
  add-back in `applyAutoGainAndISP`. Each needs `g`, which is why the extraction in step 3
  comes before it.

Steps. Each is one PR that passes `bash scripts/verify.sh`, with renders from
`RenderHarness` before and after.

1. **Transition test harness and failing tests.** A test helper that processes blocks
   while changing `subGuardFreq` at given blocks and reports the peak sample step against
   the steady-state baseline of both settings (with distortion on, the baseline has to
   come from the same settings). Write the section 6 tests. F8, F1, F2 and F3 should fail.
   **Any that doesn't fail is downgraded, not fixed.** This is the point where the
   hypotheses become facts, and it may reorder what follows.
2. **F8 alone.** Reset all three banks in `resetDSPState`. One small change that makes its
   test pass and changes nothing else. Ship it first: it's the likeliest pop (any preset
   change while playing) and costs nothing.
3. **Extract `SubGuardSplitter`** (prepare, set target frequency, process block, reset,
   plus read access to the low band and `g`). This changes structure only. Prove it with
   bit-exact `RenderHarness` renders at Sub Guard off, 60, 100 and 150 Hz, before and
   after, and the existing tests unchanged. The failing tests from step 1 must still fail
   the same way. Doing this before steps 4 and 5 keeps their state (`g`, warm banks,
   ramp direction) inside one class instead of more `pb_*` and `sg*` members.
4. **Warm banks, then F3 and F5.** Run all three banks whenever active, keep coefficients
   in step (already true in `updateSubGuardCoefficients`), and delete the incoming-bank
   resets. Then make a retarget during a fade reverse it in place
   (`pos = len - pos`, swap from/to) and add hysteresis to `determineSubGuardFilterOrder`
   (it takes the current order; for example 92 up / 88 down, 142 up / 136 down). The F3,
   F4 and chatter tests pass. Measure CPU with `DistortionSoak`.
5. **On/off weight `g` (F1 and F2).** Ramp `g` over about 10-20 ms. When `g` leaves 0:
   reset all banks and `toneFilterLow`, and snap `smoothedSubGuardFreq` to the target
   (`setCurrentAndTargetValue`), so nothing resumes from minutes ago and the crossover
   doesn't glide from an old value (the F1 smoother bullet). The cold banks start under
   `g ≈ 0`, so their onset is faded in. Take the OFF path only once `g` has reached 0.
   Drive the auto-gain reference from the same `g`, so its input envelope doesn't jump.
   `resetDSPState` sets `g` straight to its target (a state load is already a reset). The
   on-then-off and off-then-on tests pass.
6. **F6, then F7, only if measured.** Run the large-buffer sweep test after step 5. If it
   fails, move the coefficient update to sub-blocks (every 32 samples, as Shape does). F7
   stays a note unless a render shows the blend dip.

Not in this plan: automation can park `subGuardFreq` at 1-25 Hz, which is an active, very
low crossover today. Check that region for float-biquad stability at 4x separately (a
test that sweeps 2-25 Hz and asserts finite, bounded output). It is unrelated to
transitions.

Listening pass after steps 2, 4 and 5, with a real 808 or bass line: preset changes while
playing, dragging Sub Guard to off and back, and automation drawn across 92 and 142 Hz.

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
| State load | Sub Guard 150, 100 and 60 Hz with a loud 80 Hz sine, then `setStateInformation` with the same state | step in the first block after the reset near baseline (F8) |
| Boundary chatter | alternate `subGuardFreq` around 92 Hz and 142 Hz every 1-2 blocks | peak step within a small multiple of baseline; all samples finite |
| Off then on | loud 80 Hz sine, Sub Guard 60, then 0, then 60 | step in the first blocks after re-enable near baseline |
| On then off | sub tone plus mid tone, Sub Guard on then off | step at the switch near baseline |
| Reverse mid-fade | move the frequency across 92 Hz and back within one crossfade | no discontinuity, filter state finite afterwards |
| Large buffer sweep | same sweep with 2048-sample blocks | peak step near baseline |
| With distortion | repeat the above with real distortion (`distMix = 100`) | no bursts beyond expected level |

Use the relative form (`step <= k * baselineStep`) and print both numbers in the failure
message. Where possible, also compare the high-band and low-band RMS before and after a
transition to catch the dip in F4.

Running the tests: `bash scripts/verify.sh` configures, builds `DistortionTests` and runs
it; it is the definition of done (`AGENTS.md`). `DistortionTests <name filter>` runs one
group. On Linux it needs the apt packages from `.github/workflows/build.yml`.

## 7. Not checked

- No compile, no test run, no audio rendered. `Source/Tools/RenderHarness.cpp` can render
  a static Sub Guard setting but cannot automate the parameter, so it cannot exercise
  transitions without a small extension.
- Whether the JUCE `IIR::Filter` in use reacts to a coefficient change with a click at
  these magnitudes (F6) is unmeasured.
- The interaction between Sub Guard transitions and the NAM profile path
  (`profileMode`, `islandDelay`, `subGuardLowDelayLine`) is not analysed beyond noting
  that the low band is delayed by the same amount as the high band.
- Auto-gain envelope behaviour across a Sub Guard toggle is unmeasured.
