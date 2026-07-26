# Sledge Distortion — Source of Truth (AGENTS.md)

**External Documentation:**
- **Obsidian Vault:** [[03 Projects/Monolit Distortion]]
- **Projects HUB:** [[03 Projects/Projects HUB]]

## Project Overview
**Sledge Distortion** is a professional JUCE audio plugin by Monolit Beatz featuring multi-stage distortion processing, LA2A-style optical compression, and advanced signal processing. The plugin supports VST3 and Standalone formats.

## Key Specifications
- **DSP Specs**: See `DSP Architecture` section below.
- **UI Specs**: Professional custom rotary knobs (`CustomKnob`), real-time oscilloscope, and LA2A-style gain reduction meter.
- **Compact Window Mode** (USE-48): When the oscilloscope is disabled in Settings, the plugin window folds to a compact height (180px base at 100% scale). The Settings overlay stays compact: its rows live in a `SettingsContent` component inside a `juce::Viewport`, so the content scrolls (vertical neon scrollbar) and no rows are clipped at any window size or scale. LFO/Compression tabs auto-collapse on fold; re-expanding them in compact mode shows an `ExpansionBackdrop` panel (tight bounding box, dark bg + red border) that covers the parameter knobs while controls are tweaked.
- **Oscilloscope Toolbar Toggle**: A scope button in the title bar toggles compact ↔ full mode (independent of the Settings toggle — both stay in sync via `applyOscilloscopeEnabled`). The transition uses a **snap + alpha-fade** strategy to avoid X11 resize tearing on Linux: expand snaps the window to full size in a single atomic resize then fades the scope/XYMorphPad/PhaseCorrelationMeter alpha 0→1; collapse fades alpha 1→0 then snaps to compact. Animation runs on a dedicated 60 Hz `FoldAnimTimer` (vsync-aligned), `saveSettings()` is deferred via `MessageManager::callAsync`, and the scope's 30 Hz repaint timer only starts after the fade completes.
- **Scope Overlay Selector**: A single "Overlay" dropdown in the Settings INTERFACE section chooses which overlay owns the oscilloscope — **Off** / **XY Morph** / **Graphic EQ** — so only one intercepts the mouse at a time (replaces the old standalone XY Morph pill). State persists in `settings.xml` (`scopeOverlay` int: 0/1/2; the legacy `xyMorph` bool auto-migrates to mode 1). `applyScopeOverlayMode` + `updateScopeOverlays(alpha)` centralise visibility / mouse-interception / 30 Hz timers for both overlays, keyed off the scope's live visibility + fold-fade alpha. Visibility stays governed by the oscilloscope toggle.
  - **XY Morph** mode: dragging the invisible `XYMorphPad` morphs 6 distortion params via `morphDistortionParameters`.
  - Two toolbar buttons in the title bar (next to the scope/gear buttons) quick-toggle the overlays: **`XyButton`** (XY↔Off) and **`EqButton`** (EQ↔Off). Each lights up when its overlay is active and dims otherwise; they're mutually exclusive via the shared mode and stay in sync with the Settings selector through `applyScopeOverlayMode` (the same duplicate-toggle pattern the scope button uses). Toolbar order mirrors the dropdown: scope · XY · EQ.
  - **EQ bypass**: a power button in the overlay's top-left corner one-click toggles the `eqEnabled` param — the DSP ramps the applied gains to flat (click-free) while the drawn curve is kept, so it's a true A/B bypass rather than a destructive flatten. The curve dims and the label reads "(BYPASSED)" while off.
  - **Graphic EQ** mode: the `GraphicEqOverlay` lets you free-draw a magnitude curve on the scope (Catmull-Rom spline through 12 control points; double-click flattens). Each control point maps to one output-stage peaking band (`eqBand0..11` APVTS params, log-spaced ~30 Hz–16 kHz, ±12 dB). DSP is a base-rate `ProcessorDuplicator` peaking bank (`processGraphicEq`, inserted after the DC blocker, before the output-gain stage); coefficients are written in place via `writePeakFilterCoeffs` (mirrors JUCE `makePeakFilter`, no `processBlock` allocation) from per-band `SmoothedValue` gains. The whole bank bypasses when the curve is flat (all bands within `EQ_FLAT_EPS_DB` of 0), so a fresh instance is bit-transparent. A peaking biquad goes unstable once its centre reaches Nyquist (`alpha` turns negative and the poles leave the unit circle), so `prepareEqBands` marks bands at/above `EQ_MAX_FREQ_RATIO` (0.45) × base rate unusable — they get no coefficients, never process, and don't keep the bank awake. In practice the 16 kHz band drops out at base rates ≤32 kHz; at 44.1 kHz and above all 12 are live. The overlay is a UI-only view of the params — the EQ engages from the saved curve regardless of the selector, so presets always sound right; "bypass" = flatten.
