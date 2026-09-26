# State

## Now
- Branch `feat/nam-profile-prototype` (pushed, no PR): NAM profile prototype. A loaded `.nam`
  replaces the built-in clip type. Three commits on `main` (`f516f93`): `f68e237` NAM Core + Eigen
  build, `3554d0c` the feature, `04d8c41` `--profile` for DistortionRender / DistortionUiSnapshot.
- PR #54 (`chore/housekeeping-debug`), open and unmerged: GitVersion.h generated in the build tree,
  repo hygiene, stale `.jucer` + `background.png` removed, compiler warnings cleared. Its head
  commit carries `[skip ci]`, so CI has never run on it.
- Owner is testing the prototype on their own machine.

## Next
1. Test in a DAW at 48 kHz: build `Distortion_VST3` from this branch, or run the "Build VST3
   Plugin" workflow manually on it for Windows/macOS artifacts. PROFILE -> Load profile... with a
   pedal `.nam`; sweep Distortion Amount with Sub Guard on; save and reopen the session.
2. Confirm the Windows (MSVC) and macOS (AppleClang, universal) builds. The C++20 `nam_core`
   object library has only been built on Linux/GCC 13.
3. Fix issue #44 on its own branch (lead under Known issues).
4. Decide the prototype's gaps: resampling for non-48 kHz hosts, crossfade on profile swap,
   fixed latency across profile on/off, EXTREME with profiles, profile in Sledge's own presets.
5. Longer plan (curated hardware profiles, snapshot blending, fixed-slot `profile` parameter;
   uploads, TONE3000 and cross-modulation as future ideas) lives in the design doc:
   https://claude.ai/code/artifact/e68e2d2d-28d9-48fe-ace0-ee47ca6b7bcb

## Decisions
- Chain runs at 1x while a profile is loaded: a model only sounds right at its training rate.
  No resampling yet; other host rates get an amber button (owner chose this for the prototype).
- Sessions remember the profile by file path, not by embedding it (owner's choice for the prototype).
- NAM Core pinned at `0b3d3c9`, built as an OBJECT library (a static archive drops the
  architectures' self-registration) at C++20; the plugin stays C++17. Eigen pinned by SHA-256.
- NAM fast tanh on: standard WaveNet 28% -> 9% of a core per channel (measured).
- Distortion Amount maps to model input gain: 0% -24 dB, 50% -6 dB, 100% +12 dB.

## Known issues
- No `scripts/verify.sh` here. Verification = build + `DistortionTests` (3170 assertions pass on
  `04d8c41`), `DistortionSoak`, and pluginval at strictness 10.
- #44 pluginval segfault (Automation, 96 kHz / 64) is intermittent and not seed-deterministic:
  seed `0x55ecec2` crashed once, then passed. Lead: JUCE's VST3 wrapper calls `prepareToPlay`
  without the callback lock, while the processor's 50 ms timer can run `rebuildOversampling`
  under it, so two threads can rebuild the oversampler at once.
- On `main`, a build rewrites the tracked `Source/GitVersion.h` (fixed in PR #54). Revert it with
  `git checkout Source/GitVersion.h` before committing.
- In cloud sessions GitHub archive downloads are blocked; git clone works (why NAM Core uses git).
- `.agents/` is committed on this branch while other AI files (CLAUDE.md, AGENTS.md) are
  gitignored; decide before merging whether it stays.
