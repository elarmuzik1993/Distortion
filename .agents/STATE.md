# State

## Now
- `main` has NAM profiles and the Shape control (Bark ↔ Scoop, replacing the input filter's knob and
  mode selector); unreleased, the plugin still reports 2.3.0. The last public release is v2.3.0.
- Verified 2026-10-04 at `bc7e9c5`, Windows MSVC Debug: `bash scripts/verify.sh` passes,
  `DistortionTests` 3872 assertions, no warnings.
- This machine's `C:\Program Files\Common Files\VST3` holds a Release build of the same code, for FL Studio.

## Next
1. Owner: listen to Shape in FL: drive 0-10% (post-drive half fades in), Sub Guard on, return to
   Flat. Load a real v2.3.0 session that used Low Pass or Band Pass: same sound, LEGACY FILTER tag
   shown. Check each factory preset's Shape by ear (`build-rel/shape-renders{,-v2}/`, local).
2. #44 (pluginval Automation segfault, 96 kHz / 64-sample blocks, Release): 1,000 Windows runs found
   nothing on 2026-10-04 (pluginval 1.0.4, strictness 10, seeds 1-500 with and without GUI tests). It
   crashed on Linux: sweep seeds there (temporary CI job or WSL). Harden the suspect regardless:
   `prepareToPlay` shrinks the band buffers to `block * factor + 64` after `rebuildOversampling` sized
   them larger, and the pre-split overflow check sets `debugHadBufferOverflow` but carries on in
   Release. Add a fixed `--random-seed` to CI's pluginval. Close #44 once Linux is clean.
3. Bump the version (CMake `VERSION 2.3.0`; CHANGELOG `Unreleased` already describes Shape) and tag.
4. Owner: ASIO standalone at 44.1 and 96 kHz: load, swap and clear NAM profiles while playing, Mono
   Input on and off; Reaper latency stays put; A/B `build-rel/nam-regression/sd1_*.wav` (unverified).
5. Decide on the unmerged remotes: `claude/ui-screenshot-readme-6vbm01`, `docs/profile-morph`,
   `-design`, `-review`, `fix/transition-clicks`, `test/crossing-probe`.
6. Older follow-ups: host bypass freezes the island and delay lines; the input filter and oversampler
   IIR ring after a bypass gap; a block longer than prepared + 64 skips the dry capture (stale dry at Mix < 100%).
7. Owner: decide on the 41 `origin/main` commits authored `Claude <noreply@anthropic.com>`.

## Decisions
- Shape (`Source/ShapeFilter.h`): 600 Hz bell +12/−15 dB, opposing 2.5 kHz shelf ±6 dB, linear taper,
  raised after a listening pass. Pre-drive at base rate (also in true bypass); post-drive (0.75) after
  the Sub Guard subtraction, faded in with drive over 0-10% so the bypass threshold doesn't jump.
  Tests derive targets from the `SHAPE_*` constants.
- Legacy `filterMode`/`highPassFreq` stay registered (hidden, IDs unchanged); state version 2 resets
  `shape` to 0 for older states. LFO destination 2 sweeps Shape.
- Reported latency is always R = max(user oversampler latency, island delay for a 48 kHz model).
- NAM: true bypass keeps running the profile (reset allocates); mono input shares channel 0's model and
  warms channel 1 up unheard on rejoin; the loader thread starts on first load and re-prepares stale
  staged profiles; lock order callback lock, then `profileStatusLock`; long blocks run the profile in
  pieces. NAM Core pinned `0b3d3c9`; ASIO opt-in; offline renders never duck.
- The build stays warning-free on MSVC; a change that adds a warning gets fixed before merge.

## Known issues
- `scripts/verify.sh` runs `DistortionTests` only (no pluginval, no VST3); Linux needs the apt packages
  in `.github/workflows/build.yml`. Its Linux and macOS branches are untested by hand.
- Release builds keep auto bug reports on: a local pluginval run can queue reports in
  `%APPDATA%/Monolit Beatz/Sledge Distortion/reports` that send on the next launch. Check after.
- Windows: close `Sledge Distortion.exe` and any DAW holding the VST3 before rebuilding or installing.
- Untested: the mono-rejoin warm-up; the clear-during-switch race test is timing-based.
- `resetDSPState` clears a profile's islands, not its model state (NAM's reset allocates).
  Sessions remember a profile by path.
- Default renders aren't repeatable (time-seeded `juce::Random`): baselines need clipType != 0, waveshaperMix 0.
- Local only (gitignored): `docs/superpowers/` (Shape spec and plan), `docs/Architecture Contract.md`.
