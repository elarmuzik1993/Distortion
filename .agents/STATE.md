# State

## Now
- PR #55 (`feat/nam-profile-prototype`, draft) is at `771205d`, the same commit as `fix/nam-pr55-cleanup`.
  Review findings #1-#10 are all fixed; the last five commits are the macOS arm64 test build fix, then #8, #10, #7, #9.
- CI on `771205d` (2026-09-28): build-windows, build-linux, build-macos all green; GitHub reports it mergeable.
- Verified 2026-09-28 at `771205d`, Linux gcc Release: `DistortionTests` 3614 assertions pass;
  the NAM Profile suite passed 10 of 10 runs. VST3/Standalone not rebuilt locally; pluginval not run.
- Unverified (from the previous handoff, local machine only): `fix/filter-mode-switch-cutoff`
  (`ff0ad79`, worktree `~/Distortion-filterfix`) and PR #54.

## Next
1. Owner: ASIO standalone at 44.1 and 96 kHz: load, swap and clear profiles while playing, with
   Mono Input on and off; Reaper latency display stays put; A/B
   `build-rel/nam-regression/sd1_{44100,48000,96000}.wav` (local files, unverified).
2. Run pluginval strictness 10 on the PR's VST3; then update the PR #55 description (it still says
   3518 assertions and lists macOS/Linux builds as not done).
3. Decide whether `.agents/` stays on the branch before merging; then mark PR #55 ready.
4. Follow-ups predating the branch: host bypass still freezes the island and delay lines, and the
   input filter and oversampler IIR ring after any bypass gap; a block longer than prepared + 64
   skips the dry capture, so global Mix < 100% blends stale dry.
5. Owner (unverified, local): filter modes in `~/Distortion-filterfix`; re-test #44.

## Decisions
- Reported latency is always R = max(user oversampler latency, island delay for a 48 kHz model).
- True bypass with a profile keeps running it, output discarded: NAM's reset allocates.
- A block longer than the scratch runs the profile in pieces, never the zero-latency built-in clip.
- Mono input (Mono Input on, or a mono bus): channel 1 takes channel 0's model output and its models
  rest (half the CPU). Back to stereo they warm up unheard (settle time, max 100 ms), then fade in over 30 ms.
- A stale staged profile is re-prepared on the loader thread, not the message thread, because prepare()
  prewarms the model. `stagedRefreshesInFlight` keeps `isProfileSwitchIdle` honest meanwhile.
- The loader thread is created on first load, not per instance. Profiles carry their load request id;
  re-staged only while newest, under `profileStatusLock`. Lock order: callback lock, then `profileStatusLock`.
- Offline renders never duck. NAM Core pinned `0b3d3c9`. ASIO opt-in. Sessions remember a profile by path.

## Known issues
- No `scripts/verify.sh`. Verify = build `DistortionTests` and run it (Linux needs the apt packages
  in `.github/workflows/build.yml` and `xvfb-run -a`; Windows uses MSVC via vcvars64).
- The mono-rejoin warm-up has no test of its own: the test models settle in under 1 ms.
- The clear-during-switch race test is timing-based (spins on a flag); the staged-refresh races are gated.
- `resetDSPState` clears a profile's islands, not its model state (NAM's reset allocates).
- Default renders aren't repeatable (time-seeded `juce::Random`): baselines need clipType != 0, waveshaperMix 0.
- Windows: close `Sledge Distortion.exe` before rebuilding (LNK1104). `Source/GitVersion.h` is skip-worktree.
- `AGENTS.md` and `docs/Architecture Contract.md` are gitignored (local only).
