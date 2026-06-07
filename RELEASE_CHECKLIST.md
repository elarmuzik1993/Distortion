# Release Checklist — Monolit Distortion

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
- [ ] ✍️ **Host compatibility** — load, automate, save/restore, bypass in ≥3 DAWs (e.g. Reaper, Ableton Live, Bitwig / Logic on macOS).

## 5. Packaging & distribution (resource-gated — see Open Questions)

- [x] 🤖 Format claims match the build (VST3 + Standalone; VST2 dropped).
- [ ] 🤖 Windows code-signing (Authenticode / Azure Trusted Signing) — *needs cert*.
- [ ] 🤖 macOS build + AU + Developer-ID sign + notarize + staple — *needs Apple Developer account*.
- [ ] 🤖 Installers — Inno (Win) / `.pkg` (mac) / tarball + `SHA256SUMS` (Linux).
- [ ] 🤖 Signed installers + checksums attached to the GitHub Release on tag.

---

## How to run the gated checks locally

```bash
# Unit suite
cmake --build build --target DistortionTests -j && \
  ./build/DistortionTests_artefacts/Debug/DistortionTests

# pluginval (Release build recommended; Debug jasserts abort on fuzzing)
cmake --build build-release --target Distortion_VST3 -j
pluginval --strictness-level 10 --validate \
  "build-release/Distortion_artefacts/Release/VST3/Monolit Distortion.vst3"

# Soak / multi-instance (10 instances, ~2 min)
cmake --build build --target DistortionSoak -j && \
  ./build/DistortionSoak_artefacts/Debug/DistortionSoak --instances 10 --seconds 120
```
