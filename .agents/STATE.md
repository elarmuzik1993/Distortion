# State

## Now
- `feat/nam-profile-prototype` (no PR), 16 commits ahead of origin, unpushed: the NAM resampling
  plan (`docs/superpowers/plans/2026-09-27-nam-profile-resampling.md`, local) is implemented,
  `70932e2..bf22ca1`. Profiles run at their trained rate in a `ResamplingIsland`; one fixed
  latency R on every path (host bypass too); ducked rebuilds (none offline); crossfaded swaps.
- Verified 2026-09-27: `DistortionTests` 3518 assertions pass; Standalone + VST3 build and carry
  `SlimmableContainer`; deterministic renders (clipType 1, waveshaperMix 0) match `70932e2`
  exactly (48 kHz) and after a 22-sample shift (44.1 kHz). Owner listening and DAW checks: not done.
- `fix/filter-mode-switch-cutoff` (`ff0ad79`, unpushed, worktree `~/Distortion-filterfix`): a filter-mode
  switch moves a cutoff still at the old mode's default to the new one's. Not tried in the UI.
  PR #54 (`chore/housekeeping-debug`) open. Both unchanged, not verified this session.

## Next
1. Owner: ASIO standalone at 44.1 and 96 kHz: load, swap and clear profiles while playing (swap
   seamless, load/clear a short dip); Reaper latency display stays put; A/B
   `build-rel/nam-regression/sd1_{44100,48000,96000}.wav`.
2. Choose: push `feat/nam-profile-prototype` + PR, or keep iterating. Then dispatch
   "Build VST3 Plugin" to cover macOS and Linux (untested there).
3. Follow-up task: state frozen through true or host bypass (input filter, oversampler IIR,
   bypass line, dry delay lines, island) replays or rings on resume. Predates the branch, likely audible.
4. Follow-up: tag staged profiles with their request id so an off-thread clear can't be lost.
5. Remaining deferred minors: `.superpowers/sdd/2026-09-27-nam-profile-resampling/progress.md` (local ledger).
6. Owner: click through the filter modes in `~/Distortion-filterfix`; if happy, push it + PR.
7. Re-test issue #44 on this branch: `prepareToPlay` now holds the callback lock (its lead).
8. Owner decision: 41 commits on `origin/main` authored `Claude <noreply@anthropic.com>`.

## Decisions
- Reported latency is always R = max(user oversampler latency, island delay for a 48 kHz model),
  so hosts never see it change; +0.5 ms off 48 kHz, nothing at 48 kHz.
- Bypass branch delays by the path latency, then shares the output pad with the active path, so
  crossings replay no stale pad audio.
- Offline renders never duck: a rebuild switches at once (may click, never a silent gap).
- NAM Core pinned `0b3d3c9`, OBJECT library, objects linked into each format target.
- ASIO is opt-in (`-DDISTORTION_ASIO_SDK_DIR=...`) in a separate `build-asio` tree.
- Filter-mode rule fires only on a user gesture; presets, sessions, automation untouched.
- Sessions remember a profile by path. Distortion Amount -> model input gain (0% -24, 50% -6, 100% +12 dB).

## Known issues
- No `scripts/verify.sh`. Verify = MSVC via vcvars64, build Standalone + `DistortionTests`, run
  the tests, check the plugin binary contains "SlimmableContainer".
- `Source/GitVersion.h` has git's skip-worktree bit here; `git checkout -- Source/GitVersion.h` errors harmlessly.
- Brutal Fuzz (clip 0) and waveshaper hiss use time-seeded `juce::Random`, so default renders are
  not repeatable; regression baselines need clipType != 0 and waveshaperMix 0.
- Close `Sledge Distortion.exe` before rebuilding (LNK1104).
- #44 pluginval segfault (Automation, 96 kHz / 64), intermittent; see Next 7.
- Low Pass at a low cutoff mutes the distortion by design: the filter runs before the drive.
- `AGENTS.md` and `.agents/` handling: `AGENTS.md` is gitignored; decide before merging.
