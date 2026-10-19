# State

## Now
- `main` has NAM profiles with a profile browser, the Shape control, the Shape / Legacy switch
  (Settings > Legacy Filter) and the four-pill clip-type column. Unreleased; the plugin still
  reports 2.3.0, the last public release.
- `docs/SUB_GUARD_TRANSITION_REVIEW.md` reviews Sub Guard and plans its fix. Section 2a is
  measured: with any drive the sub is not untouched (transient tamer −3.2 dB on a 45 Hz sine,
  drive products below the crossover). Section 2 (F1-F8) is from reading the code, unreproduced.
  Section 5 is the plan; section 8 is the implementation hand-off for step 1.
- Verified 2026-10-19 on Linux (cloud, Debug) at `f3d7131`: `bash scripts/verify.sh` passes,
  4024 assertions. No source changed since 2026-10-18. pluginval not run since the profile browser.

## Next
1. Sub Guard step 1 (doc section 8): failing tests for clean sub, state load, off/on, on/off,
   boundary chatter, in a pending class gated by `DISTORTION_PENDING_TESTS=1`.
2. Steps 2-3: F8 (reset all banks in `resetDSPState`); extract `SubGuardSplitter`, bit-identical.
3. Owner, before step 4: Sub Guard compatibility, accept the sound change and re-voice the seven
   presets at 100-150 Hz (recommended), or gate older sessions behind state version 4.
4. Steps 4-7: tamer and Clean Boost on the high band; LR48 split around the drive (`double` or
   TPT); on/off weight `g` on the drive input, the post-drive high-pass, the low band and the
   auto-gain reference, with the splitter kept warm. Owner listens after each.
5. Owner, Release standalone or FL: the clip column, Settings > Legacy Filter, the profile browser.
6. CHANGELOG `Unreleased`: the profile arrows are on PROFILE's ends, not "either side"; add the
   column. In the next code PR.
7. #44 (pluginval Automation segfault, Linux, 96 kHz / 64-sample blocks): sweep seeds; stop
   `prepareToPlay` shrinking band buffers below `rebuildOversampling`'s size; fixed CI seed.
8. pluginval (strictness 10) on the Release VST3, bump CMake `VERSION 2.3.0`, tag.
9. Follow-ups: the Legacy switch may click mid-playback; XY Y-axis is inaudible in Legacy mode;
   each arrow click lists the library twice; a failed import copy is silent.

## Decisions
- Sub Guard gets one LR48 crossover at every setting, split around the drive (half before, half
  after), so drive products below the crossover are removed and the clean sum stays flat. The
  150 Hz gentle slope goes; if wanted back it is its own control, never automatic zones, which
  cause the transition bugs. Snap points stay (owner, 2026-10-19).
- Sub Guard on/off is a smoothed weight `g`, not a glide, with the splitter running warm even
  while off: resetting it on enable would bring back F4's thump (LR48 rings longer than the ramp).
- `legacyFilter` is a session parameter; state version 3 turns it on for older states exactly
  when `LegacyInputFilter::isActive`, so v2.3 sessions sound the same.
- Profiles live in a library Sledge owns; sessions fall back by content fingerprint, never name.
- Merge PRs manually once all three builds pass (`build-macos` is not a required check).

## Known issues
- `verify.sh` runs tests only; on Linux it needs `build.yml`'s apt packages (a fresh cloud
  container lacks them: install first, about 2 minutes).
- `AGENTS.md` cites `docs/Architecture Contract.md` and `Distortion.jucer`; neither is in the repo.
- Two tests share the name "Sub Guard Crossover Flatness" (`SubGuardCrossoverFlatnessTest`,
  `SubGuardFlatnessTest`), so a name filter can't pick one.
- `DistortionRender` has no clip-type or automation flags; Sub Guard transitions need tests.
- Tests never construct the editor: check UI with `DistortionUiSnapshot`, not the standalone.
- Release builds keep bug reports on: set `bugReports="0"` in settings.xml before a local pluginval.
- A DAW scan that calls `prepareToPlay` may load the last profile per instance (unverified).