- **LFO BPM Sync**: SYNC button in the LFO panel switches the rate knob from free-running Hz mode to BPM-locked division mode. The rate knob steps through 1/1 → 1/2 → 1/4 → 1/8 → 1/16 → 1/32 → 1/4T → 1/8T → 1/16T; the label below updates live to show the selected division. Falls back to 120 BPM when no host playhead is available.
- **LFO INV Toggle**: Inverts LFO polarity in both DSP (`lfoSign = -1`) and the arc visualizer (arc sweeps below the knob value instead of above).
- **CyclingComboBox** (`Source/CyclingComboBox.h`): Custom `juce::ComboBox` subclass — single-click cycles to next item (timer-debounced), double-click opens the full list. Used for clip type, LFO waveform/destination, compression ratio, and settings dropdowns.
- **Safety**: Includes `realtime-audio-safety-checklist.md` for thread safety and DSP best practices.
- **CI/CD**: GitHub Actions for automated Windows/Linux release artifacts.

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
1. **Input Stage** → Multimode Input Filter (HP/LP/BP, 20-20kHz, Butterworth TPT).
2. **Oversampling** → 4x Polyphase IIR
3. **Pre-Distortion Transient Tamer** → Hardcoded (1ms attack, 50ms release, 2.5:1 ratio, -12dB threshold)
4. **Sub Guard Band-Split** (Optional) → 50-200Hz crossover protecting low band from distortion.
5. **Distortion Stage** → 7 Professional Clip Types (Brutal Fuzz, Tube, Bit Crusher, Tape, Transformer, Diode, Decimator).
6. **Tone Filter & Waveshaper** → Order depends on "Clean Mode" toggle. Runs in oversampled domain.
7. **Auto-Gain Compensation** → RMS-based (±12dB).
8. **LA2A Compression** → Optical cell simulation in oversampled domain (Attack 10ms, Release 500ms, 2dB knee, Tube harmonics).
9. **Soft Clipper** → ISP protection at -0.3dBFS (oversampled domain).
10. **Sub Guard Recombine** → Phase-matched toneFilterLow applied to low band, then summed.
11. **Downsampling** → Return to original sample rate.
12. **DC Blocking** → One-pole (~3.5Hz cutoff).
13. **Graphic EQ** (Optional) → 12-band peaking bank at base rate (`processGraphicEq`); whole bank bypasses while the drawn curve is flat. Bands whose centre sits at/above `EQ_MAX_FREQ_RATIO` (0.45) × sample rate are dropped, so the 16 kHz band is inactive at base rates ≤32 kHz.
14. **Output Limiter** → Safety limiter (-0.5dBFS).
15. **Output Stage** → Final gain staging (±9dB).

## Engineering Standards
- **Memory**: No dynamic allocation in `processBlock`.
- **Thread Safety**: Use `std::atomic` for parameters and `SpinLock`/`AbstractFifo` for Scope data.
- **Smoothing**: Always consume `SmoothedValue` in a **sample-first loop** to avoid buffer exhaustion.
- **Bypass**: True bypass when `distortion < 0.5%` and `compression OFF`. Skips filters and oversampling.
- **Namespace**: DSP constants are centralized in the `DSPConstants` namespace in `PluginProcessor.h`.

## Build & Test
- **Framework**: JUCE UnitTest runner.
- **Coverage**: 2460+ assertions (100% PASS RATE).
- **Categories**: DSP, Compression, LFO, ProcessBlock, ThreadSafety, SampleRate (44.1k-192k), GraphicEq (sweeps 22.05k-192k to cover the Nyquist band-drop), State I/O, FactoryPresets.
- **Golden Audio**: Reference file comparison tests included.
- **Host validation**: CI gates on `pluginval --strictness-level 10` (Windows + Linux; xvfb on Linux).
- **Soak/stress**: `DistortionSoak` console tool (`Source/Tools/SoakHarness.cpp`) runs N instances faster-than-realtime, failing on non-finite output or RSS growth (DoD 24h/10+-instance gates).

