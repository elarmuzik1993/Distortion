# Final Limiter Ceiling Clamp Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make `applyFinalLimiter` enforce a strict -0.5 dBFS ceiling on every sample (including the first transient sample) by adding a memoryless, stereo-linked soft ceiling clamp after the envelope stage.

**Architecture:** The existing envelope limiter stays as the musical gain-reduction stage. A new backstop, inside the same per-sample loop and after the envelope multiply, re-measures the post-envelope stereo-linked peak and, when it exceeds a soft point just under the ceiling, applies a single tanh-shaped gain factor to both channels. The shaping function is transparent below the soft point and asymptotes to (never reaches) the ceiling, so output magnitude is strictly below -0.5 dBFS. The clamp is memoryless: no new runtime state, no `prepareToPlay` changes.

**Tech Stack:** C++17, JUCE 7, JUCE UnitTest runner, CMake.

**Spec:** `docs/superpowers/specs/2026-06-07-final-limiter-ceiling-clamp-design.md`

---

### Task 1: Add the soft-zone constant

**Files:**
- Modify: `Source/PluginProcessor.h:124` (inside the `DSPConstants` namespace, after `OUTPUT_LIMITER_KNEE_DB`)

- [ ] **Step 1: Add the constant**

In `Source/PluginProcessor.h`, the output limiter block currently reads:

```cpp
    // Output limiter (final safety, always-on, stereo-linked)
    constexpr float OUTPUT_LIMITER_THRESHOLD_DB = -0.5f;       // -0.5 dBFS ceiling (safe headroom)
    constexpr float OUTPUT_LIMITER_ATTACK_TIME_S = 0.0005f;    // 0.5ms attack (catch transients)
    constexpr float OUTPUT_LIMITER_RELEASE_TIME_S = 0.050f;    // 50ms release (preserve punch)
    constexpr float OUTPUT_LIMITER_KNEE_DB = 1.0f;             // 1dB soft knee (transparent onset)
```

Add one line after `OUTPUT_LIMITER_KNEE_DB`:

```cpp
    constexpr float OUTPUT_LIMITER_KNEE_DB = 1.0f;             // 1dB soft knee (transparent onset)
    constexpr float OUTPUT_LIMITER_CLAMP_SOFTNESS_DB = 1.0f;   // soft zone below ceiling for the hard backstop
```

- [ ] **Step 2: Verify it compiles**

Run:
```bash
cd /home/boris/Projects/Distortion-main && mkdir -p build && cd build && cmake .. -DCMAKE_BUILD_TYPE=Debug >/dev/null && cmake --build . --target DistortionTests -j$(nproc)
```
Expected: build succeeds (the constant is unused for now; `constexpr` causes no warning).

- [ ] **Step 3: Commit**

```bash
cd /home/boris/Projects/Distortion-main
git add Source/PluginProcessor.h
git commit -m "Add OUTPUT_LIMITER_CLAMP_SOFTNESS_DB constant for final limiter backstop"
```

---

### Task 2: Write the failing regression test

**Files:**
- Modify: `Source/Tests/DistortionTests.h:373` (declare `testFirstSampleCeiling`)
- Modify: `Source/Tests/DistortionTests.cpp:891` (register the test in `runTest`)
- Modify: `Source/Tests/DistortionTests.cpp` (add the test body in the `OutputLimiterTests Implementation` section)

This task isolates the exact bug: a hot first sample while the envelope is still at unity. It calls `applyFinalLimiter` directly (the test class is a `friend` of `PluginProcessor`) so no other DSP stage interferes.

- [ ] **Step 1: Declare the new test method**

In `Source/Tests/DistortionTests.h`, the `OutputLimiterTests` private section currently reads:

```cpp
private:
    void testThresholdEnforcement();
    void testTransparencyBelowThreshold();
    void testStereoLinking();
    void testSoftKnee();
    void testEnvelopeAttackRelease();
    void testStateReset();
    void testGlobalMixCeiling();
};
```

Add `testFirstSampleCeiling`:

```cpp
private:
    void testThresholdEnforcement();
    void testTransparencyBelowThreshold();
    void testStereoLinking();
    void testSoftKnee();
    void testEnvelopeAttackRelease();
    void testStateReset();
    void testGlobalMixCeiling();
    void testFirstSampleCeiling();
};
```

