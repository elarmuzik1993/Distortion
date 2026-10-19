# State

## Now
- `main` has NAM profiles with a profile browser (a `Profiles` library, content-fingerprint
  fallback, new instances start with the last profile), the Shape control, the Shape / Legacy
  switch (Settings > Legacy Filter), and a redesigned clip-type column: clip type, EXTREME, BOOST
  and PROFILE as four matching pills (`ClipColumnLookAndFeel`), the profile arrows inside PROFILE's
  ends. Unreleased; the plugin still reports 2.3.0. The last public release is v2.3.0.
- Sub Guard: `docs/SUB_GUARD_TRANSITION_REVIEW.md` reviews it and plans the fix (section 5,
  hand-off in section 8). Steps 1-3 are on `main`: #73 pending tests (`SubGuardPendingTests`,
  asserting only with `DISTORTION_PENDING_TESTS=1`); #74 the F8 state-load reset; #75
  `SubGuardSplitter` extracted, renders byte-identical.
- Verified 2026-10-19 on Linux (cloud, Debug) at the step 3 head: `bash scripts/verify.sh`
  passes, 4072 assertions. pluginval not run since the profile browser.
- CI: a `changes` job skips the three builds for docs-only pushes (seen working on `5712b57`).
  Dev runs upload only the three archives (7 days); installers upload on tags only.

## Next
1. Before Sub Guard step 4: a test seeding hook for `distortionRandom`/`waveshaperRandom`, usable
   from tests and `DistortionRender`, so renders with drive on are repeatable (doc section 8).
2. Sub Guard step 4 (transient tamer on the high band), then 5-7. Compatibility is decided:
   accept the change. The owner listens before each of steps 4-7 merges.
3. Owner, Release standalone or FL: the clip column. Hover and click each PROFILE end (steps
   profiles), click the middle (menu); lit, the arrows turn white. The clip-type letters now share
   the pills' smaller font: keep, or enlarge just that pill?
4. Owner, same build: Settings > Legacy Filter (knob and picker swap in, Shape silent, LFO target
   reads Filter); a real v2.3.0 session that used Low or Band Pass opens in Legacy mode, same sound.
5. Owner: the profile browser (Load profile... with several files; arrows wrap; card slides, holds
   while loading, fades; bubble with the scope folded; a moved download still loads from the
   library; a new instance gets the last profile, none with Recall Profile off). Shape by ear.
6. #44 (pluginval Automation segfault, Linux, 96 kHz / 64-sample blocks): sweep seeds; stop
   `prepareToPlay` shrinking band buffers below `rebuildOversampling`'s size; fixed CI seed.
7. pluginval (strictness 10) on the Release VST3, bump the version (CMake `VERSION 2.3.0`), tag.
8. Owner: ASIO standalone at 44.1/96 kHz: swap/clear profiles while playing, Mono Input, latency.
9. Follow-ups: the switch may click mid-playback; XY Y-axis is inaudible in Legacy mode; each arrow
   click lists the library twice; a failed import copy is silent. Decide on the unmerged remotes
   (`claude/ui-screenshot-readme-6vbm01`, `docs/profile-morph*`, `fix/profile-browser-review`,
   `fix/transition-clicks`, `test/crossing-probe`) and `origin/main`'s
   `Claude <noreply@anthropic.com>` commits. The Sub Guard step branches can go once merged.

## Decisions
- `legacyFilter` is a session parameter (it changes the sound); state version 3 turns it on for
  older states exactly when `LegacyInputFilter::isActive`, so v2.3 sessions sound the same.
- Profiles live in a library Sledge owns; sessions fall back by content fingerprint, never by name.
- The clip column cannot widen (texture cutouts are registered to the knobs), so the arrows live
  inside the PROFILE pill. The stack sits 3 px below the knob centre, set by the owner's eye.
- Merge PRs manually once all three builds pass (`build-macos` is not a required check).
- Sub Guard gets one LR48 crossover at every setting, split around the drive (half before, half
  after). The zones and their slope switching go; snap points stay (owner, 2026-10-19).
- Sub Guard on/off is a smoothed weight `g`, not a glide, with the splitter kept warm while off.
- Sub Guard's sound change is accepted (owner, 2026-10-19): no version gate; step 6 re-voices
  the seven presets, regenerates Golden Audio and adds a CHANGELOG note.

## Known issues
- `verify.sh` runs tests only (no pluginval, no VST3); on Linux it needs `build.yml`'s apt packages.
- Tests never construct the editor: check UI with `DistortionUiSnapshot`, not the standalone.
- Release builds keep bug reports on: set `bugReports="0"` in settings.xml before a local pluginval.
- A DAW scan that calls `prepareToPlay` may load the last profile per instance (unverified).
- `pkill -f` on the standalone's path also kills the calling shell; kill by PID.
- Older: host bypass freezes the island and delay lines; IIR ring after a bypass gap.
- Sub Guard coefficients: `prepareToPlay` writes float (`make*`), the runtime writes double
  (`FilterCoeffs.h`); at 60 Hz, 4x they differ until the frequency moves more than 0.5 Hz.
- Renders with drive on are not repeatable: `distortionRandom`/`waveshaperRandom` are
  clock-seeded.
- Pending F1 check at `distMix` 100 passes narrowly (1.95x against a 2.0x limit).
