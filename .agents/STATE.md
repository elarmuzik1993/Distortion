# State

## Now
- `main` at `87f02e1`: PRs #55 (NAM resampling), #59 (repo kit), #60 (state) and #54 (housekeeping:
  `GitVersion.h` generated in the build tree, warning-free build) are merged; CI green on each.
- Verified 2026-10-03 at `87f02e1`, Windows MSVC Debug: `bash scripts/verify.sh` passes,
  `DistortionTests` 3615 assertions.
- Open PR: #56 (`fix/filter-mode-switch-cutoff`, 1 commit `ff0ad79`, worktree `~/Distortion-filterfix`).
  CI was green when last checked; it predates the #54 merge, so re-check it against `main`.

## Next
1. Owner: listen to the filter modes in `~/Distortion-filterfix`, re-test #44, then merge #56
   (rebase on `main` first if GitHub reports a conflict).
2. Owner: ASIO standalone at 44.1 and 96 kHz: load, swap and clear profiles while playing, with
   Mono Input on and off; Reaper latency display stays put; A/B
   `build-rel/nam-regression/sd1_{44100,48000,96000}.wav` (local files, unverified).
3. Decide on the unmerged remote branches: `claude/ui-screenshot-readme-6vbm01` (1 commit),
   `docs/profile-morph`, `-design`, `-review` (2 each), `fix/transition-clicks` (3), `test/crossing-probe` (1).
4. Follow-ups predating the NAM branch: host bypass still freezes the island and delay lines, and the
   input filter and oversampler IIR ring after any bypass gap; a block longer than prepared + 64
   skips the dry capture, so global Mix < 100% blends stale dry.
5. Then dispatch "Build VST3 Plugin" for macOS and Linux; the Linux and macOS branches of
   `verify.sh` are untested by hand too.
6. Owner: decide on the 41 `origin/main` commits authored `Claude <noreply@anthropic.com>`
   (not rechecked since the last handoff).

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
- Windows: close `Sledge Distortion.exe` before rebuilding (LNK1104).
- `docs/Architecture Contract.md` is gitignored (local only), as are `CLAUDE.md` and `GEMINI.md`.