- [ ] **Step 2: Register the test in `runTest`**

In `Source/Tests/DistortionTests.cpp`, the end of `OutputLimiterTests::runTest()` reads:

```cpp
    beginTest("Ceiling Holds With Hot Dry Under Global Mix");
    testGlobalMixCeiling();
}
```

Add the new `beginTest` before the closing brace:

```cpp
    beginTest("Ceiling Holds With Hot Dry Under Global Mix");
    testGlobalMixCeiling();

    beginTest("First-Sample Transient Ceiling");
    testFirstSampleCeiling();
}
```

- [ ] **Step 3: Add the test body**

In `Source/Tests/DistortionTests.cpp`, in the `OutputLimiterTests Implementation` section (after the existing `testGlobalMixCeiling` implementation, before the next class's implementation section), add:

```cpp
void OutputLimiterTests::testFirstSampleCeiling()
{
    using namespace TestUtilities;

    PluginProcessor processor;
    processor.setRateAndBufferSizeDetails(44100.0, 512);
    processor.prepareToPlay(44100.0, 512);

    // Fresh state: envelope at unity (1.0). This is the worst case for the bug —
    // the envelope has not begun attacking, so the first sample is multiplied by
    // ~unity gain and (pre-fix) sails past the ceiling.
    const float ceiling = juce::Decibels::decibelsToGain(
        DSPConstants::OUTPUT_LIMITER_THRESHOLD_DB);

    // Build a buffer whose FIRST sample is a +6 dBFS impulse (2.0 linear) on both
    // channels, with the rest near silent so only the transient matters.
    juce::AudioBuffer<float> buffer(2, 512);
    buffer.clear();
    const float impulse = juce::Decibels::decibelsToGain(6.0f); // ~2.0 linear
    buffer.setSample(0, 0, impulse);
    buffer.setSample(1, 0, impulse);

    // Call the limiter directly (OutputLimiterTests is a friend of PluginProcessor).
    processor.applyFinalLimiter(buffer);

    // Every sample, especially sample 0, must be at or under the ceiling.
    // Tight tolerance: this is a real -0.5 dBFS guarantee, not the loose 0.5 dB
    // that testThresholdEnforcement allows for the settled envelope.
    const float tol = 1.0e-4f;
    bool ceilingHeld = true;
    float worst = 0.0f;
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        const float* data = buffer.getReadPointer(ch);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const float a = std::abs(data[i]);
            if (a > worst) worst = a;
            if (a > ceiling + tol) ceilingHeld = false;
        }
    }

    expect(ceilingHeld,
        "First-sample transient exceeded ceiling: peak " +
        juce::String(juce::Decibels::gainToDecibels(worst), 3) +
        " dB (ceiling " +
        juce::String(DSPConstants::OUTPUT_LIMITER_THRESHOLD_DB, 2) + " dB)");

    expect(!containsInvalidSamples(buffer), "Output contains NaN or Inf");
}
```

- [ ] **Step 4: Build and run the test — verify it FAILS**

Run:
```bash
cd /home/boris/Projects/Distortion-main/build && cmake --build . --target DistortionTests -j$(nproc) && ./DistortionTests_artefacts/Debug/DistortionTests 2>&1 | grep -A3 "First-Sample Transient Ceiling"
```
Expected: FAIL — the message shows a peak around +5.5 dB (the +6 dBFS impulse minus the tiny envelope step), well above the -0.5 dB ceiling. This confirms the test reproduces the bug.

- [ ] **Step 5: Commit the failing test**

```bash
cd /home/boris/Projects/Distortion-main
git add Source/Tests/DistortionTests.h Source/Tests/DistortionTests.cpp
git commit -m "Add failing regression test for first-sample limiter ceiling overshoot"
```

---

### Task 3: Implement the soft ceiling clamp

**Files:**
- Modify: `Source/PluginProcessor.cpp:1791-1796` (the per-sample channel-apply block inside `applyFinalLimiter`)

- [ ] **Step 1: Add the clamp after the envelope multiply**

In `Source/PluginProcessor.cpp`, the tail of the per-sample loop in `applyFinalLimiter` currently reads:

```cpp
        // Apply gain reduction to all channels (stereo-linked)
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            auto* channelData = buffer.getWritePointer(channel);
            channelData[sample] *= outputLimiterEnvelope;
        }
    }
}
```

Replace that block with the envelope apply followed by the stereo-linked soft clamp:

```cpp
        // Apply gain reduction to all channels (stereo-linked)
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            auto* channelData = buffer.getWritePointer(channel);
            channelData[sample] *= outputLimiterEnvelope;
        }

        // Hard backstop: the envelope has a finite attack, so the first samples
        // of a transient can still exceed the ceiling before gain reduction
        // catches up. Re-measure the post-envelope stereo-linked peak and apply
        // a memoryless soft clamp that is transparent below the soft point and
        // asymptotes to (never reaches) the ceiling — guaranteeing |out| < ceiling
        // on every sample, including sample 0.
        const float softPoint = thresholdLinear
            * juce::Decibels::decibelsToGain(-DSPConstants::OUTPUT_LIMITER_CLAMP_SOFTNESS_DB);

        float postPeak = 0.0f;
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            const float absValue = std::abs(buffer.getSample(channel, sample));
            if (absValue > postPeak)
                postPeak = absValue;
        }

        if (postPeak > softPoint)
        {
            const float range = thresholdLinear - softPoint; // > 0 (softness > 0)
            const float softened = softPoint
                + range * std::tanh((postPeak - softPoint) / range);
            const float clampFactor = softened / postPeak; // <= 1, stereo-linked

            for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            {
                auto* channelData = buffer.getWritePointer(channel);
                channelData[sample] *= clampFactor;
            }
        }
    }
}
```

Note: `thresholdLinear` is already computed at the top of `applyFinalLimiter`
(`Source/PluginProcessor.cpp:1738`), so it is in scope here.

- [ ] **Step 2: Build and run the new test — verify it PASSES**

Run:
```bash
cd /home/boris/Projects/Distortion-main/build && cmake --build . --target DistortionTests -j$(nproc) && ./DistortionTests_artefacts/Debug/DistortionTests 2>&1 | grep -A3 "First-Sample Transient Ceiling"
```
Expected: PASS — no overshoot reported.

- [ ] **Step 3: Run the full Output Limiter suite — verify no regressions**

Run:
```bash
cd /home/boris/Projects/Distortion-main/build && ./DistortionTests_artefacts/Debug/DistortionTests 2>&1 | grep -iA12 "Output Limiter"
```
Expected: all `Output Limiter` subtests PASS (Threshold Enforcement, Transparency Below Threshold, Stereo Linking, Soft Knee Behavior, Envelope Attack/Release, State Reset, Ceiling Holds With Hot Dry Under Global Mix, First-Sample Transient Ceiling). In particular, Transparency Below Threshold must still pass — the clamp is a no-op below the soft point.

- [ ] **Step 4: Commit**

```bash
cd /home/boris/Projects/Distortion-main
git add Source/PluginProcessor.cpp
git commit -m "Clamp final limiter output to a strict -0.5 dBFS ceiling after the envelope"
```

---

### Task 4: Full regression sweep

**Files:** none (verification only)

- [ ] **Step 1: Run the entire test suite**

Run:
```bash
cd /home/boris/Projects/Distortion-main/build && ./DistortionTests_artefacts/Debug/DistortionTests 2>&1 | tail -30
```
Expected: the runner reports all tests passed (the suite asserts a 100% pass rate; look for "All tests completed successfully" / zero failures).

- [ ] **Step 2: Confirm no failures**

Run:
```bash
cd /home/boris/Projects/Distortion-main/build && ./DistortionTests_artefacts/Debug/DistortionTests 2>&1 | grep -i "fail" || echo "NO FAILURES"
```
Expected: `NO FAILURES`.

---

## Notes for the implementer

- **Why direct `applyFinalLimiter` in the test:** routing an impulse through the
  full `processBlock` would smear it across oversampling/filters and not isolate
  the first-sample envelope bug. The test class is already declared
  `friend class OutputLimiterTests;` in `PluginProcessor.h`, so calling the
  private method directly is the established pattern for this suite.
- **No state, no `prepareToPlay` change:** the clamp is purely arithmetic on the
  current sample; do not add fields or coefficients.
- **Real-time safety:** no allocation, no locks added — consistent with the
  "no dynamic allocation in processBlock" rule in AGENTS.md.
- **Build reminder:** per AGENTS.md, never use `--target Distortion`; the test
  target is `DistortionTests`.
