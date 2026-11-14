# State

## Now
- `main` is v2.4.0, untagged (last release v2.3.0): NAM profiles with a library browser,
  drag-and-drop import, capture calibration (`input_level_dbu` vs 12 dBu) and EXTREME's +9 dB
  push; Shape with the Shape / Legacy switch; four-pill clip column; XY Morph pad feedback;
  Sub Guard as one LR48 split around the drive; the graphic EQ (31 drawn dots driving 61 fitted
  peaks, dot 0 / dot 30 a 12 dB/oct HP / LP). `CHANGELOG.md` has v2.4.
- Windows installer: silent installs skip the previous uninstaller (`[InstallDelete]` removes
  the old bundle); interactive upgrades still offer it and wait at most 5 s (Known issues).
- `README.md`, `CHANGELOG.md` and `AGENTS.md` match `main`; NAM is still "prototype" (Next 4).
- Verified 2026-11-14 (Windows): `bash scripts/verify.sh` passes, 4584 assertions.

## Next
0. Owner: CI's `build-macos` is cancelled before it starts (no runner) on every run since
   2026-11-09, `main` included; the CLIP / NAM switch is unbuilt on macOS. Check Actions billing
   and macOS runner access, re-run `main`, confirm the universal build and auval pass before tagging.
1. Owner, listening: flip CLIP / NAM while a bass plays (expect a short fade, no click) with a
   real, large capture; it comes back after its prewarm, so a big model may lag a moment.
2. Owner, listening: EXTREME at +9 dB on a clip type and a NAM capture; then the EQ end dots
   (HP/LP sweep, whole curve at -24 dB = band-pass).
3. Owner, one host pass on a Release build: draw on the EQ with the drive at zero, open a v2.3
   session with a drawn curve; a real `.nam` drag-and-drop; XY pad feel; profile browser and
   the CLIP / NAM switch; clip column font; Legacy Filter on a v2.3 session; the new installers,
   including one interactive upgrade over an older install (Known issues).
4. Owner, listening: NAM calibrated files; decide if 12 dBu should be a setting; then drop
   "prototype" or label NAM beta.
5. Owner, listening: Sub Guard (808, bass; on vs bypassed; preset changes while playing).
   Re-voice Hard Clip, 808 Safe, Vocal Warmth, EXTREME, Bass Driver, Lo-Fi Crush, Diode Bite at
   100-150 Hz; rename PRESERVE/CONTROL/AGGRO; measure Release CPU (~12% more in Debug).
6. Release: after 1-5, tag `v2.4.0` on `main` (CI builds and attaches every asset). Owner: ASIO
   standalone pass first. `RELEASE_CHECKLIST.md` is gitignored and only on the Windows machine.
7. Sub Guard follow-ups: warm switch-on only proven at distMix 0; NAM-mode `g` leads by D.

## Decisions
- CLIP / NAM switch (owner, 2026-11-09): under PROFILE, not automatable, presets never move it.
  Parked, not run, on CLIP and re-prepared on return so stale model state is never heard.
  Nothing loaded: NAM resumes the last profile, the menu only when there is none.
- Graphic EQ fits peak gains to the drawing (owner, 2026-10-25); end dots are HP/LP filters.
  New param IDs: v2.3 curves migrate (state v4, end filters off), old automation does not.
- EXTREME pushes clip types and profiles alike, +9 dB (owner, 2026-11-01; +12 was too hot);
  captures follow `input_level_dbu` against 12 dBu (owner, 2026-10-22).
- Profiles live in Sledge's library; sessions fall back by content fingerprint, never by name.
- Merge PRs once all three builds pass (the CLIP / NAM switch went in without macOS, Next 0).
  Manual workflow dispatch is refused (403): to re-run CI, merge `main` into the branch, push.
  Docs-only PRs were admin-merged while the `changes` check sat queued (owner, 2026-11-14).

## Known issues
- No test proves a re-enabled profile starts from rest: the test model forgets its state within
  the fade, so a check for it passed with the re-prepare removed. It rests on `prepare()`.
- Crossing the true-bypass threshold (drive 0.5%) jumps: active-path filters shift the phase,
  bypass does not; a 45 Hz + 1 kHz tone steps by up to ~1.7x its peak.
- EQ refit ~97 µs per block on the audio thread while a dot moves; 36 dB jumps ring ~8.6 dB.
- `verify.sh` builds with unbounded `-j` and never builds the editor: check UI with
  `DistortionUiSnapshot`. On Linux it needs `build.yml`'s apt packages (installed by hand here).
- Host bypass freezes the island, delay lines and Sub Guard splitter; IIR ring after a gap.
- Legacy switch may click. Still to do: demo clips (808, NAM bass DI, drawn EQ).
- Installer, interactive upgrade: the old uninstaller's detached phase deletes files, its own exe
  last. The wait is 5 s (was 30 s) and silent on timeout, so a slow phase could delete the new
  bundle or `unins000.exe`. Untested; if seen, restore 30 s.
