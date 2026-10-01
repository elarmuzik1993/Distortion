# Plan: transition clicks and reset gaps

Branch `fix/transition-clicks`, based on `test/crossing-probe` (commit `e838ee0`), which adds the
Crossing Probe this plan uses as its gate. Line numbers below are at `e838ee0`.

Goal: stop the clicks heard when the plugin switches between its bypass and active paths, close
the reset gap, and keep both fixed with tests. Restructure code only where the fix needs it.

| Step | What | Status |
|---|---|---|
| 1 | Engage fade on every bypass/active switch | todo |
| 2 | `resetDSPState` clears every filter, guarded by a "reset equals fresh" test | todo |
| 3 | Write down the profile handoff protocol (comments) | todo |
| 4 | Measure again with the probe, then stop | todo |

Suggested order: 2 first (smallest, independent), then 1 (the one users hear), 3, 4.

## Background

### Where the transition bugs came from

The transition state lives in `PluginProcessor` as ordinary members: Sub Guard `sg*`, profile swap
`swap*` / `incomingProfile`, mono rejoin `profileRight*`, duck `duck*`. It is not in the `pb_*`
members, which are per-block parameter snapshots written before they are read in every block.
The PR #55 fixes fall into three families:

1. A block can leave by several routes (active, internal true bypass, host bypass, three early
   exits, duck closed), and state not kept moving on one route replays stale audio on the next
   switch: 6f84ffa, ec5bd17, d251e6a, 7a47fbd, b00c0dd.
2. `resetDSPState` is a hand-kept list: 9d8933b, e68d7d1.
3. The profile handoff between loader, timer and audio thread: 36d01a8, 8ca343b, 35a315c.

### What the Crossing Probe measured (Linux, gcc, Release)

A held 110 Hz note with harmonics at -12 dBFS, Distortion crossing 0.4% <-> 0.6% and host bypass
engaging and releasing, at 44.1 and 48 kHz, clip types 0 and 1, linear-phase oversampling, and the
`wavenet.nam` profile, each at 8 points in the waveform. "Treble burst" is the loudest 1 ms of
content above 4 kHz within 30 ms of the switch, in dB above the loudest treble the steady sound
itself has within 100 ms; above about 0 dB the switch sticks out as a click.

| Version | Typical | Worst |
|---|---|---|
| Today | +14 to +21 dB | +28 dB |
| Every element kept running, hard switch | +15 to +20 dB | +24 dB |
| 10 ms fade only | -3 to 0 dB | +2.7 dB |
| Both | about 0 dB | +0.3 dB |

- The click is the hard cut itself. At 0.5% the clip types engage at full strength (Brutal Fuzz
  multiplies by about 2.3 and clips at 0.3), so the output jumps from clean to fully distorted in
  one sample. Keeping state warm does not remove that; a fade does.
- Leftover stale audio is real (up to +12 dB above the signal RMS in the first 30 ms) but does not
  stick out under a fade. From 30 ms to 1 s it is 36 to 49 dB below the signal.
- Confirmed gap: the bypass delay line (`bypassLatencyDelay`) is fed only on bypass blocks, so
  switching off a second time replays its old contents (+8 dB vs +2 dB the first time at 48 kHz;
  25 samples of second-old audio with a profile at 44.1 kHz).
- Host bypass with a profile leaves more behind than the internal bypass (+3.8 dB vs -0.8 dB),
  as the earlier STATE.md follow-up said; a fade covers it (worst +2.5 dB).

Run it: `DistortionTests "Crossing Probe"` (about 40 s; a full test run leaves it out).
`CROSSING_PROBE_OUT=<folder>` also writes listening reels: today, fade only, kept running, both.

### Reset gap (from reading; step 2's test confirms)

`resetDSPState` (`Source/PluginProcessor.cpp:4067`) says "Reset all filters" but skips the six
LR12/LR18 Sub Guard filters (`subGuardLP12`, `subGuardHP12`, `subGuardLP18_1/2`,
`subGuardHP18_1/2`) and the Clean Boost shelves (`emphasisFilter`, `deEmphasisFilter`). Only
`prepareToPlay` and `rebuildOversampling` clear them, so a preset or session load during playback
(`stateNeedsReset` -> `resetDSPState`) keeps their old ring.

## Step 1: engage fade

One per-sample weight, ramped over 10 ms like the duck (`DUCK_FADE_TIME_S`): 1 means the active
chain is heard, 0 means the bypass signal is heard. Target 1 when the plugin should process
(Distortion >= 0.5% or the compressor on, `PluginProcessor.cpp:1862`) and the host is not
bypassing; 0 otherwise.

- Output = weight x active + (1 - weight) x bypass. Blend after each path's output gain and global
  mix, before the stages they share: limiter (`applyFinalLimiter`) -> scope -> output pad -> duck.
  Nothing shared then runs twice per sample.
