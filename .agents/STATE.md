# State

## Now
- `main` has NAM profiles with a profile browser (a `Profiles` library, fingerprint fallback,
  last profile recalled), Shape with the Shape / Legacy switch, and the four-pill clip column with
  the profile arrows inside PROFILE's ends. Unreleased; the plugin reports 2.3.0 (last release).
- Sub Guard plan (`docs/SUB_GUARD_TRANSITION_REVIEW.md`, section 5; hand-off section 8): steps
  1-3 are in. Pending tests (`SubGuardPendingTests`, assert only with
  `DISTORTION_PENDING_TESTS=1`), state-load reset of all three banks (always-on
  `SubGuardStateLoadTest`), crossover extracted to `SubGuardSplitter` (renders byte-identical).
- Verified 2026-10-19 on Linux (cloud, Debug) at `f318d68`: `verify.sh` passes, 4072 assertions;
  all three CI builds green on the step 2 and step 3 heads. pluginval not run recently.

## Next
1. Merge the seeding hook PR (`test/seed-drive-noise`): `seedNoiseForTesting`,
   `DistortionRender --seed` (default 1, renders repeat byte for byte), seeded test rigs.
2. Sub Guard step 4 on `fix/subguard-tamer-high-band` (stacked on the seeding PR): tamer on
   the high band; sub level now within 0.3 dB (was -3 to -5 dB), Sub Guard off byte-identical.
   The product below the crossover rose 4-11 dB (step 6 removes it). Owner listens, then merge.
   Step 5 on `fix/subguard-boost-high-band` (stacked on step 4): Clean Boost on the high band;
   sub with Boost +3.5..+5.7 dB -> within 0.4 dB. Owner asked to go on without listening
   (2026-10-19); listen before release instead. Then steps 6, 7.
3. Owner, Release build: clip column (PROFILE ends step, middle opens menu; keep the small
   clip-type font?), Settings > Legacy Filter (v2.3 Low/Band Pass session opens in Legacy, same
   sound), profile browser (multi-load, wrap, card, bubble, moved file, Recall Profile), Shape.
4. #44 (pluginval Automation segfault, Linux 96 kHz / 64 samples); `prepareToPlay` shrinks band
   buffers below `rebuildOversampling`'s size.
5. pluginval (strictness 10), bump CMake `VERSION 2.3.0`, tag. Owner: ASIO standalone pass.
6. Follow-ups: Legacy switch may click; XY Y-axis silent in Legacy; arrow click lists the library
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

## Known issues
- `verify.sh` runs tests only; on Linux it needs `build.yml`'s apt packages.
- Tests never build the editor: check UI with `DistortionUiSnapshot`.
- Set `bugReports="0"` in settings.xml before a local pluginval of a Release build.
- `pkill -f` on the standalone's path kills the calling shell; kill by PID.
- Host bypass freezes the island and delay lines; IIR ring after a bypass gap.
- Sub Guard coefficients: float in `prepareToPlay`, double at runtime; differ at 60 Hz, 4x.
- Pending F1 check at `distMix` 100 passes narrowly (1.95x vs 2.0x).
