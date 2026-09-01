# Sledge Distortion — Source of Truth (AGENTS.md)

**External Documentation:**
- **Obsidian Vault:** [[03 Projects/Monolit Distortion]]
- **Projects HUB:** [[03 Projects/Projects HUB]]

## Project Overview
**Sledge Distortion** is a professional JUCE audio plugin by Monolit Beatz featuring multi-stage distortion processing, LA2A-style optical compression, and advanced signal processing. It ships as **VST3** on Windows and Linux, and as **VST3 + AU (Audio Unit) + Standalone** on macOS. See "Release & Packaging" below — the Standalone target builds everywhere but is only packaged by the macOS job.

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
- **CI/CD**: GitHub Actions for automated Windows/Linux/macOS release artifacts. Windows + Linux run on every push/PR; **macOS (`macos-14`) runs only on `v*` tags and manual dispatch** because macOS runners bill at 10× minutes (Linux 1×, Windows 2×). A `concurrency` group cancels superseded non-tag runs. Consequence: macOS breakage surfaces at tag time, so dispatch the workflow manually after touching `CMakeLists.txt`, `installer/macos/`, or any platform-conditional code.

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
1. **Input Stage** → Multimode Input Filter (HP/LP/BP, 20-20kHz, Butterworth TPT).
2. **Oversampling** → Polyphase IIR, selectable Off / 2x / 4x (4x default; `oversamplingMode` in `settings.xml`)
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
14. **Output Stage** → Final gain staging (±9dB).
15. **Global Mix** → dry/wet blend, dry phase-aligned through the matched `dryOversampling` instance (comb-free at every frequency).
16. **Output Limiter** → Safety limiter (-0.5dBFS), **after** the blend so the ceiling bounds the mixed output, not just the wet path. Always the last gain stage on both the active and bypass paths.

Steps 12–14 live inside `applyAutoGainAndISP`; 15–16 are in `processBlock` proper. `docs/Architecture Contract.md` carries the fully expanded ordering — keep the two in step.

## Engineering Standards
- **Memory**: No dynamic allocation in `processBlock`.
- **Thread Safety**: Use `std::atomic` for parameters and a lock-free `juce::AbstractFifo` (SPSC) for Scope data. The scope path holds **no lock** — the old `SpinLock` was removed to keep the audio thread contention-free, so do not reintroduce one.
- **Smoothing**: Always consume `SmoothedValue` in a **sample-first loop** to avoid buffer exhaustion.
- **Bypass**: True bypass when `distortion < 0.5%` and `compression OFF`. Skips oversampling and the whole distortion/compression chain. **The multimode input filter still runs** (only when `isInputFilterActive()`), so it works standalone; a filter at its transparent default leaves bypass bit-clean. The oversampler's reported latency is re-imposed via `bypassLatencyDelay` so toggling causes no timing jump. Full ordering in `docs/Architecture Contract.md`.
- **Namespace**: DSP constants are centralized in the `DSPConstants` namespace in `PluginProcessor.h`.

## Build & Test
- **Framework**: JUCE UnitTest runner.
- **Coverage**: 2580+ assertions (100% PASS RATE).
- **Categories**: DSP, Compression, LFO, ProcessBlock, ThreadSafety, SampleRate (44.1k-192k), GraphicEq (sweeps 22.05k-192k to cover the Nyquist band-drop), State I/O, FactoryPresets, Diagnostics, Settings Persistence.
- **Golden Audio**: Reference file comparison tests included.
- **Host validation**: CI gates on `pluginval --strictness-level 10` (Windows + Linux; xvfb on Linux).
- **Soak/stress**: `DistortionSoak` console tool (`Source/Tools/SoakHarness.cpp`) runs N instances faster-than-realtime, failing on non-finite output or RSS growth (DoD 24h/10+-instance gates). **Run it on Linux** — the leak half reads `/proc/self/statm`, so it is Linux-only. Elsewhere the harness reports `SOAK INCOMPLETE` and exits non-zero rather than `PASSED`: it checked finiteness but not memory growth, and only a `PASSED` earns the `RELEASE_CHECKLIST.md` §4 box. It used to print `PASSED` on Windows with the leak check silently skipped, which is how a half-run gate can look like a clean one.
- **UI/layout changes**: verify with `DistortionUiSnapshot` (`Source/Tools/UiSnapshot.cpp`), **not** by screenshotting the standalone — its menu bar and "audio input is muted" banner shift and clip the editor, pushing the knob row outside the window. The tool paints the real `PluginEditor` offscreen at exact geometry:
  ```bash
  cmake --build build --target DistortionUiSnapshot
  ./DistortionUiSnapshot --out shots              # all four window scales
  ./DistortionUiSnapshot --collapsed --out shots  # scope folded away
  ./DistortionUiSnapshot --extreme --out shots    # after the EXTREME crack fade settles
  ```
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
  source ones: Orbitron (SIL OFL 1.1, embedded via `BinaryData`) and the bundled
  Steinberg VST3 SDK (BSD-3-Clause) each require their terms to accompany every
  built artifact. `THIRD-PARTY-NOTICES.md` is that vehicle and ships with every
  release asset and both installers. Adding a distribution channel means carrying
  `LICENSE` and `THIRD-PARTY-NOTICES.md` on it too.
