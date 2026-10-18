# State

## Now
- `main` has NAM profiles with a profile browser (arrows beside PROFILE, a name card, a `Profiles`
  library, content-fingerprint fallback, new instances start with the last profile), the Shape
  control, and the Shape / Legacy switch: Settings > Legacy Filter turns the knob beside Input Gain
  into the v2.3 input filter (cutoff knob + High/Low/Band Pass picker) with Shape held Flat.
  Unreleased; the plugin still reports 2.3.0. The last public release is v2.3.0.
- Verified 2026-10-18 on Linux (local, GCC Debug) at `a5466ad`: `bash scripts/verify.sh` passes,
  4024 assertions. The Release standalone builds and launches there. CI built the switch green on
  Linux, Windows and macOS the same day. pluginval not run since the profile browser.
- CI (`.github/workflows/build.yml`): a `changes` job skips the three platform builds when a push or
  PR touches only Markdown, `docs/` or `.agents/` (`THIRD-PARTY-NOTICES.md` still builds). Dev runs
  upload only the Windows ZIP, macOS ZIP and Linux tarball (~48 MB, 7 days); installers upload on
  tags only. Actions pinned at `upload-artifact` v7.0.2 and `download-artifact` v8.0.2.

## Next
1. Owner, in FL with a fresh Release VST3: flip Settings > Legacy Filter (knob and picker swap in,
   Shape goes silent, the LFO target reads Filter; flipping back restores both). Open a real v2.3.0
   session that used Low or Band Pass: it opens in Legacy mode and sounds the same.
2. Owner, same build: the profile browser (several files via Load profile...; arrows step and wrap;
   the card slides, holds while loading, fades; bubble with the scope folded). Move a download away
   and reopen the project: the library copy loads. New instance: last profile (Recall Profile off: none).
3. Owner: Shape by ear (drive 0-10%, Sub Guard on, back to Flat); each factory preset's Shape
   (`build-rel/shape-renders{,-v2}/`, local).
4. Check this hand-off's push: `changes` green and the three builds skipped (the docs-only gate).
5. #44 (pluginval Automation segfault, Linux, 96 kHz / 64-sample blocks): sweep seeds on Linux. Stop
   `prepareToPlay` shrinking the band buffers below `rebuildOversampling`'s size and the pre-split
   overflow check carrying on in Release; give CI's pluginval a fixed `--random-seed`.
6. Run pluginval (strictness 10) on the Release VST3, bump the version (CMake `VERSION 2.3.0`;
   CHANGELOG `Unreleased` is written) and tag. The tag's `release` job first exercises download-artifact 8.0.2.
7. Owner: ASIO standalone at 44.1/96 kHz: swap/clear profiles while playing, Mono Input, Reaper latency.
8. Follow-ups: flipping the switch mid-playback may click (the SVF changes type at once); XY Morph's
   Y axis drives Shape, inaudible in Legacy mode; each profile arrow click lists the library twice on
   the message thread; a failed profile import copy is silent.
9. Decide on the unmerged remotes (`claude/ui-screenshot-readme-6vbm01`, `docs/profile-morph*`,
   `fix/transition-clicks`, `test/crossing-probe`) and `origin/main`'s `Claude <noreply@anthropic.com>` commits.
10. Older: host bypass freezes the island and delay lines; IIR ring after a bypass gap; a block longer
    than prepared + 64 skips the dry capture.

## Decisions
- `legacyFilter` is a session parameter, not a machine setting: it changes the sound. Shape mode keeps
  the legacy filter at its 20 Hz high pass, so Shape sessions sound as before. State version 3 turns it
  on for older states exactly when `LegacyInputFilter::isActive`, so v2.3 sessions sound the same.
- Profiles live in a library Sledge owns (`diag::profilesDir()`); sessions point at the copy and fall
  back by content fingerprint, never by name. The startup profile loads at the first `prepareToPlay`,
  only if no session, load or clear came first, and only in real wrappers.
- CI docs-only gate is a job-level `if:`, not `paths-ignore`, so skipped required checks still clear.
- Shape, the switch, latency R, NAM threading: see `AGENTS.md`. MSVC builds stay warning-free.

## Known issues
- `verify.sh` runs tests only (no pluginval, no VST3); on Linux it needs `build.yml`'s apt packages
  first. GCC warns: two `-Wfloat-equal` in `DistortionTests.cpp`, JUCE's `-Woverloaded-virtual`.
- Tests never construct the editor: UI glue is checked by `DistortionUiSnapshot` (`--legacy`,
  `--settings`, `--profile <f> --profile-card`).
- A DAW scan that calls `prepareToPlay` loads the last profile per scanned instance (unverified).
- Release builds keep bug reports on: set `bugReports="0"` in settings.xml before a local pluginval.
- Windows: close `Sledge Distortion.exe` and any DAW holding the VST3 before rebuilding or installing.
- `AGENTS.md` still describes a Projucer build; the repo has no `Distortion.jucer`.
- Untested: the mono-rejoin warm-up; `resetDSPState` clears a profile's islands, not its model state.
- Local only (gitignored): `docs/superpowers/`, `docs/Architecture Contract.md`.
- Default renders aren't repeatable (time-seeded `juce::Random`): baselines need clipType != 0, waveshaperMix 0.
