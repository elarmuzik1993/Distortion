# DAW QA Matrix — Sledge Distortion (Monolit Beatz)

Manual host-compatibility pass for the release. `pluginval` (in CI) covers the
host *contract*; this covers real-DAW *behaviour* that can't be automated.
Run on the **signed installer build**, not a dev build. Tick per DAW.

Target ≥ 3 DAWs (DoD §4). Recommended: **Reaper**, **Ableton Live**, **Bitwig**
(or **FL Studio**). On **macOS** add **Logic Pro** (exercises the **AU** format,
which VST3-only hosts don't) — run it against the `.component` from the `.pkg`.

| Check | Reaper | Ableton | Bitwig | Logic (AU) | Notes |
|---|---|---|---|---|---|
| Scans / loads without error | ☐ | ☐ | ☐ | ☐ | Logic: `auval -v aufx Dist Mbds` |
| Editor opens; no missing assets/fonts | ☐ | ☐ | ☐ | ☐ | |
| Audio passes; all 7 clip types audible | ☐ | ☐ | ☐ | ☐ | |
| Window scaling 70–100% renders clean | ☐ | ☐ | ☐ | ☐ | X11 fold animation (Linux) |
| Automate Distortion / Mix / Output — no zipper/clicks | ☐ | ☐ | ☐ | ☐ | |
| Save → reopen session restores all params + preset name | ☐ | ☐ | ☐ | ☐ | |
| Bypass toggle is click-free at a transient | ☐ | ☐ | ☐ | ☐ | DoD §2 follow-up |
| Oversampling change (Settings) is glitch-free | ☐ | ☐ | ☐ | ☐ | deferred rebuild |
| Latency/PDC correct against a dry parallel track | ☐ | ☐ | ☐ | ☐ | mix knob phase |
| 10+ instances in one project — stable, independent | ☐ | ☐ | ☐ | ☐ | also `DistortionSoak` |
| Meters (GR, phase, scope) read plausibly | ☐ | ☐ | ☐ | ☐ | |
| No crash on close / project teardown | ☐ | ☐ | ☐ | ☐ | |

## Sign-off

- [ ] All rows pass in ≥ 3 DAWs.
- [ ] DSP-freeze declared on `ship/vX.Y` (bugfixes only thereafter).
- [ ] All 7 clip types auditioned and confirmed musical (DoD §3).

Tester: ______________  Date: __________  Build: __________
