# State

## Now
- `main` has NAM profiles with a profile browser (arrows beside PROFILE, a name card, a `Profiles`
  library, content-fingerprint fallback, new instances start with the last profile) and the Shape
  control; unreleased, the plugin still reports 2.3.0. The last public release is v2.3.0.
- Verified 2026-10-08 on Linux (cloud, GCC Debug): `bash scripts/verify.sh` passes, 3987 assertions;
  pluginval 1.0.4 strictness 10 passes on the Release VST3, also with a `lastProfile` set.
  Windows and macOS have not been built since the browser (CI runs on PRs and `main` only).

## Next
1. Owner: rebuild and install the Release VST3, then in FL: Load profile... with several files; arrows
   step and wrap; the card slides in from the clicked side, holds while loading, fades; a bubble with
   the scope folded; PROFILE lights only once loaded. Move the original download away and reopen the
   project: the library copy loads. New instance: last profile (Settings > Recall Profile off: none).
2. Owner: listen to Shape in FL (drive 0-10%, Sub Guard on, back to Flat); a v2.3.0 Low/Band Pass
   session sounds the same with the LEGACY FILTER tag; each factory preset's Shape by ear.
3. #44 (pluginval Automation segfault, Linux, 96 kHz / 64-sample blocks): sweep seeds on Linux; stop
   `prepareToPlay` shrinking the band buffers below `rebuildOversampling`'s size and the pre-split
   overflow check carrying on in Release; give CI's pluginval a fixed `--random-seed`.
4. Bump the version (CMake `VERSION 2.3.0`; CHANGELOG `Unreleased` is written) and tag.
5. Owner: ASIO standalone at 44.1/96 kHz: swap/clear profiles while playing, Mono Input, Reaper latency.
6. Decide on the unmerged remotes (`claude/ui-screenshot-readme-6vbm01`, `docs/profile-morph*`,
   `fix/transition-clicks`, `test/crossing-probe`) and `origin/main`'s `Claude <noreply@anthropic.com>` commits.
7. Older: host bypass freezes the island and delay lines; IIR ring after a bypass gap; a block longer
   than prepared + 64 skips the dry capture.

## Decisions
- Profiles live in a library Sledge owns (`diag::profilesDir()`); imports copy in and sessions point
  at the copy. A missing file falls back by content fingerprint (size + FNV-1a 64), never by name.
- The startup profile loads at the first `prepareToPlay`, only if no session, load or clear came first
  and only in real wrappers, so projects and tool renders never depend on the machine.
- The pill always reads PROFILE; the card names it. The clip-type column can't widen (the texture's
  knob cutouts), so the arrows are `S(9)` in the gaps beside the pill.
- `SettingsState::saveToFile` keeps attributes it doesn't own (`lastProfile`, `profileImportFolder`).
- Shape, legacy filter, latency R, NAM threading: see `AGENTS.md`. MSVC builds stay warning-free.

## Known issues
- `verify.sh` runs tests only; Linux needs `build.yml`'s apt packages; macOS branch untested. GCC warns
  twice (`-Wfloat-equal` in `DistortionTests.cpp`). Tests never construct the editor, so the arrow and
  menu glue is checked only by `DistortionUiSnapshot --profile <f> --profile-card`.
- A DAW scan that calls `prepareToPlay` loads the last profile per scanned instance (unverified).
- Release builds keep bug reports on: set `bugReports="0"` in settings.xml before a local pluginval.
- Windows: close `Sledge Distortion.exe` and any DAW holding the VST3 before rebuilding or installing.
- `AGENTS.md` still describes a Projucer build; the repo has no `Distortion.jucer`.
- Untested: the mono-rejoin warm-up; `resetDSPState` clears a profile's islands, not its model state.
- Local only (gitignored): `docs/superpowers/`, `docs/Architecture Contract.md`.
- Default renders aren't repeatable (time-seeded `juce::Random`): baselines need clipType != 0, waveshaperMix 0.