- Adding a dependency means adding its notice to `THIRD-PARTY-NOTICES.md` and
  checking it is GPL v3-compatible.

## Release & Packaging
- **Formats shipped**: **VST3 only on Windows + Linux**; VST3 + AU + Standalone on macOS (universal arm64 + x86_64). The Standalone target is built on macOS alone (`build-macos` is the only job passing `Distortion_Standalone`) and reaches users solely through the `.pkg`. Windows/Linux release artifacts carry the VST3 bundle and nothing else — do not advertise a Standalone on those platforms without wiring the target into those jobs first.
- **Factory presets**: single source of truth in `Source/FactoryPresets.h` (baseline + table + `apply`/`isFactory`), consumed by the editor and tests. Do NOT re-hardcode preset lists in `PluginEditor`.
- **Windows installer**: Inno Setup (`installer/Distortion.iss`) → CommonFiles\VST3; built in CI. Bundles the VC++ 2015-2022 x64 runtime (CI downloads + Authenticode-verifies `vc_redist.x64.exe` into `installer/redist/`, gitignored) and installs it when the target machine's runtime is missing, older than 14.30, or has deleted DLLs; the plugin links the dynamic CRT so this is load-bearing on clean Windows 10 machines. Code-signing (Azure Trusted Signing) is wired but **inert until the `AZURE_*` repo secrets are set** — unsigned installer builds fine without them.
- **macOS installer**: `.pkg` (`installer/macos/build_pkg.sh` + `distribution.xml`) installing VST3 → `/Library/Audio/Plug-Ins/VST3`, AU → `/Library/Audio/Plug-Ins/Components`, Standalone → `/Applications`; built in CI. Developer ID code-signing + notarization (`installer/macos/entitlements.plist`) is wired but **inert until the `APPLE_*` repo secrets are set** — an unsigned `.pkg` builds fine without them (Gatekeeper warns until signed).
- **Linux**: tarball + `SHA256SUMS`.
- **Plain ZIPs (no installer)**: Windows ships `SledgeDistortion-<version>-Windows.zip` (VST3 bundle + `vc_redist.x64.exe` + `LICENSE.txt` + `THIRD-PARTY-NOTICES.md` + `README.txt`) and macOS ships `SledgeDistortion-<version>-macOS.zip` (VST3 + AU + `LICENSE.txt` + `THIRD-PARTY-NOTICES.md`); the Linux tarball carries the same two licence files alongside the bundle. Extracting and copying a bundle does not trip SmartScreen the way running the unsigned installer does, so these are the low-friction route until signing lands (USE-49/USE-55). **The Windows ZIP must keep carrying the redist** — the plugin links the dynamic CRT, so a bare `.vst3` silently fails to load on clean Windows 10, which is the failure the installer's redist bundling was added to fix. Every release asset is version-stamped.
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
- `Source/`: PluginProcessor, PluginEditor, CustomKnob, `FactoryPresets.h`, and DSP logic.
- `Source/Diagnostics/`: bug-reporting subsystem (`diag` namespace) — sink, store, composer, sender, transport. See "Diagnostics / Bug Reporting" above.
- `Source/Tools/`: headless console tools — `RenderHarness` (audition), `SoakHarness` (stress), `UiSnapshot` (editor screenshots).
- `Resources/`: embedded binary data — `ui_texture.png` / `ui_texture_extreme.png` (see "UI Texture" below), logo, Orbitron fonts.
- `installer/`: Inno Setup script for the Windows installer.
- `library/`: Shared utility code.
- `scripts/`: Python fix scripts and build utilities.
- `docs/`: Extra documentation, safety checklists, DAW QA matrix.
- `RELEASE_CHECKLIST.md`: gated Definition-of-Done for cutting a release.
- `Distortion.jucer`: Projucer project file.
