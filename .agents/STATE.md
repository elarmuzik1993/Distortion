# State

## Now
- `main` has NAM profiles and the Shape control (Bark ↔ Scoop, replacing the input filter's knob and
  mode selector); unreleased, the plugin still reports 2.3.0. The last public release is v2.3.0.
- CI (`.github/workflows/build.yml`): a `changes` job skips the three platform builds when a push or
  PR touches only Markdown, `docs/` or `.agents/` (`THIRD-PARTY-NOTICES.md` still builds). Dev runs
  upload only the Windows ZIP, macOS ZIP and Linux tarball (~48 MB, 7 days); installers upload on
  tags only. The repo is public, so Actions minutes bill at $0 (run #214: 0 ms billable on all OSes).
- Verified 2026-10-09 on Linux (cloud container, GCC Debug) at this hand-off: `bash scripts/verify.sh`
  passes, `DistortionTests` 3872 assertions.
- Verified 2026-10-04 at `bc7e9c5`, Windows MSVC Debug: `bash scripts/verify.sh` passes, 3872
  assertions, no warnings. This machine's `C:\Program Files\Common Files\VST3` holds a Release build.

## Next
1. After the CI change merges: confirm the next STATE.md-only push shows `changes` green and the
   three builds skipped, and that a code PR still builds all three.
2. Owner: listen to Shape in FL: drive 0-10% (post-drive half fades in), Sub Guard on, return to
   Flat. Load a real v2.3.0 session that used Low Pass or Band Pass: same sound, LEGACY FILTER tag
   shown. Check each factory preset's Shape by ear (`build-rel/shape-renders{,-v2}/`, local).
3. #44 (pluginval Automation segfault, 96 kHz / 64-sample blocks, Release): 1,000 Windows runs found
   nothing on 2026-10-04. It crashed on Linux: sweep seeds there (temporary CI job or WSL). Harden the
   suspect regardless: `prepareToPlay` shrinks the band buffers to `block * factor + 64` after
   `rebuildOversampling` sized them larger, and the pre-split overflow check sets
   `debugHadBufferOverflow` but carries on in Release. Add a fixed `--random-seed` to CI's pluginval.
4. Bump the version (CMake `VERSION 2.3.0`; CHANGELOG `Unreleased` already describes Shape) and tag.
5. Owner: ASIO standalone at 44.1 and 96 kHz: load, swap and clear NAM profiles while playing, Mono
   Input on and off; Reaper latency stays put; A/B `build-rel/nam-regression/sd1_*.wav` (unverified).
6. Decide on the unmerged remotes: `claude/ui-screenshot-readme-6vbm01`, `docs/profile-morph`,
   `-design`, `-review`, `fix/transition-clicks`, `test/crossing-probe`.
7. Older follow-ups: host bypass freezes the island and delay lines; the input filter and oversampler
   IIR ring after a bypass gap; a block longer than prepared + 64 skips the dry capture.
8. Owner: decide on the 41 `origin/main` commits authored `Claude <noreply@anthropic.com>`.

## Decisions
- CI docs-only gate is a job-level `if:`, not workflow `paths-ignore`: skipped jobs report success,
  so required checks still clear. Anything uncertain (tags, dispatch, new branch, missing base) builds.
- Shape: 600 Hz bell +12/−15 dB, opposing 2.5 kHz shelf ±6 dB, linear taper. Pre-drive at base rate
  (also in true bypass); post-drive (0.75) after the Sub Guard subtraction, faded in over 0-10% drive.
- Legacy `filterMode`/`highPassFreq` stay registered (hidden); state version 2 resets `shape` to 0.
- Reported latency is always R = max(user oversampler latency, island delay for a 48 kHz model).
- NAM: true bypass keeps running the profile; lock order callback lock, then `profileStatusLock`.
  NAM Core pinned `0b3d3c9`; ASIO opt-in; offline renders never duck.
- The build stays warning-free on MSVC; a change that adds a warning gets fixed before merge.

## Known issues
- `scripts/verify.sh` runs `DistortionTests` only (no pluginval, no VST3). On Linux it needs the apt
  packages from `.github/workflows/build.yml` first (a fresh cloud container lacks `Xrandr.h`).
- Release builds keep auto bug reports on: a local pluginval run can queue reports in
  `%APPDATA%/Monolit Beatz/Sledge Distortion/reports` that send on the next launch. Check after.
- Windows: close `Sledge Distortion.exe` and any DAW holding the VST3 before rebuilding or installing.
- GCC (Linux) is not warning-free, unlike MSVC: two `-Wfloat-equal` in `Source/Tests/DistortionTests.cpp`
  (lines ~5825, ~6060) and JUCE's own `processBlockBypassed` `-Woverloaded-virtual`.
- Untested: the mono-rejoin warm-up; the clear-during-switch race test is timing-based.
- `resetDSPState` clears a profile's islands, not its model state. Sessions remember a profile by path.
- Default renders aren't repeatable (time-seeded `juce::Random`): baselines need clipType != 0.
- Local only (gitignored): `docs/superpowers/` (Shape spec and plan), `docs/Architecture Contract.md`.
