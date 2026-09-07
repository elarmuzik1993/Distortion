# Release Checklist — Sledge Distortion (Monolit Beatz)

Gate for promoting a build from Release Candidate to a published, sellable
release. Derived from `library/Definition of done for vst plugins.md`. Check
every box on the `ship/vX.Y` branch before tagging `vX.Y`.

**Branch name is load-bearing.** `cmake/GitVersion.cmake` matches `^ship/(vX.Y)$`
exactly, so a release branch on that name advertises `vX.Y-<sha>` in the UI
before the tag exists. Any other name (`release/v2.3-rc1`, `ship/v2.3-rc1`)
fails the match and the build falls back to *last tag + commit count* — i.e. the
previous version's number on the new release's binary.

**Legend:** 🤖 automated in CI · 🧪 automated, run locally · ✍️ manual

---

## 1. Technical baseline (automated)

- [x] 🤖 Unit suite green — `DistortionTests` (2580+ assertions) runs in CI on every push/PR. *(Last local run 2026-08-10: 2584 assertions, all passed.)*
- [x] 🤖 **pluginval strictness 10** passes on Windows + Linux — `Validate with pluginval` step in `.github/workflows/build.yml`.
- [x] 🧪 Sample-rate sweep 44.1k–192k (covered by `SampleRateTests`).
- [x] 🧪 No heap allocation in `processBlock` (covered by `RTAllocationGuard*` tests).
- [x] 🧪 NaN/Inf cannot propagate (covered by `processBlock` guards + tests).
- [x] 🧪 Latency reported for PDC (`setLatencySamples`; `DryWetAlignmentTests`).
- [x] 🧪 State recall round-trips, incl. legacy migration (`StateIOTests`).

## 2. UX & integration baseline

- [x] 🧪 Phase-coherent global mix (`DryWetAlignmentTests`).
- [x] 🧪 Clip-type level matching — no volume jumps switching algorithms (`NormalizationTests`).
- [x] ✍️ Bypass toggle is click-free (verify by ear at the bypass<->active boundary; see follow-up in memory `project_state`).
- [x] ✍️ Meters (GR, phase correlation, scope) read correctly in a DAW.
- [x] 🧪 Factory bank 10–20 presets, all valid + finite (`FactoryPresetTests`; 16 presets).

## 3. Artistic / curation ("the freeze")

- [ ] ✍️ **DSP freeze** — no new algorithms/features on this branch; bugfixes only.
      *Policy: feature work targets `main`/next minor; `ship/vX.Y` takes only fixes.*
- [ ] ✍️ Algorithm validation — every clip type auditioned and signed off as musical.
- [x] 🧪 Clean Mode order (tone vs waveshaper) verified (`ProcessBlock`/golden tests).

## 4. Final validation ("the shipping test")

- [x] 🧪 **Soak test** — N instances rendering for an extended run without RSS growth or non-finite output. *(See `DistortionSoak` harness — `Source/Tools/SoakHarness.cpp`.)* **Last run (2026-08-10, Linux):** 12 instances × 300 s → all output finite, RSS 14 MB → 14 MB (0 MB growth), `SOAK PASSED`. Re-run before tagging.
      ⚠️ **Run this on Linux.** The leak half reads `/proc/self/statm`, so it only works there. On Windows/macOS the harness now reports `SOAK INCOMPLETE` and exits non-zero — it checked finiteness but *not* memory growth. Only a `SOAK PASSED` earns this box.
- [x] 🧪 **Multi-instance** — 10+ instances, independent state/RNG (spot-checked by `ThreadSafetyTests::testMultiInstanceIndependence`; full count via the soak harness — 12 instances in the run above).
- [ ] ✍️ **Host compatibility** — ≥3 DAWs per `docs/DAW_QA_matrix.md` (load, automate, save/restore, bypass).

## 5. Packaging & distribution (Windows + Linux + macOS)

