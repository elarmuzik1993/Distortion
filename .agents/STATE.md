# State

## Now
- `main` has NAM profiles with a profile browser, the Shape control, the Shape / Legacy switch
  (Settings > Legacy Filter) and the four-pill clip-type column. Unreleased; the plugin still
  reports 2.3.0, the last public release.
- `docs/SUB_GUARD_TRANSITION_REVIEW.md` reviews Sub Guard and plans its fix. Section 2a is
  measured (`DistortionRender`, Release, 44.1 kHz): with any drive the sub is not untouched.
  The transient tamer takes 3.2 dB off a 45 Hz sine even at 150 Hz (0.2 dB with the tamer
  disabled), and the drive's products land below the crossover (8 Hz at −30 to −41 dBFS).
  Section 2 lists transition clicks from reading the code (F1-F8), unreproduced.
- Verified 2026-10-19 on Linux (cloud, Debug) at `2962b84`: `bash scripts/verify.sh` passes,
  4024 assertions. No code changed since 2026-10-18. pluginval not run since the profile browser.
- CI: docs-only pushes skip the three builds. Dev runs upload the three archives (7 days).

## Next
1. Sub Guard, plan in the doc's section 5, one PR per step: (1) failing tests, the clean-sub
   test and the transition tests; (2) F8, reset all banks in `resetDSPState`; (3) transient
   tamer on the high band only; (4) Clean Boost on the high band only; (5) extract
   `SubGuardSplitter`, bit-identical; (6) LR48 split around the drive; (7) on/off weight.
   The owner listens after steps 3, 4, 6 and 7.
2. Owner, Release standalone or FL: the clip column (PROFILE ends step, middle opens the menu;
   keep or enlarge the clip-type letters?), Settings > Legacy Filter, the profile browser.
3. CHANGELOG `Unreleased` says the profile arrows sit "either side of the PROFILE button"; they
   are on its ends. Fix it and add the column, in the next code PR.
4. #44 (pluginval Automation segfault, Linux, 96 kHz / 64-sample blocks): sweep seeds; stop
   `prepareToPlay` shrinking band buffers below `rebuildOversampling`'s size; fixed CI seed.
5. pluginval (strictness 10) on the Release VST3, bump CMake `VERSION 2.3.0`, tag.
6. Owner: ASIO standalone at 44.1/96 kHz: swap/clear profiles while playing, Mono Input, latency.
7. Follow-ups: the Legacy switch may click mid-playback; XY Y-axis is inaudible in Legacy mode;
   each arrow click lists the library twice; a failed import copy is silent. Decide on the
   unmerged remotes and `origin/main`'s `Claude <noreply@anthropic.com>` commits.

## Decisions
- Sub Guard gets one LR48 crossover at every setting, split around the drive: half before, half
  after, so the drive's low products are removed and the clean sum stays flat (owner,
  2026-10-19). The 150 Hz gentle-slope character goes; if wanted back it is its own control,
  never automatic zones, which cause the transition bugs. Snap points stay.
- Sub Guard on/off becomes a smoothed weight on the low band, not a glide: one drive pass, no
  very low cutoffs.
- `legacyFilter` is a session parameter; state version 3 turns it on for older states exactly
  when `LegacyInputFilter::isActive`, so v2.3 sessions sound the same.
- Profiles live in a library Sledge owns; sessions fall back by content fingerprint, never name.
- The clip column cannot widen (texture cutouts are registered to the knobs).
- Merge PRs manually once all three builds pass (`build-macos` is not a required check).

## Known issues
- `verify.sh` runs tests only; on Linux it needs `build.yml`'s apt packages (a fresh cloud
  container lacks them: install them first, about 2 minutes).
- Tests never construct the editor: check UI with `DistortionUiSnapshot`, not the standalone.
- `DistortionRender` has no clip-type or automation flags; Sub Guard transitions need tests.
- Release builds keep bug reports on: set `bugReports="0"` in settings.xml before a local pluginval.
- A DAW scan that calls `prepareToPlay` may load the last profile per instance (unverified).
- `pkill -f` on the standalone's path also kills the calling shell; kill by PID.
- Older: host bypass freezes the island and delay lines; IIR ring after a bypass gap.
