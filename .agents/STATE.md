# State

## Now
- `main` has NAM profiles with a profile browser (a `Profiles` library, content-fingerprint
  fallback, new instances start with the last profile), the Shape control, the Shape / Legacy
  switch (Settings > Legacy Filter), and a redesigned clip-type column: clip type, EXTREME, BOOST
  and PROFILE as four matching pills (`ClipColumnLookAndFeel`), the profile arrows inside PROFILE's
  ends. Unreleased; the plugin still reports 2.3.0. The last public release is v2.3.0.
- Verified 2026-10-18 on Linux (local, GCC Debug) at `8ad1b80`: `bash scripts/verify.sh` passes,
  4024 assertions. Release VST3 and Standalone build and launch. CI built the column green on
  Linux, Windows and macOS the same day. pluginval not run since the profile browser.
- CI: a `changes` job skips the three builds for docs-only pushes (seen working on `5712b57`).
  Dev runs upload only the three archives (7 days); installers upload on tags only.

## Next
1. Owner, Release standalone or FL: the clip column. Hover and click each PROFILE end (steps
   profiles), click the middle (menu); lit, the arrows turn white. The clip-type letters now share
   the pills' smaller font: keep, or enlarge just that pill?
2. CHANGELOG `Unreleased` > UI: add a line for the clip-column redesign (four matching pills).
3. Owner, same build: Settings > Legacy Filter (knob and picker swap in, Shape silent, LFO target
   reads Filter); a real v2.3.0 session that used Low or Band Pass opens in Legacy mode, same sound.
4. Owner: the profile browser (Load profile... with several files; arrows wrap; card slides, holds
   while loading, fades; bubble with the scope folded; a moved download still loads from the
   library; a new instance gets the last profile, none with Recall Profile off). Shape by ear.
5. #44 (pluginval Automation segfault, Linux, 96 kHz / 64-sample blocks): sweep seeds; stop
   `prepareToPlay` shrinking band buffers below `rebuildOversampling`'s size; fixed CI seed.
6. pluginval (strictness 10) on the Release VST3, bump the version (CMake `VERSION 2.3.0`), tag.
7. Owner: ASIO standalone at 44.1/96 kHz: swap/clear profiles while playing, Mono Input, latency.
8. Follow-ups: the switch may click mid-playback; XY Y-axis is inaudible in Legacy mode; each arrow
   click lists the library twice; a failed import copy is silent. Decide on the unmerged remotes
   (`claude/ui-screenshot-readme-6vbm01`, `docs/profile-morph*`, `fix/transition-clicks`,
   `test/crossing-probe`) and `origin/main`'s `Claude <noreply@anthropic.com>` commits.

## Decisions
- `legacyFilter` is a session parameter (it changes the sound); state version 3 turns it on for
  older states exactly when `LegacyInputFilter::isActive`, so v2.3 sessions sound the same.
- Profiles live in a library Sledge owns; sessions fall back by content fingerprint, never by name.
- The clip column cannot widen (texture cutouts are registered to the knobs), so the arrows live
  inside the PROFILE pill. The stack sits 3 px below the knob centre, set by the owner's eye.
- Merge PRs manually once all three builds pass (`build-macos` is not a required check).

## Known issues
- `verify.sh` runs tests only (no pluginval, no VST3); on Linux it needs `build.yml`'s apt packages.
- Tests never construct the editor: check UI with `DistortionUiSnapshot`, not the standalone.
- Release builds keep bug reports on: set `bugReports="0"` in settings.xml before a local pluginval.
- A DAW scan that calls `prepareToPlay` may load the last profile per instance (unverified).
- `pkill -f` on the standalone's path also kills the calling shell; kill by PID.
- Older: host bypass freezes the island and delay lines; IIR ring after a bypass gap.
