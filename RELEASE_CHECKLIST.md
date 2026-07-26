# Release Checklist — Sledge Distortion (Monolit Beatz)

Gate for promoting a build from Release Candidate to a published, sellable
release. Derived from `library/Definition of done for vst plugins.md`. Check
every box on the `release/vX.Y-*` branch before tagging `vX.Y`.

**Legend:** 🤖 automated in CI · 🧪 automated, run locally · ✍️ manual

---

## 1. Technical baseline (automated)

- [x] 🤖 Unit suite green — `DistortionTests` (2400+ assertions) runs in CI on every push/PR.
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
      *Policy: feature work targets `main`/next minor; `release/vX.Y-*` takes only fixes.*
- [ ] ✍️ Algorithm validation — every clip type auditioned and signed off as musical.
- [x] 🧪 Clean Mode order (tone vs waveshaper) verified (`ProcessBlock`/golden tests).

## 4. Final validation ("the shipping test")

- [ ] 🧪 **Soak test** — N instances rendering for an extended run without RSS growth or non-finite output. *(See `DistortionSoak` harness — `Source/Tools/SoakHarness.cpp`.)*
- [ ] 🧪 **Multi-instance** — 10+ instances, independent state/RNG (spot-checked by `ThreadSafetyTests::testMultiInstanceIndependence`; full count via the soak harness).
- [ ] ✍️ **Host compatibility** — ≥3 DAWs per `docs/DAW_QA_matrix.md` (load, automate, save/restore, bypass).

## 5. Packaging & distribution (Windows + Linux + macOS)

- [x] 🤖 Format claims match the build (VST3 + Standalone everywhere; + AU on macOS; VST2 dropped).
- [x] 🤖 Installers wired — Inno (`installer/Distortion.iss`, Win) + tarball + `SHA256SUMS` (Linux) + `.pkg` (`installer/macos/build_pkg.sh`, macOS); built in CI on every run.
- [x] 🤖 **VC++ runtime bundled** — installer detects a missing/outdated/broken VC++ 2015-2022 x64 runtime and installs the bundled `vc_redist.x64.exe` (Authenticode-verified in CI); redist exit code checked, failure surfaces an error. Upgrade flow waits for the previous uninstall to complete and purges legacy `Monolit Distortion.vst3` bundles.
- [x] 🤖 Release-publish wired — Windows installer + Linux tarball + macOS `.pkg` + `SHA256SUMS` attached to the GitHub Release on tag.
- [x] 🤖 **macOS build + AU** — universal (arm64 + x86_64) VST3/AU/Standalone built in CI on `macos-14`; unit suite + VST3 pluginval strictness 10 + AU `auval` run there.
- [ ] 🤖 **Windows code-signing** — Azure Trusted Signing steps wired but *inert until the `AZURE_*` repo secrets are set* (needs an Azure Trusted Signing account + identity validation).
- [ ] 🤖 **macOS code-signing + notarization** — Developer ID codesign + `notarytool`/`stapler` steps wired but *inert until the `APPLE_*` repo secrets are set* (needs an Apple Developer account: Developer ID Application + Installer certs, an app-specific password, and the team ID). Until then macOS ships an unsigned `.pkg` (Gatekeeper right-click-open workaround).

## 6. Privacy & bug reporting (USE-53)

- [x] ✍️ **Report endpoint set** — `DISTORTION_REPORT_ENDPOINT` (`Source/Diagnostics/ReportEndpoint.h`, or a build define) points at the real receiver, **not** the `REPLACE-ME` placeholder. A release must not ship the placeholder. *(Confirmed 2026-07-04: live Supabase `report` function.)*
- [ ] 🧪 First-run notice shows exactly once; the **Settings consent toggle persists** across relaunch (written as `bugReports` in `settings.xml`, read by both editor and processor).
- [ ] 🧪 **Opt-out works** — with reporting off, no `auto` report is queued/sent; queued `auto` reports are purged unsent. "Report a Bug" still sends only on explicit Send.
- [ ] 🧪 Queue is **bounded** — a permanently-unreachable endpoint never grows the on-disk queue without limit (count + age caps in `ReportStore`).
- [ ] ✍️ **`docs/PRIVACY.md` published** and linked from the product/marketing page; the "don't include personal data" note is present in the in-app dialog.

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
iscc /DMyAppVersion=2.2.0 installer\Distortion.iss   # -> dist\SledgeDistortion-2.2.0-Windows.exe
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
