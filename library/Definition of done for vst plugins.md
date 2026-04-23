# Definition of Done: VST Plugin Release

This document defines the criteria for transitioning a VST plugin from the **Development/Experimental** phase to a **Release-Ready Product**.

## 1. Technical Baseline (Stability & Integrity)
- [ ] **Sample Rate Agnostic:** DSP remains stable and sounds consistent across all standard rates (44.1, 48, 88.2, 96, 176.4, 192 kHz).
- [ ] **Real-Time Safety:** No allocations (`new`/`malloc`), no file I/O, and no locking on the audio thread (verified with `RT_ASSERT_SCOPE`).
- [ ] **Denormal & NaN Protection:** Audio thread uses `ScopedNoDenormals` and includes explicit NaN/Inf checks at the end of the signal chain.
- [ ] **Latency Reporting:** Plugin correctly reports internal latency (e.g., from oversampling filters) to the host for PDC (Plugin Delay Compensation).
- [ ] **State Recall:** Every UI parameter is correctly saved and restored by the DAW.
- [ ] **Automation Stability:** All parameters can be automated without producing audible "zipper noise," clicks, or DSP crashes.

## 2. UX & Integration Baseline
- [ ] **Phase Alignment:** Global Wet/Dry mix is phase-coherent (fractional delay compensation is applied to the dry path if the wet path has latency).
- [ ] **Gain Staging:** Default settings provide a sane output level; switching algorithms (e.g., Clip Types) does not cause massive, unexpected volume jumps.
- [ ] **Bypass Behavior:** The plugin implements a "soft bypass" (crossfaded) to prevent pops when toggling the effect.
- [ ] **Visual Feedback:** All meters (Gain Reduction, Phase Correlation, Oscilloscope) are accurate and calibrated.
- [ ] **Preset Management:** A factory bank of at least 10-20 presets is included to demonstrate the range of the plugin.

## 3. Artistic & Curation Baseline (The "Freeze")
- [ ] **DSP Freeze:** No new algorithms or "just one more feature" additions.
- [ ] **Algorithm Validation:** All distortion modes are tuned and finalized (e.g., tube bias and tape hysteresis envelopes feel "musical").
- [ ] **Clean Mode Verification:** The order of processing (Tone vs. Waveshaper) is verified to provide the intended sonic character.

## 4. Final Validation (The "Shipping" Test)
- [ ] **Stress Test:** Plugin runs in a heavy session for 24+ hours without memory leaks or CPU spikes.
- [ ] **Multi-Instance Test:** 10+ instances of the plugin run simultaneously without interfering with each other's state or random generators.
- [ ] **Host Compatibility:** Verified in at least three major DAWs (e.g., Ableton Live, Reaper, Logic Pro).

---
*Note: Use this checklist to resist the "Endless Tweak" loop. Once these boxes are checked, the plugin is legally "Finished."*