- [x] 🤖 Format claims match the build — **VST3 only on Windows + Linux**; VST3 + AU + Standalone on macOS; VST2 dropped. Corrected 2026-08-09: this box previously read "VST3 + Standalone everywhere", which no job has ever produced (`Distortion_Standalone` is built only in `build-macos`). Re-check the user-facing claims in `INSTALL.md` and any marketing copy whenever this line changes.
- [x] 🤖 Installers wired — Inno (`installer/Distortion.iss`, Win) + tarball + `SHA256SUMS` (Linux) + `.pkg` (`installer/macos/build_pkg.sh`, macOS); built in CI on every run.
- [x] 🤖 **VC++ runtime bundled** — installer detects a missing/outdated/broken VC++ 2015-2022 x64 runtime and installs the bundled `vc_redist.x64.exe` (Authenticode-verified in CI); redist exit code checked, failure surfaces an error. Upgrade flow waits for the previous uninstall to complete and purges legacy `Monolit Distortion.vst3` bundles.
- [x] 🤖 Release-publish wired — Windows installer + Windows ZIP + macOS ZIP + Linux tarball + macOS `.pkg` + `SHA256SUMS` attached to the GitHub Release on tag. All assets version-stamped; `SHA256SUMS` is the only checksum file published.
- [ ] ✍️ **Windows ZIP still carries `vc_redist.x64.exe`** — the ZIP is the recommended download while unsigned, and the plugin links the dynamic CRT, so a bare `.vst3` fails to load on clean Windows 10. Unzip the built artifact and confirm the redist, `LICENSE.txt` and `THIRD-PARTY-NOTICES.md` are inside before tagging.
      ⚠️ **Unticked 2026-09-01: the packaging step changed.** The relicense to GPL v3
      added `THIRD-PARTY-NOTICES.md` to the Windows ZIP, the Linux tarball and the
      macOS ZIP, and rewrote the ZIP's `README.txt` footer. The verification below
      attests to an artifact built before that change, so it no longer covers what
      CI now produces. Re-verify against a fresh artifact.
      **Previously verified 2026-08-10** against the CI artifact from run [`31417710123`](https://github.com/elarmuzik1993/Distortion/actions/runs/31417710123) (`main` @ `331bc24`): `SledgeDistortion-2.3.0-Windows.zip`, 29.4 MB — VST3 bundle 7.2 MB, `vc_redist.x64.exe` 24.4 MB, `LICENSE.txt`, `README.txt` (BOM-free). Archive paths use forward slashes (0 backslash entries), so the bundle extracts as a bundle. **Re-verify if anything in the packaging step changes before the tag** — this attests to one specific artifact, not to the step in perpetuity.
- [x] 🤖 **macOS build + AU** — universal (arm64 + x86_64) VST3/AU/Standalone built in CI on `macos-14`; unit suite + VST3 pluginval strictness 10 + AU `auval` run there. **Runs on every push and PR** alongside Windows and Linux, so it gates PRs like the other two and needs no manual dispatch before tagging. **Strengthened 2026-09-06** (`b5de46e`): it was previously tag-and-dispatch-only to avoid the 10× minutes multiplier, but that multiplier bills *metered* minutes and standard runners are free on public repos, so the gate cost macOS coverage and saved nothing — a release tag used to be the first thing to exercise the universal binary and `auval`.
- [ ] 🤖 **Windows code-signing** — Azure Trusted Signing steps wired but *inert until the `AZURE_*` repo secrets are set* (needs an Azure Trusted Signing account + identity validation). **Accepted for v2.3: ships unsigned** — SmartScreen shows an "unrecognised app" warning (More info → Run anyway). Documented in the README and CHANGELOG "Known limitations". Not a tag blocker for this release; revisit when the account exists.
- [ ] 🤖 **macOS code-signing + notarization** — Developer ID codesign + `notarytool`/`stapler` steps wired but *inert until the `APPLE_*` repo secrets are set* (needs an Apple Developer account: Developer ID Application + Installer certs, an app-specific password, and the team ID). Until then macOS ships an unsigned `.pkg` (Gatekeeper right-click-open workaround). **Accepted for v2.3: ships unsigned**, documented in the README and CHANGELOG "Known limitations". Not a tag blocker for this release; revisit when the Apple Developer account exists.

- [ ] ✍️ **Licence terms reach the user on every channel** — Sledge Distortion is
      GPL v3, and Orbitron (SIL OFL 1.1), the Steinberg VST3 SDK, FLAC, Ogg Vorbis,
      libpng, IJG libjpeg, zlib and the AudioUnit SDK all oblige us to reproduce
      their terms in *binary* distributions, which the GPL `LicenseFile` alone does
      not do. A CMake post-build step embeds `LICENSE` + `THIRD-PARTY-NOTICES.md`
      in each bundle's `Contents/Resources`, so every channel inherits them; the
      archives also carry top-level copies. Each packaging step asserts this and
      fails the build if a file is missing.
      **Still needs a human:** open one built artifact per channel and confirm the
      files are present and readable. The CI assertions cover presence, not
      legibility, and the macOS path has not run since these changes — dispatch
      the workflow before ticking this.

## 6. Privacy & bug reporting (USE-53)

- [x] ✍️ **Report endpoint set** — `DISTORTION_REPORT_ENDPOINT` (`Source/Diagnostics/ReportEndpoint.h`, or a build define) points at the real receiver, **not** the `REPLACE-ME` placeholder. A release must not ship the placeholder. *(Confirmed 2026-07-04: live Supabase `report` function.)*
- [x] 🧪 **Settings consent toggle persists** across relaunch — written as `bugReports` in `settings.xml`, read back by both editor and processor (`SettingsPersistenceTest`: full round-trip, the processor-side `parseXML`/`getBoolAttribute` read, and a path-contract assertion that `PluginEditor::getSettingsFile() == diag::settingsFile()`). Both sides now derive that path from `diag::productDir()`, so they cannot silently fork.
- [ ] ✍️ First-run notice shows **exactly once** — needs an eyes-on pass (fresh app-data folder → notice appears; relaunch → it does not). The persistence half is covered above; the one-shot trigger keys off `settings.xml` not existing at editor construction.
- [x] 🧪 **Opt-out works** — with reporting off, no `auto` report is queued (`DiagProcessorTest`: "consent OFF queues no auto report at teardown", plus the ON mirror) and queued `auto` reports are purged unsent at drain (`DiagSenderTest`). "Report a Bug" still sends only on explicit Send.
- [x] 🧪 Queue is **bounded** — a permanently-unreachable endpoint never grows the on-disk queue without limit; count cap (≤50) and age cap (30 days) both covered by `DiagReportStoreTest`.
- [x] ✍️ **`docs/PRIVACY.md` published** and linked from the README ("Privacy & bug reporting") and the marketing note; the "don't include personal data" note is present in the in-app dialog (`PluginEditor.h`).

---

## How to run the gated checks locally

```bash
# Unit suite
cmake --build build --target DistortionTests -j && \
  ./build/DistortionTests_artefacts/Debug/DistortionTests

# pluginval (Release build recommended; Debug jasserts abort on fuzzing)
cmake --build build-release --target Distortion_VST3 -j
pluginval --strictness-level 10 --validate \
  "build-release/Distortion_artefacts/Release/VST3/Sledge Distortion.vst3"

# Soak / multi-instance (10 instances, ~2 min)
cmake --build build --target DistortionSoak -j && \
  ./build/DistortionSoak_artefacts/Debug/DistortionSoak --instances 10 --seconds 120

# Windows installer (on Windows, with Inno Setup installed).
# First place vc_redist.x64.exe (https://aka.ms/vs/17/release/vc_redist.x64.exe)
# at installer\redist\ or the build skips the VC++ runtime safety net (warning).
iscc /DMyAppVersion=2.3.0 installer\Distortion.iss   # -> dist\SledgeDistortion-2.3.0-Windows.exe
```

## Activating Windows code-signing

The CI signing steps stay inert until these repo secrets exist (Settings →
Secrets → Actions), at which point every build signs the VST3 + installer:

`AZURE_TENANT_ID`, `AZURE_CLIENT_ID`, `AZURE_CLIENT_SECRET`,
`AZURE_TS_ENDPOINT`, `AZURE_TS_ACCOUNT`, `AZURE_TS_CERT_PROFILE`

Prerequisite: an [Azure Trusted Signing](https://learn.microsoft.com/azure/trusted-signing/)
account with a validated identity and a certificate profile.

## Activating macOS code-signing + notarization

The `build-macos` job's sign/notarize steps stay inert until these repo secrets
exist, at which point every build signs the plugins + `.pkg` and notarizes:

- `APPLE_CERT_BASE64` — base64 of a `.p12` containing **both** the *Developer ID
  Application* and *Developer ID Installer* certs (+ private keys).
- `APPLE_CERT_PASSWORD` — the `.p12` export password.
- `APPLE_SIGN_IDENTITY` — e.g. `Developer ID Application: Monolit Beatz (TEAMID)`.
- `APPLE_INSTALLER_IDENTITY` — e.g. `Developer ID Installer: Monolit Beatz (TEAMID)`.
- `APPLE_TEAM_ID`, `APPLE_ID`, `APPLE_APP_PASSWORD` — notarization
  (`notarytool`) credentials; `APPLE_APP_PASSWORD` is an app-specific password.

Prerequisite: a paid [Apple Developer Program](https://developer.apple.com/programs/)
membership (for Developer ID certs + notarization).