- The bypass path (input filter when active, output gain, global mix, then `bypassLatencyDelay`)
  runs every block. It is cheap, and it gets its own output-gain and global-mix smoothers so the
  active path's are not advanced twice. Its delay line then never holds stale audio, which closes
  the "switch off again" replay without a separate fix.
- The active chain runs while the weight is above 0 or rising. While fading out it keeps the last
  Distortion amount at or above 0.5%, so it does not cut its own distortion mid-fade
  (`applyDistortionStage` at `:3023`, `applyProfileChunk` at `:3273`).
- Host bypass: `processBlockBypassed` (`:1495`) sets the target to 0 and keeps running the chain
  until the fade ends. Its bypass signal stays raw (no output gain, mix or limiter), as today.
- The weight snaps to its target when nothing is audible: the first block after `prepareToPlay`,
  and after a ducked rebuild. Existing tests that set parameters right after prepare see no fade.
- At weight 0 nothing changes from today: bit-clean bypass, the same latency, the same CPU saving.
- The internal bypass branch (`:1862`-`:1931`) and its duplicated tail (global mix, limiter,
  scope) merge into the blend as a side effect. `endBlockEarly` (`:1485`) stays as it is: an
  error path, no fade.

Gate:
- A fast pass/fail version of the probe in the normal run: switch on, switch off and host bypass,
  at 44.1 and 48 kHz, a few switch points each. Worst treble burst at most +3 dB.
- Every existing bypass and latency test passes unchanged: "True Bypass Mode", "True-bypass path is
  latency-compensated", "Crossing into bypass and back leaves no stale audio in the output pad",
  "A profile keeps running through true bypass, so resuming replays nothing stale", "Host bypass
  keeps the reported latency".
- The RT allocation test passes: no allocation added on the audio thread.

Risks:
- The active path's output-gain loop also carries the LFO tremolo (`:3942`). The bypass path's own
  gain smoother leaves it out, as today's bypass branch does.
- Hosts that stop calling the plugin entirely while bypassed get no fade-out; nothing the plugin
  can do about that.

## Step 2: reset coverage

- `resetDSPState` also clears the six LR12/LR18 Sub Guard filters and the two Clean Boost shelves.
- Guard: "reset equals fresh". Play loud audio through an instance, call `resetDSPState`, then
  compare it with a freshly prepared twin on the same input. Cover a built-in clip type other than
  0 (clip type 0 adds time-seeded noise), each Sub Guard slope, and Clean Boost, EQ and compression
  on. `prepareToPlay` seeds the ramps, smoothers and auto-gain to the values `resetDSPState` uses,
  so the twins can match. The test should fail before the fix and catches any filter a later change
  forgets.

## Step 3: profile handoff protocol (comments only)

One comment block at the profile slots (`PluginProcessor.h:756` onward): for `stagedProfile`,
`pendingProfile`, `retiredProfile`, `activeProfile` and `incomingProfile`, who writes it, who reads
it and under which lock; the lock order (callback lock, then `profileStatusLock`); and the order in
which competing intents win in `installRequestedProfile` (`:2711`).

## Step 4: measure again, then stop

Run the probe after steps 1 and 2. Do more only if something still sticks out. Candidates:
- Keep the profile running during host bypass (the earlier STATE.md follow-up). Costs the model's
  CPU while the host says the plugin is bypassed.
- An LFO on Distortion crossing 0.5%: the same hard cut, inside `applyDistortionStage`. Not
  measured yet.

## Dropped from the earlier plan, and why

- Keeping every element running on every route (oversampler while bypassed, profile under host
  bypass, abandoned blocks): measured not to fix the click; under a fade the leftovers do not stick
  out.
- A checked-in characterization reference: it would flag every sound change. The probe and the
  property tests cover the same ground.
- New owning types (`DuckGate`, `LatencyLines`, `SwapClock`, `RightChannelShare`, Sub Guard): they
  fix no bug and would rework code that just came through ten review findings.
- A standalone shared-exit refactor: step 1 produces the shape it was for.
- Replacing the `pb_*` members: not the cause of any bug.

## Open questions

1. Merge PR #54 and PR #56 first? Step 1 rewrites the bypass branch; #54 touches the
   `processBlock` declaration and `prepareToPlay`, #56 touches `isInputFilterActive`.
2. Sound design, separate from this fix: at 0.6% the clip types are fully on, so the bottom of the
   Distortion knob jumps from clean to full distortion. The fade makes the jump click-free, not
   gradual. Should the effect fade in over the first few percent instead?

## Verify

- Linux needs the apt packages in `.github/workflows/build.yml` (the test target needs neither
  curl nor WebKit) and `xvfb-run -a`.
- `cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build --target DistortionTests`
- Full run: `xvfb-run -a ./build/DistortionTests_artefacts/Release/DistortionTests`
  (3615 assertions at `e838ee0`).
- Probe: `xvfb-run -a ./build/DistortionTests_artefacts/Release/DistortionTests "Crossing Probe"`
