# State

## Now
- v2.5.0 released 2026-11-25 (tag `v2.5.0`, six assets; notes = `CHANGELOG.md` v2.5): CLIP /
  NAM switch, EQ with 61 fitted peaks and HP / LP end dots, EXTREME +9 dB, Linux install
  scripts, installer guards. v2.4.0 (2026-11-03): NAM profiles, Shape, Sub Guard, 31-dot EQ.
  `README.md`, `INSTALL.md`, `docs/USER_GUIDE.md`, `CHANGELOG.md` and `AGENTS.md` match `main`.
- CI macOS runs on `macos-15`, JUCE patched for its SDK (`cmake/PatchJuce.cmake`). 2026-11-25:
  all three builds pass (macOS: tests, pluginval 10, auval); `verify.sh` 4584 assertions.
  Without a Developer ID CI ad-hoc signs the macOS bundles (never yet run on a real Mac).

## Next
1. Owner, post-release listening: CLIP / NAM flip on a bass with a large capture (no click);
   EXTREME +9 dB on clip types and NAM; EQ end dots (HP/LP sweep, all -24 dB = band-pass).
2. Owner, one Release host pass: EQ at zero drive, v2.3 sessions (drawn curve, Legacy Filter),
   `.nam` drag and drop, XY pad, profile browser, the installers incl. an interactive upgrade,
   the unsigned macOS `.pkg` via Open Anyway on macOS 15 (does the Standalone app also need
   the `xattr` step? INSTALL.md only clears the VST3 and AU); an ASIO standalone pass.
3. Owner: NAM calibrated files; 12 dBu as a setting?; then drop "prototype" or label it beta.
4. Owner: Sub Guard listening (808, bass, preset changes while playing); re-voice Hard Clip,
   808 Safe, Vocal Warmth, EXTREME, Bass Driver, Lo-Fi Crush, Diode Bite at 100-150 Hz; rename
   PRESERVE/CONTROL/AGGRO; measure Release CPU.
5. Website: Windows exe + zip, Linux tarball, macOS ZIP now; `.pkg` after Next 2. Add GPL source, checksums, guide.
6. Owner: on a real Mac (Apple Silicon), check the ad-hoc signed ZIP loads; then `.pkg` every run.

## Decisions
- v2.5.0 ships before the owner's listening and host checks (owner, 2026-11-25): they are
  post-release testing now. No Apple Developer ID (cost): macOS stays unsigned.
- JUCE stays 7.0.12 with a fetch-time patch, not JUCE 8 (different licence terms; Sledge is GPL v3).
- CLIP / NAM (owner, 2026-11-09): not automatable, presets never move it; CLIP parks the
  profile and NAM re-prepares it; with nothing loaded NAM resumes the last profile.
- Graphic EQ fits peaks to the drawing; v2.3 curves migrate (state v4). EXTREME +9 dB (owner).
- Merge once all three builds pass. Manual dispatch is refused (403): merge `main` in and push.
- Releases: bump CMake + `installer/Distortion.iss`, new CHANGELOG section (never edit a shipped
  one), merge, then `git push origin refs/tags/vX.Y.Z` (the workflow hook reads a bare tag
  name as a branch); put the CHANGELOG section atop the generated release notes.

## Known issues
- v2.4.0 sessions with EQ dot 0 or 30 below 0 dB hear a HP / LP now (no migration; in changelog).
- A `JUCE_PATH` / `../JUCE` checkout is unpatched: a Mac on Xcode 16 needs the fetched JUCE.
- No test proves a re-enabled profile starts from rest. Crossing drive 0.5% jumps ~1.7x peak.
- EQ refit ~97 µs per block while a dot moves; 36 dB jumps ring ~8.6 dB.
- `verify.sh` never builds the editor (use `DistortionUiSnapshot`). Legacy switch may click.
- Host bypass freezes the island, delay lines and Sub Guard splitter; IIR ring after a gap.
- Installer interactive upgrade waits 5 s for the old uninstaller; untested, else restore 30 s.
