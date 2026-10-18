# State

## Now
- `main` has NAM profiles with a profile browser (arrows beside PROFILE, a name card, a `Profiles`
  library, content-fingerprint fallback, new instances start with the last profile) and the Shape
  control; unreleased, the plugin still reports 2.3.0. The last public release is v2.3.0.
- Verified 2026-10-08 on Linux (cloud, GCC Debug): `bash scripts/verify.sh` passes, 3987 assertions;
  pluginval 1.0.4 strictness 10 passes on the Release VST3, also with a `lastProfile` set. PR #65's
  CI built Linux, Windows and macOS green on 2026-10-08, before `main` was merged in.
- CI (`.github/workflows/build.yml`): a `changes` job skips the three platform builds when a push or
  PR touches only Markdown, `docs/` or `.agents/` (`THIRD-PARTY-NOTICES.md` still builds). Dev runs
  upload only the Windows ZIP, macOS ZIP and Linux tarball (~48 MB, 7 days); installers upload on
  tags only. The repo is public, so Actions minutes bill at $0 (run #214: 0 ms billable on all OSes).
  Actions pinned at `upload-artifact` v7.0.2 (#67) and `download-artifact` v8.0.2 (#68).

## Next
1. Owner: rebuild and install the Release VST3, then in FL: Load profile... with several files; arrows
   step and wrap; the card slides in from the clicked side, holds while loading, fades; a bubble with
   the scope folded; PROFILE lights only once loaded. Move the original download away and reopen the
   project: the library copy loads. New instance: last profile (Settings > Recall Profile off: none).
2. Confirm the CI docs-only gate: the next STATE.md-only push shows `changes` green and the three
   builds skipped, and a code PR still builds all three.
3. Owner: listen to Shape in FL (drive 0-10%, Sub Guard on, back to Flat); a v2.3.0 Low/Band Pass
   session sounds the same with the LEGACY FILTER tag; each factory preset's Shape by ear
   (`build-rel/shape-renders{,-v2}/`, local).
4. #44 (pluginval Automation segfault, Linux, 96 kHz / 64-sample blocks): 1,000 Windows runs found
   nothing on 2026-10-04; sweep seeds on Linux (temporary CI job or WSL). Stop `prepareToPlay`
   shrinking the band buffers below `rebuildOversampling`'s size and the pre-split overflow check
   carrying on in Release; give CI's pluginval a fixed `--random-seed`.
5. Bump the version (CMake `VERSION 2.3.0`; CHANGELOG `Unreleased` is written) and tag. The tag's
   `release` job is the first run to exercise `download-artifact` v8.0.2 (#68).
6. Owner: ASIO standalone at 44.1/96 kHz: swap/clear profiles while playing, Mono Input, Reaper latency;
   A/B `build-rel/nam-regression/sd1_*.wav` (unverified).
7. Profile browser follow-ups: each arrow click lists the library twice on the message thread
   (`step()`, `positionOf()`); a failed import copy is silent and loads the original path instead.
8. Decide on the unmerged remotes (`claude/ui-screenshot-readme-6vbm01`, `docs/profile-morph*`,
   `fix/transition-clicks`, `test/crossing-probe`) and `origin/main`'s `Claude <noreply@anthropic.com>` commits.
9. Older: host bypass freezes the island and delay lines; IIR ring after a bypass gap; a block longer
   than prepared + 64 skips the dry capture.

## Decisions
- Profiles live in a library Sledge owns (`diag::profilesDir()`); imports copy in and sessions point
  at the copy. A missing file falls back by content fingerprint (size + FNV-1a 64), never by name.
- The startup profile loads at the first `prepareToPlay`, only if no session, load or clear came first
  and only in real wrappers, so projects and tool renders never depend on the machine.
- The pill always reads PROFILE; the card names it. The clip-type column can't widen (the texture's
  knob cutouts), so the arrows are `S(9)` in the gaps beside the pill.
- `SettingsState::saveToFile` keeps attributes it doesn't own (`lastProfile`, `profileImportFolder`).
- CI docs-only gate is a job-level `if:`, not workflow `paths-ignore`: skipped jobs report success,
  so required checks still clear. Anything uncertain (tags, dispatch, new branch, missing base) builds.
- Shape, legacy filter, latency R, NAM threading: see `AGENTS.md`. MSVC builds stay warning-free.

## Known issues
- `verify.sh` runs tests only (no pluginval, no VST3); on Linux it needs `build.yml`'s apt packages
  first (a fresh cloud container lacks `Xrandr.h`); macOS branch untested. GCC is not warning-free:
  two `-Wfloat-equal` in `DistortionTests.cpp` and JUCE's `processBlockBypassed` `-Woverloaded-virtual`.
- Tests never construct the editor, so the arrow and menu glue is checked only by
  `DistortionUiSnapshot --profile <f> --profile-card`.
- A DAW scan that calls `prepareToPlay` loads the last profile per scanned instance (unverified).
- Release builds keep bug reports on: set `bugReports="0"` in settings.xml before a local pluginval, or
  check `%APPDATA%/Monolit Beatz/Sledge Distortion/reports` after.
- Windows: close `Sledge Distortion.exe` and any DAW holding the VST3 before rebuilding or installing.
- `AGENTS.md` still describes a Projucer build; the repo has no `Distortion.jucer`.
- Untested: the mono-rejoin warm-up; `resetDSPState` clears a profile's islands, not its model state.
- Local only (gitignored): `docs/superpowers/`, `docs/Architecture Contract.md`.
- Default renders aren't repeatable (time-seeded `juce::Random`): baselines need clipType != 0, waveshaperMix 0.
