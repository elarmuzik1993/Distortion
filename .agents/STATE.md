# State

## Now
- `main` at `a0b9d5d`: PR #56 (filter mode switch moves a default cutoff to the new mode's default)
  merged, plus a commit clearing the warning its test added. CI green; branch and worktree deleted.
- Verified 2026-10-03 at `a0b9d5d`, Windows MSVC Debug: `verify.sh` passes, 3628 assertions, 0 warnings.
- No open PRs (checked with `gh pr list`).

## Next
1. Owner: try the filter modes by hand in the plugin window (HP 20 Hz → LP → BP → HP should land
   on 20 kHz, 1 kHz, 20 Hz; a cutoff you set stays put), then re-test #44 and close it.
2. Owner: ASIO standalone at 44.1 and 96 kHz: load, swap and clear profiles while playing, with
   Mono Input on and off; Reaper latency display stays put; A/B
   `build-rel/nam-regression/sd1_{44100,48000,96000}.wav` (local files, unverified).
3. Decide on the unmerged remote branches: `claude/ui-screenshot-readme-6vbm01`,
   `docs/profile-morph`, `-design`, `-review`, `fix/transition-clicks`, `test/crossing-probe`.
4. Follow-ups predating the NAM branch: host bypass still freezes the island and delay lines, and the
   input filter and oversampler IIR ring after any bypass gap; a block longer than prepared + 64
   skips the dry capture, so global Mix < 100% blends stale dry.
5. Run the Linux and macOS branches of `verify.sh` by hand (untested).
6. Owner: decide on the 41 `origin/main` commits authored `Claude <noreply@anthropic.com>`
   (not rechecked since the last handoff).

## Decisions
- Filter mode switch: only a user gesture moves the cutoff, and only when it sits at the old mode's
  default, so presets, sessions, randomize and automation restore exactly what they saved.
- Reported latency is always R = max(user oversampler latency, island delay for a 48 kHz model).
- True bypass with a profile keeps running it, output discarded: NAM's reset allocates.
- A block longer than the scratch runs the profile in pieces, never the zero-latency built-in clip.
- Mono input: channel 1 takes channel 0's model output and its models rest. Back to stereo they warm
  up unheard (max 100 ms), then fade in over 30 ms.
- A stale staged profile is re-prepared on the loader thread (prepare() prewarms the model);
  `stagedRefreshesInFlight` keeps `isProfileSwitchIdle` honest. Lock order: callback lock, then
  `profileStatusLock`. The loader thread is created on first load, not per instance.
- Offline renders never duck. NAM Core pinned `0b3d3c9`. ASIO opt-in. Sessions remember a profile by path.
- The build stays warning-free on MSVC; a PR that adds a warning gets fixed before merge.

## Known issues
- `scripts/verify.sh` builds and runs `DistortionTests` only, as CI does: no pluginval, no VST3 build.
- The mono-rejoin warm-up has no test of its own: the test models settle in under 1 ms.
- The clear-during-switch race test is timing-based (spins on a flag); the staged-refresh races are gated.
- `resetDSPState` clears a profile's islands, not its model state (NAM's reset allocates).
- Default renders aren't repeatable (time-seeded `juce::Random`): baselines need clipType != 0, waveshaperMix 0.
- Windows: close `Sledge Distortion.exe` before rebuilding (LNK1104).
- `docs/Architecture Contract.md` is gitignored (local only), as are `CLAUDE.md` and `GEMINI.md`.
