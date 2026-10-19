# State

## Now
- `main` has NAM profiles with a profile browser (a `Profiles` library, fingerprint fallback,
  last profile recalled), Shape with the Shape / Legacy switch, and the four-pill clip column with
  the profile arrows inside PROFILE's ends. Unreleased; the plugin reports 2.3.0 (last release).
- Sub Guard plan (`docs/SUB_GUARD_TRANSITION_REVIEW.md`) steps 1-7 done: the sub leaves within
  0.3 dB of its input, drive products below the crossover under -110 dBFS, one LR48 split around
  the drive (double), on/off click-free (15 ms weight `g`, splitter warm). Sub Guard off renders
  byte-identical to before. `SubGuardBehaviourTests` asserts it; nothing is pending.
- Drive noise is seedable (`seedNoiseForTesting`; `DistortionRender --seed`, default 1).
- Verified 2026-10-19 on Linux (cloud, Debug) at `2a3e511`: `verify.sh` passes, 4152
  assertions; soak 10 x 20 s passes (unverified this session). pluginval not run.

## Next
1. Golden references (owner asked, 2026-10-19). `GoldenAudioTests` compares nothing today. Make
   it: seed the noise; ~15 cases with explicit params, never factory presets (7 clip types on a
   fixed 808 + mids input; Sub Guard off/60/150; comp on; bypass), 0.5 s stereo float 44.1 kHz
   WAVs in `Resources/TestAudio/`; generate with an env var (`DISTORTION_GOLDEN_GENERATE=1`)
   instead of `GENERATE_MODE`; compare with a tolerance (about -90 dBFS max error, not bytes:
   MSVC, GCC and arm64 round differently); pass the repo path in from CMake instead of
   climbing from the executable. Add an AGENTS.md rule: a PR that changes the sound
   regenerates the references and says why. Branch `test/golden-references`.
2. Owner: one listening pass on the Sub Guard work (real 808 and bass line, Sub Guard on vs
   bypassed, preset changes while playing, Sub Guard off and back); re-voice the seven presets
   at 100-150 Hz (Hard Clip, 808 Safe, Vocal Warmth, EXTREME, Bass Driver, Lo-Fi Crush, Diode
   Bite); the "150Hz AGGRO" label no longer fits. Sub Guard costs ~12% more CPU (Debug); measure
   Release.
3. Plan step 8, only if measured: per-sub-block coefficients (F6); the 20 Hz input high-pass
   (C5, owner decides); a C3 hint on the knob ("set it above your 808's note").
4. Owner, Release build: clip column (keep the small clip-type font?), Legacy Filter (v2.3 Low or
   Band Pass session opens in Legacy, same sound), profile browser, Shape by ear.
5. #44 (pluginval Automation segfault, Linux 96 kHz / 64 samples); `prepareToPlay` shrinks band
   buffers below `rebuildOversampling`'s size. Then pluginval (strictness 10), bump CMake
   `VERSION 2.3.0`, tag. Owner: ASIO standalone pass.
6. Follow-ups: Legacy switch may click; XY Y-axis silent in Legacy; arrow click lists the library
   twice; failed import is silent. Delete merged Sub Guard branches (owner to confirm); decide
   `claude/ui-screenshot-readme-6vbm01`, `docs/profile-morph*`, `fix/profile-browser-review`,
   `fix/transition-clicks`, `test/crossing-probe`.

## Decisions
- `legacyFilter` is a session param; state v3 turns it on for v2.3 states that used the filter.
- Profiles live in Sledge's library; sessions fall back by content fingerprint, never by name.
- Clip column can't widen (texture cutouts); stack sits 3 px below knob centre (owner's eye).
- Merge PRs once all three builds pass. A base change or a manual workflow run doesn't start CI
  (dispatch is refused, 403): merge `main` into the branch and push.
- Sub Guard: one LR48 split around the drive, no zones; on/off as weight `g`, splitter warm;
  sound change accepted, no version gate; one listening pass for steps 4-7 (owner, 2026-10-19).
- Golden cases set params directly, so re-voicing presets never invalidates them.

## Known issues
- `verify.sh` runs tests only; on Linux it needs `build.yml`'s apt packages. Two builds at once
  can run the container out of memory (`cc1plus` killed): build one at a time or with `-j2`.
- Tests never build the editor: check UI with `DistortionUiSnapshot`.
- Set `bugReports="0"` in settings.xml before a local pluginval of a Release build.
- `pkill -f` on the standalone's path kills the calling shell; kill by PID.
- Host bypass freezes the island, delay lines and Sub Guard splitter; IIR ring after a bypass gap.
