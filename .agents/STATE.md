# State

## Now
- `main` at `74ff1af`: PR #62 (Shape control, replaces the input filter) merged; CI green on
  Windows, Linux and macOS (pluginval 10, auval). PR #56 (filter mode-switch fix) merged earlier
  (`a0b9d5d`); Shape later removed that selector. No open PRs.
- Verified 2026-10-03 at `74ff1af`, Windows MSVC Debug: `bash scripts/verify.sh` passes,
  `DistortionTests` 3872 assertions, no warnings.
- The merged Release VST3 (`v2.3.0.114-74ff1af`) is installed in `C:\Program Files\Common Files\VST3`
  for testing in FL Studio (this machine only).

## Next
1. Owner: listen to the merged Shape in FL: drive 0-10% (post-drive half fades in), Sub Guard on,
   return to Flat. Load a real v2.3.0 session that used Low Pass or Band Pass: it should sound
   unchanged and show the LEGACY FILTER tag. Check each factory preset's Shape value by ear
   (`build-rel/shape-renders{,-v2}/`, local renders; Screamer is ~6 dB louder than the rest).
2. Bump the version (CMake `VERSION 2.3.0`; CHANGELOG `Unreleased` already describes Shape) before tagging.
   Re-test #44 first (pluginval Automation segfault at 96 kHz / 64-sample blocks, Release) and
   close it if it no longer reproduces.
3. Owner: ASIO standalone at 44.1 and 96 kHz: load, swap and clear NAM profiles while playing, with
   Mono Input on and off; Reaper latency display stays put; A/B
   `build-rel/nam-regression/sd1_{44100,48000,96000}.wav` (local files, unverified).
4. Delete merged branches: local `feat/shape-control`, `docs/state-after-filter-fix`, remote
   `feat/shape-control`. Decide on the unmerged remotes: `claude/ui-screenshot-readme-6vbm01`,
   `docs/profile-morph`, `-design`, `-review`, `fix/transition-clicks`, `test/crossing-probe`.
5. Follow-ups predating the NAM branch: host bypass still freezes the island and delay lines, and the
   input filter and oversampler IIR ring after any bypass gap; a block longer than prepared + 64
   skips the dry capture, so global Mix < 100% blends stale dry.
6. Owner: decide on the 41 `origin/main` commits authored `Claude <noreply@anthropic.com>`.

## Decisions
- Shape (`Source/ShapeFilter.h`): 600 Hz bell +12/−15 dB, opposing 2.5 kHz shelf ±6 dB, linear taper;
  raised from +9/−12, ±4, |s|^1.5 after a listening pass. Tests derive targets from `SHAPE_*`.
- Pre-drive Shape at base rate (also in true bypass). Post-drive Shape (0.75) runs after the Sub Guard
  subtraction and fades in with drive (0 at 0%, full at 10%), so the bypass threshold does not jump.
- Legacy `filterMode`/`highPassFreq` stay registered (hidden, IDs unchanged, host names "Legacy
  Filter …"); state version 2 resets `shape` to 0 for older states. LFO destination 2 now sweeps Shape.
- Reported latency is always R = max(user oversampler latency, island delay for a 48 kHz model).
- True bypass with a profile keeps running it, output discarded: NAM's reset allocates.
- Mono input: channel 1 takes channel 0's model output; back to stereo warms up unheard, fades in 30 ms.
- Profile loader thread created on first load; stale staged profiles re-prepare there (prepare prewarms),
  tracked by `stagedRefreshesInFlight`; lock order: callback lock, then `profileStatusLock`.
- A block longer than the scratch runs the profile in pieces, never the zero-latency built-in clip.
- The build stays warning-free on MSVC; a PR that adds a warning gets fixed before merge.
- Offline renders never duck. NAM Core pinned `0b3d3c9`. ASIO opt-in. Sessions remember a profile by path.

## Known issues
- `scripts/verify.sh` builds and runs `DistortionTests` only; no pluginval, no VST3. Linux needs the
  apt packages in `.github/workflows/build.yml` (it uses `xvfb-run` without a display); its Linux and
  macOS branches are untested by hand (CI covers those platforms).
- The spec and plan for Shape live in `docs/superpowers/` (gitignored, local only).
- The mono-rejoin warm-up has no test of its own; the clear-during-switch race test is timing-based.
- `resetDSPState` clears a profile's islands, not its model state (NAM's reset allocates).
- Default renders aren't repeatable (time-seeded `juce::Random`): baselines need clipType != 0, waveshaperMix 0.
- Windows: close `Sledge Distortion.exe` and any DAW holding the VST3 before rebuilding/installing.
- `docs/Architecture Contract.md` is gitignored (local only), as are `CLAUDE.md` and `GEMINI.md`.
