# Sledge Distortion — Source of Truth (AGENTS.md)

<!-- Shared by every coding agent; no CLAUDE.md needed. The Obsidian line names the vault project
     folder (exists only locally). Scaffolded by /workflow:kit. Keep the state import below. -->

Obsidian: [[Monolit Distortion]]

**External Documentation:**
- **Projects HUB:** [[03 Projects/Projects HUB]]

## Current state

@.agents/STATE.md

## Session workflow
- **Start**: read the current state above (`.agents/STATE.md`). If `git log` shows commits after
  its last update, that work is unrecorded; check it before trusting the note.
- **Work**: nothing is done until `bash scripts/verify.sh` passes. Add tests for fixes and features.
- **End**: hand off. Verify, rewrite `.agents/STATE.md` (Now, Next, Decisions, Known issues),
  commit, and in a cloud session push the branch.

## Commands
| Task | Command |
|---|---|
| Verify (definition of done) | `bash scripts/verify.sh` (configure, build `DistortionTests`, run it; `BUILD_TYPE`, `BUILD_DIR` override; finds MSVC itself on Windows) |
| Quick check (session start) | `bash scripts/verify.sh --quick` (static checks only) |
| Build plugin | `cmake --build build --target Distortion_Standalone --target Distortion_VST3` (see Build System) |
| Run one test group | `DistortionTests <name filter>` |

## Project Overview
**Sledge Distortion** is a professional JUCE audio plugin by Monolit Beatz featuring multi-stage distortion processing, LA2A-style optical compression, and advanced signal processing. It ships as **VST3** on Windows and Linux, and as **VST3 + AU (Audio Unit) + Standalone** on macOS. See "Release & Packaging" below — the Standalone target builds everywhere but is only packaged by the macOS job.

## Key Specifications
- **DSP Specs**: See `DSP Architecture` section below.
- **UI Specs**: Professional custom rotary knobs (`CustomKnob`), real-time oscilloscope, and LA2A-style gain reduction meter.
- **Compact Window Mode** (USE-48): When the oscilloscope is disabled in Settings, the plugin window folds to a compact height (180px base at 100% scale). The Settings overlay stays compact: its rows live in a `SettingsContent` component inside a `juce::Viewport`, so the content scrolls (vertical neon scrollbar) and no rows are clipped at any window size or scale. LFO/Compression tabs auto-collapse on fold; re-expanding them in compact mode shows an `ExpansionBackdrop` panel (tight bounding box, dark bg + red border) that covers the parameter knobs while controls are tweaked.
- **Oscilloscope Toolbar Toggle**: A scope button in the title bar toggles compact ↔ full mode (independent of the Settings toggle — both stay in sync via `applyOscilloscopeEnabled`). The transition uses a **snap + alpha-fade** strategy to avoid X11 resize tearing on Linux: expand snaps the window to full size in a single atomic resize then fades the scope/XYMorphPad/PhaseCorrelationMeter alpha 0→1; collapse fades alpha 1→0 then snaps to compact. Animation runs on a dedicated 60 Hz `FoldAnimTimer` (vsync-aligned), `saveSettings()` is deferred via `MessageManager::callAsync`, and the scope's 30 Hz repaint timer only starts after the fade completes.
- **Scope Overlay Selector**: A single "Overlay" dropdown in the Settings INTERFACE section chooses which overlay owns the oscilloscope — **Off** / **XY Morph** / **Graphic EQ** — so only one intercepts the mouse at a time (replaces the old standalone XY Morph pill). State persists in `settings.xml` (`scopeOverlay` int: 0/1/2; the legacy `xyMorph` bool auto-migrates to mode 1). `applyScopeOverlayMode` + `updateScopeOverlays(alpha)` centralise visibility / mouse-interception / 30 Hz timers for both overlays, keyed off the scope's live visibility + fold-fade alpha. Visibility stays governed by the oscilloscope toggle.
  - **XY Morph** mode: dragging the `XYMorphPad` morphs 6 distortion params via `morphDistortionParameters`. At rest the pad draws only its "XY MORPH" label (crosshair cursor on hover); while dragged it draws a neon puck at the smoothed morph position with a crosshair, a 12-point trail, axis hints (DRIVE →, SCOOP/BARK) and a live readout of Distortion Amount and Shape next to the label. The puck holds 0.25 s after release, then fades over 0.5 s. Timing and trail are `XYPadFeedback` (`Source/XYPadFeedback.h`, pure, unit-tested); the pad repaints only while the puck shows. `DistortionUiSnapshot --xy-touch` renders it held.
  - Two toolbar buttons in the title bar (next to the scope/gear buttons) quick-toggle the overlays: **`XyButton`** (XY↔Off) and **`EqButton`** (EQ↔Off). Each lights up when its overlay is active and dims otherwise; they're mutually exclusive via the shared mode and stay in sync with the Settings selector through `applyScopeOverlayMode` (the same duplicate-toggle pattern the scope button uses). Toolbar order mirrors the dropdown: scope · XY · EQ.
  - **EQ bypass**: a power button in the overlay's top-left corner one-click toggles the `eqEnabled` param — the DSP ramps the applied gains to flat (click-free) while the drawn curve is kept, so it's a true A/B bypass rather than a destructive flatten. The curve dims and the label reads "(BYPASSED)" while off.
  - **Graphic EQ** mode: the `GraphicEqOverlay` lets you free-draw a magnitude curve on the scope (Catmull-Rom spline through 12 control points; double-click flattens). Each control point maps to one output-stage peaking band (`eqBand0..11` APVTS params, log-spaced ~30 Hz–16 kHz, ±12 dB). DSP is a base-rate `ProcessorDuplicator` peaking bank (`processGraphicEq`, inserted after the DC blocker, before the output-gain stage); coefficients are written in place via `writePeakFilterCoeffs` (mirrors JUCE `makePeakFilter`, no `processBlock` allocation) from per-band `SmoothedValue` gains. The whole bank bypasses when the curve is flat (all bands within `EQ_FLAT_EPS_DB` of 0), so a fresh instance is bit-transparent. A peaking biquad goes unstable once its centre reaches Nyquist (`alpha` turns negative and the poles leave the unit circle), so `prepareEqBands` marks bands at/above `EQ_MAX_FREQ_RATIO` (0.45) × base rate unusable — they get no coefficients, never process, and don't keep the bank awake. In practice the 16 kHz band drops out at base rates ≤32 kHz; at 44.1 kHz and above all 12 are live. The overlay is a UI-only view of the params — the EQ engages from the saved curve regardless of the selector, so presets always sound right; "bypass" = flatten.
