# State

## Now
- Unreleased on `main` (plugin still reports 2.3.0; last release v2.3.0): NAM profiles with a
  library browser, drag-and-drop import, capture calibration (`input_level_dbu` vs 12 dBu) and
  EXTREME's +12 dB push; Shape with the Shape / Legacy switch; four-pill clip column; XY Morph
  pad puck and readout; Sub Guard as one LR48 split around the drive; the 31-band graphic EQ
  (ISO 1/3 octave, -24..+12 dB, gains fitted to the drawing, also plays in true bypass).
- Golden audio: 16 seeded cases, none with a profile, all with the EQ flat.
- Verified 2026-11-01 (Linux, local, Debug) at `a7c0c6b`: `verify.sh` passes, 4429 assertions;
  CI green on Windows, Linux and macOS for that commit. Since then only CI config and docs
  changed. pluginval not run locally.

## Next
1. Owner, one host pass on a Release build: draw on the EQ with the drive at zero (notch,
   low/high cut, air lift) and open a v2.3 session with a drawn curve; a real `.nam`
   drag-and-drop; XY pad feel; profile browser; clip column font; Legacy Filter on a v2.3 session.
2. Owner, listening: NAM captures with EXTREME and with calibrated files (hot captures now drive
   less); decide if 12 dBu should be a setting; then drop "prototype" or label NAM beta.
3. Owner, listening: Sub Guard (808, bass; on vs bypassed; preset changes while playing).
   Re-voice Hard Clip, 808 Safe, Vocal Warmth, EXTREME, Bass Driver, Lo-Fi Crush, Diode Bite at
   100-150 Hz; rename PRESERVE/CONTROL/AGGRO; measure Release CPU (~12% more in Debug).
4. Sub Guard follow-ups: reset the splitter, `toneFilterLow` and `g` when leaving true bypass;
   warm switch-on only proven at distMix 0; NAM-mode `g` leads by D during the ramp.
5. Release: fix #44 (pluginval Automation segfault, Linux 96 kHz / 64 samples) and
   `prepareToPlay` shrinking band buffers below `rebuildOversampling`'s size; pluginval
   strictness 10; bump CMake `VERSION`; tag. Owner: ASIO standalone pass.
6. Marketing: three demo clips (808 on a phone speaker, bass DI through a NAM capture with Sub
   Guard on/off, the drawn EQ); a crossover-vs-blend page. Check NAM 9000 / Prisma claims first.
7. Small: Legacy switch may click; XY Y-axis hints show in Legacy though the axis is silent.

## Decisions
- Graphic EQ fits band gains to the drawing (owner chose this over a free-draw FIR, 2026-10-25).
  New param IDs: v2.3 curves migrate (state v4), v2.3 automation of the old 12 bands does not.
- EXTREME pushes a profile's input x4; captures follow their `input_level_dbu` against 12 dBu,
  the NAM plugin's default (owner, 2026-10-22). Sub Guard's sound change has no version gate.
- Profiles live in Sledge's library; sessions fall back by content fingerprint, never by name.
- `legacyFilter` is a session param; state v3 turns it on for v2.3 states that used the filter.
- Clip column can't widen (texture cutouts registered to the knobs).
- Merge PRs once all three builds pass. Manual workflow dispatch is refused (403): to re-run CI,
  merge `main` into the branch and push.
- Positioning: free, open-source bass distortion ("crush the top, keep the sub, load any NAM
  capture"); pay-what-you-want; don't claim "first".

## Known issues
- `verify.sh` builds with unbounded `-j`; on a 4-core / 15 GB machine build `DistortionTests`
  with `-j2` first. It never builds the editor: check UI with `DistortionUiSnapshot`. On Linux
  it needs `build.yml`'s apt packages.
- Local pluginval of a Release build: set `bugReports="0"` first. Kill the standalone by PID.
- Host bypass freezes the island, delay lines and Sub Guard splitter; IIR ring after a gap.
- Importing many large captures at once briefly blocks the UI (vetting runs on that thread).
- Graphic EQ: a -24 dB plateau ripples ~3 dB between centres; an old v2.3 peak midway between
  two new centres comes back up to ~1.5 dB lower.
