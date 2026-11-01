# State

## Now
- `main` is v2.4.0, untagged (last release v2.3.0): NAM profiles with a library browser,
  drag-and-drop import, capture calibration (`input_level_dbu` vs 12 dBu) and EXTREME's +12 dB
  push; Shape with the Shape / Legacy switch; four-pill clip column; XY Morph pad feedback; Sub
  Guard as one LR48 split around the drive; the 31-band graphic EQ. `CHANGELOG.md` has the v2.4
  section; ten entries once listed as unreleased shipped in v2.3.0 and sit under v2.3.
- Leaving true bypass now starts the skipped filters cold (`clearFiltersSkippedByBypass`), and
  `releaseResources` takes the callback lock (#44, a Linux timer-thread race).
- Golden audio: 16 seeded cases, none with a profile, all with the EQ flat.
- Verified 2026-11-01 (Linux, local, Debug) at `d278e08`: `verify.sh` passes, 4438 assertions.
  pluginval strictness 10 on the Release VST3 with CI's options: 3,000 runs under gdb, no
  crash (before the lock fix about 1 in 600). CI for that commit not seen at hand-off.

## Next
1. Owner, one host pass on a Release build: draw on the EQ with the drive at zero (notch,
   low/high cut, air lift) and open a v2.3 session with a drawn curve; a real `.nam`
   drag-and-drop; XY pad feel; profile browser; clip column font; Legacy Filter on a v2.3 session.
2. Owner, listening: NAM captures with EXTREME and with calibrated files (hot captures now drive
   less); decide if 12 dBu should be a setting; then drop "prototype" or label NAM beta.
3. Owner, listening: Sub Guard (808, bass; on vs bypassed; preset changes while playing).
   Re-voice Hard Clip, 808 Safe, Vocal Warmth, EXTREME, Bass Driver, Lo-Fi Crush, Diode Bite at
   100-150 Hz; rename PRESERVE/CONTROL/AGGRO; measure Release CPU (~12% more in Debug).
4. Release: after 1-3, tag `v2.4.0` on `main` (CI builds and attaches every asset). Owner: ASIO
   standalone pass first. `RELEASE_CHECKLIST.md` is gitignored and only on the Windows machine.
5. Sub Guard follow-ups: warm switch-on only proven at distMix 0; NAM-mode `g` leads by D during
   the ramp.
6. Marketing: three demo clips (808 on a phone speaker, bass DI through a NAM capture with Sub
   Guard on/off, the drawn EQ); a crossover-vs-blend page. Check NAM 9000 / Prisma claims first.
7. Small: Legacy switch may click; XY Y-axis hints show in Legacy though the axis is silent.

## Decisions
- Graphic EQ fits band gains to the drawing (owner, 2026-10-25). New param IDs: v2.3 curves
  migrate (state v4), v2.3 automation of the old 12 bands does not.
- EXTREME pushes a profile's input x4; captures follow their `input_level_dbu` against 12 dBu
  (owner, 2026-10-22). Sub Guard's sound change has no version gate.
- Profiles live in Sledge's library; sessions fall back by content fingerprint, never by name.
- `legacyFilter` is a session param; state v3 turns it on for v2.3 states that used the filter.
- Clip column can't widen (texture cutouts registered to the knobs).
- True bypass clears every filter it skips, or none: clearing only some made the rest louder.
- Merge PRs once all three builds pass. Manual workflow dispatch is refused (403): to re-run CI,
  merge `main` into the branch and push.
- Positioning: free, open-source bass distortion; pay-what-you-want; don't claim "first".

## Known issues
- Crossing the true-bypass threshold (drive 0.5%) jumps: the active path's filters shift the
  phase and bypass does not. A 45 Hz + 1 kHz tone toggled across it steps by up to ~1.7x its peak.
- `~PluginProcessor`'s `stopTimer()` may not wait for a callback already running on the Linux
  message thread (same family as #44; unverified, not fixed).
- `prepareToPlay` re-prepares the Sub Guard band buffers smaller than `rebuildOversampling` did.
  Harmless (the splitter refuses a block that doesn't fit) and not #44's cause.
- `verify.sh` builds with unbounded `-j`; on a 4-core / 15 GB machine build `DistortionTests`
  with `-j2` first. It never builds the editor: check UI with `DistortionUiSnapshot`. On Linux
  it needs `build.yml`'s apt packages.
- Local pluginval: run it with `HOME` at a scratch folder holding `settings.xml` with
  `bugReports="0"`, so it sends nothing and recalls no profile. Kill the standalone by PID.
- Host bypass freezes the island, delay lines and Sub Guard splitter; IIR ring after a gap.
- Importing many large captures at once briefly blocks the UI (vetting runs on that thread).
- Graphic EQ: a -24 dB plateau ripples ~3 dB between centres.