## Release & Packaging
- **Formats shipped**: VST3 + Standalone, Windows + Linux. macOS deferred (see Linear USE-50).
- **Factory presets**: single source of truth in `Source/FactoryPresets.h` (baseline + table + `apply`/`isFactory`), consumed by the editor and tests. Do NOT re-hardcode preset lists in `PluginEditor`.
- **Windows installer**: Inno Setup (`installer/Distortion.iss`) → CommonFiles\VST3; built in CI. Bundles the VC++ 2015-2022 x64 runtime (CI downloads + Authenticode-verifies `vc_redist.x64.exe` into `installer/redist/`, gitignored) and installs it when the target machine's runtime is missing, older than 14.30, or has deleted DLLs; the plugin links the dynamic CRT so this is load-bearing on clean Windows 10 machines. Code-signing (Azure Trusted Signing) is wired but **inert until the `AZURE_*` repo secrets are set** — unsigned installer builds fine without them.
- **Linux**: tarball + `SHA256SUMS`.
- **Release gate**: `RELEASE_CHECKLIST.md` (DoD gates, automated/manual). Manual host pass: `docs/DAW_QA_matrix.md`.
- **Publish**: tag `v*` → CI attaches installer + tarball + `SHA256SUMS` to the GitHub Release.

## Diagnostics / Bug Reporting (USE-53)
Privacy-light, **opt-out** bug reporting. Lives in `Source/Diagnostics/` (namespace `diag`), owned by `PluginProcessor` so it works headless.
- **Flow**: an RT-safe non-finite probe at the top of `processBlock` feeds a lock-free `DiagnosticsSink`; reports (auto on anomalies at teardown, or user-initiated via the Settings "Report a Bug" dialog → `submitUserReport`) are written to a durable on-disk queue (`ReportStore`, `…/Monolit Beatz/Sledge Distortion/reports/*.json`) and drained on the **next launch** via a deferred, scan-safe timer → background `ReportSender` → HTTPS POST behind the `ITransport` interface (`CurlTransport` in production).
- **Consent**: default ON via a `std::atomic<bool>` on the processor, initialised from `settings.xml` (`bugReports` attribute — written by the editor, read by the processor) and flipped live by the Settings toggle; a one-time first-run notice explains it. `user` reports always send; `auto` reports are re-checked at drain and purged on revocation.
- **Multi-instance/process safe**: single-drainer `juce::InterProcessLock` + atomic `*.json`→`*.sending` claim; the queue is bounded (≤50 files / 30 days).
- **Payload**: minimal/anonymous (version, OS, host, SR/block, anomaly counts, random install-id, optional user text). The free-text message is the only PII vector — see `docs/PRIVACY.md`.
- **Build**: `JUCE_USE_CURL=1` + `JUCE_LOAD_CURL_SYMBOLS_LAZILY=1` on the **plugin target only**; tests/render/soak stay curl-free and use a `FakeTransport`. `DISTORTION_UNIT_TEST=1` gates appdata I/O + the drain out of unit-test builds. Set `DISTORTION_REPORT_ENDPOINT` (`Source/Diagnostics/ReportEndpoint.h`) before release.

## Project Layout
- `Source/`: PluginProcessor, PluginEditor, CustomKnob, `FactoryPresets.h`, and DSP logic.
- `Source/Diagnostics/`: bug-reporting subsystem (`diag` namespace) — sink, store, composer, sender, transport. See "Diagnostics / Bug Reporting" above.
- `Source/Tools/`: headless console tools — `RenderHarness` (audition), `SoakHarness` (stress).
- `installer/`: Inno Setup script for the Windows installer.
- `library/`: Shared utility code.
- `scripts/`: Python fix scripts and build utilities.
- `docs/`: Extra documentation, safety checklists, DAW QA matrix.
- `RELEASE_CHECKLIST.md`: gated Definition-of-Done for cutting a release.
- `Distortion.jucer`: Projucer project file.
