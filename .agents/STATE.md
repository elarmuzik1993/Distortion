# State

## Now
- `main` is v2.4.0, untagged (last release v2.3.0): NAM profiles with a library browser,
  drag-and-drop import, capture calibration (`input_level_dbu` vs 12 dBu) and EXTREME's +9 dB
  push (was +12); Shape with the Shape / Legacy switch; four-pill clip column; XY Morph pad
  feedback; Sub Guard as one LR48 split around the drive; the graphic EQ: 31 drawn dots driving
  61 fitted peaks, dot 0 / dot 30 a 12 dB/oct high-pass / low-pass. `CHANGELOG.md` has v2.4.
- Leaving true bypass starts the skipped filters cold (`clearFiltersSkippedByBypass`), and
  `releaseResources` takes the callback lock (#44, a Linux timer-thread race).
- Golden audio: 16 seeded cases, none with a profile, all with the EQ flat; `extreme`
  regenerated for the +9 dB push (only that case changed).
- Verified 2026-11-01 (Windows, local, Debug) on the +9 dB change: `verify.sh` passes, 4526
  assertions. The EQ change: CI tests and pluginval strictness 10 green on Windows and Linux.

## Next
1. Owner, listening: EXTREME at +9 dB on a clip type and a NAM capture; the `EXTREME` preset is
   now 3 dB less driven. Then the EQ end dots (HP/LP sweep, whole curve at -24 dB = band-pass).
2. Owner, one host pass on a Release build: draw on the EQ with the drive at zero (notch,
   low/high cut, air lift) and open a v2.3 session with a drawn curve; a real `.nam`
   drag-and-drop; XY pad feel; profile browser; clip column font; Legacy Filter on a v2.3 session.
3. Owner, listening: NAM calibrated files (hot captures drive less); decide if 12 dBu should be
   a setting; then drop "prototype" or label NAM beta.
4. Owner, listening: Sub Guard (808, bass; on vs bypassed; preset changes while playing).
   Re-voice Hard Clip, 808 Safe, Vocal Warmth, EXTREME, Bass Driver, Lo-Fi Crush, Diode Bite at
   100-150 Hz; rename PRESERVE/CONTROL/AGGRO; measure Release CPU (~12% more in Debug).
5. Release: after 1-4, tag `v2.4.0` on `main` (CI builds and attaches every asset). Owner: ASIO
   standalone pass first. `RELEASE_CHECKLIST.md` is gitignored and only on the Windows machine.
6. Sub Guard follow-ups: warm switch-on only proven at distMix 0; NAM-mode `g` leads by D.
7. Marketing: three demo clips (808 on a phone speaker, bass DI through a NAM capture with Sub
   Guard on/off, the drawn EQ); a crossover-vs-blend page. Check NAM 9000 / Prisma claims first.
8. Small: Legacy switch may click; XY Y-axis hints show in Legacy though the axis is silent.

## Decisions
- Graphic EQ fits peak gains to the drawing (owner, 2026-10-25); end dots are HP/LP filters.
  New param IDs: v2.3 curves migrate (state v4, end filters off), old automation does not.
- EXTREME pushes clip types and profiles alike, +9 dB (owner, 2026-11-01; +12 was too hot);
  captures follow `input_level_dbu` against 12 dBu (owner, 2026-10-22).
- Profiles live in Sledge's library; sessions fall back by content fingerprint, never by name.
- `legacyFilter` is a session param; state v3 turns it on for v2.3 states that used the filter.
- Clip column can't widen (texture cutouts registered to the knobs).
- True bypass clears every filter it skips, or none: clearing only some made the rest louder.
- Merge PRs once all three builds pass. Manual workflow dispatch is refused (403): to re-run CI,
  merge `main` into the branch and push.
- Positioning: free, open-source bass distortion; pay-what-you-want; don't claim "first".

## Known issues
- Crossing the true-bypass threshold (drive 0.5%) jumps: active-path filters shift the phase,
  bypass does not; a 45 Hz + 1 kHz tone steps by up to ~1.7x its peak.
- `~PluginProcessor`'s `stopTimer()` may not wait for a running Linux timer callback (unverified).
- EQ refit is ~97 µs on the audio thread per block while a dot moves; heavy at 32-64 samples.
- Graphic EQ: steep jumps (36 dB) still ring up to ~8.6 dB of excess wiggle (was 24.5 dB).
- `juce_String.cpp:315` assertion in a Clean Boost test on Linux CI; pre-existing, harmless.
- `verify.sh` builds with unbounded `-j` (use `-j2` on a 4-core box) and never builds the
  editor: check UI with `DistortionUiSnapshot`. On Linux it needs `build.yml`'s apt packages.
- Local pluginval: `HOME` at a scratch folder with `settings.xml` `bugReports="0"`.
- Host bypass freezes the island, delay lines and Sub Guard splitter; IIR ring after a gap.
- Importing many large captures at once briefly blocks the UI (vetting runs on that thread).
