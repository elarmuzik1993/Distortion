# State

## Now
- `chore/repo-kit` (off `origin/main` `75e1348`, PR #55 merged) adds `scripts/verify.sh`, tracks
  `AGENTS.md` (removed from `.gitignore`) with the kit sections, and sets `*.sh` to LF: awaiting review.
- Verified 2026-10-03 on the kit branch, Windows MSVC Debug: `bash scripts/verify.sh` passes,
  `DistortionTests` 3615 assertions; `--quick` is silent and takes under a second.
- Unverified (from the previous handoff, local machine only): `fix/filter-mode-switch-cutoff`
  (`ff0ad79`, worktree `~/Distortion-filterfix`) and PR #54.

## Next
1. Owner: ASIO standalone at 44.1 and 96 kHz: load, swap and clear profiles while playing, with
   Mono Input on and off; Reaper latency display stays put; A/B
   `build-rel/nam-regression/sd1_{44100,48000,96000}.wav` (local files, unverified).
2. Review and merge `chore/repo-kit`. Check `AGENTS.md` before it goes public: it is now tracked
   (it was gitignored); `CLAUDE.md`, `GEMINI.md` and `docs/Architecture Contract.md` stay local.
3. Follow-ups predating the branch: host bypass still freezes the island and delay lines, and the
   input filter and oversampler IIR ring after any bypass gap; a block longer than prepared + 64
   skips the dry capture, so global Mix < 100% blends stale dry.
4. Delete the stale remote branch `origin/feat/nam-profile-prototype` (recreated by a push after
   PR #55 merged; all its commits are already in `main`).
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
- `scripts/verify.sh` builds and runs `DistortionTests` only, as CI does. Linux needs the apt packages
  in `.github/workflows/build.yml` (the script uses `xvfb-run` when there is no display); its Linux and
  macOS branches are untested. It does not run pluginval or build the VST3.
- The mono-rejoin warm-up has no test of its own: the test models settle in under 1 ms.
- The clear-during-switch race test is timing-based (spins on a flag); the staged-refresh races are gated.
- `resetDSPState` clears a profile's islands, not its model state (NAM's reset allocates).
- Default renders aren't repeatable (time-seeded `juce::Random`): baselines need clipType != 0, waveshaperMix 0.
- Windows: close `Sledge Distortion.exe` before rebuilding (LNK1104). `Source/GitVersion.h` is skip-worktree.
- `docs/Architecture Contract.md` is gitignored (local only), as are `CLAUDE.md` and `GEMINI.md`.
