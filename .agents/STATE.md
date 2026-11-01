# State

## Now
- Unreleased work on `main` (plugin still reports 2.3.0): NAM profiles with a library browser,
  drag-and-drop import, capture calibration (`input_level_dbu` vs a 12 dBu reference) and
  EXTREME's +12 dB push; Shape with the Shape / Legacy switch; four-pill clip column; XY Morph
  pad puck and readout; Sub Guard as one LR48 split around the drive (plan steps 1-7).
- Branch `feat/graphic-eq-31-band` (not merged): graphic EQ from 12 to 31 bands (ISO 1/3 octave,
  20 Hz-20 kHz), drawn -24..+12 dB. Params `eqThird0..30` hold the drawing; the processor fits band
  gains at the session rate on the audio thread (`GraphicEqCurve::Fitter`); double-precision bank,
  32-sample coefficient updates, per-band idle hold; also plays in true bypass (was skipped there).
  State v4 draws v2.3 `eqBand0..11` curves. `DistortionUiSnapshot --eq` renders it.
- Golden audio: 16 seeded cases, none with a profile (AGENTS.md, Build & Test).
- `main` verified 2026-10-23 (Linux, Debug) at `3023286`: 4315 assertions, CI green on all three.
- `feat/graphic-eq-31-band` verified 2026-10-25 (Linux, Debug): `verify.sh` passes; CI not seen.

## Next
0. Owner, in a host: draw on the 31-band EQ (notch, low/high cut, air) and say whether it feels
   right; open a v2.3 session with a drawn curve. Then a PR for `feat/graphic-eq-31-band`.
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
- Graphic EQ: 31 bands at 1/3 octave, cuts to -24 dB, gains fitted to the drawing (owner chose
  this over a free-draw FIR, 2026-10-25). New param IDs, so v2.3 automation on the old 12 bands
  no longer applies; the curve itself migrates.
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
- Local pluginval of a Release build: set `bugReports="0"` first. Kill the standalone by PID.
- Host bypass freezes the island, delay lines and Sub Guard splitter; IIR ring after a gap.
- Importing many large captures at once briefly blocks the UI (vetting runs on that thread).
- Graphic EQ: a -24 dB plateau ripples ~3 dB between centres (the price of a notch that reaches
  -24); an old v2.3 peak midway between two new centres comes back up to ~1.5 dB lower.
