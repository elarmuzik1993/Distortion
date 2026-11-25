# State

## Now
- `main` is v2.4.0, untagged (last release v2.3.0): NAM profiles (library browser, drag and
  drop, `input_level_dbu` calibration vs 12 dBu, CLIP / NAM switch), EXTREME +9 dB, Shape with
  the Shape / Legacy switch, Sub Guard as one LR48 split, the graphic EQ (31 dots, 61 fitted
  peaks, end dots HP / LP). `README.md`, `CHANGELOG.md` and `AGENTS.md` match `main`.
- CI macOS runs on `macos-15`, JUCE patched for its SDK (`cmake/PatchJuce.cmake`). 2026-11-25:
  all three builds pass (macOS: tests, pluginval 10, auval); Windows `verify.sh` 4584 assertions.

## Next
0. Branch `chore/repo-housekeeping` (EQ grid bypass dim #37, dead locals, STATE trim) has only
   a cancelled macOS check: merge `main` into it, push, and keep `main`'s STATE.md on conflict.
1. Owner, listening: CLIP / NAM flip on a bass with a large capture (short fade, no click);
   EXTREME +9 dB on clip types and NAM; EQ end dots (HP/LP sweep, all -24 dB = band-pass).
2. Owner, one Release host pass: EQ at zero drive, a v2.3 session with a drawn curve and one
   with Legacy Filter, `.nam` drag and drop, XY pad, profile browser, clip column font, the
   installers incl. one interactive upgrade (Known issues).
3. Owner: NAM calibrated files; 12 dBu as a setting?; then drop "prototype" or label it beta.
4. Owner: Sub Guard listening (808, bass, preset changes while playing); re-voice Hard Clip,
   808 Safe, Vocal Warmth, EXTREME, Bass Driver, Lo-Fi Crush, Diode Bite at 100-150 Hz; rename
   PRESERVE/CONTROL/AGGRO; measure Release CPU.
5. Release: after 1-4 and an ASIO standalone pass, tag `v2.4.0` on `main`.
   `RELEASE_CHECKLIST.md` is gitignored and only on the Windows machine.
6. Sub Guard follow-ups: warm switch-on only proven at distMix 0; NAM-mode `g` leads by D.

## Decisions
- JUCE stays 7.0.12 with a fetch-time patch, not JUCE 8 (different licence terms; Sledge is GPL v3).
- CLIP / NAM (owner, 2026-11-09): not automatable, presets never move it; CLIP parks the
  profile and NAM re-prepares it; with nothing loaded NAM resumes the last profile.
- Graphic EQ fits peaks to the drawing; new param IDs, v2.3 curves migrate (state v4).
- EXTREME +9 dB for clip types and profiles (owner, 2026-11-01); profiles live in the library,
  sessions fall back by content fingerprint, never by name.
- Merge once all three builds pass. Manual dispatch is refused (403): merge `main` in and push.

## Known issues
- A `JUCE_PATH` / `../JUCE` checkout is unpatched: a Mac on Xcode 16 needs the fetched JUCE.
- No test proves a re-enabled profile starts from rest; it rests on `prepare()`.
- Crossing the true-bypass threshold (drive 0.5%) jumps up to ~1.7x peak (phase mismatch).
- EQ refit ~97 µs per block on the audio thread while a dot moves; 36 dB jumps ring ~8.6 dB.
- `verify.sh` never builds the editor (use `DistortionUiSnapshot`); unbounded `-j`.
- Host bypass freezes the island, delay lines and Sub Guard splitter; IIR ring after a gap.
- Legacy switch may click. Demo clips still to do (808, NAM bass DI, drawn EQ).
- Installer interactive upgrade waits 5 s for the old uninstaller; a slow one could delete the
  new bundle. Untested; if seen, restore 30 s.
