# State

## Now
- `main` has NAM profiles with a profile browser (a `Profiles` library, fingerprint fallback,
  last profile recalled), Shape with the Shape / Legacy switch, and the four-pill clip column with
  the profile arrows inside PROFILE's ends. Unreleased; the plugin reports 2.3.0 (last release).
- Sub Guard plan (`docs/SUB_GUARD_TRANSITION_REVIEW.md`, section 5): steps 1-3 on `main`;
  the seeding hook and steps 4-7 are a stack of open PRs, each based on the one before:
  #76 `test/seed-drive-noise` (main) → #77 `fix/subguard-tamer-high-band` → #78
  `fix/subguard-boost-high-band` → #79 `fix/subguard-lr48-split` → #80
  `fix/subguard-onoff-weight`. With all five: the sub leaves within 0.3 dB of its input, drive
  products below the crossover under -110 dBFS, one LR48 split around the drive (double), and
  on/off/toggling click-free (15 ms weight `g`, splitter warm). Sub Guard off renders
  byte-identical throughout. Nothing is pending: `SubGuardBehaviourTests` asserts it all.
- Verified 2026-10-19 on Linux (cloud, Debug) at the top of the stack: `verify.sh` passes,
  4152 assertions; soak 10 x 20 s passes. CI on the stack not yet checked. pluginval not run.

## Next
1. Merge the stack in order once CI is green on each (#76, then retarget #77 to `main`, and so
   on). Owner chose to skip the per-step listening (2026-10-19): do one listening pass on the
   whole stack before release (section 5's pass: real 808 and bass line, Sub Guard on vs
   bypassed, preset changes while playing, Sub Guard off and back).
2. Owner, by ear: re-voice the seven presets at 100-150 Hz (Hard Clip, 808 Safe, Vocal Warmth,
   EXTREME, Bass Driver, Lo-Fi Crush, Diode Bite); the knob's "150Hz AGGRO" label no longer
   describes a gentler slope. Sub Guard now costs ~12% more CPU (Debug soak); measure Release.
3. Plan step 8, only if measured: per-sub-block coefficient updates (F6); the 20 Hz input
   high-pass (C5, owner decides). C3 hint on the knob ("set it above your 808's note").
4. Owner, Release build: clip column (PROFILE ends step, middle opens menu; keep the small
   clip-type font?), Settings > Legacy Filter (v2.3 Low/Band Pass session opens in Legacy, same
   sound), profile browser (multi-load, wrap, card, bubble, moved file, Recall Profile), Shape.
5. #44 (pluginval Automation segfault, Linux 96 kHz / 64 samples); `prepareToPlay` shrinks band
   buffers below `rebuildOversampling`'s size.
6. pluginval (strictness 10), bump CMake `VERSION 2.3.0`, tag. Owner: ASIO standalone pass.
7. Follow-ups: Legacy switch may click; XY Y-axis silent in Legacy; arrow click lists the library
   twice; failed import is silent. Delete merged Sub Guard step branches (owner to confirm);
   decide `claude/ui-screenshot-readme-6vbm01`, `docs/profile-morph*`,
   `fix/profile-browser-review`, `fix/transition-clicks`, `test/crossing-probe`.

## Decisions
- `legacyFilter` is a session param; state v3 turns it on for v2.3 states that used the filter.
- Profiles live in Sledge's library; sessions fall back by content fingerprint, never by name.
- Clip column can't widen (texture cutouts); stack sits 3 px below knob centre (owner's eye).
- Merge PRs manually once all three builds pass; base changes don't trigger CI, merge `main` in.
- Sub Guard: one LR48 crossover split around the drive, no zones; on/off as a smoothed weight
  `g` with the splitter warm; sound change accepted with no version gate (owner, 2026-10-19).
- Sub Guard steps 4-7 went in without a listening pass per step (owner, 2026-10-19).
- `DistortionRender` seeds the drive noise (`--seed`, default 1) and prepares again after its
  flags, so renders repeat and compare byte for byte across builds.

## Known issues
- `verify.sh` runs tests only; on Linux it needs `build.yml`'s apt packages.
- Tests never build the editor: check UI with `DistortionUiSnapshot`.
- Set `bugReports="0"` in settings.xml before a local pluginval of a Release build.
- `pkill -f` on the standalone's path kills the calling shell; kill by PID.
- Host bypass freezes the island and delay lines (and the Sub Guard splitter); IIR ring after
  a bypass gap.
- Golden Audio compares nothing: `GoldenAudioTests` checks finite, non-silent output and the
  repo holds no reference files.
