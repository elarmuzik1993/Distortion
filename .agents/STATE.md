# State

## Now
- Unreleased work on `main` (plugin still reports 2.3.0): NAM profiles with a library browser,
  drag-and-drop import, capture calibration (`input_level_dbu` vs a 12 dBu reference) and
  EXTREME's +12 dB push; Shape with the Shape / Legacy switch; four-pill clip column; XY Morph
  pad puck and readout; Sub Guard as one LR48 split around the drive (plan steps 1-7).
- Golden audio: 16 seeded cases, none with a profile (AGENTS.md, Build & Test).
- Verified 2026-10-23 on Linux (cloud, Debug) at `3023286`: `verify.sh` 4315 assertions.
  CI green on Windows, Linux and macOS for that tree. pluginval not run locally.

## Next
1. Owner, listening: NAM captures with EXTREME and with calibrated files (hot captures now drive
   less); decide if 12 dBu should be a setting; then drop "prototype" or label NAM beta.
2. Owner, listening: Sub Guard (808, bass; on vs bypassed; preset changes while playing).
   Re-voice Hard Clip, 808 Safe, Vocal Warmth, EXTREME, Bass Driver, Lo-Fi Crush, Diode Bite at
   100-150 Hz; rename PRESERVE/CONTROL/AGGRO; measure Release CPU (~12% more in Debug).
3. Owner, in a host (Release build): a real `.nam` drag-and-drop, XY pad feel, profile browser,
   clip column font, Legacy Filter on a v2.3 session.
4. Sub Guard follow-ups: reset the splitter, `toneFilterLow` and `g` when leaving true bypass;
   warm switch-on only proven at distMix 0; NAM-mode `g` leads by D during the ramp.
5. Release: fix #44 (pluginval Automation segfault, Linux 96 kHz / 64 samples) and
   `prepareToPlay` shrinking band buffers below `rebuildOversampling`'s size; pluginval
   strictness 10; bump CMake `VERSION`; tag. Owner: ASIO standalone pass.
6. Marketing prep: three demo clips (808 on a phone speaker, bass DI through a NAM capture with
   Sub Guard on/off, the drawn EQ); a crossover-vs-blend page. Verify NAM 9000 "Split Drive"
   and UnplugRed Prisma's Linux support first.
7. Small: Legacy switch may click; XY Y-axis hints show in Legacy though the axis is silent.
   Owner to confirm deleting merged and stale branches.

## Decisions
- EXTREME pushes a profile's input x4 like a clip type's drive; captures follow their own
  `input_level_dbu` against 12 dBu, the NAM plugin's default (owner, 2026-10-22).
- Profiles live in Sledge's library; sessions fall back by content fingerprint, never by name.
- `legacyFilter` is a session param; state v3 turns it on for v2.3 states that used the filter.
- Clip column can't widen (texture cutouts registered to the knobs).
- Merge PRs once all three builds pass. Manual workflow dispatch is refused (403): to re-run CI
  merge `main` into the branch and push.
- Sub Guard sound change accepted without a version gate (owner, 2026-10-19).
- Positioning: free, open-source distortion for modern bass ("crush the top, keep the sub, load
  any NAM capture"); pay-what-you-want; don't claim "first".

## Known issues
- `verify.sh` builds tests only, never the editor: check UI with `DistortionUiSnapshot`. On Linux
  it needs `build.yml`'s apt packages; build with `-j2` or one target at a time (memory).
- Set `bugReports="0"` in settings.xml before a local pluginval of a Release build.
- `pkill -f` on the standalone's path kills the calling shell; kill by PID.
- Host bypass freezes the island, delay lines and Sub Guard splitter; IIR ring after a gap.
- Importing many large captures at once briefly blocks the UI (vetting runs on that thread).
