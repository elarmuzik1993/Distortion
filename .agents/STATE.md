# State

## Now
- `main` has NAM profiles with a profile browser, Shape with the Shape / Legacy switch, and the
  four-pill clip column. Unreleased; the plugin reports 2.3.0 (last release).
- Sub Guard plan (`docs/SUB_GUARD_TRANSITION_REVIEW.md`) steps 1-7 done: the sub leaves within
  0.3 dB of its input, drive products below the crossover under -110 dBFS, one LR48 split around
  the drive (double), click-free on/off (15 ms weight `g`, splitter warm), tested by
  `SubGuardBehaviourTests`. Drive noise is seedable (`seedNoiseForTesting`, `DistortionRender
  --seed`).
- Golden audio: 16 seeded cases with every parameter pinned, compared with
  `Resources/TestAudio/golden_*.wav` (see AGENTS.md, Build & Test). CI green on all three OSes.
- `fix/nam-loader`, PR #83: offline prepare waits for a profile load; imports vetted
  (`NamProfile::check`), problems shown in a dialog; arrow step reads the library once.
  Verified 2026-10-20 (Linux cloud, Debug): `verify.sh` 4292 assertions; snapshot renders.

## Next
1. Owner: listening pass on Sub Guard (real 808 and bass line; on vs bypassed; preset changes
   while playing; off and back). Re-voice the seven presets at 100-150 Hz (Hard Clip, 808 Safe,
   Vocal Warmth, EXTREME, Bass Driver, Lo-Fi Crush, Diode Bite). Decide the knob labels
   (PRESERVE/CONTROL/AGGRO describe slopes that are gone). Measure Release CPU (~12% more, Debug).
2. Sub Guard follow-ups: leaving true bypass resumes the splitter stale (reset it, `toneFilterLow`
   and `g` there, as `shapePost` is); warm switch-on only proven at distMix 0; NAM-mode `g` leads
   by D during the ramp. Plan step 8 only if measured (F6, C5, a C3 hint on the knob).
3. Owner, NAM before it leaves prototype: EXTREME with a profile only skips the tamer (push
   ~+12 dB or grey out?); `input_level_dbu` calibration ignored. Both change the sound. Also
   drag-and-drop; offline state restored after prepare still waits for the timer.
4. Release prep for the marketing direction below: NAM out of prototype (or labelled beta); three
   demo clips (808 on a phone speaker, bass DI through a NAM capture with Sub Guard on/off, the
   drawn EQ); a short comparison page (crossover vs blend). First verify rivals' claims: NAM 9000
   "Split Drive" (price, platforms) and UnplugRed Prisma's Linux support (unverified).
5. Owner, Release build: clip column font, Legacy Filter on a v2.3 session, profile browser.
6. #44 (pluginval Automation segfault, Linux 96 kHz / 64 samples); `prepareToPlay` shrinks band
   buffers below `rebuildOversampling`'s size. Then pluginval (strictness 10), bump CMake
   `VERSION 2.3.0`, tag. Owner: ASIO standalone pass.
7. Follow-ups: Legacy switch may click; XY Y-axis silent in Legacy. Delete merged Sub Guard
   branches (owner to confirm); decide `claude/ui-screenshot-readme-6vbm01`,
   `docs/profile-morph*`, `fix/profile-browser-review`, `fix/transition-clicks`,
   `test/crossing-probe`.

## Decisions
- `legacyFilter` is a session param; state v3 turns it on for v2.3 states that used the filter.
- Profiles live in Sledge's library; sessions fall back by content fingerprint, never by name.
- Clip column can't widen (texture cutouts); stack sits 3 px below knob centre (owner's eye).
- Merge PRs once all three builds pass. A base change or manual workflow run doesn't start CI
  (dispatch is refused, 403): merge `main` into the branch and push.
- Sub Guard: one LR48 split around the drive; on/off as weight `g`, splitter warm; sound change
  accepted, no version gate (owner, 2026-10-19). Golden cases never use presets.
- Positioning (2026-10-20, from a web check): "free, open-source distortion for modern bass:
  crush the top, keep the sub, load any NAM capture". Stay pay-what-you-want; a price would
  pit it against Neural DSP Parallax X (~€99) and Saturn 2. Audiences: NAM/TONE3000, Linux
  audio, budget 808 producers, bass players. Don't claim "first": NAM 9000 and Prisma overlap.

## Known issues
- `verify.sh` runs tests only; on Linux it needs `build.yml`'s apt packages. Two builds at once
  can run the container out of memory: build one at a time or with `-j2`.
- Tests never build the editor: check UI with `DistortionUiSnapshot`.
- Set `bugReports="0"` in settings.xml before a local pluginval of a Release build.
- `pkill -f` on the standalone's path kills the calling shell; kill by PID.
- Host bypass freezes the island, delay lines and Sub Guard splitter; IIR ring after a bypass gap.
