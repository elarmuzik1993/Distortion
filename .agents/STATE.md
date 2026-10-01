# State

## Now
- Branch `fix/transition-clicks` (`d793f76`, pushed, no PR): `test/crossing-probe` (`e838ee0`, adds the
  Crossing Probe) plus `.agents/PLAN-transition-fixes.md`. No fix code yet; all four plan steps are todo.
- PR #55 is merged into `main` (`75e1348`). Open PRs: #54 (`chore/housekeeping-debug`), #56
  (`fix/filter-mode-switch-cutoff`).
- Verified 2026-10-01 at `d793f76`, Linux gcc Release (cloud): `DistortionTests` 3615 assertions pass
  (probe left out); `DistortionTests "Crossing Probe"` 42 pass. CI has not run on these branches.

## Next
1. Plan step 2: `resetDSPState` also clears the six LR12/LR18 Sub Guard filters and the two Clean Boost
   shelves, behind a "reset equals fresh" test that fails first. Details in the plan.
2. Plan step 1: one 10 ms engage fade on every bypass/active switch, host bypass included. Gate: a fast
   probe in the normal run (worst treble burst <= +3 dB) and the existing bypass and latency tests.
3. Plan steps 3 (profile handoff comments) and 4 (re-measure with the probe, then stop).
4. Owner: merge #54 and #56 before step 1? Should the bottom of the Distortion knob fade in gradually?
5. Owner: delete `origin/test/crossing-probe` (in this branch) and the empty `origin/ccr-54d2fa3a-0eyuqy`.
6. Owner (unverified, from the previous handoff): ASIO standalone at 44.1/96 kHz with profiles; re-test #44.

## Decisions
- Transition clicks get one fade, not "keep every element running": the probe measured the hard cut as
  the click (+15 to +20 dB even with warm state); a 10 ms fade alone leaves at most +2.7 dB.
- No refactor that fixes no measured bug (new owning types, a checked-in reference, `pb_` cleanup):
  it would churn code that just came through ten review findings.
- Probes are UnitTests in category "Probe": a full run leaves them out; name one to run it.
  `CROSSING_PROBE_OUT=<folder>` writes listening reels.
- Reported latency is always R = max(user oversampler latency, island delay for a 48 kHz model).
- True bypass with a profile keeps running it, output discarded: NAM's reset allocates.
- Profile loading: re-prepare on the loader thread, created on first load; lock order is callback lock,
  then `profileStatusLock`. Offline renders never duck. NAM Core pinned `0b3d3c9`.

## Known issues
- No `scripts/verify.sh`. Verify = build `DistortionTests` and run it. Linux needs the apt packages in
  `.github/workflows/build.yml` (the test target needs neither curl nor WebKit) and `xvfb-run -a`.
- Every bypass/active switch clicks (measured); switching off again replays `bypassLatencyDelay` (confirmed);
  `resetDSPState` misses 8 filters (from reading; step 2's test confirms). All covered by the plan.
- Not in the plan: a block longer than prepared + 64 skips the dry capture (stale dry at global Mix < 100%).
- The mono-rejoin warm-up has no test; the clear-during-switch race test is timing-based;
  `resetDSPState` clears a profile's islands, not its model state (NAM's reset allocates).
- Clip type 0 and the waveshaper use time-seeded `juce::Random`: repeatable renders need clipType != 0,
  waveshaperMix 0.
- Cloud git defaults to a "Claude" identity: commit as Boris Miscenco <aidevblock@gmail.com>, no AI credit.
- Windows: close `Sledge Distortion.exe` before rebuilding (LNK1104). `Source/GitVersion.h` is skip-worktree.
  `AGENTS.md` and `docs/Architecture Contract.md` are gitignored (local only).
