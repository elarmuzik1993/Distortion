# State

## Now
- `feat/nam-profile-prototype`, PR #55 open (remote head `5e899b1`). Local and unpushed:
  `6f84ffa..8ca343b`, six fixes for correctness findings #1-#6 of the PR #55 code review
  (bypass feed, island reset, oversized blocks, early-exit tail, pending-profile order, clear races).
- Verified 2026-09-27 at `8ca343b`: `DistortionTests` 3595 assertions pass; every one of the six
  commits built and passed the NAM Profile suite alone; Debug Standalone builds and carries
  `SlimmableContainer`. VST3 not rebuilt this session. Owner listening and DAW checks: not done.
- `fix/filter-mode-switch-cutoff` (`ff0ad79`, worktree `~/Distortion-filterfix`) and PR #54: unchanged, unverified.

## Next
1. Owner: push `feat/nam-profile-prototype` to update PR #55 (not pushed; push only when asked).
2. Remaining PR #55 review findings (cleanups; #7, #8 and #10 done on `fix/nam-pr55-cleanup`):
   #9 `refreshStagedProfile` prepares and prewarms on the message thread: move it to `profileLoader`.
3. Owner: ASIO standalone at 44.1 and 96 kHz: load, swap and clear profiles while playing; Reaper
   latency display stays put; A/B `build-rel/nam-regression/sd1_{44100,48000,96000}.wav`.
4. Then dispatch "Build VST3 Plugin" for macOS and Linux (untested there).
5. Follow-ups predating the branch: host bypass still freezes the island and delay lines, and the
   input filter and oversampler IIR ring after any bypass gap; a block longer than prepared + 64
   skips the dry capture, so global Mix < 100% blends stale dry.
6. Deferred minors: `.superpowers/sdd/2026-09-27-nam-profile-resampling/progress.md` (local).
7. Owner: filter modes in `~/Distortion-filterfix`; re-test #44 here; decide on the 41
   `origin/main` commits authored `Claude <noreply@anthropic.com>`.

## Decisions
- Reported latency is always R = max(user oversampler latency, island delay for a 48 kHz model).
- True bypass with a profile keeps running it on the input, output discarded: NAM's reset
  allocates, so resuming can't reset it. Costs about the active path's CPU while bypassed.
- A block longer than the scratch runs the profile in pieces, never the zero-latency built-in clip.
- Profiles carry their load request id; re-staged only while it is the newest, under
  `profileStatusLock`. Lock order: callback lock, then `profileStatusLock`.
- Mono input (Mono Input on, or a mono bus): channel 1 takes channel 0's model output and its
  models rest. Back to stereo they warm up unheard (settle time, max 100 ms), then fade in over
  30 ms. The warm-up has no test of its own: the test models settle in under 1 ms.
- Offline renders never duck. NAM Core pinned `0b3d3c9`. ASIO opt-in in `build-asio`.
  Sessions remember a profile by path.

## Known issues
- No `scripts/verify.sh`. Verify = MSVC via vcvars64 (VS 18 BuildTools), `cmake --build build
  --target DistortionTests Distortion_Standalone`, run the tests, check the binary has "SlimmableContainer".
- `resetDSPState` clears a profile's islands, not its model state (NAM's reset allocates).
- The two clear-race tests are timing-based (spin on a slot or flag): 10 of 10 clean runs.
- Default renders aren't repeatable (time-seeded `juce::Random`): baselines need clipType != 0, waveshaperMix 0.
- Close `Sledge Distortion.exe` before rebuilding (LNK1104). `Source/GitVersion.h` is skip-worktree.
- `AGENTS.md` and `docs/Architecture Contract.md` are gitignored (local only); decide before merging.