- **Clip-type column**: clip type, EXTREME, BOOST and PROFILE are four matching pills (`ClipColumnLookAndFeel`: one shape, outline and font; each outline takes its control's accent), the full column wide and stacked, centred on the neighbouring knobs. The label slot below stays empty. The column cannot widen, because the texture's knob cutouts are registered to the knob positions.
- **NAM profile browser** (prototype): `‹`/`›` arrows (`ProfileArrowButton`, filled triangles inked like the pill's text) lie over the two ends of the PROFILE pill (`ClipColumnLookAndFeel::endZoneWidth`); the pill carries `hasEndZones`, so its label keeps clear of them. They used to sit in the gaps beside the pill, where the left one ran into the Distortion Amount label. They step through the **profile library** (`Source/Nam/ProfileLibrary.{h,cpp}`, a `Profiles` folder at `diag::profilesDir()`, next to Presets): every `.nam` under it, subfolders included, natural order by relative path, wrapping at both ends. "Load profile..." (multi-select) **imports** (copies) into the library (`ProfileLibrary::importAll`): each file is vetted first with `NamProfile::check` (parses and builds one model without prewarming it, mono only), so a broken file never enters the library; an identical same-named file is reused, different content gets ` (2)`. A session therefore points at Sledge's copy. Every file that is skipped, or that plays from its own folder because the copy failed, is listed in a warning dialog; nothing is dropped silently. **Drag and drop**: `.nam` files, or folders of them (subfolders included, `ProfileLibrary::collectProfiles`), dropped anywhere on the editor go through the same import and the first loads; the window dims with "DROP TO LOAD PROFILE" while a drag is over it (`DistortionUiSnapshot --drop-hint`), and a drop holding no profile says so. The pill **always reads PROFILE**: lit (toggle on) once a profile has loaded, amber on an error; the name lives on the **`ProfileCard`**, which goes over the scope while the scope shows and becomes a bubble above the pill when it is folded away. It never takes the mouse, so XY/EQ keep working under it. Timing is `ProfileCardAnimation` (`Source/ProfileCardAnimation.h`, pure arithmetic, unit-tested): slide in from the clicked side (0.18 s, quintic), hold 1.5 s after the last click and while the named profile is still loading (`keepAlive`), fade 0.3 s; a click while it shows slides the name inside the card. Driven by the editor's 60 Hz tick; repaints only while moving.
  - **Sessions never lose a profile.** State carries `namProfilePath` plus `namProfileFingerprint` (`ProfileLibrary::fingerprint`: size + 64-bit FNV-1a). When the path is missing, the loader (off the message thread) loads a **library file with the same fingerprint**, never a same-named file with other content, and the session points at it from the next save. Restore skips a path only while it is loaded or loading, so a path whose last load failed is retried. Neither attribute ever enters the parameter tree, so presets carry no profile. A load superseded while queued returns before reading its file (`profileFilesRead` counts reads for tests).
  - **Offline renders get the profile from the first block.** When the host prepares non-realtime, `prepareToPlay` starts the startup profile itself and waits (`waitForProfileLoads`, up to `PROFILE_OFFLINE_WAIT_MS`) for any load in flight, so `prepareProfilesForHost` installs it without the timer. A render started straight after a session restore used to play its opening through the clip type. Only a prepare covers this: a state restored into an already-prepared offline instance still goes in on the timer.
  - **New instances start with the last profile** (`lastProfile` in `settings.xml`, written by the editor once a profile chosen there has loaded; Settings > **Recall Profile**, `recallLastProfile`, default on). The processor loads it at the **first `prepareToPlay`** and only if no `setStateInformation`, load or clear came first (`profileIntentKnown`), so a saved project always decides; a host that prepares before restoring gets the session's answer in the end. A missing file is skipped silently. Only real wrappers read it (`wrapperType != wrapperType_Undefined`): render/soak/snapshot tools never do, and unit-test builds read no app data at all (tests use `setStartupProfile` / `setProfileLibraryRoot`).
  - **`SettingsState::saveToFile` starts from the file on disk**, so attributes written on their own (`lastProfile`, `profileImportFolder`, via `PluginEditor::writeSettingsAttribute`) survive an editor that was open before they were written. Writing a fresh `XmlElement` there would silently drop them.
- **Shape** (replaces the input filter): one bipolar knob, Bark (mid boost) ↔ Flat ↔ Scoop (mid cut, brighter). `ShapeFilter` (`Source/ShapeFilter.h`, constants `DSPConstants::SHAPE_*`) is a 600 Hz peaking band (Q 0.7; +12 dB at full Bark, −15 dB at full Scoop) plus an opposing 2.5 kHz high shelf (Q 0.707; −6 dB Bark, +6 dB Scoop), with a linear taper (half a turn gives half the effect). These were raised from +9/−12 dB, ±4 dB and a |s|^1.5 taper after a listening pass found the first voicing too subtle. Param `shape` (−100..100, default 0, text "Bark 60"/"Flat"/"Scoop 40"); coefficients are rewritten in place every 32 samples from a smoothed value, and the filter is skipped entirely when flat (bit-transparent), after a 10 ms hold (`SHAPE_FLAT_HOLD_S`) that lets its tail decay so clearing the filter memory cannot click. It runs twice: pre-drive at base rate (`shapePre`) and post-drive in the oversampled domain at up to three-quarter amount (`shapePost`, `SHAPE_POST_AMOUNT` 0.75, faded in with the drive over the first 10%). LFO destination 2 swings it ±50 knob units (clamped ±100, block rate). XY Morph's Y axis drives it (bottom −60 … top +60, inaudible in Legacy mode); Randomize sets ±70. `migrateState` resets `shape` to 0 for states older than version 2. The v2.3 input filter (`filterMode`/`highPassFreq`, DSP unchanged) is behind the **Shape / Legacy switch**, param `legacyFilter` (bool, default off, Settings > Legacy Filter, saved with the session). Off: Shape has the knob and the legacy filter runs at its transparent default (High Pass 20 Hz) whatever its params hold. On: the slot shows the filter knob with a `CyclingComboBox` mode picker in the label slot (`PluginEditor::updateInputStageControls`), Shape is held Flat (`PluginProcessor::effectiveShape`), and LFO destination 2 sweeps the cutoff ±1.5 octaves as v2.3 did (`LegacyInputFilter::modulatedCutoff`; the target pill reads "Filter"). Randomize and the lock menu follow the slot. State version is 3: `migrateState` sets `legacyFilter` for older states from `LegacyInputFilter::isActive`, so v2.3 sessions that used the filter open in Legacy mode. The knob is bipolar (`CustomKnob::setBipolar`, centre-out arc, double-click → 0). Factory presets never touch the legacy params and set `legacyFilter` off; 13 of them set Shape (and Sub Guard where the old low cut tightened the bass).
- **LFO BPM Sync**: SYNC button in the LFO panel switches the rate knob from free-running Hz mode to BPM-locked division mode. The rate knob steps through 1/1 → 1/2 → 1/4 → 1/8 → 1/16 → 1/32 → 1/4T → 1/8T → 1/16T; the label below updates live to show the selected division. Falls back to 120 BPM when no host playhead is available.
- **LFO INV Toggle**: Inverts LFO polarity in both DSP (`lfoSign = -1`) and the arc visualizer (arc sweeps below the knob value instead of above).
- **CyclingComboBox** (`Source/CyclingComboBox.h`): Custom `juce::ComboBox` subclass — single-click cycles to next item (timer-debounced), double-click opens the full list. Used for clip type, LFO waveform/destination, compression ratio, and settings dropdowns.
- **Safety**: Includes `realtime-audio-safety-checklist.md` for thread safety and DSP best practices.
- **CI/CD**: GitHub Actions for automated Windows/Linux/macOS release artifacts. **Windows, Linux and macOS (`macos-14`) all run on every push/PR that changes more than docs**; only the `release` job is gated, to `v*` tags. A `changes` job skips the three builds when a push or PR touches only Markdown, `docs/` or `.agents/` (`THIRD-PARTY-NOTICES.md` excepted, since it is packaged); it is a job-level `if:`, not workflow `paths-ignore`, so skipped builds still report success to required checks, and anything uncertain (tags, manual runs, new branches, a missing base commit) builds. Artifacts: dev runs upload only the Windows ZIP, macOS ZIP and Linux tarball, kept 7 days; the installers upload on tags only (the `release` job is their sole consumer, and they are still built every run so packaging stays tested); tag artifacts keep 30 days. A `concurrency` group cancels superseded non-tag runs. macOS was once tag-and-dispatch-only because those runners bill at 10× minutes (Linux 1×, Windows 2×), but that multiplier only applies to *metered* minutes and standard runners are free on public repos — the gate cost coverage and saved nothing. It matters because macOS is the only job building the universal binary (arm64 + x86_64) and running `auval`, and the platform most exposed to SDK drift (pinned JUCE 7.0.12 uses APIs newer macOS SDKs removed), so a release tag used to be the first thing to exercise that path. It is now covered before merge.

## Build System
### CMake (Linux/Mac/Windows - Recommended)
```bash
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
# Build plugin (VST3 + Standalone)
cmake --build . --target Distortion_Standalone --target Distortion_VST3 -j$(nproc)
# Build and run tests
cmake --build . --target DistortionTests -j$(nproc)
./DistortionTests_artefacts/Debug/DistortionTests
```
**IMPORTANT**: Do NOT use `--target Distortion`. Always use `--target Distortion_Standalone` to ensure fresh linking.

### macOS (CMake)
On Apple, CMake adds an **AU** format target (`Distortion_AU`) alongside VST3 + Standalone, and builds a **universal binary** (`arm64;x86_64`, min deployment target 11.0) — the arch/target are set at the top of `CMakeLists.txt` under `if(APPLE)` and can be overridden on the command line for a faster single-arch dev build:
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build build --target Distortion_VST3 Distortion_AU Distortion_Standalone -j
```
Artefacts land in `build/Distortion_artefacts/<Config>/{VST3,AU,Standalone}/`. Local install dirs: VST3 → `~/Library/Audio/Plug-Ins/VST3`, AU → `~/Library/Audio/Plug-Ins/Components`. The `.pkg` installer is built by `installer/macos/build_pkg.sh` (see Release & Packaging).

### Projucer (Windows VS2022)
- Edit settings via `Distortion.jucer`.
- Build solution in `Builds/VisualStudio2022/Distortion.sln`.
- Test Runner: `Builds/VisualStudio2022_Tests/x64/Debug/ConsoleApp/DistortionTests.exe`.

### VST3 moduleinfo.json Fix
JUCE generates invalid JSON manifests. A post-build script fixes them:
```bash
python scripts/fix_moduleinfo_json.py build --all
```

## Architecture & Signal Chain
1. **Input Stage** → Legacy v2.3 input filter (`filterMode`/`highPassFreq`; transparent High Pass 20 Hz unless `legacyFilter` is on), then **Shape (pre-drive)** (held Flat while `legacyFilter` is on): bipolar Bark ↔ Scoop voicing (600 Hz peak/dip + opposing 2.5 kHz shelf, `ShapeFilter`, amount `SHAPE_PRE_AMOUNT`).
2. **Oversampling** → Polyphase IIR, selectable Off / 2x / 4x (4x default; `oversamplingMode` in `settings.xml`)
3. **Pre-Distortion Transient Tamer** → Hardcoded (1ms attack, 50ms release, 2.5:1 ratio, -12dB threshold). It runs on the **drive input** built in step 4: full range with Sub Guard off, the high band only with it on (blended by the on/off weight between), so the clean low band is never compressed. **Clean Boost** (lift + pre-emphasis shelf before the drive, de-emphasis after it) and the auto-gain input reference use the same drive input, so the order is always tamer → auto-gain reference → Clean Boost emphasis → drive → de-emphasis. All of it lives in `applySubGuardSplit`, which therefore runs in profile mode and with Sub Guard off too.
4. **Sub Guard Band-Split** (Optional) → crossover up to 200 Hz (≤25 Hz snaps to off in the UI) protecting low band from distortion: one 8th-order Linkwitz-Riley (LR48) at every frequency, **split around the drive**. The low band takes the full LR48 low-pass here; the high band takes one 4th-order Butterworth high-pass here and the other in step 11, so low + high still sums flat and the drive's products below the crossover are cut. All sections run in double (state and coefficients) from one writer. **On/off is a weight `g`** (0..1 per sample, `SUBGUARD_SWITCH_TIME_S` 15 ms ramp): drive input `(1−g)·x + g·high`, post-drive half `(1−g)·y + g·HP(y)`, low band added back as `g·low`. The splitter, the second half, `toneFilterLow` and the low band's island delay run **warm on every active block, Sub Guard on or off** (while off the crossover holds the last on frequency), so switching on never starts a cold filter; at `g = 0` nothing of the split is heard and the output is the full-range path's, byte for byte. A state load snaps `g` to its target (`resetDSPState`). `SubGuardSplitter` (`Source/SubGuardSplitter.{h,cpp}`) owns the crossover (sections, frequency smoother, band buffers; `process` before the drive, `processPostDrive` after); `applySubGuardSplit` drives its high band and recombines. The old LR24/LR18/LR12 zones and their order crossfade are gone (`docs/SUB_GUARD_TRANSITION_REVIEW.md`, step 6).
5. **Distortion Stage** → 7 Professional Clip Types (Brutal Fuzz, Tube, Bit Crusher, Tape, Transformer, Diode, Decimator), or a loaded NAM profile (prototype). A profile's input is Distortion Amount's curve (−24 dB at 0%, +12 dB at 100%), times EXTREME's push (`EXTREME_DRIVE_MULT`, ×4 / +12 dB, the same as a clip type's drive, ramped per block by `extremePushRamp`), times the capture's **calibration trim**: `NamProfile::referenceInputDbu` (12 dBu, the NAM plugin's default interface level) minus the file's `input_level_dbu`, applied at the model's rate inside `NamProfile::process` so a crossfading profile gets its own trim. A file without `input_level_dbu` is untrimmed. The output is still normalised to −18 dB by `loudness`. In profile mode the chain runs at 1x and each model runs at its trained rate inside a `ResamplingIsland` (`Source/Nam/`), which delays the wet path by a whole number of samples, D. The Distortion Mix dry half, the Sub Guard low band and the global Mix dry wait the same D.
6. **Shape (post-drive)** → the same curve at up to `SHAPE_POST_AMOUNT`, oversampled domain. It shapes the harmonics the drive created, which is what makes Shape audible on near-sine 808s. Its amount fades in with the drive (`ShapeFilter::postDriveAmount`: 0 at 0% distortion, full at `SHAPE_POST_FULL_DRIVE` 10%), so crossing the true-bypass threshold, where only the pre-drive half runs, does not jump the EQ; true bypass also clears its memory. Runs after the Clean Boost de-emphasis, on a block that never holds Sub Guard's clean low band (that is added back at step 11), so the clean low band is never shaped after the drive; before auto-gain, compression and the soft clipper; re-prepared with the oversampler (1x in NAM profile mode). The global-Mix dry capture precedes both Shape stages, so Shape colours only the wet path.
7. **Tone Filter & Waveshaper** → Order depends on "Clean Mode" toggle. Runs in oversampled domain.
8. **Auto-Gain Compensation** → RMS-based (±12dB).
9. **LA2A Compression** → Optical cell simulation in oversampled domain (Attack 10ms, Release 500ms, 2dB knee, Tube harmonics).
10. **Soft Clipper** → ISP protection at -0.3dBFS (oversampled domain).
11. **Sub Guard Recombine** → the crossover's second high-pass half on the high band (after every nonlinear stage, so it can lift peaks back over the soft clipper's ceiling; the output limiter still bounds the output, and the LA-2A sees sub products this then removes), then the phase-matched toneFilterLow on the low band, then the sum. In profile mode it runs on the island's output.
12. **Downsampling** → Return to original sample rate.
13. **DC Blocking** → One-pole (~3.5Hz cutoff).
14. **Graphic EQ** (Optional) → 12-band peaking bank at base rate (`processGraphicEq`); whole bank bypasses while the drawn curve is flat. Bands whose centre sits at/above `EQ_MAX_FREQ_RATIO` (0.45) × sample rate are dropped, so the 16 kHz band is inactive at base rates ≤32 kHz.
15. **Output Stage** → Final gain staging (±9dB).
16. **Global Mix** → dry/wet blend, dry phase-aligned through the matched `dryOversampling` instance (comb-free at every frequency).
17. **Output Limiter** → Safety limiter (-0.5dBFS), **after** the blend so the ceiling bounds the mixed output, not just the wet path. Always the last gain-shaping stage on both the active and bypass paths; only the latency pad and the duck fade follow it.

Steps 13–15 live inside `applyAutoGainAndISP`; 16–17 are in `processBlock` proper. `docs/Architecture Contract.md` carries the fully expanded ordering — keep the two in step.

**Latency and rebuilds.** Sledge always reports one figure, R: the larger of the oversampler's latency for the user's setting and the island delay a 48 kHz model needs at the host rate. Every path pads up to R, which adds nothing at 48 kHz. While a profile trained at a rate other than 48 kHz is loaded, R rises to that profile's island delay when it is larger. Every runtime oversampler rebuild (the Oversampling setting, linear phase, NAM profile mode on or off) goes through a duck: a 10 ms fade out, the rebuild under the callback lock, then a 10 ms fade in. Swapping between profiles with the same island delay crossfades instead: a warm-up of at most 100 ms, then a 30 ms blend. `prepareToPlay` holds the callback lock and finishes any pending switch without fading.

## Channel Layouts
- **Supported: mono->mono, stereo->stereo, and mono->stereo.** The mono->stereo case is not
  optional politeness: a bass or guitar arrives on one interface input, and JUCE's
  `AudioProcessorPlayer::findMostSuitableLayout` *tries that layout first* whenever the device
  offers a single input. Refusing it makes JUCE fall back to `{2,2}` with a silent right
  channel, which is why the plugin used to play a mono instrument out of one speaker.
- **The mono copy happens before the RT anomaly probe.** Under mono-in/stereo-out the host
  sizes the buffer to the *output* bus, so channels above the input count hold uninitialised
  memory. The probe reads one frame per channel; probing unwritten memory can report a
  non-finite block and raise a false diagnostic report.
- **Size internal buffers from the wider of input and output**, never
  `getTotalNumInputChannels()` alone. Under mono-in/stereo-out that is 1 while the buffer
  actually processed has 2 channels, so the oversampler would be built for one channel and
  handed two. `prepareToPlay` and `rebuildOversampling` both do this.
- **`monoInput` never engages itself.** Detection is advisory: it lights the toolbar indicator
  and nothing more. Silence on one channel is evidence of silence, not of a mono source — a
  hard-panned part is legitimately one-sided, and collapsing it unasked would change a mix,
  engage mid-take, and break render reproducibility. Detection is a *relative* comparison
  (one side ~30 dB below the other); an absolute silence floor only ever fires on synthetic
  all-zero buffers, never on a real input's noise floor.

## Engineering Standards
- **Memory**: No dynamic allocation in `processBlock`.
- **Thread Safety**: Use `std::atomic` for parameters and a lock-free `juce::AbstractFifo` (SPSC) for Scope data. The scope path holds **no lock** — the old `SpinLock` was removed to keep the audio thread contention-free, so do not reintroduce one.
- **Smoothing**: Always consume `SmoothedValue` in a **sample-first loop** to avoid buffer exhaustion.
- **Bypass**: True bypass when `distortion < 0.5%` and `compression OFF`. Skips oversampling and the whole distortion/compression chain. **The legacy input filter (only when `isInputFilterActive()`) and the pre-drive Shape (only when off Flat) still run**, so Shape works as a standalone tone control; both at their transparent defaults leave bypass bit-clean. The bypass branch imposes the path's own latency through `bypassLatencyDelay`, then the shared output pad, reaching R (see Latency and rebuilds) in total, so toggling causes no timing jump. Full ordering in `docs/Architecture Contract.md`.
- **Namespace**: DSP constants are centralized in the `DSPConstants` namespace in `PluginProcessor.h`, except the Shape voicing (`SHAPE_*`) and Sub Guard (`SUBGUARD_*`), which extend the same namespace in `Source/ShapeFilter.h` and `Source/SubGuardSplitter.h` so those headers stand alone.

## Build & Test
- **Framework**: JUCE UnitTest runner.
- **Coverage**: 3800+ assertions (100% PASS RATE).
- **Categories**: DSP, Compression, LFO, ProcessBlock, ThreadSafety, SampleRate (44.1k-192k), GraphicEq (sweeps 22.05k-192k to cover the Nyquist band-drop), State I/O, FactoryPresets, Diagnostics, Settings Persistence, NAM Profile, Profile Library, Profile Card Animation, XY Pad Feedback.
- **Golden Audio**: `GoldenAudioTests` renders 16 fixed cases (seeded noise, a synthetic 50 Hz sub + 1000/1037 Hz mids, 44.1 kHz, 512-sample blocks, 1 s warm-up, 0.5 s captured) and compares each with `Resources/TestAudio/golden_<case>_44100.wav` (32-bit float stereo; CMake passes the folder in as `DISTORTION_TEST_AUDIO_DIR`; not BinaryData). Limit: max abs sample error −90 dBFS (looser per case where float IIRs at 4x amplify rounding: Clean Boost −66, Shape −80, compression −85), set ~13 dB above what a GCC FMA build measured, so cross-compiler rounding passes and a one-step knob change fails. A missing reference fails. Regenerate: `DISTORTION_GOLDEN_GENERATE=1 build/DistortionTests_artefacts/Debug/DistortionTests Golden`. **A PR that changes the sound regenerates them and says why in its description.** Cases pin every parameter explicitly (`goldenBaseline()`; a new parameter fails until pinned there), never factory presets, so re-voicing presets never touches them.
- **Host validation**: CI gates on `pluginval --strictness-level 10` (Windows + Linux; xvfb on Linux).
- **Soak/stress**: `DistortionSoak` console tool (`Source/Tools/SoakHarness.cpp`) runs N instances faster-than-realtime, failing on non-finite output or RSS growth (DoD 24h/10+-instance gates). **Run it on Linux** — the leak half reads `/proc/self/statm`, so it is Linux-only. Elsewhere the harness reports `SOAK INCOMPLETE` and exits non-zero rather than `PASSED`: it checked finiteness but not memory growth, and only a `PASSED` earns the `RELEASE_CHECKLIST.md` §4 box. It used to print `PASSED` on Windows with the leak check silently skipped, which is how a half-run gate can look like a clean one.
- **UI/layout changes**: verify with `DistortionUiSnapshot` (`Source/Tools/UiSnapshot.cpp`), **not** by screenshotting the standalone — its menu bar and "audio input is muted" banner shift and clip the editor, pushing the knob row outside the window. The tool paints the real `PluginEditor` offscreen at exact geometry:
  ```bash
  cmake --build build --target DistortionUiSnapshot
  ./DistortionUiSnapshot --out shots              # all four window scales
  ./DistortionUiSnapshot --collapsed --out shots  # scope folded away
  ./DistortionUiSnapshot --extreme --out shots    # after the EXTREME crack fade settles
  ./DistortionUiSnapshot --signal --out shots     # scope shows a waveform (docs images)
  ./DistortionUiSnapshot --shape -60 --out shots  # Shape knob at a given value (-100..100)
  ./DistortionUiSnapshot --legacy --out shots     # Legacy Filter mode: the filter knob and mode picker
  ./DistortionUiSnapshot --settings --out shots   # the Settings panel open
  ./DistortionUiSnapshot --xy-touch --out shots   # XY Morph pad held: puck, trail, readout
  ./DistortionUiSnapshot --drop-hint --out shots  # a .nam drag over the window
  ./DistortionUiSnapshot --profile drive.nam --out shots                 # a NAM profile loaded (pill lit)
  ./DistortionUiSnapshot --profile drive.nam --profile-card --out shots  # the browser's name card, settled
  ```
  **`--signal` renders are not bit-comparable across builds.** The trace alpha follows the
  scope's smoothed signal-presence value, and the number of timer ticks fitting the harness's
  settle window shifts with build timing. Use a **no-signal** render for byte comparisons.
  Note also that the version stamp is baked into every render, so a hash is only comparable
  within a single commit.

  `--signal` runs a fixed 220 Hz tone through the processor so the scope draws a
  waveform; it exists for documentation images. **Do not use a `--signal` render
  as a layout reference** — layout verification must not depend on DSP output.
  Without the flag the renders are byte-identical run to run.

  It is the only target built with `JUCE_MODAL_LOOPS_PERMITTED=1`, so it can pump the message loop and capture timer-driven animation in its settled state.

  **The harness owns the state it renders from.** It is also the only target built with `DISTORTION_UI_SNAPSHOT=1`, which exposes `PluginEditor::setDataRootOverride`; the tool points the settings/preset root at a scratch dir under TEMP and writes the fold state and window scale it wants. Your real `settings.xml` is neither read nor written. This is load-bearing, not tidiness: the editor takes its fold state from settings (`paint()` keys off `settingsState.oscilloscopeEnabled`, deliberately not the measured height), so when the tool merely resized the window, `--collapsed` on a machine saved as *expanded* laid out expanded and squeezed it into a 180px window — every panel overlapping the knob row, which reads as a severe layout regression that is not there. A verification gate must not depend on ambient machine state.

## Licensing
- **Sledge Distortion is GPL v3.** This is not a free choice: JUCE 7 is offered
  under either paid tier-leveled terms or the GPL v3, and this project takes the
  GPL route. A permissive licence (MIT/BSD/Apache) is **not** available while the
  build links the non-ISC JUCE modules — do not "simplify" `LICENSE`.
- Taking the GPL route also removes JUCE's $50k revenue limit, and permits
  setting `JUCE_DISPLAY_SPLASH_SCREEN=0` (currently unset, so the badge shows).
- **Third-party notice obligations are binary-distribution obligations**, not just
  source ones: Orbitron (SIL OFL 1.1, embedded via `BinaryData`), the bundled
  Steinberg VST3 SDK, FLAC and Ogg Vorbis (via `juce_audio_formats`), libpng,
  IJG libjpeg and zlib (via `juce_graphics`/`juce_core`), and the AudioUnit SDK
  (macOS AU only) each require their terms to accompany every built artifact.
  `THIRD-PARTY-NOTICES.md` is that vehicle.
- **The bundle carries its own notices.** A CMake post-build step copies
  `LICENSE` + `THIRD-PARTY-NOTICES.md` into each format's `Contents/Resources`,
  so the terms survive installation instead of only sitting beside the bundle in
  an archive. Every channel inherits this for free — the Inno installer copies
  the bundle recursively, the `.pkg` stages built bundles as its payload, and all
  three archives contain the bundle. **A new distribution channel needs no extra
  licence wiring**, and the archives additionally carry top-level copies purely
  so nobody has to open a bundle to read the terms.
- **The packaging steps assert this.** Each of the Windows ZIP, Linux tarball,
  macOS ZIP and `.pkg` steps fails the build if the licence files are missing
  from the artifact. Do not weaken those checks to make a build pass.
- Adding a dependency means adding its notice to `THIRD-PARTY-NOTICES.md` and
  checking it is GPL v3-compatible. Scope the notices to what the build actually
  *compiles*: `JUCE_USE_FLAC`/`JUCE_USE_OGGVORBIS` are on, MP3 and LAME are off.

## Release & Packaging
- **Formats shipped**: **VST3 only on Windows + Linux**; VST3 + AU + Standalone on macOS (universal arm64 + x86_64). The Standalone target is built on macOS alone (`build-macos` is the only job passing `Distortion_Standalone`) and reaches users solely through the `.pkg`. Windows/Linux release artifacts carry the VST3 bundle and nothing else — do not advertise a Standalone on those platforms without wiring the target into those jobs first.
- **Factory presets**: single source of truth in `Source/FactoryPresets.h` (baseline + table + `apply`/`isFactory`), consumed by the editor and tests. Do NOT re-hardcode preset lists in `PluginEditor`.
- **Windows installer**: Inno Setup (`installer/Distortion.iss`) → CommonFiles\VST3; built in CI. Bundles the VC++ 2015-2022 x64 runtime (CI downloads + Authenticode-verifies `vc_redist.x64.exe` into `installer/redist/`, gitignored) and installs it when the target machine's runtime is missing, older than 14.30, or has deleted DLLs; the plugin links the dynamic CRT so this is load-bearing on clean Windows 10 machines. Code-signing (Azure Trusted Signing) is wired but **inert until the `AZURE_*` repo secrets are set** — unsigned installer builds fine without them.
- **macOS installer**: `.pkg` (`installer/macos/build_pkg.sh` + `distribution.xml`) installing VST3 → `/Library/Audio/Plug-Ins/VST3`, AU → `/Library/Audio/Plug-Ins/Components`, Standalone → `/Applications`; built in CI. Developer ID code-signing + notarization (`installer/macos/entitlements.plist`) is wired but **inert until the `APPLE_*` repo secrets are set** — an unsigned `.pkg` builds fine without them (Gatekeeper warns until signed).
- **Linux**: tarball + `SHA256SUMS`.
- **Plain ZIPs (no installer)**: Windows ships `SledgeDistortion-<version>-Windows.zip` (VST3 bundle + `vc_redist.x64.exe` + `LICENSE.txt` + `THIRD-PARTY-NOTICES.md` + `README.txt`) and macOS ships `SledgeDistortion-<version>-macOS.zip` (VST3 + AU + `LICENSE.txt` + `THIRD-PARTY-NOTICES.md`); the Linux tarball carries the same two files alongside the bundle, each archive
  under a single versioned top-level directory so extracting cannot scatter loose
  files into the user's working directory. Extracting and copying a bundle does not trip SmartScreen the way running the unsigned installer does, so these are the low-friction route until signing lands (USE-49/USE-55). **The Windows ZIP must keep carrying the redist** — the plugin links the dynamic CRT, so a bare `.vst3` silently fails to load on clean Windows 10, which is the failure the installer's redist bundling was added to fix. Every release asset is version-stamped.
- **Release gate**: `RELEASE_CHECKLIST.md` (DoD gates, automated/manual). Manual host pass: `docs/DAW_QA_matrix.md`.
- **Publish**: tag `v*` → CI attaches the Windows installer, the Windows + macOS ZIPs, the Linux tarball, the macOS `.pkg`, and `SHA256SUMS` to the GitHub Release. `SHA256SUMS` is the single checksum list; per-file `.sha256` sidecars are deliberately not published.

## Diagnostics / Bug Reporting (USE-53)
Privacy-light, **opt-out** bug reporting. Lives in `Source/Diagnostics/` (namespace `diag`), owned by `PluginProcessor` so it works headless.
- **Flow**: an RT-safe non-finite probe at the top of `processBlock` feeds a lock-free `DiagnosticsSink`; reports (auto on anomalies at teardown, or user-initiated via the Settings "Report a Bug" dialog → `submitUserReport`) are written to a durable on-disk queue (`ReportStore`, `…/Monolit Beatz/Sledge Distortion/reports/*.json`) and drained on the **next launch** via a deferred, scan-safe timer → background `ReportSender` → HTTPS POST behind the `ITransport` interface (`CurlTransport` in production).
- **Consent**: default ON via a `std::atomic<bool>` on the processor, initialised from `settings.xml` (`bugReports` attribute — written by the editor, read by the processor) and flipped live by the Settings toggle; a one-time first-run notice explains it. `user` reports always send; `auto` reports are re-checked at drain and purged on revocation.
- **Settings path is single-sourced.** `diag::productDir()` is the *only* place the `Monolit Beatz/Sledge Distortion` app-data literals live; `PluginEditor::getDataRoot()` derives from it (applying the snapshot override) and `getSettingsFile()` / `diag::settingsFile()` must resolve to the same file. Deriving them separately is what lets consent silently stop persisting with nothing failing — `SettingsPersistenceTest` asserts the two agree, so keep new path helpers hanging off `getDataRoot()`. Note `getSettingsFile()` creates nothing: `saveSettings()` ensures the root exists before writing.
- **Multi-instance/process safe**: single-drainer `juce::InterProcessLock` + atomic `*.json`→`*.sending` claim; the queue is bounded (≤50 files / 30 days).
- **Payload**: minimal/anonymous (version, OS, host, SR/block, anomaly counts, random install-id, optional user text). The free-text message is the only PII vector — see `docs/PRIVACY.md`.
- **Build**: `JUCE_USE_CURL=1` + `JUCE_LOAD_CURL_SYMBOLS_LAZILY=1` on the **plugin target only**; tests/render/soak stay curl-free and use a `FakeTransport`. `DISTORTION_UNIT_TEST=1` gates appdata I/O + the drain out of unit-test builds. Set `DISTORTION_REPORT_ENDPOINT` (`Source/Diagnostics/ReportEndpoint.h`) before release.
- **Launch ping companion**: `diag::PingSender` (`Source/Diagnostics/PingSender.{h,cpp}`, `Ping.h`, `PingEndpoint.h`) sends an anonymous install/version/OS/host ping at most once per install per UTC day, POSTing to the Supabase Edge Function `ping` → locked-down `pings` table (mirrors the `report`/`reports` pair, same RLS-enabled-zero-policies lockdown, service-role-only insert). Shares the `bugReportsEnabled` consent flag — there is no separate toggle. Unlike reports, there is **no on-disk queue**: a single best-effort POST fires on a short-lived background thread, and the local date marker (`diag::lastPingFile()`) is only written on a successful (2xx) response, so a failed/offline attempt naturally retries next launch instead of needing retry logic. See `docs/PRIVACY.md` for the user-facing disclosure.

## UI Texture
Cracked-glass / diamond-plate artwork behind the editor. `Resources/ui_texture.png` is authored **1:1 against the expanded 960x564 layout** — metal at the title strip, red glass across the scope, metal with knob cutouts below — and is stored at exactly 960x564, the largest the window ever reaches (100% scale). Every scale setting therefore downsamples; nothing upscales.

**Alpha semantics matter here.** In the artwork the red *fill* is low-alpha but vivid (a≈25, RGB≈163,0,0) while the *cracks* are high-alpha but dark (a≈221, RGB≈33,0,0). Two consequences:
- Drawn **behind** an opaque backdrop the cracks disappear entirely, and lifting alpha globally hazes the whole band red instead of revealing them. `Oscilloscope::paint` therefore composites the texture **on top of** its own backdrop (still under the grid and trace), which is what lets a crack darken the grey it covers.
- The scope gets an alpha-curved copy (`kScopeAlphaGamma`, gamma > 1) that collapses the fill toward nothing while leaving the cracks intact. The title/knob bands use the **raw** artwork — they already match the design reference untouched.

`PluginEditor` owns both images, rescales them on size change (`rebuildScaledTextureIfNeeded`), and hands the scope layers sized to the *full editor*; the scope samples the slice under its own bounds, so neither side tracks offsets. When the scope is folded away the editor draws only the two metal bands, taken from the rows they occupy when expanded.

**EXTREME layer**: `ui_texture_extreme.png` is a full-canvas shatter — cracks cross the title strip and knob band as well as the scope — so `PluginEditor` owns the crossfade and pushes the eased value down to the scope (`setExtremeMix`) to keep them in step. It repaints only while the fade is moving; settled states cost nothing beyond the extra layer. The two alpha curves pull in **opposite directions on purpose**: `kScopeAlphaGamma` (> 1) curves the resting state *down* so the band stays clean, while `kExtremeAlphaGamma` (< 1) curves the engaged state *up* so it reads as an event. Applying the resting curve to the EXTREME layer guts it — the shatter's fill sits near alpha 25, which that curve drops to ~4, compositing at ~1.5%.

**Neon frame — keep it in `paintOverChildren()`.** The 1.5px border must be drawn *after* the children, not in `paint()`. Several components reach into it: the scope is positioned at `(int) 1.5f` == 1, i.e. one pixel inside the border, and the JUCE badge sits on the bottom-right corner. Drawn in `paint()` they overpaint it — the left stroke thinned to grey for the scope's whole height and the bottom/right stroke was blanked to black. Moving it back is a silent visual regression, so verify any change to it with `DistortionUiSnapshot` and check all four edges.

**Tunables**: `kTextureOpacity`, `kScopeAlphaGamma`, `kExtremeAlphaGamma` (`PluginEditor.cpp`); the scope backdrop gradient and `kExtremeFadeSeconds` (`PluginEditor.h`). `kTextureOpacity` governs all three bands: the editor applies it to the metal bands and hands it to the scope through `setTextureLayers`, so the scope band cannot drift from the rest when it is turned down. It stays declared in `PluginEditor.cpp` — the scope is passed the value rather than owning a second copy.

**Replacing the artwork**: keep the 960x564 canvas and registration; export flattened, since Photoshop blend modes do not survive a PNG (a Screen/Linear-Dodge layer baked into alpha is what produces the low-alpha fill described above). Source art is authored on a 1202x778 canvas and cropped to `y=71..777, x=0..1200` before scaling — both layers share that crop, which is what registers them. Verify a replacement with `DistortionUiSnapshot --extreme` rather than by eye.

## Project Layout
- `Source/`: PluginProcessor, PluginEditor, CustomKnob, `FactoryPresets.h`, and DSP logic (e.g. `ShapeFilter.h`, `SubGuardSplitter`, the in-place coefficient writers in `FilterCoeffs.h`).
- `Source/Nam/`: NAM profiles: `NamProfile` (model + `ResamplingIsland`), `ProfileLibrary` (the browsable, importing Profiles folder). See "NAM profile browser" above.
- `Source/Diagnostics/`: bug-reporting subsystem (`diag` namespace) — sink, store, composer, sender, transport. See "Diagnostics / Bug Reporting" above.
- `Source/Tools/`: headless console tools — `RenderHarness` (audition; `--signal 808|bassnote`, `--shape <v>`, `--preset <name>`; the drive's noise is seeded, `--seed <n>` default 1, so renders repeat byte for byte), `SoakHarness` (stress), `UiSnapshot` (editor screenshots).
- `Resources/`: embedded binary data — `ui_texture.png` / `ui_texture_extreme.png` (see "UI Texture" below), logo, Orbitron fonts.
- `installer/`: Inno Setup script for the Windows installer.
- `library/`: Shared utility code.
- `scripts/`: Python fix scripts and build utilities.
- `docs/`: Extra documentation, safety checklists, DAW QA matrix.
- `RELEASE_CHECKLIST.md`: gated Definition-of-Done for cutting a release.
- `Distortion.jucer`: Projucer project file.
