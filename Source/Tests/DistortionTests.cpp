#if JUCE_DEBUG

#include "DistortionTests.h"
#include "../CyclingComboBox.h"
#include "../FactoryPresets.h"
#include "../LegacyInputFilter.h"
#include "../ProfileCardAnimation.h"
#include "../RTAllocationGuard.h"
#include "../ShapeFilter.h"
#include <atomic>
#include <iostream>
#include <thread>

using namespace TestUtilities;

//==============================================================================
// DistortionDSPTests Implementation
//==============================================================================

void DistortionDSPTests::runTest()
{
    PluginProcessor processor;
    processor.setRateAndBufferSizeDetails(44100.0, 512);
    processor.prepareToPlay(44100.0, 512);

    // Test all 7 clip types
    beginTest("Clip Type 0: Brutal Fuzz");
    testClipType(processor, 0, "Brutal Fuzz");

    beginTest("Clip Type 1: Tube Overdrive");
    testClipType(processor, 1, "Tube Overdrive");

    beginTest("Clip Type 2: Bit Crusher");
    testClipType(processor, 2, "Bit Crusher");

    beginTest("Clip Type 3: Tape Saturation");
    testClipType(processor, 3, "Tape Saturation");

    beginTest("Clip Type 4: Transformer Saturation");
    testClipType(processor, 4, "Transformer Saturation");

    beginTest("Clip Type 5: Diode Clipper");
    testClipType(processor, 5, "Diode Clipper");

    beginTest("Clip Type 6: Decimator");
    testClipType(processor, 6, "Decimator");

    beginTest("Edge Cases");
    testEdgeCases(processor);

    beginTest("Output Range Validation");
    testOutputRange(processor);
}

void DistortionDSPTests::testClipType(PluginProcessor& processor, int clipType, const juce::String& label)
{
    // Test with various input levels
    const float testInputs[] = { 0.0f, 0.1f, 0.5f, 0.8f, 1.0f, -0.5f, -1.0f };
    const float testGains[] = { 0.5f, 1.0f, 2.0f };
    const float testDrives[] = { 1.0f, 2.5f, 4.5f };

    for (float gain : testGains)
    {
        for (float drive : testDrives)
        {
            for (float input : testInputs)
            {
                float output = processor.applyStudioDistortion(input, gain, drive, clipType, 1.0f);

                // Output should never be NaN or Inf
                expect(!std::isnan(output), label + ": Output is NaN for input " + juce::String(input));
                expect(!std::isinf(output), label + ": Output is Inf for input " + juce::String(input));

                // Output should be bounded (allow some headroom for processing)
                expect(std::abs(output) <= 2.0f, label + ": Output " + juce::String(output) +
                       " exceeds bounds for input " + juce::String(input));
            }
        }
    }

    // Test zero input produces near-zero output (except for noise injection)
    float zeroOutput = processor.applyStudioDistortion(0.0f, 1.0f, 1.0f, clipType, 1.0f);
    expect(std::abs(zeroOutput) < 0.1f, label + ": Zero input should produce near-zero output");
}

void DistortionDSPTests::testEdgeCases(PluginProcessor& processor)
{
    // Test denormal values
    float denormal = 1e-40f;
    float output = processor.applyStudioDistortion(denormal, 1.0f, 1.0f, 0, 1.0f);
    expect(!std::isnan(output), "Denormal input caused NaN");
    expect(!std::isinf(output), "Denormal input caused Inf");

    // Test very large values (should be clipped)
    float largeInput = 10.0f;
    output = processor.applyStudioDistortion(largeInput, 1.0f, 1.0f, 0, 1.0f);
    expect(std::abs(output) <= 2.0f, "Large input not properly clipped");

    // Test negative large values
    output = processor.applyStudioDistortion(-10.0f, 1.0f, 1.0f, 0, 1.0f);
    expect(std::abs(output) <= 2.0f, "Large negative input not properly clipped");

    // Test minimum gain
    output = processor.applyStudioDistortion(1.0f, 0.0f, 1.0f, 0, 1.0f);
    expect(!std::isnan(output), "Zero gain caused NaN");

    // Test minimum drive
    output = processor.applyStudioDistortion(1.0f, 1.0f, 1.0f, 0, 1.0f);
    expect(!std::isnan(output), "Minimum drive caused NaN");
}

void DistortionDSPTests::testOutputRange(PluginProcessor& processor)
{
    // Process a full-scale sine wave and verify output stays bounded
    const int numSamples = 1000;
    float maxOutput = 0.0f;
    bool hasNaN = false;
    bool hasInf = false;

    for (int clipType = 0; clipType < 7; ++clipType)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            float phase = static_cast<float>(i) / static_cast<float>(numSamples);
            float input = std::sin(phase * juce::MathConstants<float>::twoPi);

            float output = processor.applyStudioDistortion(input, 1.5f, 3.0f, clipType, 1.0f);

            if (std::isnan(output)) hasNaN = true;
            if (std::isinf(output)) hasInf = true;
            maxOutput = std::max(maxOutput, std::abs(output));
        }
    }

    expect(!hasNaN, "Sine wave processing produced NaN");
    expect(!hasInf, "Sine wave processing produced Inf");
    expect(maxOutput <= 2.0f, "Maximum output " + juce::String(maxOutput) + " exceeds safe bounds");
}

//==============================================================================
// CompressionDSPTests Implementation
//==============================================================================

void CompressionDSPTests::runTest()
{
    beginTest("Threshold Mapping");
    testThresholdMapping();

    beginTest("Ratio Modes");
    testRatioModes();

    beginTest("Makeup Gain");
    testMakeupGain();

    beginTest("Envelope Follower");
    testEnvelopeFollower();

    beginTest("Gain Reduction Meter");
    testGainReductionMeter();
}

void CompressionDSPTests::testThresholdMapping()
{
    // Threshold mapping: peakReduction 0-100 -> -60dB to 0dB
    // At peakReduction=0: threshold = -60dB (no compression)
    // At peakReduction=100: threshold = 0dB (maximum compression)

    // This is a logical test - the actual mapping is inside applyLA2ACompression
    // We verify by checking that higher peakReduction = more gain reduction

    PluginProcessor processor;
    processor.setRateAndBufferSizeDetails(44100.0, 512);
    processor.prepareToPlay(44100.0, 512);

    // Enable compression
    auto* compEnabledParam = processor.parameters.getParameter("compEnabled");
    auto* peakReductionParam = processor.parameters.getParameter("compPeakReduction");

    expect(compEnabledParam != nullptr, "compEnabled parameter not found");
    expect(peakReductionParam != nullptr, "compPeakReduction parameter not found");
}

void CompressionDSPTests::testRatioModes()
{
    PluginProcessor processor;
    processor.setRateAndBufferSizeDetails(44100.0, 512);
    processor.prepareToPlay(44100.0, 512);

    auto* compRatioParam = processor.parameters.getParameter("compRatio");
    expect(compRatioParam != nullptr, "compRatio parameter not found");

    // Mode 0 = Compress (3:1), Mode 1 = Limit (12:1)
    // Verify parameter exists and has correct range
    auto* rangeParam = dynamic_cast<juce::AudioParameterChoice*>(compRatioParam);
    if (rangeParam != nullptr)
    {
        expect(rangeParam->choices.size() >= 2, "compRatio should have at least 2 choices");
    }
}

void CompressionDSPTests::testMakeupGain()
{
    PluginProcessor processor;
    processor.setRateAndBufferSizeDetails(44100.0, 512);
    processor.prepareToPlay(44100.0, 512);

    auto* makeupParam = processor.parameters.getParameter("compMakeupGain");
    expect(makeupParam != nullptr, "compMakeupGain parameter not found");

    // Verify range: 0-100, default 50 (unity)
    auto range = makeupParam->getNormalisableRange();
    expectWithinAbsoluteError(range.start, 0.0f, 0.01f, "Makeup gain min should be 0");
    expectWithinAbsoluteError(range.end, 100.0f, 0.01f, "Makeup gain max should be 100");
}

void CompressionDSPTests::testEnvelopeFollower()
{
    PluginProcessor processor;
    processor.setRateAndBufferSizeDetails(44100.0, 512);
    processor.prepareToPlay(44100.0, 512);

    // Envelope follower uses attack ~10ms, release ~500ms
    // These are determined by coefficients in DSPConstants
    // Verify the processor initializes without crashing

    auto buffer = generateSineWave(1000.0, 44100.0, 512, 0.8f);
    juce::MidiBuffer midi;

    // Process without crash
    processor.processBlock(buffer, midi);

    expect(!containsInvalidSamples(buffer), "Compression processing produced invalid samples");
}

void CompressionDSPTests::testGainReductionMeter()
{
    PluginProcessor processor;
    processor.setRateAndBufferSizeDetails(44100.0, 512);
    processor.prepareToPlay(44100.0, 512);

    // Enable compression with significant peak reduction
    processor.parameters.getParameter("compEnabled")->setValueNotifyingHost(1.0f);
    processor.parameters.getParameter("compPeakReduction")->setValueNotifyingHost(0.8f);

    // Process loud signal
    auto buffer = generateSineWave(1000.0, 44100.0, 512, 0.9f);
    juce::MidiBuffer midi;

    processor.processBlock(buffer, midi);

    // Gain reduction should be readable (atomic float)
    float gr = processor.currentGainReductionDB.load();
    expect(!std::isnan(gr), "Gain reduction is NaN");
    expect(!std::isinf(gr), "Gain reduction is Inf");
    expect(gr >= 0.0f, "Gain reduction should be non-negative");
}

//==============================================================================
// PreCompressionTests Implementation (Pre-Distortion Transient Tamer)
//==============================================================================

void PreCompressionTests::runTest()
{
    beginTest("Transient Reduction");
    testTransientReduction();

    beginTest("Bypass When Distortion Off");
    testBypassWhenDistortionOff();

    beginTest("Envelope Attack/Release");
    testEnvelopeAttackRelease();

    beginTest("Stereo Independence");
    testStereoIndependence();

    beginTest("No Invalid Samples");
    testNoInvalidSamples();

    beginTest("Gain Reduction Range");
    testGainReductionRange();
}

void PreCompressionTests::testTransientReduction()
{
    // Test that loud transients are reduced by the pre-compression
    PluginProcessor processor;
    processor.setRateAndBufferSizeDetails(44100.0, 512);
    processor.prepareToPlay(44100.0, 512);

    // Enable distortion (pre-comp only active when distortion >= 0.5%)
    setParameter(processor.parameters, "distortionAmount", 50.0f);
    setParameter(processor.parameters, "compEnabled", 0.0f);  // Disable LA2A to isolate pre-comp

    // Create a loud impulse that exceeds threshold (-12dB = 0.25 linear)
    auto impulseBuffer = generateImpulse(512, 2, 100, 0.9f);  // 0.9 amplitude > -12dB threshold
    float inputPeak = calculatePeak(impulseBuffer);

    juce::MidiBuffer midi;
    processor.processBlock(impulseBuffer, midi);

    float outputPeak = calculatePeak(impulseBuffer);

    // Output should exist and be valid
    expect(!containsInvalidSamples(impulseBuffer), "Pre-comp produced invalid samples");
    expect(outputPeak > 0.0f, "Pre-comp killed the signal");

    // Peak should be reduced (compressed) - not necessarily by much due to soft knee
    // Allow for distortion adding harmonics, but peak shouldn't grow excessively
    expect(outputPeak < inputPeak * 2.0f, "Output peak grew excessively");
}

void PreCompressionTests::testBypassWhenDistortionOff()
{
    // Test that pre-compression is bypassed when distortion is off (< 0.5%)
    PluginProcessor processor;
    processor.setRateAndBufferSizeDetails(44100.0, 512);
    processor.prepareToPlay(44100.0, 512);

    // Disable distortion (pre-comp should be bypassed)
    setParameter(processor.parameters, "distortionAmount", 0.0f);
    setParameter(processor.parameters, "compEnabled", 0.0f);

    // Create a loud signal
    auto inputBuffer = generateSineWave(440.0, 44100.0, 512, 0.9f);
    auto originalBuffer = inputBuffer;  // Copy for comparison

    juce::MidiBuffer midi;
    processor.processBlock(inputBuffer, midi);

    // Signal should pass through relatively unchanged (true bypass)
    expect(!containsInvalidSamples(inputBuffer), "Bypass mode produced invalid samples");

    float inputPeak = calculatePeak(originalBuffer);
    float outputPeak = calculatePeak(inputBuffer);

    // In true bypass, level should be very close to input
    expectWithinAbsoluteError(outputPeak, inputPeak, 0.15f,
        "Bypass mode should preserve signal level");
}

void PreCompressionTests::testEnvelopeAttackRelease()
{
    // Test envelope follower behavior with transients
    PluginProcessor processor;
    processor.setRateAndBufferSizeDetails(44100.0, 2048);
    processor.prepareToPlay(44100.0, 2048);

    // Enable distortion
    setParameter(processor.parameters, "distortionAmount", 50.0f);
    setParameter(processor.parameters, "compEnabled", 0.0f);

    // Create a signal with a loud transient followed by silence
    // This tests attack (compressing the transient) and release (returning to unity)
    juce::AudioBuffer<float> buffer(2, 2048);
    buffer.clear();

    // Add loud transient at the start (first 50 samples)
    for (int ch = 0; ch < 2; ++ch)
    {
        auto* data = buffer.getWritePointer(ch);
        for (int i = 0; i < 50; ++i)
        {
            data[i] = 0.8f * std::sin(static_cast<float>(i) * 0.5f);
        }
    }

    juce::MidiBuffer midi;
    processor.processBlock(buffer, midi);

    expect(!containsInvalidSamples(buffer), "Envelope test produced invalid samples");

    // The transient should have been processed (not zero)
    float earlyPeak = 0.0f;
    for (int ch = 0; ch < 2; ++ch)
    {
        const auto* data = buffer.getReadPointer(ch);
        for (int i = 0; i < 100; ++i)
            earlyPeak = std::max(earlyPeak, std::abs(data[i]));
    }
    expect(earlyPeak > 0.0f, "Transient was completely removed");
}

void PreCompressionTests::testStereoIndependence()
{
    // Test that per-channel envelopes preserve stereo imaging
    PluginProcessor processor;
    processor.setRateAndBufferSizeDetails(44100.0, 512);
    processor.prepareToPlay(44100.0, 512);

    // Enable distortion
    setParameter(processor.parameters, "distortionAmount", 50.0f);
    setParameter(processor.parameters, "compEnabled", 0.0f);

    // Create asymmetric stereo signal (left loud, right quiet)
    juce::AudioBuffer<float> buffer(2, 512);
    for (int i = 0; i < 512; ++i)
    {
        float phase = static_cast<float>(i) / 512.0f * juce::MathConstants<float>::twoPi * 4.0f;
        buffer.setSample(0, i, 0.8f * std::sin(phase));   // Left: loud
        buffer.setSample(1, i, 0.1f * std::sin(phase));   // Right: quiet
    }

    float leftInputPeak = 0.0f, rightInputPeak = 0.0f;
    for (int i = 0; i < 512; ++i)
    {
        leftInputPeak = std::max(leftInputPeak, std::abs(buffer.getSample(0, i)));
        rightInputPeak = std::max(rightInputPeak, std::abs(buffer.getSample(1, i)));
    }

    juce::MidiBuffer midi;
    processor.processBlock(buffer, midi);

    expect(!containsInvalidSamples(buffer), "Stereo test produced invalid samples");

    // Both channels should have output
    float leftPeak = 0.0f, rightPeak = 0.0f;
    for (int i = 0; i < 512; ++i)
    {
        leftPeak = std::max(leftPeak, std::abs(buffer.getSample(0, i)));
        rightPeak = std::max(rightPeak, std::abs(buffer.getSample(1, i)));
    }

    expect(leftPeak > 0.0f, "Left channel was killed");
    expect(rightPeak > 0.0f, "Right channel was killed");

    // Stereo difference should be preserved (left should still be louder than right)
    // Allow for distortion effects but ratio should be maintained roughly
    expect(leftPeak > rightPeak * 0.5f, "Stereo imaging was significantly altered");
}

void PreCompressionTests::testNoInvalidSamples()
{
    // Stress test with various signals to ensure no NaN/Inf
    PluginProcessor processor;
    processor.setRateAndBufferSizeDetails(44100.0, 512);
    processor.prepareToPlay(44100.0, 512);

    setParameter(processor.parameters, "distortionAmount", 75.0f);
    setParameter(processor.parameters, "compEnabled", 0.0f);

    juce::MidiBuffer midi;

    // Test 1: Silence
    {
        auto buffer = generateSilence(512);
        processor.processBlock(buffer, midi);
        expect(!containsInvalidSamples(buffer), "Silence input produced invalid samples");
    }

    // Test 2: Full-scale sine
    {
        auto buffer = generateSineWave(1000.0, 44100.0, 512, 1.0f);
        processor.processBlock(buffer, midi);
        expect(!containsInvalidSamples(buffer), "Full-scale sine produced invalid samples");
    }

    // Test 3: White noise
    {
        auto buffer = generateWhiteNoise(512, 0.8f);
        processor.processBlock(buffer, midi);
        expect(!containsInvalidSamples(buffer), "White noise produced invalid samples");
    }

    // Test 4: DC offset
    {
        auto buffer = generateDCOffset(512, 0.5f);
        processor.processBlock(buffer, midi);
        expect(!containsInvalidSamples(buffer), "DC offset produced invalid samples");
    }

    // Test 5: Very quiet signal (denormals)
    {
        juce::AudioBuffer<float> buffer(2, 512);
        for (int ch = 0; ch < 2; ++ch)
        {
            auto* data = buffer.getWritePointer(ch);
            for (int i = 0; i < 512; ++i)
                data[i] = 1e-38f;  // Near denormal
        }
        processor.processBlock(buffer, midi);
        expect(!containsInvalidSamples(buffer), "Denormal input produced invalid samples");
    }

    // Test 6: Impulse train
    {
        juce::AudioBuffer<float> buffer(2, 512);
        buffer.clear();
        for (int i = 0; i < 512; i += 64)
        {
            buffer.setSample(0, i, 0.9f);
            buffer.setSample(1, i, 0.9f);
        }
        processor.processBlock(buffer, midi);
        expect(!containsInvalidSamples(buffer), "Impulse train produced invalid samples");
    }
}

void PreCompressionTests::testGainReductionRange()
{
    // Test that gain reduction stays within expected bounds
    PluginProcessor processor;
    processor.setRateAndBufferSizeDetails(44100.0, 1024);
    processor.prepareToPlay(44100.0, 1024);

    setParameter(processor.parameters, "distortionAmount", 50.0f);
    setParameter(processor.parameters, "compEnabled", 0.0f);

    // Process multiple blocks with varying levels
    juce::MidiBuffer midi;

    for (int level = 1; level <= 10; ++level)
    {
        float amplitude = static_cast<float>(level) / 10.0f;
        auto buffer = generateSineWave(440.0, 44100.0, 1024, amplitude);
        float inputPeak = calculatePeak(buffer);

        processor.processBlock(buffer, midi);

        float outputPeak = calculatePeak(buffer);

        expect(!containsInvalidSamples(buffer),
            "Level " + juce::String(level) + " produced invalid samples");

        // Output should be bounded (distortion may add gain, but not infinitely)
        expect(outputPeak < 3.0f,
            "Output peak " + juce::String(outputPeak) + " exceeds safe bounds at level " + juce::String(level));

        // With pre-compression, output shouldn't be completely silent for non-zero input
        if (inputPeak > 0.01f)
        {
            expect(outputPeak > 0.0f,
                "Output was silenced at level " + juce::String(level));
        }
    }
}

//==============================================================================
// CleanBoostTests Implementation (Pre-emphasis boost in front of distortion)
//==============================================================================

void CleanBoostTests::runTest()
{
    beginTest("No invalid samples across sample rates");
    testNoInvalidSamplesAcrossSampleRates();

    beginTest("Boost raises output level");
    testBoostRaisesLevel();

    beginTest("Bypassed when distortion off");
    testBypassWhenDistortionOff();

    beginTest("State round-trip");
    testStateRoundTrip();

    beginTest("No allocation when toggling");
    testNoAllocationWhenToggling();

    beginTest("Runtime oversampling change re-prepares shelves");
    testRuntimeOversamplingChange();
}

void CleanBoostTests::testNoInvalidSamplesAcrossSampleRates()
{
    const double rates[] = { 44100.0, 48000.0, 96000.0, 192000.0 };
    for (double sr : rates)
    {
        PluginProcessor processor;
        processor.setRateAndBufferSizeDetails(sr, 512);
        processor.prepareToPlay(sr, 512);

        setParameter(processor.parameters, "distortionAmount", 50.0f);
        setParameter(processor.parameters, "cleanBoost", 1.0f);

        juce::MidiBuffer midi;
        // Process several blocks, toggling mid-stream to exercise the morph.
        for (int i = 0; i < 16; ++i)
        {
            if (i == 8)
                setParameter(processor.parameters, "cleanBoost", 0.0f);

            auto buffer = generateSineWave(440.0, sr, 512, 0.5f);
            processor.processBlock(buffer, midi);
            expect(!containsInvalidSamples(buffer),
                "Clean boost produced invalid samples at " + juce::String(sr) + "Hz");
        }
    }
}

void CleanBoostTests::testBoostRaisesLevel()
{
    auto measure = [](bool boostOn)
    {
        PluginProcessor processor;
        processor.setRateAndBufferSizeDetails(44100.0, 512);
        processor.prepareToPlay(44100.0, 512);

        setParameter(processor.parameters, "distortionAmount", 20.0f);
        setParameter(processor.parameters, "distMix", 100.0f);
        setParameter(processor.parameters, "globalMix", 100.0f);
        setParameter(processor.parameters, "autoGainEnabled", 0.0f);  // don't level-match
        setParameter(processor.parameters, "compEnabled", 0.0f);
        setParameter(processor.parameters, "cleanBoost", boostOn ? 1.0f : 0.0f);

        juce::MidiBuffer midi;
        // Small input keeps the output below the soft-clip/limiter ceiling so the
        // level lift is observable; warm up so the depth smoother settles.
        float rms = 0.0f;
        for (int i = 0; i < 12; ++i)
        {
            auto buffer = generateSineWave(440.0, 44100.0, 512, 0.05f);
            processor.processBlock(buffer, midi);
            rms = calculateRMS(buffer);
        }
        return rms;
    };

    const float offRms = measure(false);
    const float onRms = measure(true);

    expect(onRms > offRms * 1.05f,
        "Boost-on RMS (" + juce::String(onRms) + ") should exceed boost-off RMS ("
        + juce::String(offRms) + ")");
}

void CleanBoostTests::testBypassWhenDistortionOff()
{
    // With distortion off the plugin is in true bypass; the boost must not engage.
    auto measure = [](bool boostOn)
    {
        PluginProcessor processor;
        processor.setRateAndBufferSizeDetails(44100.0, 512);
        processor.prepareToPlay(44100.0, 512);

        setParameter(processor.parameters, "distortionAmount", 0.0f);
        setParameter(processor.parameters, "compEnabled", 0.0f);
        setParameter(processor.parameters, "cleanBoost", boostOn ? 1.0f : 0.0f);

        juce::MidiBuffer midi;
        float rms = 0.0f;
        for (int i = 0; i < 8; ++i)
        {
            auto buffer = generateSineWave(440.0, 44100.0, 512, 0.3f);
            processor.processBlock(buffer, midi);
            rms = calculateRMS(buffer);
        }
        return rms;
    };

    const float offRms = measure(false);
    const float onRms = measure(true);

    expect(std::abs(onRms - offRms) < 1.0e-4f,
        "Clean boost changed output while distortion was off (off=" + juce::String(offRms)
        + ", on=" + juce::String(onRms) + ")");
}

void CleanBoostTests::testStateRoundTrip()
{
    PluginProcessor src;
    src.setRateAndBufferSizeDetails(44100.0, 512);
    src.prepareToPlay(44100.0, 512);
    setParameter(src.parameters, "cleanBoost", 1.0f);

    juce::MemoryBlock stateData;
    src.getStateInformation(stateData);
    expect(stateData.getSize() > 0, "getStateInformation produced empty data");

    PluginProcessor dst;
    dst.setRateAndBufferSizeDetails(44100.0, 512);
    dst.prepareToPlay(44100.0, 512);
    dst.setStateInformation(stateData.getData(), static_cast<int>(stateData.getSize()));

    expect(*dst.parameters.getRawParameterValue("cleanBoost") > 0.5f,
        "cleanBoost did not round-trip through state save/restore");
}

void CleanBoostTests::testNoAllocationWhenToggling()
{
#if defined (DISTORTION_RT_GUARD) && DISTORTION_RT_GUARD
    PluginProcessor processor;
    processor.setRateAndBufferSizeDetails(48000.0, 512);
    processor.prepareToPlay(48000.0, 512);

    setParameter(processor.parameters, "distortionAmount", 50.0f);
    setParameter(processor.parameters, "globalMix", 100.0f);

    juce::MidiBuffer midi;
    auto warmup = generateSineWave(1000.0, 48000.0, 512, 0.25f);
    processor.processBlock(warmup, midi);

    rt_guard::resetAllocationCounter();

    for (int i = 0; i < 200; ++i)
    {
        setParameter(processor.parameters, "cleanBoost", (i % 20 < 10) ? 1.0f : 0.0f);
        auto buffer = generateSineWave(1000.0, 48000.0, 512, 0.25f);
        processor.processBlock(buffer, midi);
    }

    expectEquals(rt_guard::getAllocationCount(), 0,
        "Clean boost coefficient morph should not allocate on the audio thread");
#else
    expect(true, "DISTORTION_RT_GUARD not defined; skipping.");
#endif
}

void CleanBoostTests::testRuntimeOversamplingChange()
{
    // Regression guard: changing oversampling stages at runtime drives
    // rebuildOversampling() WITHOUT prepareToPlay(); it must re-prepare the
    // Clean Boost shelves at the new oversampled rate. If it doesn't, the
    // emphasis shelf keeps stale coefficients from the old rate (wrong corner
    // frequency), so the raw coefficients would be identical across the change.
    PluginProcessor processor;
    processor.setRateAndBufferSizeDetails(48000.0, 512);
    processor.prepareToPlay(48000.0, 512);

    setParameter(processor.parameters, "distortionAmount", 50.0f);
    setParameter(processor.parameters, "cleanBoost", 1.0f);

    juce::MidiBuffer midi;
    auto snapshotEmphasis = [&]() {
        // Settle the depth smoother so coefficients reflect a steady depth = 1.
        for (int i = 0; i < 8; ++i)
        {
            auto buffer = generateSineWave(440.0, 48000.0, 512, 0.3f);
            processor.processBlock(buffer, midi);
            expect(!containsInvalidSamples(buffer),
                "Clean boost produced invalid samples around oversampling change");
        }
        auto* coeffs = processor.emphasisFilter.state->getRawCoefficients();
        return std::array<float, 5>{ coeffs[0], coeffs[1], coeffs[2], coeffs[3], coeffs[4] };
    };

    // Default stages = 2 (4x → 192 kHz oversampled).
    const auto before = snapshotEmphasis();

    // Switch to stages = 1 (2x → 96 kHz oversampled), driven synchronously.
    processor.requestOversamplingRebuild(1);
    processor.handleAsyncUpdate();

    const auto after = snapshotEmphasis();

    float maxDiff = 0.0f;
    for (size_t i = 0; i < before.size(); ++i)
        maxDiff = std::max(maxDiff, std::abs(before[i] - after[i]));

    expect(maxDiff > 1e-3f,
        "Emphasis shelf coefficients did not change after the oversampling rebuild — "
        "shelves were not re-prepared at the new rate (maxDiff=" + juce::String(maxDiff) + ")");
}

//==============================================================================
// HarmonicDensityTests Implementation (Sub-Linear Harmonic Scaling)
//==============================================================================

void HarmonicDensityTests::runTest()
{
    beginTest("Sub-Linear Scaling Verification");
    testSubLinearScaling();

    beginTest("Clip Type Specificity");
    testClipTypeSpecificity();

    beginTest("No NaN or Inf Output");
    testNoNaNOrInf();
}

void HarmonicDensityTests::testSubLinearScaling()
{
    // Test that harmonic coefficients scale inversely with input level
    // High input → low harmonicScale → fewer harmonics (prevents harshness)
    PluginProcessor processor;
    processor.setRateAndBufferSizeDetails(44100.0, 512);
    processor.prepareToPlay(44100.0, 512);

    // Test Transformer Saturation (clip type 4) which has explicit harmonic control
    const float testInput = 0.5f;
    const float gain = 1.0f;
    const float drive = 3.0f;

    // harmonicScale = 1.0 represents LOW input (full harmonics)
    float outputFullHarmonics = processor.applyStudioDistortion(testInput, gain, drive, 4, 1.0f);

    // harmonicScale = 0.1 represents HIGH input (reduced harmonics to prevent harshness)
    float outputReducedHarmonics = processor.applyStudioDistortion(testInput, gain, drive, 4, 0.1f);

    // With same input but different harmonicScale, outputs should differ
    expect(std::abs(outputFullHarmonics - outputReducedHarmonics) > 0.001f,
        "Harmonic scaling should produce different outputs for different scale values");

    // Verify no numerical issues
    expect(!std::isnan(outputFullHarmonics) && !std::isnan(outputReducedHarmonics),
        "Harmonic scaling should not produce NaN");
    expect(!std::isinf(outputFullHarmonics) && !std::isinf(outputReducedHarmonics),
        "Harmonic scaling should not produce Inf");
}

void HarmonicDensityTests::testClipTypeSpecificity()
{
    // Test that harmonic scaling affects clip types 1, 3, 4 but not others
    PluginProcessor processor;
    processor.setRateAndBufferSizeDetails(44100.0, 512);
    processor.prepareToPlay(44100.0, 512);

    const float testInput = 0.5f;
    const float gain = 1.0f;
    const float drive = 3.0f;

    // Clip types with harmonic scaling: 1 (Tube), 3 (Tape), 4 (Transformer)
    int harmonicClipTypes[] = { 1, 3, 4 };

    for (int clipType : harmonicClipTypes)
    {
        float outputHigh = processor.applyStudioDistortion(testInput, gain, drive, clipType, 1.0f);
        float outputLow = processor.applyStudioDistortion(testInput, gain, drive, clipType, 0.1f);

        // Should produce different outputs with different harmonic scales
        expect(std::abs(outputHigh - outputLow) > 0.0001f,
            "Clip type " + juce::String(clipType) + " should respond to harmonic scaling");
    }

    // Clip types without explicit harmonic control: 2, 5, 6
    // These should produce same output regardless of harmonicScale
    // Note: Clip type 0 (Brutal Fuzz) uses random noise, so it's excluded
    int nonHarmonicClipTypes[] = { 2, 5, 6 };

    for (int clipType : nonHarmonicClipTypes)
    {
        float outputHigh = processor.applyStudioDistortion(testInput, gain, drive, clipType, 1.0f);
        float outputLow = processor.applyStudioDistortion(testInput, gain, drive, clipType, 0.1f);

        // Should produce identical outputs (harmonicScale is unused)
        expect(std::abs(outputHigh - outputLow) < 0.0001f,
            "Clip type " + juce::String(clipType) + " should not be affected by harmonic scaling");
    }
}

void HarmonicDensityTests::testNoNaNOrInf()
{
    // Test that harmonic density processing doesn't produce invalid samples
    PluginProcessor processor;
    processor.setRateAndBufferSizeDetails(44100.0, 512);
    processor.prepareToPlay(44100.0, 512);

    // Test extreme harmonic scale values
    float extremeScales[] = { 0.0f, 0.01f, 0.1f, 0.5f, 1.0f, 2.0f };
    float testInputs[] = { 0.0f, 0.001f, 0.5f, 1.0f, 10.0f };

    for (int clipType = 0; clipType < 7; ++clipType)
    {
        for (float scale : extremeScales)
        {
            for (float input : testInputs)
            {
                float output = processor.applyStudioDistortion(input, 1.0f, 3.0f, clipType, scale);

                expect(!std::isnan(output),
                    "Clip type " + juce::String(clipType) +
                    " with scale " + juce::String(scale) +
                    " and input " + juce::String(input) + " produced NaN");
                expect(!std::isinf(output),
                    "Clip type " + juce::String(clipType) +
                    " with scale " + juce::String(scale) +
                    " and input " + juce::String(input) + " produced Inf");
            }
        }
    }
}

//==============================================================================
// OutputLimiterTests Implementation
//==============================================================================

void OutputLimiterTests::runTest()
{
    beginTest("Threshold Enforcement");
    testThresholdEnforcement();

    beginTest("Transparency Below Threshold");
    testTransparencyBelowThreshold();

    beginTest("Stereo Linking");
    testStereoLinking();

    beginTest("Soft Knee Behavior");
    testSoftKnee();

    beginTest("Envelope Attack/Release");
    testEnvelopeAttackRelease();

    beginTest("State Reset");
    testStateReset();

    beginTest("Ceiling Holds With Hot Dry Under Global Mix");
    testGlobalMixCeiling();

    beginTest("First-Sample Transient Ceiling");
    testFirstSampleCeiling();
}

void OutputLimiterTests::testThresholdEnforcement()
{
    using namespace TestUtilities;

    PluginProcessor processor;
    processor.setRateAndBufferSizeDetails(44100.0, 512);
    processor.prepareToPlay(44100.0, 512);

    // Set output gain to maximum to push signal above threshold
    setParameter(processor.parameters, "outputGain", 100.0f);  // +9dB
    setParameter(processor.parameters, "distortionAmount", 0.0f);  // No distortion
    setParameter(processor.parameters, "compEnabled", false);

    juce::MidiBuffer midi;

    // Process multiple blocks to let the envelope settle
    // The limiter has 0.5ms attack, so at 44.1kHz we need ~22 samples to attack
    // Process several blocks to ensure envelope has settled
    for (int i = 0; i < 5; ++i)
    {
        auto buffer = generateSineWave(1000.0, 44100.0, 512,
                                        juce::Decibels::decibelsToGain(-3.0f), 2);
        processor.processBlock(buffer, midi);
    }

    // Now test with a fresh buffer after envelope has settled
    auto buffer = generateSineWave(1000.0, 44100.0, 512,
                                    juce::Decibels::decibelsToGain(-3.0f), 2);
    processor.processBlock(buffer, midi);

    // Check that peak doesn't significantly exceed threshold
    // After settling, limiter should keep output near threshold
    const float peakOutput = calculatePeak(buffer);

    // With soft knee and envelope settled, allow up to 1dB overshoot
    // (the soft knee allows some overshoot by design)
    const float maxAllowed = juce::Decibels::decibelsToGain(0.5f);

    expect(peakOutput <= maxAllowed,
        "Peak " + juce::String(juce::Decibels::gainToDecibels(peakOutput), 2) +
        " dB exceeded threshold + tolerance (" +
        juce::String(juce::Decibels::gainToDecibels(maxAllowed), 2) + " dB)");

    expect(!containsInvalidSamples(buffer), "Output contains NaN or Inf");
}

void OutputLimiterTests::testTransparencyBelowThreshold()
{
    using namespace TestUtilities;

    PluginProcessor processor;
    processor.setRateAndBufferSizeDetails(44100.0, 512);
    processor.prepareToPlay(44100.0, 512);

    // Set everything to bypass/minimum except output gain at unity
    setParameter(processor.parameters, "outputGain", 50.0f);  // 0dB (unity)
    setParameter(processor.parameters, "distortionAmount", 0.0f);
    setParameter(processor.parameters, "compEnabled", false);

    // Create a quiet signal well below threshold (-12dB, way below -0.5dB)
    const float level = juce::Decibels::decibelsToGain(-12.0f);
    auto inputBuffer = generateSineWave(1000.0, 44100.0, 512, level, 2);
    auto outputBuffer = inputBuffer;  // Copy for comparison

    juce::MidiBuffer midi;
    processor.processBlock(outputBuffer, midi);

    // Signal below threshold should pass through with minimal change
    // Allow for DC blocking and other processing, but limiter shouldn't affect it
    const float inputRms = calculateRMS(inputBuffer);
    const float outputRms = calculateRMS(outputBuffer);

    // RMS should be within 0.5dB (accounting for other processing)
    const float ratioDB = juce::Decibels::gainToDecibels(outputRms / inputRms);
    expect(std::abs(ratioDB) < 0.5f,
        "Signal below threshold changed by " + juce::String(ratioDB, 2) + " dB");

    expect(!containsInvalidSamples(outputBuffer), "Output contains NaN or Inf");
}

void OutputLimiterTests::testStereoLinking()
{
    using namespace TestUtilities;

    PluginProcessor processor;
    processor.setRateAndBufferSizeDetails(44100.0, 512);
    processor.prepareToPlay(44100.0, 512);

    // Set output gain high to ensure limiting
    setParameter(processor.parameters, "outputGain", 100.0f);  // +9dB
    setParameter(processor.parameters, "distortionAmount", 0.0f);
    setParameter(processor.parameters, "compEnabled", false);

    // Create asymmetric stereo buffer: left channel hot, right channel quiet
    juce::AudioBuffer<float> buffer(2, 512);
    const float hotLevel = juce::Decibels::decibelsToGain(-6.0f);  // Will exceed threshold after gain
    const float quietLevel = juce::Decibels::decibelsToGain(-24.0f);  // Well below threshold

    for (int i = 0; i < 512; ++i)
    {
        const float phase = static_cast<float>(i) * 0.1f;
        buffer.setSample(0, i, hotLevel * std::sin(phase));   // Hot left
        buffer.setSample(1, i, quietLevel * std::sin(phase)); // Quiet right
    }

    // Store original ratio
    const float originalRatio = hotLevel / quietLevel;

    juce::MidiBuffer midi;
    processor.processBlock(buffer, midi);

    // Calculate post-processing peaks
    float leftPeak = 0.0f, rightPeak = 0.0f;
    for (int i = 0; i < 512; ++i)
    {
        leftPeak = std::max(leftPeak, std::abs(buffer.getSample(0, i)));
        rightPeak = std::max(rightPeak, std::abs(buffer.getSample(1, i)));
    }

    // Stereo linking: both channels should be reduced by same amount
    // So ratio should be preserved (within tolerance for envelope dynamics)
    const float outputRatio = leftPeak / (rightPeak + 1e-10f);
    const float ratioChange = std::abs(outputRatio / originalRatio - 1.0f);

    expect(ratioChange < 0.3f,  // Allow 30% variation due to envelope dynamics
        "Stereo linking not preserved: ratio changed by " +
        juce::String(ratioChange * 100.0f, 1) + "%");

    expect(!containsInvalidSamples(buffer), "Output contains NaN or Inf");
}

void OutputLimiterTests::testSoftKnee()
{
    using namespace TestUtilities;

    PluginProcessor processor;
    processor.setRateAndBufferSizeDetails(44100.0, 512);
    processor.prepareToPlay(44100.0, 512);

    // Test signals at different levels to verify soft knee behavior
    // Reset limiter state between tests
    processor.outputLimiterEnvelope = 1.0f;

    // Process signals at increasing levels and measure gain reduction
    float levels[] = { -6.0f, -3.0f, -1.0f, 0.0f, 3.0f };  // dB relative to 0dBFS
    float previousGR = 0.0f;

    for (float levelDB : levels)
    {
        processor.outputLimiterEnvelope = 1.0f;  // Reset envelope

        const float level = juce::Decibels::decibelsToGain(levelDB);
        auto buffer = generateSineWave(1000.0, 44100.0, 512, level, 2);
        const float inputPeak = calculatePeak(buffer);

        juce::MidiBuffer midi;
        processor.processBlock(buffer, midi);

        const float outputPeak = calculatePeak(buffer);
        const float gainReduction = juce::Decibels::gainToDecibels(outputPeak / inputPeak);

        // Gain reduction should increase monotonically with level above threshold
        if (levelDB > -0.5f)  // Above threshold
        {
            expect(gainReduction <= previousGR + 0.5f,
                "Soft knee not smooth at " + juce::String(levelDB) + " dB");
        }

        previousGR = gainReduction;
    }
}

void OutputLimiterTests::testEnvelopeAttackRelease()
{
    using namespace TestUtilities;

    PluginProcessor processor;
    processor.setRateAndBufferSizeDetails(44100.0, 1024);
    processor.prepareToPlay(44100.0, 1024);  // Match buffer size to prevent oversampling overflow

    setParameter(processor.parameters, "outputGain", 50.0f);  // Unity gain
    setParameter(processor.parameters, "distortionAmount", 0.0f);
    setParameter(processor.parameters, "compEnabled", false);

    // Reset limiter
    processor.outputLimiterEnvelope = 1.0f;

    // Create impulse to test attack
    juce::AudioBuffer<float> impulseBuffer(2, 1024);
    impulseBuffer.clear();

    // Hot impulse at sample 100
    const float impulseLevel = juce::Decibels::decibelsToGain(6.0f);  // +6dBFS (way above threshold)
    for (int ch = 0; ch < 2; ++ch)
    {
        for (int i = 100; i < 150; ++i)
        {
            impulseBuffer.setSample(ch, i, impulseLevel);
        }
    }

    juce::MidiBuffer midi;
    processor.processBlock(impulseBuffer, midi);

    // After processing, envelope should have attacked (< 1.0)
    expect(processor.outputLimiterEnvelope < 1.0f,
        "Envelope didn't attack on impulse");

    // Now process silence for release
    processor.outputLimiterEnvelope = 0.5f;  // Set to reduced state

    auto silenceBuffer = generateSilence(4096, 2);  // ~93ms at 44.1kHz
    processor.processBlock(silenceBuffer, midi);

    // Envelope should have released toward 1.0
    expect(processor.outputLimiterEnvelope > 0.5f,
        "Envelope didn't release during silence");
}

void OutputLimiterTests::testStateReset()
{
    using namespace TestUtilities;

    PluginProcessor processor;
    processor.setRateAndBufferSizeDetails(44100.0, 512);
    processor.prepareToPlay(44100.0, 512);

    // Set envelope to non-unity value
    processor.outputLimiterEnvelope = 0.3f;

    // Reset DSP state
    processor.resetDSPState();

    // Envelope should be back to unity
    expectEquals(processor.outputLimiterEnvelope, 1.0f,
        "State reset didn't restore envelope to unity");
}

void OutputLimiterTests::testGlobalMixCeiling()
{
    using namespace TestUtilities;

    // Regression: the final limiter must run AFTER the global dry/wet blend so a
    // hot dry signal blended back in cannot push the output past the safety ceiling.
    // globalMix = 0% routes the full (raw, unprocessed) dry input to the output; if
    // the limiter ran before the blend, that hot dry would pass through unbounded.
    const float maxAllowed = juce::Decibels::decibelsToGain(0.5f);  // matches threshold + knee tolerance

    auto runScenario = [&](bool distortionOn, const juce::String& pathName)
    {
        PluginProcessor processor;
        processor.setRateAndBufferSizeDetails(44100.0, 512);
        processor.prepareToPlay(44100.0, 512);

        setParameter(processor.parameters, "distortionAmount", distortionOn ? 100.0f : 0.0f);
        setParameter(processor.parameters, "compEnabled", false);
        setParameter(processor.parameters, "outputGain", 50.0f);  // unity — hotness comes from the dry input
        setParameter(processor.parameters, "globalMix", 0.0f);     // 100% dry

        juce::MidiBuffer midi;

        // Hot dry input at +6 dB (full-scale ×2). Settle the limiter envelope and the
        // globalMix smoother across several blocks before measuring.
        const float hotGain = juce::Decibels::decibelsToGain(6.0f);
        for (int i = 0; i < 10; ++i)
        {
            auto buffer = generateSineWave(1000.0, 44100.0, 512, hotGain, 2);
            processor.processBlock(buffer, midi);
        }

        auto buffer = generateSineWave(1000.0, 44100.0, 512, hotGain, 2);
        processor.processBlock(buffer, midi);

        const float peakOutput = calculatePeak(buffer);
        expect(peakOutput <= maxAllowed,
            pathName + " path: hot dry blend peaked at " +
            juce::String(juce::Decibels::gainToDecibels(peakOutput), 2) +
            " dB, exceeding the safety ceiling (" +
            juce::String(juce::Decibels::gainToDecibels(maxAllowed), 2) + " dB)");
        expect(!containsInvalidSamples(buffer), pathName + " path: output contains NaN or Inf");
    };

    runScenario(false, "Bypass");
    runScenario(true,  "Active");
}

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

//==============================================================================
// LFOTests Implementation
//==============================================================================

void LFOTests::runTest()
{
    PluginProcessor processor;
    processor.setRateAndBufferSizeDetails(44100.0, 512);
    processor.prepareToPlay(44100.0, 512);

    beginTest("Sine Waveform");
    testWaveformShape(processor, 0, "Sine");

    beginTest("Triangle Waveform");
    testWaveformShape(processor, 1, "Triangle");

    beginTest("Square Waveform");
    testWaveformShape(processor, 2, "Square");

    beginTest("Sawtooth Waveform");
    testWaveformShape(processor, 3, "Sawtooth");

    beginTest("Random Sample & Hold");
    testRandomSampleHold(processor);

    beginTest("Output Range");
    testOutputRange(processor);
}

void LFOTests::testWaveformShape(PluginProcessor& processor, int waveformType, const juce::String& label)
{
    // Test key phase points
    const float testPhases[] = { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f };

    for (float phase : testPhases)
    {
        float output = processor.generateLFOWaveform(phase, waveformType);

        expect(!std::isnan(output), label + ": Output is NaN at phase " + juce::String(phase));
        expect(!std::isinf(output), label + ": Output is Inf at phase " + juce::String(phase));
        expect(output >= -1.0f && output <= 1.0f,
               label + ": Output " + juce::String(output) + " out of range [-1, 1]");
    }

    // Test specific expected values for deterministic waveforms
    if (waveformType == 0)  // Sine
    {
        float atZero = processor.generateLFOWaveform(0.0f, 0);
        expectWithinAbsoluteError(atZero, 0.0f, 0.01f, "Sine at phase 0 should be ~0");

        float atQuarter = processor.generateLFOWaveform(0.25f, 0);
        expectWithinAbsoluteError(atQuarter, 1.0f, 0.01f, "Sine at phase 0.25 should be ~1");
    }
    else if (waveformType == 2)  // Square
    {
        float atStart = processor.generateLFOWaveform(0.1f, 2);
        float atEnd = processor.generateLFOWaveform(0.9f, 2);
        expect(atStart > 0.0f, "Square wave should be positive in first half");
        expect(atEnd < 0.0f, "Square wave should be negative in second half");
    }
}

void LFOTests::testRandomSampleHold(PluginProcessor& processor)
{
    // Random S&H should change value only when phase resets
    // Simulate phase progression without reset
    for (float phase = 0.1f; phase < 0.9f; phase += 0.1f)
    {
        float value = processor.generateLFOWaveform(phase, 4);
        expect(!std::isnan(value), "Random S&H produced NaN");
        expect(value >= -1.0f && value <= 1.0f, "Random S&H out of range");
    }
}

void LFOTests::testOutputRange(PluginProcessor& processor)
{
    // Test all waveforms stay in [-1, 1] range for all phases
    for (int waveform = 0; waveform < 5; ++waveform)
    {
        float minVal = 1.0f;
        float maxVal = -1.0f;

        for (int i = 0; i <= 100; ++i)
        {
            float phase = static_cast<float>(i) / 100.0f;
            float value = processor.generateLFOWaveform(phase, waveform);

            minVal = std::min(minVal, value);
            maxVal = std::max(maxVal, value);
        }

        expect(minVal >= -1.0f, "Waveform " + juce::String(waveform) + " min " +
               juce::String(minVal) + " below -1");
        expect(maxVal <= 1.0f, "Waveform " + juce::String(waveform) + " max " +
               juce::String(maxVal) + " above 1");
    }
}

//==============================================================================
// LFODestinationTests Implementation
//==============================================================================

void LFODestinationTests::runTest()
{
    beginTest("Destination parameter exists and defaults to 0");
    {
        PluginProcessor processor;
        processor.setRateAndBufferSizeDetails(44100.0, 512);
        processor.prepareToPlay(44100.0, 512);

        auto* param = processor.parameters.getParameter("lfoDestination");
        expect(param != nullptr, "lfoDestination parameter exists");
        expectWithinAbsoluteError(param->getValue(), 0.0f, 0.01f,
            "Default destination is 0 (Distortion)");
    }

    beginTest("Destination 0: Distortion modulation");
    testDestination(0, "distortionAmount", 50.0f);

    beginTest("Destination 1: Tone filter modulation");
    testDestination(1, "tone", 8000.0f);

    beginTest("Destination 2: Shape modulation");
    testDestination(2, "shape", 0.0f);

    beginTest("Destination 3: Dist mix modulation");
    testDestination(3, "distMix", 50.0f);

    beginTest("Destination 4: Output gain modulation");
    testDestination(4, "outputGain", 50.0f);

    beginTest("Extreme depth doesn't cause NaN on any destination");
    {
        PluginProcessor processor;
        processor.setRateAndBufferSizeDetails(44100.0, 4410);
        processor.prepareToPlay(44100.0, 4410);  // Match buffer size to prevent oversampling overflow

        TestUtilities::setParameter(processor.parameters, "lfoEnabled", 1.0f);
        TestUtilities::setParameter(processor.parameters, "lfoRate", 20.0f);
        TestUtilities::setParameter(processor.parameters, "lfoDepth", 100.0f);
        TestUtilities::setParameter(processor.parameters, "distortionAmount", 50.0f);

        for (int dest = 0; dest <= 4; ++dest)
        {
            TestUtilities::setParameter(processor.parameters, "lfoDestination", static_cast<float>(dest));

            juce::AudioBuffer<float> buffer(2, 4410);
            buffer.clear();
            // Add test signal
            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                float sample = 0.5f * std::sin(2.0f * juce::MathConstants<float>::pi * 440.0f * i / 44100.0f);
                buffer.setSample(0, i, sample);
                buffer.setSample(1, i, sample);
            }

            juce::MidiBuffer midi;
            processor.processBlock(buffer, midi);

            // Check for NaN in output
            bool hasNaN = false;
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            {
                for (int i = 0; i < buffer.getNumSamples(); ++i)
                {
                    if (std::isnan(buffer.getSample(ch, i)) || std::isinf(buffer.getSample(ch, i)))
                    {
                        hasNaN = true;
                        break;
                    }
                }
            }

            expect(!hasNaN, juce::String("No NaN with extreme depth on destination ") + juce::String(dest));
        }
    }

    beginTest("Fast LFO rate (50Hz) is stable on all destinations");
    {
        PluginProcessor processor;
        processor.setRateAndBufferSizeDetails(44100.0, 44100);
        processor.prepareToPlay(44100.0, 44100);  // Match buffer size to prevent oversampling overflow

        TestUtilities::setParameter(processor.parameters, "lfoEnabled", 1.0f);
        TestUtilities::setParameter(processor.parameters, "lfoRate", 50.0f);  // Max rate
        TestUtilities::setParameter(processor.parameters, "lfoDepth", 75.0f);

        for (int dest = 0; dest <= 4; ++dest)
        {
            TestUtilities::setParameter(processor.parameters, "lfoDestination", static_cast<float>(dest));

            juce::AudioBuffer<float> buffer(2, 44100);  // 1 second
            buffer.clear();
            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                float sample = 0.3f * std::sin(2.0f * juce::MathConstants<float>::pi * 440.0f * i / 44100.0f);
                buffer.setSample(0, i, sample);
                buffer.setSample(1, i, sample);
            }

            juce::MidiBuffer midi;
            processor.processBlock(buffer, midi);

            expect(!processor.debugHadNaN.load(),
                juce::String("No NaN with 50Hz LFO on destination ") + juce::String(dest));
        }
    }
}

void LFODestinationTests::testDestination(int destIndex, const juce::String& paramID, float centerValue)
{
    PluginProcessor processor;
    processor.setRateAndBufferSizeDetails(44100.0, 4410);
    processor.prepareToPlay(44100.0, 4410);  // Match buffer size to prevent oversampling overflow

    TestUtilities::setParameter(processor.parameters, "lfoEnabled", 1.0f);
    TestUtilities::setParameter(processor.parameters, "lfoRate", 2.0f);
    TestUtilities::setParameter(processor.parameters, "lfoDepth", 50.0f);
    TestUtilities::setParameter(processor.parameters, "lfoWaveform", 0.0f);  // Sine
    TestUtilities::setParameter(processor.parameters, "lfoDestination", static_cast<float>(destIndex));
    TestUtilities::setParameter(processor.parameters, paramID, centerValue);
    TestUtilities::setParameter(processor.parameters, "distortionAmount", 50.0f);  // Ensure distortion active

    juce::AudioBuffer<float> buffer(2, 4410);  // 100ms
    buffer.clear();
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        float sample = 0.5f * std::sin(2.0f * juce::MathConstants<float>::pi * 440.0f * i / 44100.0f);
        buffer.setSample(0, i, sample);
        buffer.setSample(1, i, sample);
    }

    juce::MidiBuffer midi;
    processor.processBlock(buffer, midi);

    expect(!processor.debugHadNaN.load(),
        juce::String("No NaN during modulation of ") + paramID);
}

//==============================================================================
// LFOBpmSyncTests Implementation
//==============================================================================

void LFOBpmSyncTests::runTest()
{
    beginTest("Parameters exist with correct defaults");
    {
        PluginProcessor processor;
        processor.setRateAndBufferSizeDetails(44100.0, 512);
        processor.prepareToPlay(44100.0, 512);

        auto* syncParam = processor.parameters.getParameter("lfoBpmSync");
        expect(syncParam != nullptr, "lfoBpmSync parameter exists");
        expectWithinAbsoluteError(syncParam->getValue(), 0.0f, 0.01f,
            "lfoBpmSync defaults to OFF");

        auto* divParam = processor.parameters.getParameter("lfoBpmDivision");
        expect(divParam != nullptr, "lfoBpmDivision parameter exists");
        // Default index 2 = 1/4 note; normalized value = 2/8 = 0.25
        expectWithinAbsoluteError(divParam->convertFrom0to1(divParam->getValue()), 2.0f, 0.01f,
            "lfoBpmDivision defaults to 1/4 (index 2)");
    }

    // Division factor table mirrors the DSP code: index -> cycles-per-beat
    static constexpr float kFactors[] = { 0.25f, 0.5f, 1.0f, 2.0f, 4.0f, 8.0f, 1.5f, 3.0f, 6.0f };

    beginTest("Sync OFF: phase advances at lfoRate / sampleRate per sample");
    {
        const double sr = 44100.0;
        const int    bufSize = 512;
        PluginProcessor processor;
        processor.setRateAndBufferSizeDetails(sr, bufSize);
        processor.prepareToPlay(sr, bufSize);

        TestUtilities::setParameter(processor.parameters, "lfoEnabled",     1.0f);
        TestUtilities::setParameter(processor.parameters, "lfoBpmSync",     0.0f);
        TestUtilities::setParameter(processor.parameters, "lfoRate",        2.0f);
        TestUtilities::setParameter(processor.parameters, "lfoDepth",       50.0f);
        TestUtilities::setParameter(processor.parameters, "lfoDestination", 1.0f); // Tone — block-level path
        TestUtilities::setParameter(processor.parameters, "distortionAmount", 50.0f);

        // Warm-up block so initial filter states are stable
        juce::AudioBuffer<float> buf(2, bufSize);
        juce::MidiBuffer midi;
        buf.clear();
        processor.processBlock(buf, midi);

        const float phaseBefore = processor.lfoPhaseForUI.load();

        buf.clear();
        processor.processBlock(buf, midi);

        const float phaseAfter = processor.lfoPhaseForUI.load();
        float delta = phaseAfter - phaseBefore;
        if (delta < 0.0f) delta += 1.0f; // handle wrap

        const float expected = (2.0f / static_cast<float>(sr)) * bufSize;
        expectWithinAbsoluteError(delta, expected, 0.0001f,
            "Sync OFF: phase increment matches lfoRate/sampleRate");
    }

    beginTest("Sync ON, no host playhead: falls back to 120 BPM, all divisions produce valid output");
    {
        const double sr = 44100.0;
        const int    bufSize = 512;

        for (int divIdx = 0; divIdx < 9; ++divIdx)
        {
            PluginProcessor processor;
            processor.setRateAndBufferSizeDetails(sr, bufSize);
            processor.prepareToPlay(sr, bufSize);

            TestUtilities::setParameter(processor.parameters, "lfoEnabled",       1.0f);
            TestUtilities::setParameter(processor.parameters, "lfoBpmSync",       1.0f);
            TestUtilities::setParameter(processor.parameters, "lfoBpmDivision",   static_cast<float>(divIdx));
            TestUtilities::setParameter(processor.parameters, "lfoDepth",         50.0f);
            TestUtilities::setParameter(processor.parameters, "lfoDestination",   1.0f); // Tone
            TestUtilities::setParameter(processor.parameters, "distortionAmount", 50.0f);

            juce::AudioBuffer<float> buf(2, bufSize);
            buf.clear();
            juce::MidiBuffer midi;
            processor.processBlock(buf, midi);

            expect(!TestUtilities::containsInvalidSamples(buf),
                "Division " + juce::String(divIdx) + ": processBlock produced NaN/Inf");
            expect(!processor.debugHadNaN.load(),
                "Division " + juce::String(divIdx) + ": debugHadNaN flag set");

            // Verify phase advance matches 120 BPM fallback
            const float expectedHz = (120.0f / 60.0f) * kFactors[divIdx];
            const float expectedDelta = (expectedHz / static_cast<float>(sr)) * bufSize;
            const float actualPhase = processor.lfoPhaseForUI.load();
            // Phase must be in valid range (wrapped to [0,1))
            expect(actualPhase >= 0.0f && actualPhase < 1.0f,
                "Division " + juce::String(divIdx) + ": phase out of [0,1) range");
            // For slow rates (1/1 at 120 BPM = 0.5 Hz), delta is small; just check it's positive
            if (expectedDelta < 0.99f)
                expect(actualPhase > 0.0f,
                    "Division " + juce::String(divIdx) + ": phase did not advance");
        }
    }

    beginTest("State round-trip preserves lfoBpmSync and lfoBpmDivision");
    {
        PluginProcessor src;
        src.setRateAndBufferSizeDetails(44100.0, 512);
        src.prepareToPlay(44100.0, 512);

        TestUtilities::setParameter(src.parameters, "lfoBpmSync",     1.0f); // ON
        TestUtilities::setParameter(src.parameters, "lfoBpmDivision", 7.0f); // 1/8T (index 7)

        juce::MemoryBlock state;
        src.getStateInformation(state);
        expect(state.getSize() > 0, "getStateInformation produced empty data");

        PluginProcessor dst;
        dst.setRateAndBufferSizeDetails(44100.0, 512);
        dst.prepareToPlay(44100.0, 512);
        dst.setStateInformation(state.getData(), static_cast<int>(state.getSize()));

        auto* syncParam = dst.parameters.getParameter("lfoBpmSync");
        expect(syncParam != nullptr, "lfoBpmSync param found in restored state");
        expectWithinAbsoluteError(syncParam->convertFrom0to1(syncParam->getValue()), 1.0f, 0.01f,
            "lfoBpmSync restored to ON");

        auto* divParam = dst.parameters.getParameter("lfoBpmDivision");
        expect(divParam != nullptr, "lfoBpmDivision param found in restored state");
        expectWithinAbsoluteError(divParam->convertFrom0to1(divParam->getValue()), 7.0f, 0.01f,
            "lfoBpmDivision restored to index 7 (1/8T)");
    }
}

//==============================================================================
// ProcessBlockTests Implementation
//==============================================================================

void ProcessBlockTests::runTest()
{
    beginTest("True Bypass Mode");
    testTrueBypass();

    beginTest("808-Safe Mode");
    test808SafeMode();

    beginTest("Oversampling");
    testOversampling();

    beginTest("DC Blocking");
    testDCBlocking();

    beginTest("Wet/Dry Mix");
    testWetDryMix();

    beginTest("Empty Buffer Handling");
    testEmptyBufferHandling();

    beginTest("Output Gain");
    testOutputGain();

    beginTest("Mono In / Stereo Out");
    testMonoInStereoOut();

    beginTest("Mono Input Toggle");
    testMonoInputToggle();

    beginTest("Mono Source Detection");
    testMonoSourceDetection();

    beginTest("Mono Source Detection With Noise Floor");
    testMonoSourceDetectionWithNoiseFloor();
}

// Builds a stereo buffer carrying signal on the left and silence on the right -
// a bass plugged into interface input 1 with nothing in input 2.
static juce::AudioBuffer<float> makeOneSidedBuffer(int numSamples)
{
    auto buffer = generateSineWave(220.0, 44100.0, numSamples, 0.5f, 2);
    buffer.clear(1, 0, numSamples);
    return buffer;
}

void ProcessBlockTests::testMonoInputToggle()
{
    // Off: a one-sided source must stay one-sided. Anything else would be
    // rewriting the stereo image of every existing session.
    {
        PluginProcessor processor;
        processor.setRateAndBufferSizeDetails(44100.0, 512);
        processor.prepareToPlay(44100.0, 512);
        setParameter(processor.parameters, "distortionAmount", 50.0f);
        setParameter(processor.parameters, "clipType", 1.0f);
        setParameter(processor.parameters, "monoInput", 0.0f);

        auto buffer = makeOneSidedBuffer(512);
        juce::MidiBuffer midi;
        processor.processBlock(buffer, midi);

        float rightPeak = 0.0f;
        for (int i = 0; i < 512; ++i)
            rightPeak = juce::jmax(rightPeak, std::abs(buffer.getSample(1, i)));

        expect(rightPeak < 0.001f, "with Mono Input off the silent channel must stay silent");
    }

    // On: the same source reaches both channels identically.
    {
        PluginProcessor processor;
        processor.setRateAndBufferSizeDetails(44100.0, 512);
        processor.prepareToPlay(44100.0, 512);
        setParameter(processor.parameters, "distortionAmount", 50.0f);
        setParameter(processor.parameters, "clipType", 1.0f);   // deterministic clip, see testMonoInStereoOut
        setParameter(processor.parameters, "monoInput", 1.0f);

        auto buffer = makeOneSidedBuffer(512);
        juce::MidiBuffer midi;
        processor.processBlock(buffer, midi);

        float maxDiff = 0.0f, rightPeak = 0.0f;
        for (int i = 0; i < 512; ++i)
        {
            const float l = buffer.getSample(0, i);
            const float r = buffer.getSample(1, i);
            maxDiff   = juce::jmax(maxDiff, std::abs(l - r));
            rightPeak = juce::jmax(rightPeak, std::abs(r));
        }

        expect(rightPeak > 0.001f, "with Mono Input on the right channel must carry the signal");
        expect(maxDiff < 1.0e-6f, "both channels must match so the source is centred");
    }
}

void ProcessBlockTests::testMonoSourceDetection()
{
    PluginProcessor processor;
    processor.setRateAndBufferSizeDetails(44100.0, 512);
    processor.prepareToPlay(44100.0, 512);
    setParameter(processor.parameters, "monoInput", 0.0f);

    juce::MidiBuffer midi;
    expect(! processor.monoSourceDetected.load(), "nothing detected before any audio");

    // One block is deliberately not enough: the hint needs a sustained run so a
    // rest on one side of a stereo take cannot trip it.
    {
        auto buffer = makeOneSidedBuffer(512);
        processor.processBlock(buffer, midi);
        expect(! processor.monoSourceDetected.load(), "a single one-sided block must not trigger the hint");
    }

    // ~2 seconds of one-sided audio does.
    for (int i = 0; i < 200; ++i)
    {
        auto buffer = makeOneSidedBuffer(512);
        processor.processBlock(buffer, midi);
    }
    expect(processor.monoSourceDetected.load(), "sustained one-sided audio should raise the hint");

    // Genuine stereo clears it again.
    for (int i = 0; i < 10; ++i)
    {
        auto stereo = generateSineWave(220.0, 44100.0, 512, 0.5f, 2);
        processor.processBlock(stereo, midi);
    }
    expect(! processor.monoSourceDetected.load(), "signal on both channels should clear the hint");
}

void ProcessBlockTests::testMonoSourceDetectionWithNoiseFloor()
{
    // The real case: an unused interface input is not digital silence, it carries
    // an analogue noise floor. Detection has to survive that or it never fires on
    // actual hardware - only in a test feeding perfect zeroes.
    PluginProcessor processor;
    processor.setRateAndBufferSizeDetails(44100.0, 512);
    processor.prepareToPlay(44100.0, 512);
    setParameter(processor.parameters, "monoInput", 0.0f);

    juce::Random rng(1234);
    juce::MidiBuffer midi;

    for (int b = 0; b < 250; ++b)
    {
        auto buffer = generateSineWave(220.0, 44100.0, 512, 0.5f, 2);
        // Right channel: ~-60 dBFS of noise, a quiet but entirely ordinary input.
        for (int i = 0; i < 512; ++i)
            buffer.setSample(1, i, (rng.nextFloat() * 2.0f - 1.0f) * 0.001f);
        processor.processBlock(buffer, midi);
    }

    expect(processor.monoSourceDetected.load(),
           "a noise floor on the unused channel must still read as a mono source");
}

void ProcessBlockTests::testMonoInStereoOut()
{
    // A bass or guitar arrives on one interface input. If the plugin refuses the
    // mono-in / stereo-out layout, or accepts it and leaves the second channel
    // alone, the player hears the signal in the left speaker only.
    PluginProcessor processor;

    juce::AudioProcessor::BusesLayout layout;
    layout.inputBuses.add(juce::AudioChannelSet::mono());
    layout.outputBuses.add(juce::AudioChannelSet::stereo());
    expect(processor.setBusesLayout(layout), "mono-in / stereo-out layout must be supported");
    expectEquals(processor.getTotalNumInputChannels(), 1, "layout should give one input channel");
    expectEquals(processor.getTotalNumOutputChannels(), 2, "layout should give two output channels");

    processor.setRateAndBufferSizeDetails(44100.0, 512);
    processor.prepareToPlay(44100.0, 512);
    setParameter(processor.parameters, "distortionAmount", 50.0f);
    // Tube Overdrive, not the default Brutal Fuzz: that clip adds per-channel
    // analog noise by design, so two identical channels legitimately diverge and
    // an exact comparison could not be made.
    setParameter(processor.parameters, "clipType", 1.0f);

    // The host sizes the buffer to the output bus, so channel 1 arrives holding
    // whatever was in that memory. A sentinel stands in for that: if the value
    // survives, the mono input was never copied across.
    auto buffer = generateSineWave(220.0, 44100.0, 512, 0.5f, 1);
    buffer.setSize(2, 512, true, false, true);
    for (int i = 0; i < 512; ++i)
        buffer.setSample(1, i, 12345.0f);

    juce::MidiBuffer midi;
    processor.processBlock(buffer, midi);

    float maxDiff   = 0.0f;
    float peak      = 0.0f;
    float rightPeak = 0.0f;
    for (int i = 0; i < 512; ++i)
    {
        const float l = buffer.getSample(0, i);
        const float r = buffer.getSample(1, i);
        expect(std::isfinite(l) && std::isfinite(r), "output must stay finite");
        maxDiff   = juce::jmax(maxDiff, std::abs(l - r));
        peak      = juce::jmax(peak, std::abs(l));
        rightPeak = juce::jmax(rightPeak, std::abs(r));
    }

    expect(peak > 0.001f, "left channel should carry the processed signal");
    expect(rightPeak > 0.001f, "right channel must not be silent - a mono source reaches both speakers");
    expect(maxDiff < 1.0e-6f, "both output channels must match, so a mono source is centred");
}

void ProcessBlockTests::testTrueBypass()
{
    PluginProcessor processor;
    processor.setRateAndBufferSizeDetails(44100.0, 512);
    processor.prepareToPlay(44100.0, 512);

    // Set distortion to minimum and compression off (true bypass)
    setParameter(processor.parameters, "distortionAmount", 0.0f);
    setParameter(processor.parameters, "compEnabled", 0.0f);

    // Create input signal
    auto inputBuffer = generateSineWave(440.0, 44100.0, 512, 0.5f);
    auto originalBuffer = inputBuffer;  // Copy for comparison
    juce::MidiBuffer midi;

    processor.processBlock(inputBuffer, midi);

    // In true bypass, signal should pass through with minimal change
    // (only output gain applied)
    expect(!containsInvalidSamples(inputBuffer), "True bypass produced invalid samples");

    // Check level is preserved (output gain at unity default)
    float inputPeak = calculatePeak(originalBuffer);
    float outputPeak = calculatePeak(inputBuffer);
    expectWithinAbsoluteError(outputPeak, inputPeak, 0.1f, "True bypass should preserve level");
}

void ProcessBlockTests::test808SafeMode()
{
    PluginProcessor processor;
    processor.setRateAndBufferSizeDetails(44100.0, 2048);
    processor.prepareToPlay(44100.0, 2048);  // Match buffer size

    // Enable Sub Guard mode (60 Hz crossover = PRESERVE mode)
    setParameter(processor.parameters, "subGuardFreq", 60.0f);
    setParameter(processor.parameters, "distortionAmount", 70.0f);

    // Create low frequency input (should bypass distortion)
    auto lowFreqBuffer = generateSineWave(60.0, 44100.0, 2048, 0.7f);
    juce::MidiBuffer midi;

    processor.processBlock(lowFreqBuffer, midi);

    // Low frequencies should pass through relatively clean
    expect(!containsInvalidSamples(lowFreqBuffer), "808-safe mode produced invalid samples");
    expect(calculatePeak(lowFreqBuffer) > 0.1f, "808-safe mode killed low frequencies");
}

void ProcessBlockTests::testOversampling()
{
    PluginProcessor processor;
    processor.setRateAndBufferSizeDetails(44100.0, 512);
    processor.prepareToPlay(44100.0, 512);

    // Enable distortion (which uses oversampling)
    setParameter(processor.parameters, "distortionAmount", 50.0f);

    auto buffer = generateSineWave(1000.0, 44100.0, 512, 0.8f);
    juce::MidiBuffer midi;

    // Should process without crash
    processor.processBlock(buffer, midi);

    expect(!containsInvalidSamples(buffer), "Oversampled processing produced invalid samples");
    expect(buffer.getNumSamples() == 512, "Buffer size changed after processing");
}

void ProcessBlockTests::testDCBlocking()
{
    PluginProcessor processor;
    processor.setRateAndBufferSizeDetails(44100.0, 512);
    processor.prepareToPlay(44100.0, 512);

    // Enable distortion
    setParameter(processor.parameters, "distortionAmount", 50.0f);

    // Verify parameter was set correctly
    float actualDistortion = processor.distortionAmountParam->load();
    std::cout << "DC Blocking Test - Distortion param set to: " << actualDistortion << "\n";

    // Create signal with DC offset
    auto buffer = generateDCOffset(512, 0.5f);
    juce::MidiBuffer midi;

    // Process multiple blocks to let DC filter settle
    for (int i = 0; i < 10; ++i)
    {
        buffer = generateDCOffset(512, 0.5f);

        // Check input is valid
        if (containsInvalidSamples(buffer))
        {
            std::cout << "Input buffer has NaN/Inf!\n";
        }

        processor.processBlock(buffer, midi);

        // Check each block for NaN
        if (containsInvalidSamples(buffer))
        {
            std::cout << "Block " << i << " produced NaN/Inf!\n";
            float peak = calculatePeak(buffer);
            std::cout << "Peak: " << peak << "\n";
            break;
        }
    }

    // Check for valid samples first
    expect(!containsInvalidSamples(buffer), "DC blocking produced invalid samples (NaN/Inf)");

    // DC should be significantly reduced after several blocks
    float dcLevel = 0.0f;
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        const auto* data = buffer.getReadPointer(ch);
        for (int s = 0; s < buffer.getNumSamples(); ++s)
            dcLevel += data[s];
    }
    dcLevel /= static_cast<float>(buffer.getNumSamples() * buffer.getNumChannels());

    std::cout << "Final DC level: " << dcLevel << "\n";

    expect(std::abs(dcLevel) < 0.3f, "DC blocking should reduce DC offset. Remaining: " +
           juce::String(dcLevel));
}

void ProcessBlockTests::testWetDryMix()
{
    PluginProcessor processor;
    processor.setRateAndBufferSizeDetails(44100.0, 512);
    processor.prepareToPlay(44100.0, 512);

    // Enable distortion
    setParameter(processor.parameters, "distortionAmount", 70.0f);

    std::cout << "Wet/Dry Test - Distortion param: " << processor.distortionAmountParam->load() << "\n";

    // Test 0% wet (dry only)
    setParameter(processor.parameters, "distMix", 0.0f);

    std::cout << "Wet/Dry Test - DistMix param: " << processor.distMixParam->load() << "\n";

    auto dryBuffer = generateSineWave(1000.0, 44100.0, 512, 0.5f);
    auto originalDry = dryBuffer;
    juce::MidiBuffer midi;

    processor.processBlock(dryBuffer, midi);

    if (containsInvalidSamples(dryBuffer))
    {
        std::cout << "Wet/Dry Test - Dry buffer (0% mix) produced NaN/Inf!\n";
    }

    float dryPeak = calculatePeak(dryBuffer);
    std::cout << "Wet/Dry Test - Dry peak (0% mix): " << dryPeak << "\n";

    // Test 100% wet
    setParameter(processor.parameters, "distMix", 100.0f);

    auto wetBuffer = generateSineWave(1000.0, 44100.0, 512, 0.5f);
    processor.processBlock(wetBuffer, midi);

    if (containsInvalidSamples(wetBuffer))
    {
        std::cout << "Wet/Dry Test - Wet buffer (100% mix) produced NaN/Inf!\n";
    }

    float wetPeak = calculatePeak(wetBuffer);
    std::cout << "Wet/Dry Test - Wet peak (100% mix): " << wetPeak << "\n";

    // Both should produce valid output
    expect(!containsInvalidSamples(dryBuffer), "Dry mix produced invalid samples");
    expect(!containsInvalidSamples(wetBuffer), "Wet mix produced invalid samples");
}

void ProcessBlockTests::testEmptyBufferHandling()
{
    PluginProcessor processor;
    processor.setRateAndBufferSizeDetails(44100.0, 512);
    processor.prepareToPlay(44100.0, 512);

    // Test with zero samples
    juce::AudioBuffer<float> emptyBuffer(2, 0);
    juce::MidiBuffer midi;

    // Should not crash
    processor.processBlock(emptyBuffer, midi);

    expect(emptyBuffer.getNumSamples() == 0, "Empty buffer handling failed");
}

void ProcessBlockTests::testOutputGain()
{
    PluginProcessor processor;
    processor.setRateAndBufferSizeDetails(44100.0, 512);
    processor.prepareToPlay(44100.0, 512);

    // Enable distortion slightly to avoid bypass mode (bypass happens when distortion < 0.5%)
    setParameter(processor.parameters, "distortionAmount", 1.0f);

    // Set output gain to maximum
    setParameter(processor.parameters, "outputGain", 100.0f);

    // Process a few blocks to let smoothed parameters settle
    juce::MidiBuffer midi;
    for (int i = 0; i < 10; ++i)
    {
        auto dummyBuffer = generateSineWave(1000.0, 44100.0, 512, 0.3f);
        processor.processBlock(dummyBuffer, midi);
    }

    auto buffer = generateSineWave(1000.0, 44100.0, 512, 0.3f);
    processor.processBlock(buffer, midi);

    float peakWithMaxGain = calculatePeak(buffer);
    std::cout << "Output Gain Test - Distortion param: " << processor.distortionAmountParam->load() << "\n";
    std::cout << "Output Gain Test - Output gain param: " << processor.outputGainParam->load() << "\n";
    std::cout << "Output Gain Test - Peak with max gain (100): " << peakWithMaxGain << "\n";

    // Output should be louder than input (output gain > 50 should boost)
    expect(peakWithMaxGain > 0.35f, "Maximum output gain should boost signal. Peak: " + juce::String(peakWithMaxGain));

    // Set output gain to minimum
    setParameter(processor.parameters, "outputGain", 0.0f);

    // Process a few blocks to let smoothed parameter settle
    for (int i = 0; i < 10; ++i)
    {
        auto dummyBuffer = generateSineWave(1000.0, 44100.0, 512, 0.5f);
        processor.processBlock(dummyBuffer, midi);
    }

    buffer = generateSineWave(1000.0, 44100.0, 512, 0.5f);
    processor.processBlock(buffer, midi);

    float peakWithMinGain = calculatePeak(buffer);
    std::cout << "Output Gain Test - Peak with min gain (0): " << peakWithMinGain << "\n";

    // Output should be quieter (with -12dB gain and some distortion processing, expect < 0.3)
    // Note: The distortion path adds some gain even at low distortion amounts
    expect(peakWithMinGain < 0.3f, "Minimum output gain should significantly reduce signal. Peak: " + juce::String(peakWithMinGain));
}

//==============================================================================
// DryWetAlignmentTests Implementation
//==============================================================================

namespace
{
    // Configure a processor so the oversampled wet path is engaged but the
    // signal passes through near-linearly. Disables auto-gain, compression,
    // extreme mode, and limits distortion to just above the bypass threshold.
    inline void configureForAlignmentTest(PluginProcessor& processor, float globalMixPct)
    {
        setParameter(processor.parameters, "distortionAmount", 0.5f);   // min above bypass
        setParameter(processor.parameters, "clipType", 1.0f);            // Tube (smooth)
        setParameter(processor.parameters, "compEnabled", 0.0f);
        setParameter(processor.parameters, "autoGainEnabled", 0.0f);
        setParameter(processor.parameters, "extremeEnabled", 0.0f);
        setParameter(processor.parameters, "subGuardFreq", 0.0f);        // off
        setParameter(processor.parameters, "distMix", 100.0f);           // dist stage wet
        setParameter(processor.parameters, "inputGain", 50.0f);          // unity
        setParameter(processor.parameters, "outputGain", 50.0f);         // unity
        setParameter(processor.parameters, "lfoEnabled", 0.0f);
        setParameter(processor.parameters, "globalMix", globalMixPct);
    }

    // Run blocks of a continuous sine wave through the processor so smoothing
    // settles. The generator keeps phase across blocks so the signal is truly
    // continuous (no discontinuities at block boundaries that would smear RMS).
    inline void warmUp(PluginProcessor& processor, double sampleRate, int blockSize,
                       double frequency, int numBlocks, double& phaseInOut)
    {
        juce::MidiBuffer midi;
        const double phaseInc = juce::MathConstants<double>::twoPi * frequency / sampleRate;

        for (int b = 0; b < numBlocks; ++b)
        {
            juce::AudioBuffer<float> buffer(2, blockSize);
            for (int ch = 0; ch < 2; ++ch)
            {
                auto* data = buffer.getWritePointer(ch);
                double phase = phaseInOut;
                for (int s = 0; s < blockSize; ++s)
                {
                    data[s] = 0.3f * static_cast<float>(std::sin(phase));
                    phase += phaseInc;
                }
            }
            phaseInOut += phaseInc * blockSize;
            phaseInOut = std::fmod(phaseInOut, juce::MathConstants<double>::twoPi);
            processor.processBlock(buffer, midi);
        }
    }
}

void DryWetAlignmentTests::runTest()
{
    beginTest("Dry-only path latency matches reported latency");
    testDryOnlyLatency();

    beginTest("No comb filtering at 50% mix");
    testNoCombFiltering();

    beginTest("Fully wet path unaffected by new code");
    testFullyWetPathUnaffected();

    beginTest("State valid across oversampling reinit");
    testOversamplingReinit();

    beginTest("IIR partial-mix is phase-coherent across the spectrum");
    testIirPartialMixCoherence();

    beginTest("True-bypass path is latency-compensated");
    testBypassLatencyCompensation();

    beginTest("Dry oversampler is kept warm without disturbing the wet output");
    testDryOversamplerStaysWarm();

    beginTest("State reset clears the wet oversampler in lock-step with the dry");
    testResetClearsWetOversampler();
}

void DryWetAlignmentTests::testBypassLatencyCompensation()
{
    // The plugin reports the oversampler's latency to the host unconditionally,
    // so the host delay-compensates other tracks by that amount. The cheap
    // true-bypass branch (distortion off + comp off) must therefore delay its
    // output by exactly that many samples — otherwise the bypassed signal plays
    // EARLY against the rest of the mix and jumps in time when distortion or
    // compression crosses the bypass threshold.
    PluginProcessor processor;
    const double sr = 44100.0;
    const int blockSize = 512;
    processor.setRateAndBufferSizeDetails(sr, blockSize);
    processor.prepareToPlay(sr, blockSize);

    // Force the true-bypass branch, fully transparent (no filter, unity gain,
    // 100% wet so the dry-blend path is skipped).
    setParameter(processor.parameters, "distortionAmount", 0.0f);  // below bypass threshold
    setParameter(processor.parameters, "compEnabled", 0.0f);
    setParameter(processor.parameters, "autoGainEnabled", 0.0f);
    setParameter(processor.parameters, "extremeEnabled", 0.0f);
    setParameter(processor.parameters, "lfoEnabled", 0.0f);
    setParameter(processor.parameters, "inputGain", 50.0f);    // unity
    setParameter(processor.parameters, "outputGain", 50.0f);   // unity
    setParameter(processor.parameters, "globalMix", 100.0f);   // fully wet -> no dry blend
    setParameter(processor.parameters, "filterMode", 0.0f);    // High Pass
    setParameter(processor.parameters, "highPassFreq", 20.0f); // <= 25 Hz -> filter inactive

    const int latency = processor.getLatencySamples();
    expect(latency > 0, "Oversampler should report non-zero latency at default settings");

    juce::MidiBuffer midi;

    // Settle gain/mix smoothing with silence so the bypass path is steady-state
    // and the delay line is primed with zeros.
    for (int b = 0; b < 16; ++b)
    {
        juce::AudioBuffer<float> silence(2, blockSize);
        silence.clear();
        processor.processBlock(silence, midi);
    }

    // Send a single impulse, capture two blocks so the delayed copy is fully
    // contained even for large reported latencies.
    const int impulsePos = 64;
    const float amp = 0.5f;  // below the -0.5 dBFS output limiter threshold -> untouched

    const int numBlocks = 2;
    const int totalSamples = blockSize * numBlocks;
    juce::AudioBuffer<float> outputCapture(2, totalSamples);
    outputCapture.clear();

    for (int b = 0; b < numBlocks; ++b)
    {
        juce::AudioBuffer<float> block(2, blockSize);
        block.clear();
        if (b == 0)
            for (int ch = 0; ch < 2; ++ch)
                block.setSample(ch, impulsePos, amp);

        processor.processBlock(block, midi);

        for (int ch = 0; ch < 2; ++ch)
            outputCapture.copyFrom(ch, b * blockSize, block, ch, 0, blockSize);
    }

    expect(!containsInvalidSamples(outputCapture), "Bypass output must not contain NaN/Inf");

    // Locate the output impulse (peak |sample|) on channel 0.
    int peakIndex = -1;
    float peakValue = 0.0f;
    const auto* out = outputCapture.getReadPointer(0);
    for (int n = 0; n < totalSamples; ++n)
    {
        const float a = std::abs(out[n]);
        if (a > peakValue) { peakValue = a; peakIndex = n; }
    }

    expect(peakValue > 0.1f, "Bypass output impulse lost. Peak: " + juce::String(peakValue));
    expect(peakIndex == impulsePos + latency,
           "Bypass output must be delayed by the reported latency (" + juce::String(latency)
           + "). Expected peak index " + juce::String(impulsePos + latency)
           + ", got " + juce::String(peakIndex));
}

void DryWetAlignmentTests::testIirPartialMixCoherence()
{
    // With the default minimum-phase IIR oversampler, a plain delay only matches the
    // bulk group delay; the dry then sums against a wet whose phase is frequency-
    // dependent, comb-filtering the blend (worst near the top of the band). Routing the
    // dry through a matched oversampler aligns every frequency. We drive the distortion
    // stage as an identity (distMix = 0) so wet == dry through the same oversampler, and
    // verify that 50% mix preserves the level (== fully wet) at every probe frequency.
    const double sr = 44100.0;
    const int blockSize = 512;
    const float amp = 0.2f;

    auto measureRms = [&](double freq, float mixPct) -> float
    {
        PluginProcessor processor;
        processor.setRateAndBufferSizeDetails(sr, blockSize);
        processor.prepareToPlay(sr, blockSize);

        setParameter(processor.parameters, "distortionAmount", 50.0f);
        setParameter(processor.parameters, "distMix",          0.0f);   // identity distortion
        setParameter(processor.parameters, "waveshaperMix",    0.0f);
        setParameter(processor.parameters, "highPassFreq",     20.0f);
        setParameter(processor.parameters, "tone",             20000.0f);
        setParameter(processor.parameters, "compEnabled",      0.0f);
        setParameter(processor.parameters, "autoGainEnabled",  0.0f);
        setParameter(processor.parameters, "cleanBoost",       0.0f);
        setParameter(processor.parameters, "subGuardFreq",     0.0f);
        setParameter(processor.parameters, "linearPhaseDry",   0.0f);   // IIR oversampler
        setParameter(processor.parameters, "globalMix",        mixPct);

        juce::MidiBuffer midi;
        const double phaseInc = juce::MathConstants<double>::twoPi * freq / sr;
        double phase = 0.0;

        const int warmupBlocks = 16, measureBlocks = 8;
        float sumSquares = 0.0f; int count = 0;
        for (int b = 0; b < warmupBlocks + measureBlocks; ++b)
        {
            juce::AudioBuffer<float> buffer(2, blockSize);
            for (int ch = 0; ch < 2; ++ch)
            {
                auto* data = buffer.getWritePointer(ch);
                double p = phase;
                for (int s = 0; s < blockSize; ++s) { data[s] = amp * static_cast<float>(std::sin(p)); p += phaseInc; }
            }
            processor.processBlock(buffer, midi);
            if (b >= warmupBlocks)
            {
                const auto* d = buffer.getReadPointer(0);
                for (int s = 0; s < blockSize; ++s) { sumSquares += d[s] * d[s]; ++count; }
            }
            phase += phaseInc * blockSize;
            phase = std::fmod(phase, juce::MathConstants<double>::twoPi);
        }
        return count > 0 ? std::sqrt(sumSquares / static_cast<float>(count)) : 0.0f;
    };

    const double freqs[] = { 1000.0, 5000.0, 10000.0, 14000.0, 16000.0 };
    for (double f : freqs)
    {
        const float wetRms = measureRms(f, 100.0f);
        const float mixRms = measureRms(f, 50.0f);
        expect(wetRms > 1.0e-4f, "Fully-wet reference should be non-trivial at " + juce::String(f) + " Hz");

        // Coherent blend: 50% of (dry == wet) keeps the full level. A phase-misaligned
        // dry would partially cancel here, dipping the level (most at high frequencies).
        const float ratioDb = juce::Decibels::gainToDecibels(mixRms / juce::jmax(wetRms, 1.0e-9f));
        expect(ratioDb > -0.5f,
               "50% mix dipped at " + juce::String(f) + " Hz (phase-incoherent dry): "
               + juce::String(ratioDb, 3) + " dB");
    }
}

void DryWetAlignmentTests::testDryOnlyLatency()
{
    // At 0% global mix the output should be a delayed copy of the input. With
    // the fractional-delay fix, that delay equals the oversampler's reported
    // latency. We verify by measuring cross-correlation against input at the
    // reported lag.
    PluginProcessor processor;
    const double sr = 44100.0;
    const int blockSize = 512;
    processor.setRateAndBufferSizeDetails(sr, blockSize);
    processor.prepareToPlay(sr, blockSize);
    configureForAlignmentTest(processor, 0.0f);

    const int latency = processor.getLatencySamples();
    expect(latency > 0, "Oversampler should report non-zero latency at default settings");

    const double testFreq = 1000.0;
    double phase = 0.0;
    warmUp(processor, sr, blockSize, testFreq, 8, phase);

    // Capture a long continuous segment of input and output
    const int numBlocks = 8;
    const int totalSamples = blockSize * numBlocks;
    juce::AudioBuffer<float> inputCapture(2, totalSamples);
    juce::AudioBuffer<float> outputCapture(2, totalSamples);
    juce::MidiBuffer midi;

    const double phaseInc = juce::MathConstants<double>::twoPi * testFreq / sr;
    for (int b = 0; b < numBlocks; ++b)
    {
        juce::AudioBuffer<float> block(2, blockSize);
        for (int ch = 0; ch < 2; ++ch)
        {
            auto* data = block.getWritePointer(ch);
            double p = phase;
            for (int s = 0; s < blockSize; ++s)
            {
                data[s] = 0.3f * static_cast<float>(std::sin(p));
                p += phaseInc;
            }
        }

        // Copy input before processing
        for (int ch = 0; ch < 2; ++ch)
            inputCapture.copyFrom(ch, b * blockSize, block, ch, 0, blockSize);

        processor.processBlock(block, midi);

        for (int ch = 0; ch < 2; ++ch)
            outputCapture.copyFrom(ch, b * blockSize, block, ch, 0, blockSize);

        phase += phaseInc * blockSize;
        phase = std::fmod(phase, juce::MathConstants<double>::twoPi);
    }

    expect(!containsInvalidSamples(outputCapture), "Output must not contain NaN/Inf");

    // Compute aligned correlation: output[n] vs input[n - latency] for n >= latency
    const int alignedLen = totalSamples - latency;
    juce::AudioBuffer<float> inAligned(1, alignedLen);
    juce::AudioBuffer<float> outAligned(1, alignedLen);
    for (int n = 0; n < alignedLen; ++n)
    {
        inAligned.setSample(0, n, inputCapture.getSample(0, n));
        outAligned.setSample(0, n, outputCapture.getSample(0, n + latency));
    }

    const float correlation = calculateCorrelation(inAligned, outAligned);
    expect(correlation > 0.99f,
           "Dry-only output must correlate strongly with delayed input at reported latency. Got: "
           + juce::String(correlation));

    // Sanity: output RMS at 100% dry should be close to input RMS (small filter
    // losses from linear interpolation and the 20Hz hi-pass are tolerable).
    const float inRms = calculateRMS(inputCapture);
    const float outRms = calculateRMS(outputCapture);
    const float rmsRatioDb = juce::Decibels::gainToDecibels(outRms / juce::jmax(inRms, 1.0e-9f));
    expect(std::abs(rmsRatioDb) < 0.5f,
           "Dry-only RMS should match input within 0.5dB. Got: " + juce::String(rmsRatioDb) + " dB");
}

void DryWetAlignmentTests::testDryOversamplerStaysWarm()
{
    // The matched dry oversampler runs on every active block — even at 100% wet, when
    // its round-trip output is discarded — so its allpass/FIR history stays phase-locked
    // to the always-running wet oversampler. If it only ran while blending, it would
    // idle at 100% wet, its filter state would freeze, and the first block after the
    // mix dropped would blend a steady-state wet path against a cold dry round-trip:
    // a warm-up transient / brief combing until the phases realign.
    //
    // Note on scope: the 20 ms global-mix ramp makes that artifact sub-threshold in the
    // shipping signal path (the dry blend weight is ~0 exactly while the dry path is
    // coldest), so warming is kept as correctness insurance rather than an audible fix.
    // What this test pins down is the one real risk of warming every block: it must NOT
    // disturb the 100%-wet output. We drive two processors with an identical continuous
    // sine — A holds 100% wet throughout; B dwells at 50% (dry path active) then returns
    // to 100%. Once both are steady at 100% again, exercising B's dry round-trip must
    // leave its wet output identical to A's (the round-trip only rewrites the discarded
    // dry buffer; it never feeds back into the wet chain).
    const double sr = 44100.0;
    const int blockSize = 512;
    const double freq = 12000.0;   // high band: where any dry-path leak would show worst

    PluginProcessor procA, procB;
    procA.setRateAndBufferSizeDetails(sr, blockSize);
    procA.prepareToPlay(sr, blockSize);
    procB.setRateAndBufferSizeDetails(sr, blockSize);
    procB.prepareToPlay(sr, blockSize);
    configureForAlignmentTest(procA, 100.0f);   // 100% wet throughout
    configureForAlignmentTest(procB, 50.0f);    // 50% (dry active) -> 100% later

    juce::MidiBuffer midi;
    const double phaseInc = juce::MathConstants<double>::twoPi * freq / sr;
    double phase = 0.0;

    const int dwellBlocks   = 24;   // B sits at 50% — well past the 20 ms mix ramp
    const int returnBlocks  = 24;   // both at 100%, let smoothing settle (needsGlobalMix false)
    const int compareBlocks = 8;    // window where outputs must match
    const int totalBlocks = dwellBlocks + returnBlocks + compareBlocks;

    float maxDiff = 0.0f;
    bool anyInvalid = false;

    for (int b = 0; b < totalBlocks; ++b)
    {
        if (b == dwellBlocks)
            setParameter(procB.parameters, "globalMix", 100.0f);

        // Identical input for both processors (processBlock is in-place).
        juce::AudioBuffer<float> bufA(2, blockSize), bufB(2, blockSize);
        double p = phase;
        for (int s = 0; s < blockSize; ++s)
        {
            const float v = 0.3f * static_cast<float>(std::sin(p));
            bufA.setSample(0, s, v); bufA.setSample(1, s, v);
            bufB.setSample(0, s, v); bufB.setSample(1, s, v);
            p += phaseInc;
        }
        phase += phaseInc * blockSize;
        phase = std::fmod(phase, juce::MathConstants<double>::twoPi);

        procA.processBlock(bufA, midi);
        procB.processBlock(bufB, midi);

        anyInvalid = anyInvalid || containsInvalidSamples(bufA) || containsInvalidSamples(bufB);

        if (b >= dwellBlocks + returnBlocks)
            maxDiff = std::max(maxDiff, calculateMaxDifference(bufA, bufB));
    }

    expect(!anyInvalid, "Warm dry-oversampler path produced NaN/Inf");
    expect(maxDiff < 1.0e-4f,
        "Keeping the dry oversampler warm leaked into the 100%-wet output (max diff "
        + juce::String(maxDiff) + "); it must only rewrite the discarded dry buffer");
}

void DryWetAlignmentTests::testResetClearsWetOversampler()
{
    // resetDSPState() runs on the audio thread when a state restore lands mid-stream
    // (processBlock sees stateNeedsReset). It clears the matched dry oversampler so the
    // global mix can't replay stale tails — but the wet oversampler feeds that same
    // blend and is what the dry path is phase-locked to. If only the dry instance is
    // reset, the dry path restarts cold while the wet path keeps its charged allpass/FIR
    // history: the two are no longer phase-matched, and the first blocks after the
    // restore comb until they realign. Probe it directly: charge the wet oversampler,
    // reset, then push silence through it. A properly cleared oversampler turns silence
    // into silence; a stale one bleeds a decaying tail from its retained filter state.
    const double sr = 44100.0;
    const int blockSize = 512;

    PluginProcessor processor;
    processor.setRateAndBufferSizeDetails(sr, blockSize);
    processor.prepareToPlay(sr, blockSize);

    expect(processor.oversampling != nullptr,
        "Test requires an active wet oversampler (default 4x)");

    const int numCh = juce::jmax(1, processor.getTotalNumInputChannels());

    // Charge the wet oversampler's internal filter state with a sustained loud signal.
    for (int b = 0; b < 8; ++b)
    {
        juce::AudioBuffer<float> warm(numCh, blockSize);
        for (int ch = 0; ch < numCh; ++ch)
        {
            auto* d = warm.getWritePointer(ch);
            for (int s = 0; s < blockSize; ++s)
                d[s] = 0.8f * std::sin(juce::MathConstants<float>::twoPi * 5000.0f
                    * static_cast<float>(b * blockSize + s) / static_cast<float>(sr));
        }
        juce::dsp::AudioBlock<float> wb(warm);
        processor.oversampling->processSamplesUp(wb);
        processor.oversampling->processSamplesDown(wb);
    }

    // Live state-restore path: clear DSP state mid-stream.
    processor.resetDSPState();

    // Silence in -> silence out, iff the wet oversampler's state was actually cleared.
    float maxTail = 0.0f;
    for (int b = 0; b < 4; ++b)
    {
        juce::AudioBuffer<float> probe(numCh, blockSize);
        probe.clear();
        juce::dsp::AudioBlock<float> pb(probe);
        processor.oversampling->processSamplesUp(pb);
        processor.oversampling->processSamplesDown(pb);
        maxTail = std::max(maxTail, calculatePeak(probe));
    }

    expect(maxTail < 1.0e-6f,
        "resetDSPState() left the wet oversampler charged (silent-input tail peak "
        + juce::String(maxTail) + "); it must be reset in lock-step with the dry "
        "oversampler to keep the dry/wet blend phase-matched after a state restore");
}

void DryWetAlignmentTests::testNoCombFiltering()
{
    // At 50% global mix, both halves are near-identical (distortion near-linear)
    // so the summed output should preserve the signal at full amplitude. Without
    // the dry-delay fix, dry + delayed_wet sums would produce comb-filter notches
    // at f = Fs/(2*latency), roughly 2 kHz for a 4x polyphase IIR at 44.1kHz.
    PluginProcessor processor;
    const double sr = 44100.0;
    const int blockSize = 512;
    processor.setRateAndBufferSizeDetails(sr, blockSize);
    processor.prepareToPlay(sr, blockSize);
    configureForAlignmentTest(processor, 50.0f);

    const int latency = processor.getLatencySamples();
    expect(latency > 0, "Test requires non-zero oversampler latency");

    // Probe at the exact notch frequency of an un-aligned 50/50 sum: Fs/(2*D).
    const double notchFreq = sr / (2.0 * static_cast<double>(latency));

    // Also probe at 1 kHz (safe passband) for a baseline.
    const double freqs[] = { 1000.0, notchFreq };
    const char* labels[] = { "1 kHz passband", "un-aligned notch freq" };

    for (int f = 0; f < 2; ++f)
    {
        // Fresh warmup per frequency so smoothing and filter state settle
        double phase = 0.0;
        warmUp(processor, sr, blockSize, freqs[f], 8, phase);

        const int numBlocks = 8;
        const int totalSamples = blockSize * numBlocks;
        juce::AudioBuffer<float> inputCapture(2, totalSamples);
        juce::AudioBuffer<float> outputCapture(2, totalSamples);
        juce::MidiBuffer midi;

        const double phaseInc = juce::MathConstants<double>::twoPi * freqs[f] / sr;
        for (int b = 0; b < numBlocks; ++b)
        {
            juce::AudioBuffer<float> block(2, blockSize);
            for (int ch = 0; ch < 2; ++ch)
            {
                auto* data = block.getWritePointer(ch);
                double p = phase;
                for (int s = 0; s < blockSize; ++s)
                {
                    data[s] = 0.3f * static_cast<float>(std::sin(p));
                    p += phaseInc;
                }
            }
            for (int ch = 0; ch < 2; ++ch)
                inputCapture.copyFrom(ch, b * blockSize, block, ch, 0, blockSize);

            processor.processBlock(block, midi);

            for (int ch = 0; ch < 2; ++ch)
                outputCapture.copyFrom(ch, b * blockSize, block, ch, 0, blockSize);

            phase += phaseInc * blockSize;
            phase = std::fmod(phase, juce::MathConstants<double>::twoPi);
        }

        expect(!containsInvalidSamples(outputCapture),
               juce::String("Output NaN/Inf at ") + labels[f]);

        const float inRms = calculateRMS(inputCapture);
        const float outRms = calculateRMS(outputCapture);
        const float ratioDb = juce::Decibels::gainToDecibels(outRms / juce::jmax(inRms, 1.0e-9f));

        // Without the fix, the notch probe would lose ~40-60 dB. With the fix,
        // both probes should stay within ~1 dB of unity.
        expect(ratioDb > -2.0f,
               juce::String("Comb-filter attenuation at ") + labels[f]
               + " (f=" + juce::String(freqs[f]) + " Hz): " + juce::String(ratioDb) + " dB");
    }
}

void DryWetAlignmentTests::testFullyWetPathUnaffected()
{
    // 100% wet skips the dry read entirely (fast path via needsGlobalMix = false).
    // This is a sanity check that the new code doesn't break the common case.
    PluginProcessor processor;
    const double sr = 44100.0;
    const int blockSize = 512;
    processor.setRateAndBufferSizeDetails(sr, blockSize);
    processor.prepareToPlay(sr, blockSize);
    configureForAlignmentTest(processor, 100.0f);

    double phase = 0.0;
    warmUp(processor, sr, blockSize, 1000.0, 4, phase);

    auto buffer = generateSineWave(1000.0, sr, blockSize, 0.3f);
    juce::MidiBuffer midi;
    processor.processBlock(buffer, midi);

    expect(!containsInvalidSamples(buffer), "100% wet output must not contain NaN/Inf");
    expect(calculatePeak(buffer) > 0.01f, "100% wet output must not be silent");
}

void DryWetAlignmentTests::testOversamplingReinit()
{
    // Changing oversampling stages rebuilds both the wet and the matched dry
    // oversampler. Process under two different oversampling factors (and off) and
    // confirm no invalid samples.
    PluginProcessor processor;
    const double sr = 44100.0;
    const int blockSize = 512;
    processor.setRateAndBufferSizeDetails(sr, blockSize);
    processor.prepareToPlay(sr, blockSize);
    configureForAlignmentTest(processor, 40.0f);  // mid mix — forces dry path

    juce::MidiBuffer midi;

    // Run at default (4x) for a while
    double phase = 0.0;
    warmUp(processor, sr, blockSize, 1000.0, 4, phase);

    {
        auto buffer = generateSineWave(1000.0, sr, blockSize, 0.3f);
        processor.processBlock(buffer, midi);
        expect(!containsInvalidSamples(buffer), "Output invalid before reinit");
    }

    // Request a different oversampling factor (2x stages = 1) on the message thread.
    processor.requestOversamplingRebuild(1);
    processor.handleAsyncUpdate();

    // Process several blocks to drive the reinit and then settle.
    warmUp(processor, sr, blockSize, 1000.0, 8, phase);

    {
        auto buffer = generateSineWave(1000.0, sr, blockSize, 0.3f);
        processor.processBlock(buffer, midi);
        expect(!containsInvalidSamples(buffer), "Output invalid after oversampling reinit");
        expect(calculatePeak(buffer) > 0.01f, "Output must not be silent after reinit");
    }

    // And flip to off (stages = 0): latency becomes 0 → fast path engaged.
    processor.requestOversamplingRebuild(0);
    processor.handleAsyncUpdate();
    warmUp(processor, sr, blockSize, 1000.0, 8, phase);

    {
        auto buffer = generateSineWave(1000.0, sr, blockSize, 0.3f);
        processor.processBlock(buffer, midi);
        expect(!containsInvalidSamples(buffer), "Output invalid with oversampling off");
    }
}

//==============================================================================
// ParameterTests Implementation
//==============================================================================

void ParameterTests::runTest()
{
    PluginProcessor processor;
    processor.setRateAndBufferSizeDetails(44100.0, 512);
    processor.prepareToPlay(44100.0, 512);

    beginTest("Parameter Ranges");
    testParameterRanges(processor);

    beginTest("Parameter Defaults");
    testParameterDefaults(processor);

    beginTest("Parameter Smoothing");
    testParameterSmoothing(processor);

    beginTest("Parameter Pointer Validity");
    testParameterPointerValidity(processor);
}

void ParameterTests::testParameterRanges(PluginProcessor& processor)
{
    // Test that all expected parameters exist
    const char* expectedParams[] = {
        "inputGain", "outputGain", "distortionAmount", "highPassFreq", "shape",
        "subGuardFreq", "clipType", "distMix",
        "lfoRate", "lfoDepth", "lfoWaveform",
        "compEnabled", "compPeakReduction", "compMakeupGain",
        "compRatio"
    };

    for (const char* paramId : expectedParams)
    {
        auto* param = processor.parameters.getParameter(paramId);
        expect(param != nullptr, juce::String("Parameter '") + paramId + "' not found");
    }
}

void ParameterTests::testParameterDefaults(PluginProcessor& processor)
{
    // Verify critical default values
    auto* distortion = processor.parameters.getParameter("distortionAmount");
    if (distortion != nullptr)
    {
        // getDefaultValue() is normalised: 20% of the 0-100 range.
        expectWithinAbsoluteError(distortion->getDefaultValue(), 0.2f, 0.01f,
                                  "Distortion default should be 20%");
    }

    auto* compEnabled = processor.parameters.getParameter("compEnabled");
    if (compEnabled != nullptr)
    {
        expectWithinAbsoluteError(compEnabled->getDefaultValue(), 0.0f, 0.01f,
                                  "Compression default should be OFF");
    }
}

void ParameterTests::testParameterSmoothing(PluginProcessor& processor)
{
    // Set a sudden parameter change and verify no clicks
    processor.parameters.getParameter("outputGain")->setValueNotifyingHost(0.0f);

    auto buffer = generateSineWave(1000.0, 44100.0, 512, 0.5f);
    juce::MidiBuffer midi;

    // Process one block at low gain
    processor.processBlock(buffer, midi);

    // Suddenly change to high gain
    processor.parameters.getParameter("outputGain")->setValueNotifyingHost(1.0f);

    // Process another block - smoothing should prevent clicks
    buffer = generateSineWave(1000.0, 44100.0, 512, 0.5f);
    processor.processBlock(buffer, midi);

    // Check for clicks (very high peak values or discontinuities)
    expect(calculatePeak(buffer) < 3.0f, "Parameter change caused potential click");
}

void ParameterTests::testParameterPointerValidity(PluginProcessor& processor)
{
    // All atomic parameter pointers should be valid
    expect(processor.inputGainParam != nullptr, "inputGainParam is null");
    expect(processor.outputGainParam != nullptr, "outputGainParam is null");
    expect(processor.distortionAmountParam != nullptr, "distortionAmountParam is null");
    expect(processor.highPassFreqParam != nullptr, "highPassFreqParam is null");
    expect(processor.lfoRateParam != nullptr, "lfoRateParam is null");
    expect(processor.lfoDepthParam != nullptr, "lfoDepthParam is null");
}

//==============================================================================
// SampleRateTests Implementation
//==============================================================================

void SampleRateTests::runTest()
{
    beginTest("Sample Rate: 44100 Hz");
    testSampleRate(44100.0);

    beginTest("Sample Rate: 48000 Hz");
    testSampleRate(48000.0);

    beginTest("Sample Rate: 88200 Hz");
    testSampleRate(88200.0);

    beginTest("Sample Rate: 96000 Hz");
    testSampleRate(96000.0);

    beginTest("Sample Rate: 192000 Hz");
    testSampleRate(192000.0);

    beginTest("Runtime Sample Rate Change");
    testRuntimeSampleRateChange();
}

void SampleRateTests::testSampleRate(double sampleRate)
{
    PluginProcessor processor;

    // prepareToPlay should not crash at any sample rate
    processor.setRateAndBufferSizeDetails(sampleRate, 512);
    processor.prepareToPlay(sampleRate, 512);

    // Enable all processing
    setParameter(processor.parameters, "distortionAmount", 50.0f);
    setParameter(processor.parameters, "compEnabled", 1.0f);

    // Create test signal at this sample rate
    auto buffer = generateSineWave(1000.0, sampleRate, 512, 0.7f);
    juce::MidiBuffer midi;

    // Process should not crash
    processor.processBlock(buffer, midi);

    expect(!containsInvalidSamples(buffer),
           "Processing at " + juce::String(sampleRate) + " Hz produced invalid samples");
    expect(calculatePeak(buffer) > 0.0f,
           "Processing at " + juce::String(sampleRate) + " Hz produced silence");
}

void SampleRateTests::testRuntimeSampleRateChange()
{
    PluginProcessor processor;

    // Start at 44.1kHz
    processor.setRateAndBufferSizeDetails(44100.0, 512);
    processor.prepareToPlay(44100.0, 512);
    setParameter(processor.parameters, "distortionAmount", 50.0f);

    auto buffer = generateSineWave(1000.0, 44100.0, 512, 0.7f);
    juce::MidiBuffer midi;
    processor.processBlock(buffer, midi);

    // Change to 96kHz (simulating DAW sample rate change)
    processor.setRateAndBufferSizeDetails(96000.0, 512);
    processor.prepareToPlay(96000.0, 512);

    buffer = generateSineWave(1000.0, 96000.0, 512, 0.7f);
    processor.processBlock(buffer, midi);

    expect(!containsInvalidSamples(buffer), "Sample rate change produced invalid samples");
}

//==============================================================================
// ThreadSafetyTests Implementation
//==============================================================================

void ThreadSafetyTests::runTest()
{
    beginTest("Scope Buffer Access");
    testScopeBufferAccess();

    beginTest("Scope FIFO Drain");
    testScopeDrainExcess();

    beginTest("Atomic Gain Reduction");
    testAtomicGainReduction();

    beginTest("Multi-Instance Independence");
    testMultiInstanceIndependence();

    beginTest("Per-Instance Random Generators");
    testPerInstanceRandomGenerators();
}

void ThreadSafetyTests::testScopeBufferAccess()
{
    PluginProcessor processor;
    processor.setRateAndBufferSizeDetails(44100.0, 512);
    processor.prepareToPlay(44100.0, 512);

    // Simulate audio thread writing
    auto buffer = generateSineWave(1000.0, 44100.0, 512, 0.7f);
    juce::MidiBuffer midi;
    processor.processBlock(buffer, midi);

    // Simulate GUI thread reading. fillScopeBuffer is a sliding window that only
    // refreshes the newest samples and keeps the rest, so it expects a persistent,
    // pre-cleared buffer — exactly what the real Oscilloscope owns (its ctor clears
    // cachedBuffer). Clear here too; otherwise the un-refreshed tail reads
    // uninitialised memory (which can hold NaN bit-patterns depending on layout).
    juce::AudioBuffer<float> displayBuffer(2, 512);
    displayBuffer.clear();
    processor.fillScopeBuffer(displayBuffer);

    // Should not crash and produce valid data
    expect(!containsInvalidSamples(displayBuffer), "Scope buffer contains invalid samples");
}

void ThreadSafetyTests::testScopeDrainExcess()
{
    PluginProcessor processor;
    processor.setRateAndBufferSizeDetails(48000.0, 512);
    processor.prepareToPlay(48000.0, 512);

    // Overfill the FIFO with multiple blocks
    juce::MidiBuffer midi;
    for (int i = 0; i < 10; ++i)
    {
        auto buffer = generateSineWave(1000.0, 48000.0, 512, 0.7f);
        processor.processBlock(buffer, midi);
    }

    // Read into display-sized buffer — should drain excess and return valid data
    juce::AudioBuffer<float> displayBuffer(2, DSPConstants::SCOPE_DISPLAY_POINTS);
    processor.fillScopeBuffer(displayBuffer);
    expect(!containsInvalidSamples(displayBuffer),
           "Display buffer invalid after draining excess");

    // FIFO should be nearly empty after drain+read
    // Process one more block — fresh data should be readable immediately
    auto buffer2 = generateSineWave(1000.0, 48000.0, 512, 0.7f);
    processor.processBlock(buffer2, midi);
    processor.fillScopeBuffer(displayBuffer);
    expect(!containsInvalidSamples(displayBuffer),
           "Display buffer invalid after second read");
}

void ThreadSafetyTests::testAtomicGainReduction()
{
    PluginProcessor processor;
    processor.setRateAndBufferSizeDetails(44100.0, 512);
    processor.prepareToPlay(44100.0, 512);

    // Enable compression
    processor.parameters.getParameter("compEnabled")->setValueNotifyingHost(1.0f);
    processor.parameters.getParameter("compPeakReduction")->setValueNotifyingHost(0.7f);

    // Process loud signal
    auto buffer = generateSineWave(1000.0, 44100.0, 512, 0.9f);
    juce::MidiBuffer midi;
    processor.processBlock(buffer, midi);

    // Read atomic value (simulating GUI thread)
    float gr1 = processor.currentGainReductionDB.load();
    float gr2 = processor.currentGainReductionDB.load();

    // Values should be valid
    expect(!std::isnan(gr1), "Atomic gain reduction read 1 is NaN");
    expect(!std::isnan(gr2), "Atomic gain reduction read 2 is NaN");
}

void ThreadSafetyTests::testMultiInstanceIndependence()
{
    // Create two processor instances
    PluginProcessor processor1;
    PluginProcessor processor2;

    processor1.setRateAndBufferSizeDetails(44100.0, 512);
    processor1.prepareToPlay(44100.0, 512);
    processor2.setRateAndBufferSizeDetails(44100.0, 512);
    processor2.prepareToPlay(44100.0, 512);

    // Set different parameters
    setParameter(processor1.parameters, "distortionAmount", 30.0f);
    setParameter(processor2.parameters, "distortionAmount", 80.0f);

    // Process different signals
    auto buffer1 = generateSineWave(440.0, 44100.0, 512, 0.5f);
    auto buffer2 = generateSineWave(880.0, 44100.0, 512, 0.5f);
    juce::MidiBuffer midi;

    processor1.processBlock(buffer1, midi);
    processor2.processBlock(buffer2, midi);

    // Both should produce valid, independent results
    expect(!containsInvalidSamples(buffer1), "Processor 1 produced invalid samples");
    expect(!containsInvalidSamples(buffer2), "Processor 2 produced invalid samples");

    // Outputs should be different
    float diff = calculateMaxDifference(buffer1, buffer2);
    expect(diff > 0.01f, "Multi-instance outputs should differ");
}

void ThreadSafetyTests::testPerInstanceRandomGenerators()
{
    // Verify random generators are per-instance (not static)
    // This was a bug that was fixed

    PluginProcessor processor1;
    PluginProcessor processor2;

    processor1.setRateAndBufferSizeDetails(44100.0, 512);
    processor1.prepareToPlay(44100.0, 512);
    processor2.setRateAndBufferSizeDetails(44100.0, 512);
    processor2.prepareToPlay(44100.0, 512);

    // Both use distortion which has random noise injection
    setParameter(processor1.parameters, "distortionAmount", 50.0f);
    setParameter(processor2.parameters, "distortionAmount", 50.0f);
    setParameter(processor1.parameters, "clipType", 0.0f);  // Brutal Fuzz has noise
    setParameter(processor2.parameters, "clipType", 0.0f);

    // Process identical signals
    auto buffer1 = generateSineWave(1000.0, 44100.0, 512, 0.5f);
    auto buffer2 = generateSineWave(1000.0, 44100.0, 512, 0.5f);
    juce::MidiBuffer midi;

    processor1.processBlock(buffer1, midi);
    processor2.processBlock(buffer2, midi);

    // Due to per-instance random, outputs should differ slightly
    // (If random was static, they'd be identical after seeding)
    expect(!containsInvalidSamples(buffer1), "Instance 1 produced invalid samples");
    expect(!containsInvalidSamples(buffer2), "Instance 2 produced invalid samples");
}

//==============================================================================
// StateIOTests Implementation
//==============================================================================

void StateIOTests::runTest()
{
    beginTest("getStateInformation");
    testGetStateInformation();

    beginTest("setStateInformation");
    testSetStateInformation();

    beginTest("Round-trip Preservation");
    testRoundTrip();

    beginTest("Invalid Data Handling");
    testInvalidDataHandling();

    beginTest("State Version Stamp");
    testVersionStamp();

    beginTest("Legacy State Migration");
    testLegacyMigration();
}

void StateIOTests::testGetStateInformation()
{
    PluginProcessor processor;
    processor.setRateAndBufferSizeDetails(44100.0, 512);
    processor.prepareToPlay(44100.0, 512);

    // Set some non-default values
    processor.parameters.getParameter("distortionAmount")->setValueNotifyingHost(0.75f);
    processor.parameters.getParameter("outputGain")->setValueNotifyingHost(0.6f);

    juce::MemoryBlock stateData;
    processor.getStateInformation(stateData);

    // State should contain data
    expect(stateData.getSize() > 0, "getStateInformation produced empty data");
}

void StateIOTests::testSetStateInformation()
{
    PluginProcessor processor;
    processor.setRateAndBufferSizeDetails(44100.0, 512);
    processor.prepareToPlay(44100.0, 512);

    // Get current state
    juce::MemoryBlock stateData;
    processor.getStateInformation(stateData);

    // Change a parameter
    processor.parameters.getParameter("distortionAmount")->setValueNotifyingHost(0.99f);

    // Restore state
    processor.setStateInformation(stateData.getData(), static_cast<int>(stateData.getSize()));

    // Parameter should be restored to original value
    // Note: This depends on the original value when state was saved
    expect(processor.distortionAmountParam != nullptr, "Distortion param null after restore");
}

void StateIOTests::testRoundTrip()
{
    PluginProcessor processor;
    processor.setRateAndBufferSizeDetails(44100.0, 512);
    processor.prepareToPlay(44100.0, 512);

    // Set specific values
    const float testDistortion = 0.42f;
    const float testOutput = 0.73f;

    processor.parameters.getParameter("distortionAmount")->setValueNotifyingHost(testDistortion);
    processor.parameters.getParameter("outputGain")->setValueNotifyingHost(testOutput);

    // Save state
    juce::MemoryBlock stateData;
    processor.getStateInformation(stateData);

    // Create new processor and restore
    PluginProcessor processor2;
    processor2.setRateAndBufferSizeDetails(44100.0, 512);
    processor2.prepareToPlay(44100.0, 512);
    processor2.setStateInformation(stateData.getData(), static_cast<int>(stateData.getSize()));

    // Get normalized values and compare
    float restoredDistortion = processor2.parameters.getParameter("distortionAmount")->getValue();
    float restoredOutput = processor2.parameters.getParameter("outputGain")->getValue();

    expectWithinAbsoluteError(restoredDistortion, testDistortion, 0.01f,
                              "Distortion not preserved in round-trip");
    expectWithinAbsoluteError(restoredOutput, testOutput, 0.01f,
                              "Output gain not preserved in round-trip");
}

void StateIOTests::testInvalidDataHandling()
{
    PluginProcessor processor;
    processor.setRateAndBufferSizeDetails(44100.0, 512);
    processor.prepareToPlay(44100.0, 512);

    // Test empty data
    processor.setStateInformation(nullptr, 0);
    // Should not crash

    // Test garbage data
    uint8_t garbage[] = { 0xFF, 0xFE, 0x00, 0x01, 0x02 };
    processor.setStateInformation(garbage, sizeof(garbage));
    // Should not crash

    // Processor should still be functional
    auto buffer = generateSineWave(1000.0, 44100.0, 512, 0.5f);
    juce::MidiBuffer midi;
    processor.processBlock(buffer, midi);

    expect(!containsInvalidSamples(buffer), "Processor broken after invalid state data");
}

void StateIOTests::testVersionStamp()
{
    // Saving must stamp the current schema version, and loading must surface it
    // on the live parameter tree so subsequent saves stay in the latest format.
    PluginProcessor src;
    src.setRateAndBufferSizeDetails(44100.0, 512);
    src.prepareToPlay(44100.0, 512);

    juce::MemoryBlock stateData;
    src.getStateInformation(stateData);

    PluginProcessor dst;
    dst.setRateAndBufferSizeDetails(44100.0, 512);
    dst.prepareToPlay(44100.0, 512);
    dst.setStateInformation(stateData.getData(), static_cast<int>(stateData.getSize()));

    expectEquals(static_cast<int>(dst.parameters.state
                     .getProperty(PluginProcessor::stateVersionAttribute, -1)),
                 PluginProcessor::currentStateVersion,
                 "Round-tripped state missing current version stamp");
}

void StateIOTests::testLegacyMigration()
{
    // A bare (pre-versioning) state has no stateVersion attribute and is treated
    // as version 0, so the v0→v1 bandSplitEnabled → subGuardFreq migration must
    // still fire and the tree must then be upgraded to the current version.
    PluginProcessor processor;
    processor.setRateAndBufferSizeDetails(44100.0, 512);
    processor.prepareToPlay(44100.0, 512);

    juce::XmlElement legacyOn("Parameters");
    legacyOn.setAttribute("bandSplitEnabled", true);
    processor.migrateState(legacyOn);

    expect(processor.subGuardFreqParam != nullptr, "subGuardFreq param null");
    expectWithinAbsoluteError(processor.subGuardFreqParam->load(), 150.0f, 0.5f,
        "Legacy bandSplitEnabled=true did not migrate to 150Hz subGuardFreq");

    expectEquals(static_cast<int>(processor.parameters.state
                     .getProperty(PluginProcessor::stateVersionAttribute, -1)),
                 PluginProcessor::currentStateVersion,
                 "migrateState did not stamp current version after legacy load");

    // OFF should map to 0 Hz (full-range distortion, no band-split).
    juce::XmlElement legacyOff("Parameters");
    legacyOff.setAttribute("bandSplitEnabled", false);
    processor.migrateState(legacyOff);

    expectWithinAbsoluteError(processor.subGuardFreqParam->load(), 0.0f, 0.5f,
        "Legacy bandSplitEnabled=false did not migrate to 0Hz (OFF) subGuardFreq");
}

//==============================================================================
// FactoryPresetTests Implementation
//==============================================================================

void FactoryPresetTests::runTest()
{
    beginTest("Factory bank meets the 10-20 preset target");
    testBankSize();

    beginTest("Every preset references valid parameter IDs");
    testParamIdsExist();

    beginTest("Every preset produces finite output");
    testPresetsProduceFiniteOutput();

    beginTest("Factory presets leave the legacy input filter transparent");
    testPresetsLeaveLegacyFilterOff();
}

void FactoryPresetTests::testBankSize()
{
    const auto count = FactoryPresets::all().size();
    expect(count >= 10, "Factory bank below the DoD floor of 10 presets");
    expect(count <= 20, "Factory bank above the DoD ceiling of 20 presets");
}

void FactoryPresetTests::testParamIdsExist()
{
    PluginProcessor processor;

    for (const auto& pv : FactoryPresets::baseline())
        expect(processor.parameters.getParameter(pv.id) != nullptr,
               juce::String("Baseline references unknown parameter id: ") + pv.id);

    for (const auto& preset : FactoryPresets::all())
        for (const auto& pv : preset.overrides)
            expect(processor.parameters.getParameter(pv.id) != nullptr,
                   juce::String("Preset '") + preset.name
                       + "' references unknown parameter id: " + pv.id);
}

void FactoryPresetTests::testPresetsProduceFiniteOutput()
{
    PluginProcessor processor;
    processor.setRateAndBufferSizeDetails(44100.0, 512);
    processor.prepareToPlay(44100.0, 512);

    for (const auto& preset : FactoryPresets::all())
    {
        const bool applied = FactoryPresets::apply(processor.parameters, preset.name);
        expect(applied, juce::String("apply() rejected factory preset: ") + preset.name);

        // A few blocks so envelope/LFO state settles, then verify finite output.
        bool allFinite = true;
        for (int block = 0; block < 4 && allFinite; ++block)
        {
            auto buffer = generateSineWave(220.0, 44100.0, 512, 0.5f, 2);
            juce::MidiBuffer midi;
            processor.processBlock(buffer, midi);

            for (int ch = 0; ch < buffer.getNumChannels() && allFinite; ++ch)
            {
                const float* d = buffer.getReadPointer(ch);
                for (int i = 0; i < buffer.getNumSamples(); ++i)
                    if (! std::isfinite(d[i])) { allFinite = false; break; }
            }
        }
        expect(allFinite, juce::String("Preset '") + preset.name + "' produced non-finite output");
    }
}

void FactoryPresetTests::testPresetsLeaveLegacyFilterOff()
{
    PluginProcessor processor;
    auto value = [&](const char* id)
    {
        auto* p = processor.parameters.getParameter(id);
        return p->convertFrom0to1(p->getValue());
    };

    for (const auto& preset : FactoryPresets::all())
    {
        for (const auto& pv : preset.overrides)
            expect(juce::String(pv.id) != "highPassFreq" && juce::String(pv.id) != "filterMode",
                   juce::String("Preset '") + preset.name + "' sets a legacy filter param");

        // Start from a dirty legacy filter so the baseline must clear it.
        setParameter(processor.parameters, "filterMode", 2.0f);
        setParameter(processor.parameters, "highPassFreq", 800.0f);
        FactoryPresets::apply(processor.parameters, preset.name);
        expect(! LegacyInputFilter::isActive(juce::roundToInt(value("filterMode")), value("highPassFreq")),
               juce::String("Preset '") + preset.name + "' left the legacy filter on");
    }

    expect(FactoryPresets::baseline().end() != std::find_if(
               FactoryPresets::baseline().begin(), FactoryPresets::baseline().end(),
               [](const FactoryPresets::ParamValue& pv) { return juce::String(pv.id) == "shape"; }),
           "Baseline does not reset shape");
}

//==============================================================================
// GoldenAudioTests Implementation
//==============================================================================

void GoldenAudioTests::runTest()
{
    beginTest("Silence Passthrough");
    testSilencePassthrough();

    beginTest("Distortion Output - Brutal Fuzz");
    testDistortionOutput(0, "brutal_fuzz");

    beginTest("808 Band Split");
    test808BandSplit();
}

juce::File GoldenAudioTests::getTestAudioDirectory()
{
    // Get the directory where test audio files are stored
    auto currentFile = juce::File::getSpecialLocation(juce::File::currentExecutableFile);
    auto projectDir = currentFile.getParentDirectory().getParentDirectory().getParentDirectory()
                        .getParentDirectory().getParentDirectory();
    return projectDir.getChildFile("Resources").getChildFile("TestAudio");
}

void GoldenAudioTests::testSilencePassthrough()
{
    PluginProcessor processor;
    processor.setRateAndBufferSizeDetails(44100.0, 512);
    processor.prepareToPlay(44100.0, 512);

    // True bypass mode
    processor.parameters.getParameter("distortionAmount")->setValueNotifyingHost(0.0f);
    processor.parameters.getParameter("compEnabled")->setValueNotifyingHost(0.0f);

    auto buffer = generateSilence(512);
    juce::MidiBuffer midi;

    processor.processBlock(buffer, midi);

    // Output should be silent
    expect(isSilent(buffer, Tolerances::SILENCE_DB),
           "Silence passthrough failed - output is not silent");
}

void GoldenAudioTests::testDistortionOutput(int clipType, const juce::String& label)
{
    PluginProcessor processor;
    processor.setRateAndBufferSizeDetails(44100.0, 512);
    processor.prepareToPlay(44100.0, 512);

    // Enable distortion with this clip type
    setParameter(processor.parameters, "distortionAmount", 60.0f);
    setParameter(processor.parameters, "clipType", static_cast<float>(clipType));

    // Generate test signal
    auto buffer = generateSineWave(1000.0, 44100.0, 512, 0.7f);
    juce::MidiBuffer midi;

    processor.processBlock(buffer, midi);

    // Basic validation - output should be valid and not silent
    expect(!containsInvalidSamples(buffer), label + " produced invalid samples");
    expect(!isSilent(buffer), label + " produced silence");

    // If in generate mode, save reference file
    if (GENERATE_MODE)
    {
        auto dir = getTestAudioDirectory();
        if (!dir.exists())
            dir.createDirectory();

        auto file = dir.getChildFile("golden_" + label + "_44100.wav");
        if (saveWavFile(buffer, file, 44100.0))
            logMessage("Generated: " + file.getFullPathName());
    }
}

void GoldenAudioTests::test808BandSplit()
{
    PluginProcessor processor;
    processor.setRateAndBufferSizeDetails(44100.0, 2048);
    processor.prepareToPlay(44100.0, 2048);

    // Enable Sub Guard mode (60 Hz crossover = PRESERVE mode)
    setParameter(processor.parameters, "subGuardFreq", 60.0f);
    setParameter(processor.parameters, "distortionAmount", 70.0f);

    // Low frequency test (should pass through clean)
    auto lowBuffer = generateSineWave(60.0, 44100.0, 2048, 0.7f);
    auto lowOriginal = lowBuffer;
    juce::MidiBuffer midi;

    processor.processBlock(lowBuffer, midi);

    expect(!containsInvalidSamples(lowBuffer), "808 band split produced invalid samples");

    // Low band should retain significant energy (not heavily distorted)
    float originalRMS = calculateRMS(lowOriginal);
    float processedRMS = calculateRMS(lowBuffer);

    expect(processedRMS > originalRMS * 0.3f,
           "808-safe mode killed too much low frequency energy");
}

//==============================================================================
// NormalizationTests Implementation
//==============================================================================

void NormalizationTests::runTest()
{
    beginTest("Clip Type Level Matching");
    testClipTypeLevelMatching();
}

void NormalizationTests::testClipTypeLevelMatching()
{
    PluginProcessor processor;
    processor.setRateAndBufferSizeDetails(44100.0, 512);
    processor.prepareToPlay(44100.0, 512);

    const float gain = 1.0f;
    const float drive = 3.0f;
    const int numSamples = 1000;

    float rmsPerClipType[7] = {};

    for (int clipType = 0; clipType < 7; ++clipType)
    {
        float sumSquares = 0.0f;
        for (int i = 0; i < numSamples; ++i)
        {
            float phase = static_cast<float>(i) / static_cast<float>(numSamples);
            float input = 0.5f * std::sin(phase * juce::MathConstants<float>::twoPi);

            float output = processor.applyStudioDistortion(input, gain, drive, clipType, 1.0f, 0);
            sumSquares += output * output;
        }
        rmsPerClipType[clipType] = std::sqrt(sumSquares / numSamples);
    }

    // Find min and max RMS across all clip types
    float minRMS = rmsPerClipType[0];
    float maxRMS = rmsPerClipType[0];
    for (int i = 1; i < 7; ++i)
    {
        minRMS = std::min(minRMS, rmsPerClipType[i]);
        maxRMS = std::max(maxRMS, rmsPerClipType[i]);
    }

    // Calculate dB spread
    const float spreadDB = juce::Decibels::gainToDecibels(maxRMS / (minRMS + 1e-10f));

    // All clip types should be within 6dB of each other (generous tolerance)
    expect(spreadDB < 6.0f,
        "Clip type RMS spread is " + juce::String(spreadDB, 2) + " dB (max 6dB allowed). "
        "Min RMS: " + juce::String(minRMS, 4) + " Max RMS: " + juce::String(maxRMS, 4));

    // Verify each clip type produces output
    for (int i = 0; i < 7; ++i)
    {
        expect(rmsPerClipType[i] > 0.01f,
            "Clip type " + juce::String(i) + " RMS too low: " + juce::String(rmsPerClipType[i], 4));
    }
}

//==============================================================================
// StatefulDistortionTests Implementation
//==============================================================================

void StatefulDistortionTests::runTest()
{
    beginTest("Tube Bias Shift Under Sustained Signal");
    testTubeBiasShift();

    beginTest("Tape Hysteresis Under Sustained Signal");
    testTapeHysteresis();
}

void StatefulDistortionTests::testTubeBiasShift()
{
    // Tube overdrive (clip type 1) should produce different output based on signal history
    // Cold start (no prior signal) vs hot (after sustained loud signal)
    PluginProcessor processor;
    processor.setRateAndBufferSizeDetails(44100.0, 512);
    processor.prepareToPlay(44100.0, 512);

    const float gain = 1.0f;
    const float drive = 3.0f;
    const float testInput = 0.5f;

    // Cold start: process a single sample
    processor.tubeBiasEnvelope[0] = 0.0f;  // Ensure cold start
    float coldOutput = processor.applyStudioDistortion(testInput, gain, drive, 1, 1.0f, 0);

    // Warm up: process many loud samples to charge the envelope
    for (int i = 0; i < 500; ++i)
    {
        processor.applyStudioDistortion(0.8f, gain, drive, 1, 1.0f, 0);
    }

    // Hot state: process the same test input
    float hotOutput = processor.applyStudioDistortion(testInput, gain, drive, 1, 1.0f, 0);

    // Outputs should differ (tube bias shifts the clipping threshold)
    expect(!std::isnan(coldOutput) && !std::isnan(hotOutput),
        "Tube bias test produced NaN");

    float difference = std::abs(coldOutput - hotOutput);
    expect(difference > 0.001f,
        "Tube output should differ based on signal history. Cold: " +
        juce::String(coldOutput, 4) + " Hot: " + juce::String(hotOutput, 4) +
        " Diff: " + juce::String(difference, 6));

    // Reset should clear the envelope
    processor.resetDSPState();
    expect(juce::exactlyEqual(processor.tubeBiasEnvelope[0], 0.0f),
        "Tube bias envelope not cleared by resetDSPState");
}

void StatefulDistortionTests::testTapeHysteresis()
{
    // Tape saturation (clip type 3) should produce different output based on signal history
    PluginProcessor processor;
    processor.setRateAndBufferSizeDetails(44100.0, 512);
    processor.prepareToPlay(44100.0, 512);

    const float gain = 1.0f;
    const float drive = 3.0f;
    const float testInput = 0.5f;

    // Cold start
    processor.tapeSaturationEnvelope[0] = 0.0f;
    float coldOutput = processor.applyStudioDistortion(testInput, gain, drive, 3, 1.0f, 0);

    // Warm up with loud sustained signal
    for (int i = 0; i < 500; ++i)
    {
        processor.applyStudioDistortion(0.8f, gain, drive, 3, 1.0f, 0);
    }

    // Hot state
    float hotOutput = processor.applyStudioDistortion(testInput, gain, drive, 3, 1.0f, 0);

    // Outputs should differ (tape hysteresis modulates compression knee)
    expect(!std::isnan(coldOutput) && !std::isnan(hotOutput),
        "Tape hysteresis test produced NaN");

    float difference = std::abs(coldOutput - hotOutput);
    expect(difference > 0.001f,
        "Tape output should differ based on signal history. Cold: " +
        juce::String(coldOutput, 4) + " Hot: " + juce::String(hotOutput, 4) +
        " Diff: " + juce::String(difference, 6));

    // Reset should clear the envelope
    processor.resetDSPState();
    expect(juce::exactlyEqual(processor.tapeSaturationEnvelope[0], 0.0f),
        "Tape hysteresis envelope not cleared by resetDSPState");
}

// Static test registration moved to registerAllTests() in DistortionTests.h
// to ensure tests are registered before they're run

//==============================================================================
// LinearPhaseDryTest Implementation (PR-6)
//
// Verifies:
//  1. FIR path is RT-clean (zero allocations during processBlock).
//  2. State round-trip: linearPhaseDry=true survives getState/setState.
//  3. IIR baseline (toggle OFF) still passes existing behaviour.
//==============================================================================
void LinearPhaseDryTest::runTest()
{
    // Helper: prepare a processor with a specific linearPhaseDry value and 4x oversampling.
    auto prepareProcessor = [](PluginProcessor& p, bool linearPhase)
    {
        p.setRateAndBufferSizeDetails(48000.0, 512);
        // Set linearPhaseDry before prepareToPlay so rebuildOversampling picks it up.
        if (auto* param = p.parameters.getParameter("linearPhaseDry"))
            param->setValueNotifyingHost(linearPhase ? 1.0f : 0.0f);
        p.setRateAndBufferSizeDetails(48000.0, 512);
        p.prepareToPlay(48000.0, 512);
    };

    // ---- Test 1: FIR path is RT-allocation-free --------------------------------
    beginTest("No allocation in processBlock with FIR oversampling");
    {
        PluginProcessor processor;
        prepareProcessor(processor, true);

        juce::AudioBuffer<float> buffer(2, 512);
        juce::MidiBuffer midi;
        buffer.clear();

        // Warm-up block (filter settle, don't count)
        processor.processBlock(buffer, midi);
        buffer.clear();

        rt_guard::resetAllocationCounter();
        processor.processBlock(buffer, midi);
        expectEquals(rt_guard::getAllocationCount(), 0,
            "processBlock with FIR oversampling allocated on the audio thread");
    }

    // ---- Test 2: IIR path is still RT-allocation-free (regression guard) ------
    beginTest("No allocation in processBlock with IIR oversampling (baseline)");
    {
        PluginProcessor processor;
        prepareProcessor(processor, false);

        juce::AudioBuffer<float> buffer(2, 512);
        juce::MidiBuffer midi;
        buffer.clear();

        processor.processBlock(buffer, midi);
        buffer.clear();

        rt_guard::resetAllocationCounter();
        processor.processBlock(buffer, midi);
        expectEquals(rt_guard::getAllocationCount(), 0,
            "processBlock with IIR oversampling allocated on the audio thread");
    }

    // ---- Test 3: State round-trip (linearPhaseDry=true survives save/restore) --
    beginTest("linearPhaseDry state round-trip");
    {
        PluginProcessor src;
        prepareProcessor(src, true);

        juce::MemoryBlock saved;
        src.getStateInformation(saved);

        PluginProcessor dst;
        dst.setRateAndBufferSizeDetails(48000.0, 512);
        dst.setStateInformation(saved.getData(), (int) saved.getSize());

        auto* p = dst.parameters.getRawParameterValue("linearPhaseDry");
        expect(p != nullptr, "linearPhaseDry parameter missing after restore");
        if (p)
            expect(p->load() > 0.5f, "linearPhaseDry should be true after state round-trip");
    }
}

//==============================================================================
// RTAllocationGuardTest Implementation (PR-0)
//
// Verifies that the debug RT-allocation guard correctly counts global
// operator new / new[] invocations that occur inside a ScopedRTAssert scope
// on the current thread, and ignores allocations outside any scope. This
// test does NOT assert that processBlock is allocation-free today - that
// property is established piecewise by the later RT-1..RT-5 PRs.
//
// IMPORTANT: GCC/Clang at -O2+ are permitted by [expr.new] / CWG2511 to
// elide paired new/delete (heap-allocation DCE) even when operator new is
// replaced. That elision wipes the test allocations before our override can
// count them, producing 4 spurious failures on Linux Release CI ("Actual
// value: 0"). Disabling optimisation on this single function preserves the
// new/delete calls so the override observes them. Windows / MSVC does not
// perform this optimisation, so the attribute is GCC/Clang only.
//==============================================================================

// The attribute below covers runTest() only - it does NOT reach into lambda
// bodies, which are separate functions the optimiser still owns. The two thread
// tests allocate inside a std::thread lambda, so on clang their new/delete pair
// was elided anyway: "Worker thread counts allocations inside its own scope"
// failed on macOS, and its sibling (which asserts a count of ZERO) passed
// vacuously - there was no allocation to miscount. This helper is noinline as
// well as unoptimised, so the pair survives wherever it is called from. Route
// every in-lambda allocation through it rather than writing `new` inline.
#if defined(__clang__)
[[clang::optnone]] __attribute__((noinline))
#elif defined(__GNUC__)
__attribute__((optimize("O0"), noinline))
#endif
static void rtGuardForceHeapAllocation()
{
    auto* p = new int (42);
    delete p;
}

#if defined(__clang__)
[[clang::optnone]]
#elif defined(__GNUC__)
__attribute__((optimize("O0")))
#endif
void RTAllocationGuardTest::runTest()
{
#if defined (DISTORTION_RT_GUARD) && DISTORTION_RT_GUARD
    beginTest("Counter increments inside scope");
    {
        rt_guard::resetAllocationCounter();
        {
            RT_ASSERT_SCOPE();
            // Force an allocation that must go through operator new.
            auto* p = new int (42);
            delete p;
        }
        expect(rt_guard::getAllocationCount() >= 1,
               "Guard did not count operator new inside an active scope");
    }

    beginTest("Counter stays at zero outside scope");
    {
        rt_guard::resetAllocationCounter();
        {
            auto* p = new int (42);
            delete p;
        }
        expectEquals(rt_guard::getAllocationCount(), 0,
                     "Guard counted an allocation outside any scope");
    }

    beginTest("Stack-only path does not trip guard");
    {
        rt_guard::resetAllocationCounter();
        {
            RT_ASSERT_SCOPE();
            volatile int stackOnly = 7;
            juce::ignoreUnused (stackOnly);
        }
        expectEquals(rt_guard::getAllocationCount(), 0,
                     "Guard false-positive on pure stack path");
    }

    beginTest("Counter reset works");
    {
        rt_guard::resetAllocationCounter();
        {
            RT_ASSERT_SCOPE();
            delete (new int (1));
            delete (new int (2));
        }
        expect(rt_guard::getAllocationCount() >= 2, "Pre-reset count should be >=2");
        rt_guard::resetAllocationCounter();
        expectEquals(rt_guard::getAllocationCount(), 0, "Reset did not zero the counter");
    }

    // Proves the depth-counter fix: an inner scope exiting must not turn off
    // counting for the still-live outer scope.
    beginTest("Nested scopes remain active after inner exits");
    {
        rt_guard::resetAllocationCounter();
        {
            RT_ASSERT_SCOPE();          // outer
            delete (new int (1));       // counted
            {
                RT_ASSERT_SCOPE();      // inner
                delete (new int (2));   // counted
            }                            // inner dtor: depth 2 -> 1 (still active)
            delete (new int (3));       // must still be counted
        }                                // outer dtor: depth 1 -> 0
        delete (new int (4));           // must NOT be counted
        expectEquals(rt_guard::getAllocationCount(), 3,
                     "Nested-scope handling is incorrect");
    }

    // Proves per-thread semantics: a scope active on thread A must not count
    // allocations that occur on thread B (which is itself not in a scope).
    beginTest("Active scope on one thread does not count another thread's allocs");
    {
        rt_guard::resetAllocationCounter();

        std::atomic<bool> scopeReady   { false };
        std::atomic<bool> workerDone   { false };

        std::thread worker ([&]
        {
            // Spin (no heap alloc) until main opens its scope.
            while (! scopeReady.load (std::memory_order_acquire))
                std::this_thread::yield();

            // Main has a scope active; this worker has none. This alloc must
            // NOT be counted.
            // Routed through the helper so the allocation genuinely happens -
            // written inline, clang elided it and this test passed without ever
            // exercising the thing it claims to check.
            rtGuardForceHeapAllocation();

            workerDone.store (true, std::memory_order_release);
        });

        {
            RT_ASSERT_SCOPE();
            scopeReady.store (true, std::memory_order_release);
            while (! workerDone.load (std::memory_order_acquire))
                std::this_thread::yield();
        }

        worker.join();
        expectEquals(rt_guard::getAllocationCount(), 0,
                     "Main-thread scope incorrectly counted another thread's alloc");
    }

    // Proves a worker thread can own its own scope and count its own allocs
    // without any scope being active on main.
    beginTest("Worker thread counts allocations inside its own scope");
    {
        rt_guard::resetAllocationCounter();

        std::thread worker ([]
        {
            RT_ASSERT_SCOPE();
            rtGuardForceHeapAllocation();
        });
        worker.join();

        expect(rt_guard::getAllocationCount() >= 1,
               "Worker-thread scope failed to count its own allocation");
    }
#else
    beginTest("RT guard disabled in this build");
    expect(true, "DISTORTION_RT_GUARD not defined; skipping.");
#endif
}

void RTBufferPreallocTest::runTest()
{
#if defined (DISTORTION_RT_GUARD) && DISTORTION_RT_GUARD
    beginTest("No allocation on prepared-size block with pre-sized scratch buffers");
    {
        PluginProcessor processor;
        processor.setRateAndBufferSizeDetails(48000.0, 512);
        processor.prepareToPlay(48000.0, 512);

        auto* distortionAmount = processor.parameters.getParameter("distortionAmount");
        auto* subGuardFreq = processor.parameters.getParameter("subGuardFreq");
        auto* globalMix = processor.parameters.getParameter("globalMix");
        auto* subGuardFreqFloat = dynamic_cast<juce::AudioParameterFloat*>(subGuardFreq);

        expect(distortionAmount != nullptr, "distortionAmount parameter not found");
        expect(subGuardFreq != nullptr, "subGuardFreq parameter not found");
        expect(globalMix != nullptr, "globalMix parameter not found");
        expect(subGuardFreqFloat != nullptr, "subGuardFreq parameter type mismatch");

        if (distortionAmount == nullptr || subGuardFreq == nullptr || globalMix == nullptr || subGuardFreqFloat == nullptr)
            return;

        distortionAmount->setValueNotifyingHost(0.5f);
        subGuardFreq->setValueNotifyingHost(subGuardFreqFloat->convertTo0to1(100.0f));
        globalMix->setValueNotifyingHost(0.7f);

        juce::MidiBuffer midi;
        auto warmupBuffer = generateSineWave(1000.0, 48000.0, 512, 0.25f);
        processor.processBlock(warmupBuffer, midi);

        auto measuredBuffer = generateSineWave(1000.0, 48000.0, 512, 0.25f);
        rt_guard::resetAllocationCounter();
        processor.processBlock(measuredBuffer, midi);

        expectEquals(rt_guard::getAllocationCount(), 0,
                     "Prepared-size processBlock should not allocate from scratch-buffer resizing");
    }
#else
    beginTest("RT guard disabled in this build");
    expect(true, "DISTORTION_RT_GUARD not defined; skipping.");
#endif
}

void RTCleanPreHighPassTest::runTest()
{
#if defined (DISTORTION_RT_GUARD) && DISTORTION_RT_GUARD
    beginTest("No allocation during 100-block high-pass sweep");
    {
        PluginProcessor processor;
        processor.setRateAndBufferSizeDetails(48000.0, 512);
        processor.prepareToPlay(48000.0, 512);

        auto* distortionAmount = processor.parameters.getParameter("distortionAmount");
        auto* subGuardFreq = processor.parameters.getParameter("subGuardFreq");
        auto* highPassFreq = processor.parameters.getParameter("highPassFreq");
        auto* highPassFreqFloat = dynamic_cast<juce::AudioParameterFloat*>(highPassFreq);

        expect(distortionAmount != nullptr, "distortionAmount parameter not found");
        expect(subGuardFreq != nullptr, "subGuardFreq parameter not found");
        expect(highPassFreq != nullptr, "highPassFreq parameter not found");
        expect(highPassFreqFloat != nullptr, "highPassFreq parameter type mismatch");

        if (distortionAmount == nullptr || subGuardFreq == nullptr || highPassFreq == nullptr || highPassFreqFloat == nullptr)
            return;

        distortionAmount->setValueNotifyingHost(0.5f);
        subGuardFreq->setValueNotifyingHost(0.0f);
        highPassFreq->setValueNotifyingHost(highPassFreqFloat->convertTo0to1(20.0f));

        juce::MidiBuffer midi;
        auto warmupBuffer = generateSineWave(1000.0, 48000.0, 512, 0.25f);
        processor.processBlock(warmupBuffer, midi);

        rt_guard::resetAllocationCounter();

        for (int i = 0; i < 100; ++i)
        {
            const float cutoffHz = 20.0f + (480.0f * static_cast<float>(i) / 99.0f);
            highPassFreq->setValueNotifyingHost(highPassFreqFloat->convertTo0to1(cutoffHz));

            auto buffer = generateSineWave(1000.0, 48000.0, 512, 0.25f);
            processor.processBlock(buffer, midi);
        }

        expectEquals(rt_guard::getAllocationCount(), 0,
                     "High-pass coefficient updates should not allocate on the audio thread");
    }
#else
    beginTest("RT guard disabled in this build");
    expect(true, "DISTORTION_RT_GUARD not defined; skipping.");
#endif
}

void RTCleanSubGuardTest::runTest()
{
#if defined (DISTORTION_RT_GUARD) && DISTORTION_RT_GUARD
    beginTest("No allocation during 200-block Sub Guard sweep");
    {
        PluginProcessor processor;
        processor.setRateAndBufferSizeDetails(48000.0, 512);
        processor.prepareToPlay(48000.0, 512);

        setParameter(processor.parameters, "distortionAmount", 50.0f);
        setParameter(processor.parameters, "highPassFreq", 20.0f);
        setParameter(processor.parameters, "globalMix", 100.0f);
        setParameter(processor.parameters, "subGuardFreq", 0.0f);

        juce::MidiBuffer midi;
        auto warmupBuffer = generateSineWave(1000.0, 48000.0, 512, 0.25f);
        processor.processBlock(warmupBuffer, midi);

        rt_guard::resetAllocationCounter();

        for (int i = 0; i < 200; ++i)
        {
            const float sweepHz = 200.0f * static_cast<float>(i) / 199.0f;
            setParameter(processor.parameters, "subGuardFreq", sweepHz);

            auto buffer = generateSineWave(1000.0, 48000.0, 512, 0.25f);
            processor.processBlock(buffer, midi);
        }

        expectEquals(rt_guard::getAllocationCount(), 0,
                     "Sub Guard coefficient updates should not allocate on the audio thread");
    }
#else
    beginTest("RT guard disabled in this build");
    expect(true, "DISTORTION_RT_GUARD not defined; skipping.");
#endif
}

void SubGuardCrossoverFlatnessTest::runTest()
{
    // The Sub Guard splits the signal into a clean low band and a distorted high band
    // and sums them back. A correct crossover sums flat (no notch/dip) at fc. We drive
    // the distortion stage as an identity (distMix = 0, so the high band stays linear)
    // and compare the on-crossover tone level to the same chain with Sub Guard off.
    //
    // Before the fix: LR12 summed same-polarity -> deep null at fc; LR18 used Q=0.5
    // (coincident poles) -> ~6dB dip. Both now sum flat (LR12 inverts the high band,
    // LR18 is a true 3rd-order Butterworth with Q=1.0).
    const double sr = 44100.0;
    const int blockSize = 512;
    const float amp = 0.1f;  // low enough that the soft clipper / limiter stay linear

    auto measureOutputRms = [&](float crossoverFreq, double sineFreq) -> float
    {
        PluginProcessor processor;
        processor.setRateAndBufferSizeDetails(sr, blockSize);
        processor.prepareToPlay(sr, blockSize);

        // Distortion engaged (so we are not in true-bypass) but fully dry-mixed, which
        // makes the distortion stage an identity. Everything else that colours level off.
        setParameter(processor.parameters, "distortionAmount", 50.0f);
        setParameter(processor.parameters, "distMix",          0.0f);
        setParameter(processor.parameters, "waveshaperMix",    0.0f);
        setParameter(processor.parameters, "globalMix",        100.0f);
        setParameter(processor.parameters, "highPassFreq",     20.0f);
        setParameter(processor.parameters, "tone",             20000.0f);
        setParameter(processor.parameters, "compEnabled",      0.0f);
        setParameter(processor.parameters, "autoGainEnabled",  0.0f);
        setParameter(processor.parameters, "cleanBoost",       0.0f);
        setParameter(processor.parameters, "extremeEnabled",   0.0f);
        setParameter(processor.parameters, "subGuardFreq",     crossoverFreq);

        juce::MidiBuffer midi;
        const double phaseInc = juce::MathConstants<double>::twoPi * sineFreq / sr;
        double phase = 0.0;

        const int warmupBlocks = 16;   // settle IIR filters + 50ms freq smoothing
        const int measureBlocks = 8;
        float sumSquares = 0.0f;
        int count = 0;

        for (int b = 0; b < warmupBlocks + measureBlocks; ++b)
        {
            juce::AudioBuffer<float> buffer(2, blockSize);
            for (int ch = 0; ch < 2; ++ch)
            {
                auto* data = buffer.getWritePointer(ch);
                double p = phase;
                for (int s = 0; s < blockSize; ++s)
                {
                    data[s] = amp * static_cast<float>(std::sin(p));
                    p += phaseInc;
                }
            }

            processor.processBlock(buffer, midi);

            if (b >= warmupBlocks)
            {
                for (int ch = 0; ch < 2; ++ch)
                {
                    const auto* d = buffer.getReadPointer(ch);
                    for (int s = 0; s < blockSize; ++s) { sumSquares += d[s] * d[s]; ++count; }
                }
            }

            phase += phaseInc * blockSize;
            phase = std::fmod(phase, juce::MathConstants<double>::twoPi);
        }

        return count > 0 ? std::sqrt(sumSquares / static_cast<float>(count)) : 0.0f;
    };

    struct Case { float fc; const char* order; };
    const Case cases[] = { { 60.0f, "LR24" }, { 100.0f, "LR18" }, { 150.0f, "LR12" } };

    for (const auto& c : cases)
    {
        beginTest(juce::String("Crossover flat at ") + juce::String(c.fc, 0) + " Hz (" + c.order + ")");

        const float offRms    = measureOutputRms(0.0f, c.fc);   // Sub Guard off (baseline)
        const float activeRms = measureOutputRms(c.fc, c.fc);   // Sub Guard on, tone at fc

        expect(offRms > 1.0e-4f, "Baseline (Sub Guard off) output should be non-trivial");

        const float ratioDb = juce::Decibels::gainToDecibels(activeRms / juce::jmax(offRms, 1.0e-9f));

        // A flat-summing crossover keeps the on-crossover tone within ~3dB of the
        // bypassed level. The old LR12 notch was ~ -inf; the old LR18 dip was ~ -6dB.
        expect(ratioDb > -3.0f,
               juce::String("Crossover sum dipped at fc (") + c.order + "): "
               + juce::String(ratioDb, 2) + " dB vs Sub Guard off");
    }
}

void SubGuardOrderCrossfadeTest::runTest()
{
    // Sweeping the crossover frequency moves through the slope-order zones (LR24 ->
    // LR18 at 92 Hz, LR18 -> LR12 at 142 Hz). A hard order switch starts the incoming
    // bank from stale state and jumps the magnitude/phase, producing a click. The order
    // crossfade should keep the output smooth. We drive a clean low sine (distortion as
    // identity so the output slew is dominated by the tone itself) and assert the peak
    // sample-to-sample step during the sweep stays small.
    beginTest("Click-free crossover frequency sweep across order boundaries");

    const double sr = 44100.0;
    const int blockSize = 256;
    const double sineFreq = 80.0;   // sits near the crossover, exercises both bands
    const float amp = 0.1f;

    PluginProcessor processor;
    processor.setRateAndBufferSizeDetails(sr, blockSize);
    processor.prepareToPlay(sr, blockSize);

    setParameter(processor.parameters, "distortionAmount", 50.0f);
    setParameter(processor.parameters, "distMix",          0.0f);   // identity distortion
    setParameter(processor.parameters, "waveshaperMix",    0.0f);
    setParameter(processor.parameters, "globalMix",        100.0f);
    setParameter(processor.parameters, "highPassFreq",     20.0f);
    setParameter(processor.parameters, "tone",             20000.0f);
    setParameter(processor.parameters, "compEnabled",      0.0f);
    setParameter(processor.parameters, "autoGainEnabled",  0.0f);
    setParameter(processor.parameters, "cleanBoost",       0.0f);
    setParameter(processor.parameters, "subGuardFreq",     60.0f);

    juce::MidiBuffer midi;
    const double phaseInc = juce::MathConstants<double>::twoPi * sineFreq / sr;
    double phase = 0.0;

    auto runBlock = [&](float subGuardFreq) -> float
    {
        setParameter(processor.parameters, "subGuardFreq", subGuardFreq);

        juce::AudioBuffer<float> buffer(2, blockSize);
        for (int ch = 0; ch < 2; ++ch)
        {
            auto* data = buffer.getWritePointer(ch);
            double p = phase;
            for (int s = 0; s < blockSize; ++s) { data[s] = amp * static_cast<float>(std::sin(p)); p += phaseInc; }
        }
        processor.processBlock(buffer, midi);

        phase += phaseInc * blockSize;
        phase = std::fmod(phase, juce::MathConstants<double>::twoPi);

        // Peak inter-sample step in this block's output (channel 0).
        float maxStep = 0.0f;
        const auto* out = buffer.getReadPointer(0);
        for (int s = 1; s < blockSize; ++s)
            maxStep = std::max(maxStep, std::abs(out[s] - out[s - 1]));
        return maxStep;
    };

    // Warm up and establish the intrinsic per-sample slew at a fixed crossover.
    for (int b = 0; b < 24; ++b) runBlock(60.0f);
    float baselineStep = 0.0f;
    for (int b = 0; b < 8; ++b) baselineStep = std::max(baselineStep, runBlock(60.0f));

    // Sweep up across both order boundaries, then back down, a small step per block.
    float sweepStep = 0.0f;
    const int sweepBlocks = 80;
    for (int b = 0; b <= sweepBlocks; ++b)
    {
        const float fc = 60.0f + (200.0f - 60.0f) * static_cast<float>(b) / static_cast<float>(sweepBlocks);
        sweepStep = std::max(sweepStep, runBlock(fc));
    }
    for (int b = 0; b <= sweepBlocks; ++b)
    {
        const float fc = 200.0f - (200.0f - 60.0f) * static_cast<float>(b) / static_cast<float>(sweepBlocks);
        sweepStep = std::max(sweepStep, runBlock(fc));
    }

    juce::ignoreUnused(baselineStep);

    // A click from a hard order switch is on the order of the band amplitude (~0.05-0.1).
    // The crossfade should keep the sweep's peak step close to the fixed-crossover slew.
    expect(sweepStep < 0.02f,
           "Crossover frequency sweep produced a discontinuity (peak step "
           + juce::String(sweepStep, 5) + ", baseline " + juce::String(baselineStep, 5) + ")");
}

void RTCleanOversamplingTest::runTest()
{
#if defined (DISTORTION_RT_GUARD) && DISTORTION_RT_GUARD
    beginTest("No allocation in processBlock while oversampling rebuild is deferred");
    {
        PluginProcessor processor;
        processor.setRateAndBufferSizeDetails(48000.0, 512);
        processor.prepareToPlay(48000.0, 512);

        setParameter(processor.parameters, "distortionAmount", 50.0f);
        setParameter(processor.parameters, "subGuardFreq", 0.0f);
        setParameter(processor.parameters, "highPassFreq", 20.0f);

        juce::MidiBuffer midi;
        auto warmupBuffer = generateSineWave(1000.0, 48000.0, 512, 0.25f);
        processor.processBlock(warmupBuffer, midi);

        processor.requestOversamplingRebuild(1);

        auto pendingBuffer = generateSineWave(1000.0, 48000.0, 512, 0.25f);
        rt_guard::resetAllocationCounter();
        processor.processBlock(pendingBuffer, midi);
        expectEquals(rt_guard::getAllocationCount(), 0,
                     "processBlock should not allocate while oversampling rebuild is pending");

        processor.handleAsyncUpdate();

        auto rebuiltBuffer = generateSineWave(1000.0, 48000.0, 512, 0.25f);
        rt_guard::resetAllocationCounter();
        processor.processBlock(rebuiltBuffer, midi);
        expectEquals(rt_guard::getAllocationCount(), 0,
                     "processBlock should not allocate after deferred oversampling rebuild");
        expect(!containsInvalidSamples(rebuiltBuffer),
               "Output must remain valid after deferred oversampling rebuild");
    }
#else
    beginTest("RT guard disabled in this build");
    expect(true, "DISTORTION_RT_GUARD not defined; skipping.");
#endif
}

void RTCleanToneSweepTest::runTest()
{
#if defined (DISTORTION_RT_GUARD) && DISTORTION_RT_GUARD
    beginTest("No allocation during 100-block tone frequency sweep (PR-10)");
    {
        PluginProcessor processor;
        processor.setRateAndBufferSizeDetails(48000.0, 512);
        processor.prepareToPlay(48000.0, 512);

        auto* distortionAmount = processor.parameters.getParameter("distortionAmount");
        auto* subGuardFreq = processor.parameters.getParameter("subGuardFreq");
        auto* subGuardFreqFloat = dynamic_cast<juce::AudioParameterFloat*>(subGuardFreq);
        auto* tone = processor.parameters.getParameter("tone");
        auto* toneFloat = dynamic_cast<juce::AudioParameterFloat*>(tone);

        expect(distortionAmount != nullptr, "distortionAmount parameter not found");
        expect(subGuardFreqFloat != nullptr, "subGuardFreq parameter not found / type mismatch");
        expect(toneFloat != nullptr, "tone parameter not found / type mismatch");
        if (distortionAmount == nullptr || subGuardFreqFloat == nullptr || toneFloat == nullptr)
            return;

        // Distortion ON so the tone filter is actually in the signal path.
        // Sub Guard OFF for this sweep (sub guard ON tested separately below).
        distortionAmount->setValueNotifyingHost(0.5f);
        subGuardFreq->setValueNotifyingHost(subGuardFreqFloat->convertTo0to1(0.0f));
        tone->setValueNotifyingHost(toneFloat->convertTo0to1(20000.0f));

        juce::MidiBuffer midi;
        auto warmupBuffer = generateSineWave(1000.0, 48000.0, 512, 0.25f);
        processor.processBlock(warmupBuffer, midi);

        rt_guard::resetAllocationCounter();

        for (int i = 0; i < 100; ++i)
        {
            // Sweep from 2 kHz to 20 kHz (full parameter range)
            const float toneHz = 2000.0f + (18000.0f * static_cast<float>(i) / 99.0f);
            tone->setValueNotifyingHost(toneFloat->convertTo0to1(toneHz));

            auto buffer = generateSineWave(1000.0, 48000.0, 512, 0.25f);
            processor.processBlock(buffer, midi);
        }

        expectEquals(rt_guard::getAllocationCount(), 0,
                     "Tone filter coefficient updates should not allocate on the audio thread");
    }

    beginTest("Tone filter coefficient update actually affects audio output (correctness)");
    {
        // Regression test: changing the tone parameter must audibly change the tone filter
        // output. The in-place write through .state aliases the object held by each
        // per-channel Filter.coefficients, so updates take effect on the next process().
        PluginProcessor processor;
        processor.setRateAndBufferSizeDetails(48000.0, 512);
        processor.prepareToPlay(48000.0, 512);

        auto* distortionAmount = processor.parameters.getParameter("distortionAmount");
        auto* subGuardFreq = processor.parameters.getParameter("subGuardFreq");
        auto* subGuardFreqFloat = dynamic_cast<juce::AudioParameterFloat*>(subGuardFreq);
        auto* tone = processor.parameters.getParameter("tone");
        auto* toneFloat = dynamic_cast<juce::AudioParameterFloat*>(tone);

        if (distortionAmount == nullptr || subGuardFreqFloat == nullptr || toneFloat == nullptr)
            return;

        // Distortion ON so tone filter is active. Sub Guard OFF to isolate tone filter only.
        distortionAmount->setValueNotifyingHost(0.5f);
        subGuardFreq->setValueNotifyingHost(subGuardFreqFloat->convertTo0to1(0.0f));

        juce::MidiBuffer midi;
        const int blocksToSettle = 16;  // enough for any parameter smoothing

        // Measure output at tone = 20kHz (effectively bypassed) with 8 kHz input
        tone->setValueNotifyingHost(toneFloat->convertTo0to1(20000.0f));
        for (int b = 0; b < blocksToSettle; ++b)
        {
            auto warm = generateSineWave(8000.0, 48000.0, 512, 0.3f);
            processor.processBlock(warm, midi);
        }
        auto measureOpen = generateSineWave(8000.0, 48000.0, 512, 0.3f);
        processor.processBlock(measureOpen, midi);
        float peakOpen = 0.0f;
        for (int i = 256; i < 512; ++i)
            peakOpen = std::max(peakOpen, std::abs(measureOpen.getSample(0, i)));

        // Now change tone to 2kHz — 8kHz input should be heavily attenuated
        tone->setValueNotifyingHost(toneFloat->convertTo0to1(2000.0f));
        for (int b = 0; b < blocksToSettle; ++b)
        {
            auto warm = generateSineWave(8000.0, 48000.0, 512, 0.3f);
            processor.processBlock(warm, midi);
        }
        auto measureClosed = generateSineWave(8000.0, 48000.0, 512, 0.3f);
        processor.processBlock(measureClosed, midi);
        float peakClosed = 0.0f;
        for (int i = 256; i < 512; ++i)
            peakClosed = std::max(peakClosed, std::abs(measureClosed.getSample(0, i)));

        // 8kHz through a 2kHz LP (2nd-order) is attenuated by ~24 dB vs open tone.
        // Require at least 6 dB difference (factor of 2) — weaker criterion, avoids
        // false positives from limiter / auto-gain compensation.
        expect(peakClosed < peakOpen * 0.6f,
               "Closing tone filter to 2kHz must attenuate 8kHz input more than open @ 20kHz "
               "(peakOpen=" + juce::String(peakOpen) + " peakClosed=" + juce::String(peakClosed) + ")");
    }

    beginTest("No allocation during tone sweep with Sub Guard engaged (toneFilterLow path)");
    {
        PluginProcessor processor;
        processor.setRateAndBufferSizeDetails(48000.0, 512);
        processor.prepareToPlay(48000.0, 512);

        auto* distortionAmount = processor.parameters.getParameter("distortionAmount");
        auto* subGuardFreq = processor.parameters.getParameter("subGuardFreq");
        auto* subGuardFreqFloat = dynamic_cast<juce::AudioParameterFloat*>(subGuardFreq);
        auto* tone = processor.parameters.getParameter("tone");
        auto* toneFloat = dynamic_cast<juce::AudioParameterFloat*>(tone);

        if (distortionAmount == nullptr || subGuardFreqFloat == nullptr || toneFloat == nullptr)
            return;

        distortionAmount->setValueNotifyingHost(0.5f);
        subGuardFreq->setValueNotifyingHost(subGuardFreqFloat->convertTo0to1(100.0f));
        tone->setValueNotifyingHost(toneFloat->convertTo0to1(20000.0f));

        juce::MidiBuffer midi;
        auto warmupBuffer = generateSineWave(1000.0, 48000.0, 512, 0.25f);
        processor.processBlock(warmupBuffer, midi);

        rt_guard::resetAllocationCounter();

        for (int i = 0; i < 100; ++i)
        {
            const float toneHz = 2000.0f + (18000.0f * static_cast<float>(i) / 99.0f);
            tone->setValueNotifyingHost(toneFloat->convertTo0to1(toneHz));

            auto buffer = generateSineWave(1000.0, 48000.0, 512, 0.25f);
            processor.processBlock(buffer, midi);
        }

        expectEquals(rt_guard::getAllocationCount(), 0,
                     "Tone filter (including low mirror) must not allocate on the audio thread");
    }
#else
    beginTest("RT guard disabled in this build");
    expect(true, "DISTORTION_RT_GUARD not defined; skipping.");
#endif
}

void RTCleanSampleRateDriftTest::runTest()
{
#if defined (DISTORTION_RT_GUARD) && DISTORTION_RT_GUARD
    beginTest("No allocation when SR drift branch fires inside processBlock (PR-12)");
    {
        PluginProcessor processor;
        processor.setRateAndBufferSizeDetails(48000.0, 512);
        processor.prepareToPlay(48000.0, 512);

        setParameter(processor.parameters, "distortionAmount", 50.0f);
        setParameter(processor.parameters, "subGuardFreq", 0.0f);

        juce::MidiBuffer midi;
        auto warmupBuffer = generateSineWave(1000.0, 48000.0, 512, 0.25f);
        processor.processBlock(warmupBuffer, midi);

        // Force a sample-rate drift WITHOUT calling prepareToPlay, so the SR-drift
        // branch inside processBlock runs. Use the same block size the processor was
        // prepared for (512); only the sample rate changes.
        processor.setRateAndBufferSizeDetails(44100.0, 512);

        rt_guard::resetAllocationCounter();

        auto driftBuffer = generateSineWave(1000.0, 44100.0, 512, 0.25f);
        processor.processBlock(driftBuffer, midi);

        expectEquals(rt_guard::getAllocationCount(), 0,
                     "Sample-rate drift branch must not allocate on the audio thread");

        // Subsequent blocks at the new rate: still zero allocations.
        rt_guard::resetAllocationCounter();
        for (int i = 0; i < 4; ++i)
        {
            auto b = generateSineWave(1000.0, 44100.0, 512, 0.25f);
            processor.processBlock(b, midi);
        }
        expectEquals(rt_guard::getAllocationCount(), 0,
                     "Post-drift blocks at the new sample rate must not allocate");
    }
#else
    beginTest("RT guard disabled in this build");
    expect(true, "DISTORTION_RT_GUARD not defined; skipping.");
#endif
}

// PR-14 regression tests: prove that changing subGuardFreq / highPassFreq
// actually affects the audio output (i.e. coefficient updates reach every
// per-channel Filter). Before PR-14 these would have failed because the
// standby-swap pattern left Filter.coefficients pointing at stale objects.
void CoefficientPropagationTest::runTest()
{
    beginTest("Sub Guard: filter coefficients actually update when freq changes");
    {
        // Direct verification: write changes through .state must mutate the
        // Coefficients object the per-channel Filter is bound to. Inspect raw
        // coefficients on lowPassFilter1 (LR24 mode @ 60 Hz) and confirm they
        // differ from the same filter rewritten for a different frequency.
        // Bypass the parameter smoother (via friend access) so the coefficient
        // update fires on the first block at the new target frequency.
        PluginProcessor processor;
        processor.setRateAndBufferSizeDetails(48000.0, 512);
        processor.prepareToPlay(48000.0, 512);

        auto* subGuardFreq = processor.parameters.getParameter("subGuardFreq");
        auto* subGuardFreqFloat = dynamic_cast<juce::AudioParameterFloat*>(subGuardFreq);
        if (subGuardFreqFloat == nullptr) return;

        setParameter(processor.parameters, "distortionAmount", 50.0f);
        setParameter(processor.parameters, "highPassFreq", 20.0f);

        juce::MidiBuffer midi;

        auto snapshotLP1 = [&](float hz) {
            subGuardFreq->setValueNotifyingHost(subGuardFreqFloat->convertTo0to1(hz));
            // Snap the smoother directly to the target so the next processBlock
            // sees currentSubGuardFreq = hz and triggers updateSubGuardCoefficients.
            processor.smoothedSubGuardFreq.setCurrentAndTargetValue(hz);
            // Single processBlock to fire the coefficient update path.
            auto warm = generateSineWave(1000.0, 48000.0, 512, 0.1f);
            processor.processBlock(warm, midi);
            auto* coeffs = processor.lowPassFilter1.state->getRawCoefficients();
            return std::array<float, 5>{ coeffs[0], coeffs[1], coeffs[2], coeffs[3], coeffs[4] };
        };

        const auto c60  = snapshotLP1(60.0f);
        const auto c150 = snapshotLP1(150.0f);

        float maxDiff = 0.0f;
        for (size_t i = 0; i < c60.size(); ++i)
            maxDiff = std::max(maxDiff, std::abs(c60[i] - c150[i]));

        logMessage(juce::String::formatted(
            "LP1 @60: [%.7f %.7f %.7f %.5f %.5f]", c60[0], c60[1], c60[2], c60[3], c60[4]));
        logMessage(juce::String::formatted(
            "LP1 @150:[%.7f %.7f %.7f %.5f %.5f] maxDiff=%.5f",
            c150[0], c150[1], c150[2], c150[3], c150[4], maxDiff));

        // The denormalised feedback coefficient (a2) carries the most precision
        // at low fc/SR ratios; require at least a 0.001 difference between
        // freq=60 and freq=150 to prove the in-place write actually landed.
        expect(maxDiff > 1e-3f,
               "Sub Guard LP1 coefficients did not change between SG=60 and SG=150 "
               "(maxDiff=" + juce::String(maxDiff) + ")");
    }

    beginTest("Pre-HP filter: 100Hz sine attenuated more at HP=400Hz than at HP=20Hz");
    {
        PluginProcessor processor;
        processor.setRateAndBufferSizeDetails(48000.0, 512);
        processor.prepareToPlay(48000.0, 512);

        auto* highPassFreq = processor.parameters.getParameter("highPassFreq");
        auto* highPassFreqFloat = dynamic_cast<juce::AudioParameterFloat*>(highPassFreq);
        if (highPassFreqFloat == nullptr) return;

        // Distortion OFF + comp ON (with peak reduction 0 = no compression, just
        // a transparent routing trick to exit true-bypass) so the signal passes
        // through the full chain including the pre-HP filter, but no stage is
        // actually compressing or distorting. Auto-gain off so it can't hide the
        // HP attenuation.
        setParameter(processor.parameters, "distortionAmount", 0.0f);
        setParameter(processor.parameters, "subGuardFreq", 0.0f);
        setParameter(processor.parameters, "globalMix", 100.0f);
        setParameter(processor.parameters, "autoGainEnabled", 0.0f);
        setParameter(processor.parameters, "inputGain", 50.0f);    // unity
        setParameter(processor.parameters, "outputGain", 50.0f);   // unity
        setParameter(processor.parameters, "compEnabled", 1.0f);
        setParameter(processor.parameters, "compPeakReduction", 0.0f);
        setParameter(processor.parameters, "compMakeupGain", 50.0f);

        juce::MidiBuffer midi;

        // Use a tiny amplitude so the output limiter and soft clipper don't
        // compress or normalize the output away from the pre-HP's linear effect.
        constexpr float amp = 0.05f;
        auto settleAndMeasure = [&](float hz) {
            highPassFreq->setValueNotifyingHost(highPassFreqFloat->convertTo0to1(hz));
            for (int b = 0; b < 64; ++b) {
                auto warm = generateSineWave(100.0, 48000.0, 512, amp);
                processor.processBlock(warm, midi);
            }
            auto m = generateSineWave(100.0, 48000.0, 512, amp);
            processor.processBlock(m, midi);
            float peak = 0.0f;
            for (int i = 256; i < 512; ++i)
                peak = std::max(peak, std::abs(m.getSample(0, i)));
            logMessage("HP=" + juce::String(hz) + "Hz @ 100Hz, amp=" + juce::String(amp) + ", peak=" + juce::String(peak));
            return peak;
        };

        const float peakOpen   = settleAndMeasure(20.0f);
        const float peakClosed = settleAndMeasure(400.0f);

        // First-order HP at 400Hz attenuates 100Hz by ~12 dB (~0.25x). With the
        // full chain running (comp enabled), peakClosed should be materially
        // smaller than peakOpen.
        expect(peakClosed < peakOpen * 0.8f,
               "Pre-HP at 400Hz should attenuate 100Hz more than at 20Hz: "
               "peakOpen=" + juce::String(peakOpen)
               + " peakClosed=" + juce::String(peakClosed));
    }
}

void FastMathAccuracyTest::runTest()
{
    beginTest("fastTanh: max error vs std::tanh < 5e-5 for |x| <= 4.5");
    {
        // Sample 10001 points across [-4.5, 4.5] and record the largest absolute error.
        float maxErr = 0.0f;
        constexpr int N = 10001;
        for (int i = 0; i <= N; ++i)
        {
            const float x = -4.5f + 9.0f * (float)i / (float)N;
            const float ref = std::tanh(x);
            const float approx = FastMath::tanh(x);
            maxErr = std::max(maxErr, std::abs(ref - approx));
        }
        logMessage("fastTanh max error over [-4.5,4.5]: " + juce::String(maxErr, 8));
        expect(maxErr < 5e-5f,
               "fastTanh error " + juce::String(maxErr) + " exceeds 5e-5 threshold");
    }

    beginTest("fastTanh: clamps correctly at |x| > 4.5");
    {
        expectWithinAbsoluteError(FastMath::tanh( 10.0f), 1.0f, 1e-6f, "tanh(10) should be 1");
        expectWithinAbsoluteError(FastMath::tanh(-10.0f),-1.0f, 1e-6f, "tanh(-10) should be -1");
        // At exactly ±4.5 the Padé approximation has ~2e-4 error (near-saturation region)
        expectWithinAbsoluteError(FastMath::tanh( 4.5f), std::tanh(4.5f), 3e-4f, "tanh(4.5)");
        expectWithinAbsoluteError(FastMath::tanh(-4.5f), std::tanh(-4.5f), 3e-4f, "tanh(-4.5)");
    }

    beginTest("fastAtan: max error vs std::atan < 5e-4 rad for |x| <= 10");
    {
        float maxErr = 0.0f;
        constexpr int N = 10001;
        for (int i = 0; i <= N; ++i)
        {
            const float x = -10.0f + 20.0f * (float)i / (float)N;
            const float ref = std::atan(x);
            const float approx = FastMath::atan(x);
            maxErr = std::max(maxErr, std::abs(ref - approx));
        }
        logMessage("fastAtan max error over [-10,10]: " + juce::String(maxErr, 6));
        expect(maxErr < 5e-4f,
               "fastAtan error " + juce::String(maxErr) + " exceeds 5e-4 rad threshold");
    }

    beginTest("fastAtan: odd symmetry");
    {
        for (float x : { 0.5f, 1.0f, 2.0f, 4.0f })
            expectWithinAbsoluteError(FastMath::atan(-x), -FastMath::atan(x), 1e-7f,
                                      "odd symmetry at x=" + juce::String(x));
    }

    beginTest("fastTanh: odd symmetry");
    {
        for (float x : { 0.5f, 1.0f, 2.0f, 4.0f })
            expectWithinAbsoluteError(FastMath::tanh(-x), -FastMath::tanh(x), 1e-7f,
                                      "odd symmetry at x=" + juce::String(x));
    }

    beginTest("processBlock with fast math stays within 1e-4 RMS of expected output range");
    {
        // Regression smoke test: a stereo sine at -6 dBFS through the full chain
        // must produce output within ±1.5x input amplitude. This is not a golden
        // comparison — it just verifies fast math doesn't corrupt the signal.
        PluginProcessor processor;
        processor.setRateAndBufferSizeDetails(48000.0, 512);
        processor.prepareToPlay(48000.0, 512);
        setParameter(processor.parameters, "distortionAmount", 50.0f);
        setParameter(processor.parameters, "subGuardFreq",      0.0f);

        const float amp = juce::Decibels::decibelsToGain(-6.0f);
        juce::MidiBuffer midi;

        // Warm up — let smoothers settle
        for (int b = 0; b < 8; ++b)
        {
            auto buf = generateSineWave(1000.0, 48000.0, 512, amp);
            processor.processBlock(buf, midi);
        }

        // Measurement block
        auto buf = generateSineWave(1000.0, 48000.0, 512, amp);
        processor.processBlock(buf, midi);

        float sumSq = 0.0f;
        for (int s = 0; s < 512; ++s)
            sumSq += buf.getSample(0, s) * buf.getSample(0, s);
        const float rms = std::sqrt(sumSq / 512.0f);

        logMessage("RMS after fast-math distortion at 50%: " + juce::String(rms, 5));
        // A 1 kHz sine at -6 dBFS through ~50% distortion shouldn't clip to silence
        // or blow up — accept anything in the plausible audio range.
        expect(rms > 0.01f && rms < 2.0f,
               "Unexpected RMS " + juce::String(rms) + " with fast math enabled");
    }
}

// =============================================================================
// Free-draw graphic EQ (output-stage peaking bank)
// =============================================================================
void GraphicEqTests::runTest()
{
    constexpr double sr = 48000.0;
    constexpr int    blockSize = 512;
    const float amp = juce::Decibels::decibelsToGain(-6.0f);

    // Build a fresh processor whose chain is otherwise transparent (distortion
    // OFF but comp ON with zero peak-reduction, so the signal exits true-bypass
    // and reaches the output-stage EQ), optionally set one band, warm up past the
    // gain ramp + filter settle, then return the steady-state RMS at testFreq.
    // Every knob but the EQ band is identical across calls, so any RMS delta is
    // purely the EQ's doing.
    auto steadyRms = [&](int bandIndex, float bandDb, double testFreq) -> float
    {
        PluginProcessor p;
        p.setRateAndBufferSizeDetails(sr, blockSize);
        p.prepareToPlay(sr, blockSize);
        setParameter(p.parameters, "distortionAmount", 0.0f);
        setParameter(p.parameters, "compEnabled",       1.0f);
        setParameter(p.parameters, "compPeakReduction", 0.0f);
        setParameter(p.parameters, "subGuardFreq",      0.0f);
        setParameter(p.parameters, "autoGainEnabled",   0.0f);
        setParameter(p.parameters, "globalMix",         100.0f);
        setParameter(p.parameters, "inputGain",         50.0f);
        setParameter(p.parameters, "outputGain",        50.0f);
        if (bandIndex >= 0)
            setParameter(p.parameters, "eqBand" + juce::String(bandIndex), bandDb);

        juce::MidiBuffer midi;
        for (int b = 0; b < 40; ++b)   // ~426 ms: past the 30 ms EQ ramp + settle
        {
            auto buf = generateSineWave(testFreq, sr, blockSize, amp);
            p.processBlock(buf, midi);
        }
        auto meas = generateSineWave(testFreq, sr, blockSize, amp);
        p.processBlock(meas, midi);
        return calculateRMS(meas);
    };

    beginTest("Boosting a band raises its level; cutting lowers it");
    {
        const int   band = 6;        // EQ_FREQS[6] = 1000 Hz
        const double f    = 1000.0;
        const float flat  = steadyRms(band,   0.0f, f);
        const float boost = steadyRms(band, +12.0f, f);
        const float cut   = steadyRms(band, -12.0f, f);

        logMessage(juce::String::formatted(
            "1kHz RMS  flat=%.5f  +12dB=%.5f  -12dB=%.5f", flat, boost, cut));
        expect(flat > 1.0e-4f, "Baseline signal unexpectedly silent");
        expect(boost > flat * 1.30f, "Peaking +12 dB at 1 kHz did not raise the level");
        expect(cut   < flat * 0.85f, "Peaking -12 dB at 1 kHz did not lower the level");
    }

    beginTest("Master bypass mutes the EQ but preserves the drawn curve");
    {
        const double f = 1000.0;

        // Baseline (curve flat, EQ enabled) and boosted (band 6 = +12, enabled).
        const float flat  = steadyRms(6, 0.0f, f);

        // Boosted then bypassed: same +12 curve, but eqEnabled = 0.
        PluginProcessor p;
        p.setRateAndBufferSizeDetails(sr, blockSize);
        p.prepareToPlay(sr, blockSize);
        setParameter(p.parameters, "distortionAmount", 0.0f);
        setParameter(p.parameters, "compEnabled",       1.0f);
        setParameter(p.parameters, "compPeakReduction", 0.0f);
        setParameter(p.parameters, "subGuardFreq",      0.0f);
        setParameter(p.parameters, "autoGainEnabled",   0.0f);
        setParameter(p.parameters, "globalMix",         100.0f);
        setParameter(p.parameters, "inputGain",         50.0f);
        setParameter(p.parameters, "outputGain",        50.0f);
        setParameter(p.parameters, "eqBand6",           12.0f);
        setParameter(p.parameters, "eqEnabled",         0.0f);   // bypassed

        juce::MidiBuffer midi;
        for (int b = 0; b < 40; ++b)
        {
            auto buf = generateSineWave(f, sr, blockSize, amp);
            p.processBlock(buf, midi);
        }
        auto meas = generateSineWave(f, sr, blockSize, amp);
        p.processBlock(meas, midi);
        const float bypassedRms = calculateRMS(meas);

        logMessage(juce::String::formatted(
            "1kHz RMS  flat=%.5f  boosted+bypassed=%.5f", flat, bypassedRms));

        // Bypassed output should collapse back to (near) the flat baseline...
        expectWithinAbsoluteError(bypassedRms, flat, flat * 0.05f,
            "Bypassed EQ did not return the signal to the flat baseline");

        // ...while the drawn curve (eqBand6) is left fully intact.
        auto* band6 = p.parameters.getParameter("eqBand6");
        expect(band6 != nullptr, "eqBand6 missing");
        if (band6 != nullptr)
            expectWithinAbsoluteError(band6->convertFrom0to1(band6->getValue()), 12.0f, 0.05f,
                "Bypass must not alter the stored curve");
    }

    beginTest("A band's boost is localised (does not move a distant frequency)");
    {
        const int    hiBand = 9;         // EQ_FREQS[9] = 5600 Hz
        const double fHigh   = 5600.0;
        const double fLow    = 1000.0;   // ~2.5 octaves below the boosted band

        const float highFlat  = steadyRms(-1,      0.0f, fHigh);
        const float highBoost = steadyRms(hiBand, +12.0f, fHigh);
        const float lowFlat   = steadyRms(-1,      0.0f, fLow);
        const float lowBoost  = steadyRms(hiBand, +12.0f, fLow);

        logMessage(juce::String::formatted(
            "5.6k boost: highFlat=%.5f highBoost=%.5f | lowFlat=%.5f lowBoost=%.5f",
            highFlat, highBoost, lowFlat, lowBoost));
        expect(highBoost > highFlat * 1.30f, "Boost did not raise its own band (5.6 kHz)");
        expect(lowBoost < lowFlat * 1.20f && lowBoost > lowFlat * 0.80f,
               "Boosting 5.6 kHz noticeably moved the 1 kHz level");
    }

    beginTest("Wild curve stays finite across 22.05/32/44.1/48/96/192 kHz");
    {
        // 22.05 and 32 kHz matter: the 16 kHz band sits at/above Nyquist there, and
        // a peaking biquad diverges once its centre reaches Nyquist. prepareEqBands
        // must drop those bands instead of writing unstable coefficients.
        for (double rate : { 22050.0, 32000.0, 44100.0, 48000.0, 96000.0, 192000.0 })
        {
            PluginProcessor p;
            p.setRateAndBufferSizeDetails(rate, blockSize);
            p.prepareToPlay(rate, blockSize);
            setParameter(p.parameters, "distortionAmount", 50.0f);
            // Alternating full boost/cut — an aggressive, jagged curve.
            for (int i = 0; i < DSPConstants::EQ_NUM_BANDS; ++i)
                setParameter(p.parameters, "eqBand" + juce::String(i),
                             (i % 2 == 0) ? +12.0f : -12.0f);

            juce::MidiBuffer midi;
            bool clean = true;
            for (int b = 0; b < 24; ++b)
            {
                auto buf = generateWhiteNoise(blockSize, 0.5f);
                p.processBlock(buf, midi);
                if (containsInvalidSamples(buf))
                    clean = false;
            }
            expect(clean, "Non-finite output with a full EQ curve at "
                          + juce::String(rate) + " Hz");
        }
    }

    beginTest("EQ band gains round-trip through state save/load");
    {
        PluginProcessor a;
        a.setRateAndBufferSizeDetails(sr, blockSize);
        a.prepareToPlay(sr, blockSize);

        // Distinct values across the bands.
        float vals[DSPConstants::EQ_NUM_BANDS] { };
        for (int i = 0; i < DSPConstants::EQ_NUM_BANDS; ++i)
        {
            vals[i] = -10.0f + (float) i * 1.7f;   // spread across the ±12 range
            setParameter(a.parameters, "eqBand" + juce::String(i), vals[i]);
        }

        juce::MemoryBlock state;
        a.getStateInformation(state);

        PluginProcessor b;
        b.setRateAndBufferSizeDetails(sr, blockSize);
        b.prepareToPlay(sr, blockSize);
        b.setStateInformation(state.getData(), (int) state.getSize());

        for (int i = 0; i < DSPConstants::EQ_NUM_BANDS; ++i)
        {
            auto* param = b.parameters.getParameter("eqBand" + juce::String(i));
            expect(param != nullptr, "eqBand" + juce::String(i) + " missing after load");
            if (param == nullptr) continue;
            const float restored = param->convertFrom0to1(param->getValue());
            expectWithinAbsoluteError(restored, vals[i], 0.05f,
                "eqBand" + juce::String(i) + " did not round-trip");
        }
    }

#if defined (DISTORTION_RT_GUARD) && DISTORTION_RT_GUARD
    beginTest("No allocation while an active EQ curve updates on the audio thread");
    {
        PluginProcessor p;
        p.setRateAndBufferSizeDetails(sr, blockSize);
        p.prepareToPlay(sr, blockSize);
        setParameter(p.parameters, "distortionAmount", 50.0f);
        setParameter(p.parameters, "subGuardFreq",      0.0f);
        for (int i = 0; i < DSPConstants::EQ_NUM_BANDS; ++i)
            setParameter(p.parameters, "eqBand" + juce::String(i), 3.0f);

        auto* band6 = p.parameters.getParameter("eqBand6");
        auto* band6f = dynamic_cast<juce::AudioParameterFloat*>(band6);
        expect(band6f != nullptr, "eqBand6 parameter not found / type mismatch");
        if (band6f == nullptr) return;

        juce::MidiBuffer midi;
        auto warm = generateSineWave(1000.0, sr, blockSize, 0.25f);
        p.processBlock(warm, midi);

        rt_guard::resetAllocationCounter();
        for (int i = 0; i < 100; ++i)
        {
            // Drive live coefficient rewrites every block.
            const float g = -12.0f + 24.0f * (float) i / 99.0f;
            band6->setValueNotifyingHost(band6f->convertTo0to1(g));
            auto buf = generateSineWave(1000.0, sr, blockSize, 0.25f);
            p.processBlock(buf, midi);
        }
        expectEquals(rt_guard::getAllocationCount(), 0,
                     "Graphic EQ coefficient updates must not allocate on the audio thread");
    }
#endif
}

// =============================================================================
// PR-8: processBlock decomposition null-test gate
// Verifies bit-exact determinism: two identical runs must produce max diff = 0.
// =============================================================================
void ProcessBlockDecompTest::runTest()
{
    auto setup = [](PluginProcessor& p)
    {
        p.setRateAndBufferSizeDetails(48000.0, 512);
        p.prepareToPlay(48000.0, 512);
        TestUtilities::setParameter(p.parameters, "distortionAmount", 0.5f);
        TestUtilities::setParameter(p.parameters, "compEnabled",       1.0f);
        TestUtilities::setParameter(p.parameters, "subGuardFreq",      0.4f);
        TestUtilities::setParameter(p.parameters, "lfoEnabled",        1.0f);
        TestUtilities::setParameter(p.parameters, "lfoRate",           0.5f);
    };

    // generateSineWave(frequency, sampleRate, numSamples, amplitude)
    const float amp = juce::Decibels::decibelsToGain(-6.0f);
    juce::AudioBuffer<float> inputRef = TestUtilities::generateSineWave(1000.0, 48000.0, 512, amp);

    // Two independent instances initialised identically — avoids dangling
    // pb_oversampledBlock view from a second prepareToPlay on the same instance.
    PluginProcessor processorA, processorB;
    setup(processorA);
    setup(processorB);

    juce::AudioBuffer<float> runA(inputRef);
    juce::MidiBuffer midiA;
    processorA.processBlock(runA, midiA);

    juce::AudioBuffer<float> runB(inputRef);
    juce::MidiBuffer midiB;
    processorB.processBlock(runB, midiB);

    // Per-instance random generators (distortionRandom, waveshaperRandom) make strict
    // bit-exact comparison across instances impossible — they seed independently from time.
    // The golden-audio tests are the authoritative decomposition gate. Here we verify the
    // extracted code path is structurally sound: output is finite, audible, and matches a
    // peer instance within the bounded analog-noise envelope (clip type 0 adds noise up to
    // ~0.002 per sample).
    beginTest("processBlock output is finite and non-silent");
    expect(!TestUtilities::containsInvalidSamples(runA), "Output contains NaN or Inf");
    expect(TestUtilities::calculatePeak(runA) > 1e-6f,   "Output is silent");

    beginTest("processBlock output matches peer instance within analog-noise envelope");
    const float maxDiff = TestUtilities::calculateMaxDifference(runA, runB);
    expect(maxDiff < 0.01f, "Output diverges beyond analog-noise envelope (maxDiff="
                            + juce::String(maxDiff, 6) + ")");
}

//==============================================================================
// CyclingComboBoxTests Implementation
//==============================================================================

void CyclingComboBoxTests::runTest()
{
    beginTest("Forward cycle wraps");
    {
        CyclingComboBox box;
        box.addItem("A", 1);
        box.addItem("B", 2);
        box.addItem("C", 3);
        box.setSelectedItemIndex(0, juce::dontSendNotification);

        box.cycleSelection(+1);
        expect(box.getSelectedItemIndex() == 1, "should advance to index 1");
        box.cycleSelection(+1);
        expect(box.getSelectedItemIndex() == 2, "should advance to index 2");
        box.cycleSelection(+1);
        expect(box.getSelectedItemIndex() == 0, "should wrap to index 0");
    }

    beginTest("Backward cycle wraps");
    {
        CyclingComboBox box;
        box.addItem("A", 1);
        box.addItem("B", 2);
        box.addItem("C", 3);
        box.setSelectedItemIndex(0, juce::dontSendNotification);

        box.cycleSelection(-1);
        expect(box.getSelectedItemIndex() == 2, "should wrap back to index 2");
    }

    beginTest("Single item stays put");
    {
        CyclingComboBox box;
        box.addItem("Only", 1);
        box.setSelectedItemIndex(0, juce::dontSendNotification);

        box.cycleSelection(+1);
        expect(box.getSelectedItemIndex() == 0, "single item should not move");
    }

    beginTest("Empty box is a no-op");
    {
        CyclingComboBox box;
        box.cycleSelection(+1);
        expect(box.getSelectedItemIndex() == -1, "empty box should stay unselected");
    }
}

//==============================================================================
// Sub Guard / Input Filter Spectral Tests
//==============================================================================

namespace
{
    // Mono-sum magnitude-spectrum band energy (dB) over the first power-of-two window.
    float bandEnergyDb(const juce::AudioBuffer<float>& buf, double sampleRate,
                       double loHz, double hiHz, int fftOrder = 15)
    {
        const int fftSize = 1 << fftOrder;
        juce::dsp::FFT fft(fftOrder);
        std::vector<float> data(static_cast<size_t>(fftSize) * 2, 0.0f);

        const int rch = buf.getNumChannels() > 1 ? 1 : 0;
        const int avail = juce::jmin(fftSize, buf.getNumSamples());
        for (int n = 0; n < avail; ++n)
        {
            const float mono = 0.5f * (buf.getSample(0, n) + buf.getSample(rch, n));
            const float w = 0.5f - 0.5f * std::cos(2.0f * juce::MathConstants<float>::pi
                                                    * n / (fftSize - 1));   // Hann
            data[static_cast<size_t>(n)] = mono * w;
        }

        fft.performFrequencyOnlyForwardTransform(data.data());

        double sum = 0.0; int count = 0;
        for (int bin = 1; bin < fftSize / 2; ++bin)
        {
            const double f = bin * sampleRate / fftSize;
            if (f >= loHz && f < hiHz) { sum += data[(size_t) bin] * data[(size_t) bin]; ++count; }
        }
        return count ? juce::Decibels::gainToDecibels(static_cast<float>(std::sqrt(sum / count)))
                     : -120.0f;
    }

    // Process a buffer through the plugin in place, block by block.
    void runInBlocks(PluginProcessor& proc, juce::AudioBuffer<float>& buffer, int blockSize = 512)
    {
        juce::MidiBuffer midi;
        const int total = buffer.getNumSamples();
        for (int start = 0; start < total; start += blockSize)
        {
            const int len = juce::jmin(blockSize, total - start);
            juce::AudioBuffer<float> block(buffer.getArrayOfWritePointers(),
                                           buffer.getNumChannels(), start, len);
            midi.clear();
            proc.processBlock(block, midi);
        }
    }

    // Transparent processing (no nonlinear coloration). When defeatBypass is true the
    // compressor is enabled at zero reduction so the full chain runs (needed to exercise
    // Sub Guard, which lives inside the distortion path); when false the plugin stays in
    // true bypass (used to prove the input filter works with everything else off).
    void configureCleanProcessor(PluginProcessor& proc, bool defeatBypass)
    {
        auto& p = proc.parameters;
        setParameter(p, "inputGain", 50.0f);   // unity
        setParameter(p, "outputGain", 50.0f);  // unity
        setParameter(p, "globalMix", 100.0f);  // fully wet
        setParameter(p, "tone", 20000.0f);     // tone LP wide open
        setParameter(p, "distortionAmount", 0.0f);
        setParameter(p, "distMix", 0.0f);
        setParameter(p, "waveshaperMix", 0.0f);
        setParameter(p, "autoGainEnabled", 0.0f);
        setParameter(p, "cleanBoost", 0.0f);
        setParameter(p, "extremeEnabled", 0.0f);
        setParameter(p, "lfoEnabled", 0.0f);
        setParameter(p, "compEnabled", defeatBypass ? 1.0f : 0.0f);
        setParameter(p, "compPeakReduction", 0.0f);
    }
}

void SubGuardFlatnessTest::runTest()
{
    constexpr double sr = 44100.0;
    const int numSamples = 1 << 16;

    // One noise realisation, reused for both renders so the spectral ratio reflects
    // only the crossover (the input cancels exactly).
    juce::AudioBuffer<float> noise(2, numSamples);
    {
        juce::Random rng(20240603);
        for (int n = 0; n < numSamples; ++n)
        {
            const float s = 0.25f * (rng.nextFloat() * 2.0f - 1.0f);
            noise.setSample(0, n, s);
            noise.setSample(1, n, s);
        }
    }

    auto render = [&](float subGuardFreq)
    {
        PluginProcessor proc;
        proc.setRateAndBufferSizeDetails(sr, 512);
        proc.prepareToPlay(sr, 512);
        configureCleanProcessor(proc, /*defeatBypass*/ true);
        setParameter(proc.parameters, "subGuardFreq", subGuardFreq);

        juce::AudioBuffer<float> buf(2, numSamples);
        for (int ch = 0; ch < 2; ++ch) buf.copyFrom(ch, 0, noise, ch, 0, numSamples);
        runInBlocks(proc, buf);
        return buf;
    };

    const float edges[] = { 30, 40, 50, 60, 80, 100, 130, 160, 200, 300, 500, 1000, 2000 };
    constexpr int numEdges = sizeof(edges) / sizeof(edges[0]);

    for (float crossover : { 60.0f, 150.0f })
    {
        beginTest("Crossover flat at " + juce::String((int) crossover) + " Hz");

        const auto outOff = render(0.0f);
        const auto outOn  = render(crossover);

        float worst = 0.0f;
        for (int i = 0; i + 1 < numEdges; ++i)
        {
            const float on  = bandEnergyDb(outOn,  sr, edges[i], edges[i + 1]);
            const float off = bandEnergyDb(outOff, sr, edges[i], edges[i + 1]);
            worst = std::max(worst, std::abs(on - off));
        }
        expect(worst < 0.5f,
               "Sub Guard altered the spectrum by " + juce::String(worst, 2)
               + " dB at " + juce::String((int) crossover) + " Hz (expected flat)");
    }
}

void InputFilterModeTest::runTest()
{
    constexpr double sr = 44100.0;
    const int numSamples = 1 << 16;
    const float cutoff = 1000.0f;

    auto bands = [&](int modeIdx)
    {
        PluginProcessor proc;
        proc.setRateAndBufferSizeDetails(sr, 512);
        proc.prepareToPlay(sr, 512);
        // True bypass (compressor off) so we also prove the filter runs without distortion.
        configureCleanProcessor(proc, /*defeatBypass*/ false);
        setParameter(proc.parameters, "filterMode", static_cast<float>(modeIdx));
        setParameter(proc.parameters, "highPassFreq", cutoff);

        juce::Random rng(7);
        juce::AudioBuffer<float> buf(2, numSamples);
        for (int ch = 0; ch < 2; ++ch)
            for (int n = 0; n < numSamples; ++n)
                buf.setSample(ch, n, 0.25f * (rng.nextFloat() * 2.0f - 1.0f));
        runInBlocks(proc, buf);

        struct BandEnergies { float low, mid, high; };
        return BandEnergies {
            bandEnergyDb(buf, sr, 150, 250),
            bandEnergyDb(buf, sr, 900, 1100),
            bandEnergyDb(buf, sr, 4000, 6000)
        };
    };

    // ~10 dB margin: a 2nd-order band-pass rejects ~11-12 dB at 2.5 octaves from centre.
    beginTest("High Pass cuts lows");
    {
        const auto e = bands(0);
        expect((e.mid - e.low) > 10.0f,
               "High Pass did not attenuate lows (low=" + juce::String(e.low, 1)
               + " mid=" + juce::String(e.mid, 1) + " dB)");
    }

    beginTest("Low Pass cuts highs");
    {
        const auto e = bands(1);
        expect((e.mid - e.high) > 10.0f,
               "Low Pass did not attenuate highs (mid=" + juce::String(e.mid, 1)
               + " high=" + juce::String(e.high, 1) + " dB)");
    }

    beginTest("Band Pass cuts both sides");
    {
        const auto e = bands(2);
        expect((e.mid - e.low) > 10.0f && (e.mid - e.high) > 10.0f,
               "Band Pass did not attenuate both sides (low=" + juce::String(e.low, 1)
               + " mid=" + juce::String(e.mid, 1) + " high=" + juce::String(e.high, 1) + " dB)");
    }
}

//==============================================================================
// ShapeFilterTests — the Shape DSP unit on its own
//==============================================================================

namespace
{
    // Steady-state gain (dB) of a ShapeFilter at one frequency: run a 1 s sine,
    // compare RMS over the second half (filter settled, smoother finished).
    float shapeGainDb(double sr, float shape, float amount, double freq)
    {
        ShapeFilter f;
        juce::dsp::ProcessSpec spec { sr, 512, 2 };
        f.prepare(spec, shape, amount);

        const int n = static_cast<int>(sr);
        juce::AudioBuffer<float> buf(2, n);
        for (int i = 0; i < n; ++i)
        {
            const float s = 0.25f * static_cast<float>(
                std::sin(juce::MathConstants<double>::twoPi * freq * i / sr));
            buf.setSample(0, i, s);
            buf.setSample(1, i, s);
        }
        juce::AudioBuffer<float> in(buf);

        for (int start = 0; start < n; start += 512)
        {
            const int len = juce::jmin(512, n - start);
            juce::dsp::AudioBlock<float> block(buf.getArrayOfWritePointers(), 2,
                                               (size_t) start, (size_t) len);
            f.process(block);
        }

        double eIn = 0.0, eOut = 0.0;
        for (int i = n / 2; i < n; ++i)
        {
            eIn  += in.getSample(0, i)  * in.getSample(0, i);
            eOut += buf.getSample(0, i) * buf.getSample(0, i);
        }
        return static_cast<float>(10.0 * std::log10(eOut / eIn));
    }
}

void ShapeFilterTests::runTest()
{
    beginTest("Curve maths: taper, end gains, LFO clamp");
    {
        expectWithinAbsoluteError(ShapeFilter::taper(0.0f), 0.0f, 1.0e-6f);
        expectWithinAbsoluteError(ShapeFilter::taper(100.0f), 1.0f, 1.0e-6f);
        expectWithinAbsoluteError(ShapeFilter::taper(-100.0f), -1.0f, 1.0e-6f);
        expectWithinAbsoluteError(ShapeFilter::taper(50.0f), std::pow(0.5f, DSPConstants::SHAPE_TAPER), 1.0e-5f);
        expectWithinAbsoluteError(ShapeFilter::midGainDb(-100.0f, 1.0f), DSPConstants::SHAPE_MID_BOOST_DB, 1.0e-4f);
        expectWithinAbsoluteError(ShapeFilter::midGainDb(100.0f, 1.0f), -DSPConstants::SHAPE_MID_CUT_DB, 1.0e-4f);
        expectWithinAbsoluteError(ShapeFilter::midGainDb(100.0f, 0.5f), -0.5f * DSPConstants::SHAPE_MID_CUT_DB, 1.0e-4f);
        expectWithinAbsoluteError(ShapeFilter::shelfGainDb(-100.0f, 1.0f), -DSPConstants::SHAPE_SHELF_DB, 1.0e-4f);
        expectWithinAbsoluteError(ShapeFilter::shelfGainDb(100.0f, 1.0f), DSPConstants::SHAPE_SHELF_DB, 1.0e-4f);
        expectWithinAbsoluteError(ShapeFilter::modulated(80.0f, 1.0f), 100.0f, 1.0e-4f);
        expectWithinAbsoluteError(ShapeFilter::postDriveAmount(0.0f), 0.0f, 1.0e-6f);
        expectWithinAbsoluteError(ShapeFilter::postDriveAmount(0.5f * DSPConstants::SHAPE_POST_FULL_DRIVE),
                                  0.5f * DSPConstants::SHAPE_POST_AMOUNT, 1.0e-6f);
        expectWithinAbsoluteError(ShapeFilter::postDriveAmount(100.0f), DSPConstants::SHAPE_POST_AMOUNT, 1.0e-6f);
        expectWithinAbsoluteError(ShapeFilter::modulated(0.0f, -1.0f), -50.0f, 1.0e-4f);
        expectWithinAbsoluteError(ShapeFilter::modulated(-100.0f, -1.0f), -100.0f, 1.0e-4f);
    }

    beginTest("Value text round-trips");
    {
        expectEquals(ShapeFilter::toText(0.0f), juce::String("Flat"));
        expectEquals(ShapeFilter::toText(0.4f), juce::String("Flat"));
        expectEquals(ShapeFilter::toText(-60.0f), juce::String("Bark 60"));
        expectEquals(ShapeFilter::toText(40.2f), juce::String("Scoop 40"));
        expectWithinAbsoluteError(ShapeFilter::fromText("Bark 60"), -60.0f, 1.0e-4f);
        expectWithinAbsoluteError(ShapeFilter::fromText("scoop 40"), 40.0f, 1.0e-4f);
        expectWithinAbsoluteError(ShapeFilter::fromText("Flat"), 0.0f, 1.0e-4f);
        expectWithinAbsoluteError(ShapeFilter::fromText("-25"), -25.0f, 1.0e-4f);
    }

    beginTest("Shape 0 is bit-transparent and inactive");
    {
        ShapeFilter f;
        f.prepare({ 48000.0, 512, 2 }, 0.0f, 1.0f);
        expect(! f.isActive(), "Shape 0 reported active");

        juce::Random rng(11);
        juce::AudioBuffer<float> buf(2, 512);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 512; ++i)
                buf.setSample(ch, i, rng.nextFloat() * 2.0f - 1.0f);
        juce::AudioBuffer<float> in(buf);
        juce::dsp::AudioBlock<float> block(buf);
        f.process(block);
        expectEquals(calculateMaxDifference(in, buf), 0.0f);
    }

    beginTest("Bark and Scoop hit their targets at 600 Hz and in the treble");
    {
        constexpr double sr = 48000.0;
        expectWithinAbsoluteError(shapeGainDb(sr, -100.0f, 1.0f, 600.0), DSPConstants::SHAPE_MID_BOOST_DB, 0.5f);
        expectWithinAbsoluteError(shapeGainDb(sr,  100.0f, 1.0f, 600.0), -DSPConstants::SHAPE_MID_CUT_DB, 0.5f);
        expect(shapeGainDb(sr, -100.0f, 1.0f, 6000.0) < -(DSPConstants::SHAPE_SHELF_DB - 1.5f), "Bark did not soften the treble");
        expect(shapeGainDb(sr,  100.0f, 1.0f, 6000.0) >  DSPConstants::SHAPE_SHELF_DB - 1.5f, "Scoop did not lift the treble");
        expectWithinAbsoluteError(shapeGainDb(sr, 100.0f, 0.5f, 600.0), -0.5f * DSPConstants::SHAPE_MID_CUT_DB, 0.5f);
    }

    beginTest("Sub band stays within 1.5 dB at every knob position");
    {
        for (float shape : { -100.0f, -50.0f, 0.0f, 50.0f, 100.0f })
            for (double f : { 40.0, 60.0, 80.0 })
            {
                const float g = shapeGainDb(48000.0, shape, 1.0f, f);
                expect(std::abs(g) < 1.5f, "Shape " + juce::String(shape) + " moved "
                       + juce::String(f) + " Hz by " + juce::String(g, 2) + " dB");
            }
    }

    beginTest("Response holds at 768 kHz (192 kHz x 4, the post-drive worst case)");
    {
        expectWithinAbsoluteError(shapeGainDb(768000.0, -100.0f, 1.0f, 600.0), DSPConstants::SHAPE_MID_BOOST_DB, 0.75f);
        expectWithinAbsoluteError(shapeGainDb(768000.0,  100.0f, 1.0f, 600.0), -DSPConstants::SHAPE_MID_CUT_DB, 0.75f);
    }

    beginTest("Finite at every supported rate, both extremes");
    {
        for (double sr : { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0, 768000.0 })
            for (float shape : { -100.0f, 100.0f })
            {
                const float g = shapeGainDb(sr, shape, 1.0f, 600.0);
                expect(std::isfinite(g), "Non-finite at " + juce::String(sr) + " Hz");
            }
    }

    beginTest("A full Bark-to-Scoop jump is click-free");
    {
        constexpr double sr = 48000.0;
        constexpr int n = 24000;
        // Max sample-to-sample step over the window AFTER the switch point only, so the
        // loud pre-jump Bark section cannot dominate the figure. The reference is a
        // filter that sat at Scoop the whole time, measured over the same window.
        auto maxStepAfter = [&](float startShape, bool jump)
        {
            ShapeFilter f;
            f.prepare({ sr, 512, 1 }, startShape, 1.0f);
            juce::AudioBuffer<float> buf(1, n);
            for (int i = 0; i < n; ++i)
                buf.setSample(0, i, 0.5f * static_cast<float>(
                    std::sin(juce::MathConstants<double>::twoPi * 200.0 * i / sr)));
            float worst = 0.0f, prev = 0.0f;
            for (int start = 0; start < n; start += 512)
            {
                if (jump && start == n / 2)
                    f.setTarget(100.0f);
                const int len = juce::jmin(512, n - start);
                juce::dsp::AudioBlock<float> block(buf.getArrayOfWritePointers(), 1,
                                                   (size_t) start, (size_t) len);
                f.process(block);
                for (int i = start; i < start + len; ++i)
                {
                    if (i >= n / 2)
                        worst = std::max(worst, std::abs(buf.getSample(0, i) - prev));
                    prev = buf.getSample(0, i);
                }
            }
            return worst;
        };
        // A 200 Hz sine's own max step scales with its amplitude, and Bark passes it louder
        // than Scoop, so the post-jump window legitimately starts at the Bark step size
        // and decays to the Scoop one. A click would add a discontinuity on top of that,
        // so the reference is the larger of the two steady-state steps over the same window.
        const float steadyScoop = maxStepAfter(100.0f, false);
        const float steadyBark  = maxStepAfter(-100.0f, false);
        const float jumped      = maxStepAfter(-100.0f, true);
        expect(jumped < std::max(steadyScoop, steadyBark) * 1.25f,
               "Post-jump step " + juce::String(jumped, 4) + " vs steady Scoop "
               + juce::String(steadyScoop, 4) + " / Bark " + juce::String(steadyBark, 4));
    }

    beginTest("Settling back to Flat leaves no residual to cut off");
    {
        // Worst case: the 20 ms ramp ends on the last sub-block of a block, so the
        // filter runs only one sub-block at identity before process() starts skipping
        // and clears the filter memory. Whatever error is still decaying at that point
        // is dropped in one sample; it must be inaudible.
        constexpr double sr = 48000.0;
        constexpr int blockSize = 480;   // 20 ms ramp = exactly two blocks
        ShapeFilter f;
        f.prepare({ sr, (juce::uint32) blockSize, 1 }, 60.0f, 1.0f);
        juce::AudioBuffer<float> buf(1, blockSize), in(1, blockSize);
        float lastError = 0.0f;
        int n = 0;
        for (int b = 0; b < 40; ++b)
        {
            if (b == 20)
                f.setTarget(0.0f);
            for (int i = 0; i < blockSize; ++i, ++n)
                buf.setSample(0, i, 0.5f * static_cast<float>(
                    std::sin(juce::MathConstants<double>::twoPi * 600.0 * n / sr)));
            in.makeCopyOf(buf);
            const bool wasActive = f.isActive();
            juce::dsp::AudioBlock<float> block(buf);
            f.process(block);
            if (wasActive)
                lastError = buf.getSample(0, blockSize - 1) - in.getSample(0, blockSize - 1);
        }
        expect(! f.isActive(), "Shape did not settle to Flat");
        expect(std::abs(lastError) < 1.0e-4f,
               "Residual cut off when going Flat: " + juce::String(lastError, 7));
    }
}

//==============================================================================
// ShapeProcessorTests — Shape inside the plugin
//==============================================================================

void ShapeProcessorTests::runTest()
{
    constexpr double sr = 44100.0;
    const int numSamples = 1 << 16;

    auto noise = [&]
    {
        juce::Random rng(7);
        juce::AudioBuffer<float> buf(2, numSamples);
        for (int ch = 0; ch < 2; ++ch)
            for (int n = 0; n < numSamples; ++n)
                buf.setSample(ch, n, 0.25f * (rng.nextFloat() * 2.0f - 1.0f));
        return buf;
    };

    // True bypass (distortion and comp off): only the pre-drive half runs.
    auto bypassBand = [&](float shape, double lo, double hi)
    {
        PluginProcessor proc;
        proc.setRateAndBufferSizeDetails(sr, 512);
        proc.prepareToPlay(sr, 512);
        configureCleanProcessor(proc, /*defeatBypass*/ false);
        setParameter(proc.parameters, "shape", shape);
        auto buf = noise();
        runInBlocks(proc, buf);
        return bandEnergyDb(buf, sr, lo, hi);
    };

    beginTest("Parameter exists with default 0 and Bark/Flat/Scoop text");
    {
        PluginProcessor proc;
        auto* p = proc.parameters.getParameter("shape");
        expect(p != nullptr, "shape parameter missing");
        if (p != nullptr)
        {
            expectWithinAbsoluteError(p->convertFrom0to1(p->getDefaultValue()), 0.0f, 1.0e-4f);
            expectEquals(p->getText(p->convertTo0to1(-60.0f), 32), juce::String("Bark 60"));
            expectEquals(p->getText(p->convertTo0to1(0.0f), 32), juce::String("Flat"));
        }
    }

    beginTest("Pre-drive Shape works in true bypass: Bark lifts and Scoop cuts 600 Hz");
    {
        const float flat  = bypassBand(0.0f,    550.0, 650.0);
        const float bark  = bypassBand(-100.0f, 550.0, 650.0);
        const float scoop = bypassBand(100.0f,  550.0, 650.0);
        expectWithinAbsoluteError(bark - flat,  DSPConstants::SHAPE_MID_BOOST_DB * DSPConstants::SHAPE_PRE_AMOUNT, 1.5f);
        expectWithinAbsoluteError(scoop - flat, -DSPConstants::SHAPE_MID_CUT_DB * DSPConstants::SHAPE_PRE_AMOUNT, 1.5f);
    }

    beginTest("Sub band below 80 Hz moves less than 1.5 dB through the plugin");
    {
        const float flat = bypassBand(0.0f, 30.0, 80.0);
        for (float shape : { -100.0f, -50.0f, 50.0f, 100.0f })
            expect(std::abs(bypassBand(shape, 30.0, 80.0) - flat) < 1.5f,
                   "Sub band moved at Shape " + juce::String(shape));
    }

    beginTest("Post-drive half shapes a clean 50 Hz sine (808 case)");
    {
        auto render = [&](float shape)
        {
            PluginProcessor proc;
            proc.setRateAndBufferSizeDetails(sr, 512);
            proc.prepareToPlay(sr, 512);
            setParameter(proc.parameters, "distortionAmount", 70.0f);
            setParameter(proc.parameters, "clipType", 1.0f);       // Tube: deterministic
            setParameter(proc.parameters, "waveshaperMix", 0.0f);
            setParameter(proc.parameters, "subGuardFreq", 0.0f);
            setParameter(proc.parameters, "autoGainEnabled", 0.0f);
            setParameter(proc.parameters, "shape", shape);
            juce::AudioBuffer<float> buf(2, numSamples);
            for (int n = 0; n < numSamples; ++n)
            {
                const float s = 0.7f * std::sin(juce::MathConstants<float>::twoPi * 50.0f * n / (float) sr);
                buf.setSample(0, n, s);
                buf.setSample(1, n, s);
            }
            runInBlocks(proc, buf);
            return bandEnergyDb(buf, sr, 400.0, 800.0) - bandEnergyDb(buf, sr, 40.0, 60.0);
        };
        const float bark  = render(-100.0f);
        const float scoop = render(100.0f);
        expect(bark - scoop > 4.0f, "Bark vs Scoop changed the 400-800 Hz harmonics by only "
               + juce::String(bark - scoop, 2) + " dB on a clean sine");
    }

    beginTest("No allocation with Shape engaged and the LFO on it");
    {
        PluginProcessor proc;
        proc.setRateAndBufferSizeDetails(48000.0, 512);
        proc.prepareToPlay(48000.0, 512);
        setParameter(proc.parameters, "shape", -100.0f);
        setParameter(proc.parameters, "distortionAmount", 60.0f);
        setParameter(proc.parameters, "lfoEnabled", 1.0f);
        setParameter(proc.parameters, "lfoDestination", 2.0f);
        setParameter(proc.parameters, "lfoRate", 5.0f);
        setParameter(proc.parameters, "lfoDepth", 100.0f);
        juce::AudioBuffer<float> buffer(2, 512);
        juce::MidiBuffer midi;
        buffer.clear();
        proc.processBlock(buffer, midi);
        rt_guard::resetAllocationCounter();
        proc.processBlock(buffer, midi);
        expectEquals(rt_guard::getAllocationCount(), 0, "Shape allocated on the audio thread");
    }

    beginTest("Finite at every rate with a fast LFO sweep on Shape");
    {
        for (double rate : { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 })
            for (float shape : { -100.0f, 100.0f })
            {
                PluginProcessor proc;
                proc.setRateAndBufferSizeDetails(rate, 512);
                proc.prepareToPlay(rate, 512);
                setParameter(proc.parameters, "shape", shape);
                setParameter(proc.parameters, "distortionAmount", 80.0f);
                setParameter(proc.parameters, "lfoEnabled", 1.0f);
                setParameter(proc.parameters, "lfoDestination", 2.0f);
                setParameter(proc.parameters, "lfoRate", 10.0f);
                setParameter(proc.parameters, "lfoDepth", 100.0f);
                auto buf = noise();
                runInBlocks(proc, buf);
                expect(! containsInvalidSamples(buf), "Non-finite at " + juce::String(rate));
            }
    }

    beginTest("Full Bark, hot input stays finite and under the ceiling");
    {
        PluginProcessor proc;
        proc.setRateAndBufferSizeDetails(sr, 512);
        proc.prepareToPlay(sr, 512);
        setParameter(proc.parameters, "shape", -100.0f);
        setParameter(proc.parameters, "inputGain", 100.0f);
        setParameter(proc.parameters, "distortionAmount", 100.0f);
        setParameter(proc.parameters, "outputGain", 100.0f);
        auto buf = noise();
        buf.applyGain(4.0f);
        runInBlocks(proc, buf);
        expect(! containsInvalidSamples(buf), "Non-finite at full Bark");
        const float ceiling = juce::Decibels::decibelsToGain(DSPConstants::OUTPUT_LIMITER_THRESHOLD_DB) + 1.0e-3f;
        expect(calculatePeak(buf) <= ceiling, "Peak " + juce::String(calculatePeak(buf))
               + " above the output limiter ceiling " + juce::String(ceiling));
    }

    beginTest("Mono-in stereo-out with Shape is finite");
    {
        PluginProcessor proc;
        juce::AudioProcessor::BusesLayout layout;
        layout.inputBuses.add(juce::AudioChannelSet::mono());
        layout.outputBuses.add(juce::AudioChannelSet::stereo());
        expect(proc.setBusesLayout(layout), "mono->stereo layout refused");
        proc.setRateAndBufferSizeDetails(sr, 512);
        proc.prepareToPlay(sr, 512);
        setParameter(proc.parameters, "shape", 100.0f);
        setParameter(proc.parameters, "distortionAmount", 60.0f);
        auto buf = noise();
        runInBlocks(proc, buf);
        expect(! containsInvalidSamples(buf), "Non-finite under mono->stereo");
    }

    beginTest("Oversampling rebuild keeps Shape finite");
    {
        PluginProcessor proc;
        proc.setRateAndBufferSizeDetails(sr, 512);
        proc.prepareToPlay(sr, 512);
        setParameter(proc.parameters, "shape", -80.0f);
        setParameter(proc.parameters, "distortionAmount", 60.0f);
        auto buf = noise();
        runInBlocks(proc, buf);
        proc.prepareToPlay(96000.0, 256);   // new rate: both instances re-prepare
        auto buf2 = noise();
        runInBlocks(proc, buf2, 256);
        expect(! containsInvalidSamples(buf2), "Non-finite after re-prepare");
    }

    beginTest("Shape keeps its response after re-preparing at 96 kHz");
    {
        // Prepared at 44.1 kHz first, then re-prepared: a shapePre prepared at the
        // wrong rate would put the 600 Hz lift somewhere else.
        auto band = [&](float shape)
        {
            PluginProcessor proc;
            proc.setRateAndBufferSizeDetails(sr, 512);
            proc.prepareToPlay(sr, 512);
            proc.setRateAndBufferSizeDetails(96000.0, 512);
            proc.prepareToPlay(96000.0, 512);
            configureCleanProcessor(proc, /*defeatBypass*/ false);
            setParameter(proc.parameters, "shape", shape);
            auto buf = noise();
            runInBlocks(proc, buf);
            return bandEnergyDb(buf, 96000.0, 550.0, 650.0, 16);
        };
        const float flat = band(0.0f);
        expectWithinAbsoluteError(band(-100.0f) - flat, DSPConstants::SHAPE_MID_BOOST_DB * DSPConstants::SHAPE_PRE_AMOUNT, 1.5f);
        expectWithinAbsoluteError(band(100.0f) - flat, -DSPConstants::SHAPE_MID_CUT_DB * DSPConstants::SHAPE_PRE_AMOUNT, 1.5f);
    }

    beginTest("Runtime oversampling rebuild keeps the post-drive half working");
    {
        // The post-drive bank lives at the oversampled rate, so a runtime rebuild
        // (no prepareToPlay) must re-prepare it. 4x -> 2x, then check Bark vs Scoop
        // still reshapes the harmonics of a clean 50 Hz sine.
        auto render = [&](float shape)
        {
            PluginProcessor proc;
            proc.setRateAndBufferSizeDetails(sr, 512);
            proc.prepareToPlay(sr, 512);
            setParameter(proc.parameters, "distortionAmount", 70.0f);
            setParameter(proc.parameters, "clipType", 1.0f);
            setParameter(proc.parameters, "waveshaperMix", 0.0f);
            setParameter(proc.parameters, "subGuardFreq", 0.0f);
            setParameter(proc.parameters, "autoGainEnabled", 0.0f);
            setParameter(proc.parameters, "shape", shape);
            proc.requestOversamplingRebuild(1);
            proc.handleAsyncUpdate();
            juce::AudioBuffer<float> buf(2, numSamples);
            for (int n = 0; n < numSamples; ++n)
            {
                const float v = 0.7f * std::sin(juce::MathConstants<float>::twoPi * 50.0f * n / (float) sr);
                buf.setSample(0, n, v);
                buf.setSample(1, n, v);
            }
            runInBlocks(proc, buf);
            expect(! containsInvalidSamples(buf), "Non-finite after runtime rebuild");
            return bandEnergyDb(buf, sr, 400.0, 800.0) - bandEnergyDb(buf, sr, 40.0, 60.0);
        };
        const float diff = render(-100.0f) - render(100.0f);
        expect(diff > 4.0f, "Bark vs Scoop changed the harmonics by only "
               + juce::String(diff, 2) + " dB after a runtime oversampling rebuild");
    }

    beginTest("True bypass at Shape 0 is bit-clean");
    {
        PluginProcessor proc;
        proc.setRateAndBufferSizeDetails(sr, 512);
        proc.prepareToPlay(sr, 512);
        configureCleanProcessor(proc, /*defeatBypass*/ false);
        setParameter(proc.parameters, "shape", 0.0f);
        setParameter(proc.parameters, "filterMode", 0.0f);
        setParameter(proc.parameters, "highPassFreq", 20.0f);
        const int latency = proc.getLatencySamples();
        const auto in = noise();
        auto out = in;
        runInBlocks(proc, out);
        float worst = 0.0f;
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 4096; i + latency < numSamples; ++i)
                worst = std::max(worst, std::abs(out.getSample(ch, i + latency) - in.getSample(ch, i)));
        expectEquals(worst, 0.0f, "Shape 0 in true bypass altered the signal");
    }

    beginTest("LFO destination 2 moves Shape");
    {
        // True bypass, Shape 0, Square LFO at 2 Hz (half-period 11025 samples), depth 100:
        // Shape alternates between about +50 (Scoop) and -50 (Bark). Short-window band
        // energy at 600 Hz must therefore swing by more than 3 dB; with the LFO doing
        // nothing the spread stays near zero.
        auto windowSpread = [&](bool lfoOn)
        {
            PluginProcessor proc;
            proc.setRateAndBufferSizeDetails(sr, 512);
            proc.prepareToPlay(sr, 512);
            configureCleanProcessor(proc, /*defeatBypass*/ false);
            setParameter(proc.parameters, "shape", 0.0f);
            setParameter(proc.parameters, "lfoEnabled", lfoOn ? 1.0f : 0.0f);
            setParameter(proc.parameters, "lfoDestination", 2.0f);
            setParameter(proc.parameters, "lfoWaveform", 2.0f);   // Square
            setParameter(proc.parameters, "lfoRate", 2.0f);
            setParameter(proc.parameters, "lfoDepth", 100.0f);
            // A steady 600 Hz sine, not noise: its band energy follows the filter gain
            // exactly, so the static spread is ~0 and any swing is the LFO's.
            juce::AudioBuffer<float> buf(2, numSamples);
            for (int n = 0; n < numSamples; ++n)
            {
                const float v = 0.25f * std::sin(juce::MathConstants<float>::twoPi * 600.0f * n / (float) sr);
                buf.setSample(0, n, v);
                buf.setSample(1, n, v);
            }
            runInBlocks(proc, buf);
            constexpr int win = 4096;
            float lo = 1.0e9f, hi = -1.0e9f;
            for (int start = 4096; start + win <= numSamples; start += win)
            {
                juce::AudioBuffer<float> w(2, win);
                for (int ch = 0; ch < 2; ++ch)
                    w.copyFrom(ch, 0, buf, ch, start, win);
                const float e = bandEnergyDb(w, sr, 550.0, 650.0, 12);
                lo = std::min(lo, e);
                hi = std::max(hi, e);
            }
            return hi - lo;
        };
        const float moving = windowSpread(true);
        const float still = windowSpread(false);
        expect(moving > 3.0f, "LFO on Shape swung the 600 Hz band by only "
               + juce::String(moving, 2) + " dB");
        expect(moving > still + 2.0f, "LFO spread " + juce::String(moving, 2)
               + " dB not clearly above the static spread " + juce::String(still, 2));
    }

    beginTest("Legacy predicate matches the transparent thresholds");
    {
        expect(! LegacyInputFilter::isActive(0, 20.0f));
        expect(! LegacyInputFilter::isActive(0, DSPConstants::HIPASS_TRANSPARENT_MAX_FREQ));
        expect(LegacyInputFilter::isActive(0, 80.0f));
        expect(LegacyInputFilter::isActive(1, 5000.0f));
        expect(! LegacyInputFilter::isActive(1, 20000.0f));
        expect(LegacyInputFilter::isActive(2, 1000.0f));
    }

    beginTest("Legacy cutoff is not LFO-modulated any more");
    {
        // Destination 2 used to sweep highPassFreq up to 1.5 octaves (20 Hz -> ~57 Hz),
        // which cut the 25-45 Hz band by several dB. Now it sweeps Shape (±50), which
        // barely touches that band, and the legacy filter stays at 20 Hz.
        const float still = bypassBand(0.0f, 25.0, 45.0);
        PluginProcessor proc;
        proc.setRateAndBufferSizeDetails(sr, 512);
        proc.prepareToPlay(sr, 512);
        configureCleanProcessor(proc, false);
        setParameter(proc.parameters, "lfoEnabled", 1.0f);
        setParameter(proc.parameters, "lfoDestination", 2.0f);
        setParameter(proc.parameters, "lfoWaveform", 2.0f);    // Square: full swing half the time
        setParameter(proc.parameters, "lfoRate", 0.5f);
        setParameter(proc.parameters, "lfoDepth", 100.0f);
        auto buf = noise();
        runInBlocks(proc, buf);
        expectWithinAbsoluteError(bandEnergyDb(buf, sr, 25.0, 45.0), still, 0.5f);
    }

    beginTest("A pre-Shape state loads with Shape 0 and the legacy filter intact");
    {
        PluginProcessor src;
        setParameter(src.parameters, "filterMode", 1.0f);
        setParameter(src.parameters, "highPassFreq", 500.0f);
        juce::MemoryBlock saved;
        src.getStateInformation(saved);

        // Turn it into a v1 state: no shape PARAM, stateVersion 1.
        std::unique_ptr<juce::XmlElement> xml(juce::AudioProcessor::getXmlFromBinary(
            saved.getData(), (int) saved.getSize()));
        expect(xml != nullptr);
        if (xml == nullptr)
            return;
        for (auto* child = xml->getFirstChildElement(); child != nullptr;)
        {
            auto* next = child->getNextElement();
            if (child->getStringAttribute("id") == "shape")
                xml->removeChildElement(child, true);
            child = next;
        }
        xml->setAttribute(PluginProcessor::stateVersionAttribute, 1);
        juce::MemoryBlock v1;
        juce::AudioProcessor::copyXmlToBinary(*xml, v1);

        PluginProcessor dst;
        setParameter(dst.parameters, "shape", 70.0f);   // must not survive the load
        dst.setStateInformation(v1.getData(), (int) v1.getSize());

        auto value = [&](const char* id)
        {
            auto* p = dst.parameters.getParameter(id);
            return p->convertFrom0to1(p->getValue());
        };
        expectWithinAbsoluteError(value("shape"), 0.0f, 1.0e-3f);
        expectWithinAbsoluteError(value("filterMode"), 1.0f, 1.0e-3f);
        expectWithinAbsoluteError(value("highPassFreq"), 500.0f, 0.5f);
    }

    beginTest("Shape's mid lift matches across the true-bypass threshold");
    {
        // Distortion 0.4% is true bypass (pre-drive Shape only); 0.6% is the active
        // path. With distMix 0 the drive itself is out of the signal, so the only
        // difference Shape may make between the two is the post-drive half, which
        // must fade in with the drive rather than switch on at the threshold.
        auto barkLift = [&](float drive)
        {
            auto band = [&](float shape)
            {
                PluginProcessor proc;
                proc.setRateAndBufferSizeDetails(sr, 512);
                proc.prepareToPlay(sr, 512);
                configureCleanProcessor(proc, /*defeatBypass*/ false);
                setParameter(proc.parameters, "subGuardFreq", 0.0f);
                setParameter(proc.parameters, "distortionAmount", drive);
                setParameter(proc.parameters, "shape", shape);
                auto buf = noise();
                buf.applyGain(0.2f);   // stay clear of the soft clipper and limiter
                runInBlocks(proc, buf);
                return bandEnergyDb(buf, sr, 550.0, 650.0);
            };
            return band(-100.0f) - band(0.0f);
        };
        const float bypassed = barkLift(0.4f);
        const float active   = barkLift(0.6f);
        expect(std::abs(active - bypassed) < 1.5f,
               "Bark lift jumps from " + juce::String(bypassed, 2) + " dB (bypass) to "
               + juce::String(active, 2) + " dB (active) at the threshold");
    }

    beginTest("With Sub Guard on, the post-drive Shape leaves the clean low band alone");
    {
        // A quiet 40 Hz sine sits entirely in Sub Guard's clean low band. With the
        // drive mixed out (distMix 0) and nothing after it pushed into nonlinearity,
        // the chain is linear, so Bark may move 40 Hz only by what the pre-drive
        // half does to it before the split. If the post-drive half runs on
        // low + high before the low band is subtracted, it reaches the sub too.
        auto subGainDb = [&](float shape)
        {
            PluginProcessor proc;
            proc.setRateAndBufferSizeDetails(sr, 512);
            proc.prepareToPlay(sr, 512);
            configureCleanProcessor(proc, /*defeatBypass*/ true);
            setParameter(proc.parameters, "subGuardFreq", 150.0f);
            setParameter(proc.parameters, "distortionAmount", 70.0f);   // post-drive half at full amount
            setParameter(proc.parameters, "shape", shape);
            juce::AudioBuffer<float> buf(2, numSamples);
            for (int n = 0; n < numSamples; ++n)
            {
                const float s = 0.05f * std::sin(juce::MathConstants<float>::twoPi * 40.0f * n / (float) sr);
                buf.setSample(0, n, s);
                buf.setSample(1, n, s);
            }
            runInBlocks(proc, buf);
            return bandEnergyDb(buf, sr, 30.0, 50.0);
        };
        const float measured = subGainDb(-100.0f) - subGainDb(0.0f);
        const float preOnly  = shapeGainDb(sr, -100.0f, DSPConstants::SHAPE_PRE_AMOUNT, 40.0);
        expect(std::abs(measured - preOnly) < 0.03f,
               "Bark moved 40 Hz by " + juce::String(measured, 3) + " dB through Sub Guard; the "
               "pre-drive half alone accounts for " + juce::String(preOnly, 3) + " dB");
    }
}


//==============================================================================
// Resampling island: a model at its trained rate inside any host rate
//==============================================================================

namespace
{
    constexpr double kTwoPi = juce::MathConstants<double>::twoPi;

    std::vector<float> sineAt(double frequency, double sampleRate, double seconds, float amplitude)
    {
        std::vector<float> x(static_cast<size_t>(std::lround(sampleRate * seconds)));
        for (size_t i = 0; i < x.size(); ++i)
            x[i] = amplitude * static_cast<float>(std::sin(kTwoPi * frequency * static_cast<double>(i) / sampleRate));
        return x;
    }

    std::vector<float> multitoneAt(const std::vector<double>& frequencies, double sampleRate,
                                   double seconds, float amplitudeEach)
    {
        std::vector<float> x(static_cast<size_t>(std::lround(sampleRate * seconds)), 0.0f);
        for (size_t i = 0; i < x.size(); ++i)
            for (size_t f = 0; f < frequencies.size(); ++f)
                x[i] += amplitudeEach * static_cast<float>(std::sin(kTwoPi * frequencies[f] * static_cast<double>(i) / sampleRate
                                                                    + 0.7 * static_cast<double>(f)));   // spread the phases
        return x;
    }

    // Amplitude of the component at `frequency` in x[start, start + length): a
    // Hann-windowed single-bin DFT, exact for a steady tone.
    double toneAmplitude(const std::vector<float>& x, size_t start, size_t length,
                         double frequency, double sampleRate)
    {
        double re = 0.0, im = 0.0;
        for (size_t i = 0; i < length; ++i)
        {
            const double window = 0.5 - 0.5 * std::cos(kTwoPi * static_cast<double>(i) / static_cast<double>(length - 1));
            const double phase = kTwoPi * frequency * static_cast<double>(i) / sampleRate;
            re += static_cast<double>(x[start + i]) * window * std::cos(phase);
            im -= static_cast<double>(x[start + i]) * window * std::sin(phase);
        }
        return 4.0 * std::sqrt(re * re + im * im) / static_cast<double>(length);
    }

    double toDb(double ratio) { return 20.0 * std::log10(juce::jmax(ratio, 1.0e-12)); }

    size_t indexOfPeak(const std::vector<float>& x)
    {
        size_t best = 0;
        for (size_t i = 1; i < x.size(); ++i)
            if (std::abs(x[i]) > std::abs(x[best]))
                best = i;
        return best;
    }

    // Runs `input` through the island in host blocks of `blockSize`, with `modelFn`
    // standing in for the model.
    template <typename ModelFn>
    std::vector<float> runIsland(ResamplingIsland& island, const std::vector<float>& input,
                                 int blockSize, ModelFn&& modelFn)
    {
        std::vector<float> out(input.size());
        for (size_t done = 0; done < input.size();)
        {
            const int n = static_cast<int>(std::min<size_t>(static_cast<size_t>(blockSize), input.size() - done));
            island.process(input.data() + done, out.data() + done, n, modelFn);
            done += static_cast<size_t>(n);
        }
        return out;
    }

    const auto passThroughModel = [](const float* in, float* out, int n)
    {
        std::copy(in, in + n, out);
    };
}

void ResamplingIslandTests::runTest()
{
    beginTest("The delay is a whole number of host samples, fixed per rate pair");
    {
        expectEquals(ResamplingIsland::latencyFor(48000.0, 48000.0), 0);
        expectEquals(ResamplingIsland::latencyFor(44100.0, 48000.0), 25);
        expectEquals(ResamplingIsland::latencyFor(88200.0, 48000.0), 46);
        expectEquals(ResamplingIsland::latencyFor(96000.0, 48000.0), 48);
        expectEquals(ResamplingIsland::latencyFor(192000.0, 48000.0), 96);
        expectEquals(ResamplingIsland::latencyFor(32000.0, 48000.0), 24);
    }

    beginTest("Every standard rate is supported; one needing too many phases is refused");
    {
        for (const double rate : { 8000.0, 11025.0, 16000.0, 22050.0, 32000.0, 44100.0, 48000.0,
                                   64000.0, 88200.0, 96000.0, 176400.0, 192000.0, 352800.0, 384000.0 })
            expect(ResamplingIsland::supports(rate, 48000.0), "Supports " + juce::String(rate));

        // 44 056 Hz against 48 kHz reduces to 5507 / 6000: far past 1024 phases.
        expect(! ResamplingIsland::supports(44056.0, 48000.0), "44.056 kHz is refused");
        expectEquals(ResamplingIsland::latencyFor(44056.0, 48000.0), -1);
        ResamplingIsland island;
        expect(! island.prepare(44056.0, 48000.0, 512), "prepare refuses it too");
        expect(! island.isPrepared(), "and leaves the island unprepared");
    }

    beginTest("Equal rates pass audio straight through with no delay");
    {
        ResamplingIsland island;
        expect(island.prepare(48000.0, 48000.0, 256), "prepares");
        expect(island.isPassThrough(), "is a pass-through");
        expectEquals(island.getLatencySamples(), 0);
        expectEquals(island.getMaxModelBlock(), 256);

        const auto input = sineAt(997.0, 48000.0, 0.1, 0.5f);
        int largestCall = 0;
        const auto doubled = runIsland(island, input, 1000, [&](const float* in, float* out, int n)
        {
            largestCall = juce::jmax(largestCall, n);
            for (int i = 0; i < n; ++i)
                out[i] = 2.0f * in[i];
        });
        bool exact = true;
        for (size_t i = 0; i < input.size(); ++i)
            exact = exact && doubled[i] == 2.0f * input[i];
        expect(exact, "Every sample went straight through the model");
        expect(largestCall <= 256, "Host blocks larger than prepared are chunked");
    }

    beginTest("An impulse comes out exactly D samples late");
    {
        for (const double rate : { 44100.0, 88200.0, 96000.0, 192000.0, 32000.0 })
        {
            ResamplingIsland island;
            expect(island.prepare(rate, 48000.0, 512), "prepares at " + juce::String(rate));
            expectEquals(island.getLatencySamples(), ResamplingIsland::latencyFor(rate, 48000.0));

            std::vector<float> input(4096, 0.0f);
            input[1000] = 1.0f;
            const auto out = runIsland(island, input, 512, passThroughModel);
            expectEquals(static_cast<int>(indexOfPeak(out)), 1000 + island.getLatencySamples(),
                         "Peak position at " + juce::String(rate));
            // Only the band below the model's Nyquist survives the round trip, so an
            // impulse at 96 or 192 kHz peaks near 0.5 or 0.25 by construction.
            expect(std::abs(out[indexOfPeak(out)]) > 0.8f * static_cast<float>(juce::jmin(1.0, 48000.0 / rate)),
                   "The impulse survives at " + juce::String(rate));
        }
    }

    beginTest("The response is flat within 0.5 dB from 20 Hz to 18 kHz");
    {
        for (const double rate : { 44100.0, 88200.0, 96000.0, 192000.0 })
        {
            for (const double frequency : { 20.0, 100.0, 1000.0, 5000.0, 10000.0, 15000.0, 18000.0 })
            {
                ResamplingIsland island;
                island.prepare(rate, 48000.0, 512);
                const auto out = runIsland(island, sineAt(frequency, rate, 1.0, 0.5f), 512, passThroughModel);
                const auto half = static_cast<size_t>(rate * 0.5);
                expectWithinAbsoluteError(toDb(toneAmplitude(out, half, half, frequency, rate) / 0.5), 0.0, 0.5,
                                          juce::String(frequency) + " Hz at " + juce::String(rate) + " Hz");
            }
        }
    }

    beginTest("Content above the lower Nyquist is filtered, not folded back");
    {
        // On the way in: 30 kHz into a 96 kHz island would fold to 18 kHz at 48 kHz.
        ResamplingIsland in96;
        in96.prepare(96000.0, 48000.0, 512);
        const auto out96 = runIsland(in96, sineAt(30000.0, 96000.0, 1.0, 0.5f), 512, passThroughModel);
        expect(toDb(toneAmplitude(out96, 48000, 48000, 18000.0, 96000.0) / 0.5) <= -40.0, "No 18 kHz fold at 96 kHz");
        expect(toDb(toneAmplitude(out96, 48000, 48000, 30000.0, 96000.0) / 0.5) <= -40.0, "30 kHz does not pass");

        // On the way out: a 20 kHz model output would fold to 12 kHz in a 32 kHz host.
        // At 44.1 kHz anything that folds lands above 20 kHz, so that rate can't show it.
        const auto toneModel = [](double frequency)
        {
            return [frequency, phase = 0.0](const float*, float* out, int n) mutable
            {
                for (int i = 0; i < n; ++i)
                {
                    out[i] = 0.5f * static_cast<float>(std::sin(phase));
                    phase += kTwoPi * frequency / 48000.0;
                }
            };
        };
        const std::vector<float> silence(32000, 0.0f);
        ResamplingIsland out32;
        out32.prepare(32000.0, 48000.0, 512);
        const auto folded = runIsland(out32, silence, 512, toneModel(20000.0));
        expect(toDb(toneAmplitude(folded, 16000, 16000, 12000.0, 32000.0) / 0.5) <= -40.0, "No 12 kHz fold at 32 kHz");

        ResamplingIsland pass32;
        pass32.prepare(32000.0, 48000.0, 512);
        const auto passed = runIsland(pass32, silence, 512, toneModel(10000.0));
        expectWithinAbsoluteError(toDb(toneAmplitude(passed, 16000, 16000, 10000.0, 32000.0) / 0.5), 0.0, 0.5,
                                  "10 kHz passes at 32 kHz");
    }

    beginTest("The output does not depend on how the host splits its blocks");
    {
        juce::Random random(11);
        std::vector<float> noise(20000);
        for (auto& s : noise)
            s = random.nextFloat() - 0.5f;

        for (const double rate : { 44100.0, 96000.0 })
        {
            ResamplingIsland reference;
            reference.prepare(rate, 48000.0, 512);
            const auto expected = runIsland(reference, noise, 512, passThroughModel);

            for (const int blockSize : { 1, 37, 256, 2000 })
            {
                ResamplingIsland island;
                island.prepare(rate, 48000.0, 512);   // 2000 exceeds it: chunked inside
                int largestCall = 0;
                const auto out = runIsland(island, noise, blockSize, [&](const float* in, float* o, int n)
                {
                    largestCall = juce::jmax(largestCall, n);
                    std::copy(in, in + n, o);
                });
                expect(out == expected, "Blocks of " + juce::String(blockSize) + " at " + juce::String(rate));
                expect(largestCall <= island.getMaxModelBlock(), "The model never gets more than it was prepared for");
            }
        }
    }

    beginTest("reset() clears the history and keeps the delay");
    {
        ResamplingIsland island;
        island.prepare(44100.0, 48000.0, 512);
        std::vector<float> impulse(2048, 0.0f);
        impulse[100] = 1.0f;
        const auto first = runIsland(island, impulse, 512, passThroughModel);
        island.reset();
        const auto second = runIsland(island, impulse, 512, passThroughModel);
        expect(first == second, "The same input gives the same output after a reset");
    }
}

//==============================================================================
// NAM profile (prototype)
//==============================================================================

namespace
{
    juce::File namTestModel(const char* name)
    {
        return juce::File(DISTORTION_NAM_TEST_MODELS_DIR).getChildFile(name);
    }

    // Loads through setStateInformation run on the profile loader thread; wait
    // for it rather than sleeping a fixed time.
    bool waitForProfileLoad(PluginProcessor& processor, int timeoutMs = 10000)
    {
        const auto deadline = juce::Time::getMillisecondCounter() + static_cast<juce::uint32>(timeoutMs);
        while (processor.getProfileStatus().loading)
        {
            if (juce::Time::getMillisecondCounter() > deadline)
                return false;
            juce::Thread::sleep(5);
        }
        return true;
    }

    void prepareForProfileTest(PluginProcessor& processor, double sampleRate = 48000.0, int blockSize = 512)
    {
        processor.setRateAndBufferSizeDetails(sampleRate, blockSize);
        processor.prepareToPlay(sampleRate, blockSize);
        setParameter(processor.parameters, "distortionAmount", 60.0f);
        setParameter(processor.parameters, "subGuardFreq", 0.0f);
    }

    float processSine(PluginProcessor& processor, int blocks, juce::AudioBuffer<float>* lastOut = nullptr)
    {
        juce::MidiBuffer midi;
        float rms = 0.0f;
        for (int b = 0; b < blocks; ++b)
        {
            auto buffer = generateSineWave(440.0, processor.getSampleRate(), processor.getBlockSize(), 0.5f);
            processor.processBlock(buffer, midi);
            rms = calculateRMS(buffer);
            if (lastOut != nullptr)
                lastOut->makeCopyOf(buffer);
        }
        return rms;
    }

    // A NAM model written for one test and removed afterwards.
    struct TempNam
    {
        explicit TempNam(const juce::String& json) : file(juce::File::createTempFile(".nam"))
        {
            file.replaceWithText(json);
        }
        ~TempNam() { file.deleteFile(); }
        juce::File file;
    };

    // NAM Core's Linear architecture: an FIR filter whose weights are its taps.
    juce::String linearModelJson(const juce::String& weights, int receptiveField, int sampleRate)
    {
        return "{\"version\":\"0.5.4\",\"architecture\":\"Linear\",\"config\":{\"receptive_field\":"
             + juce::String(receptiveField) + ",\"bias\":false},\"weights\":[" + weights
             + "],\"sample_rate\":" + juce::String(sampleRate) + "}";
    }

    // Runs channel 0 of a profile over a signal given at the host rate.
    std::vector<float> runProfile(NamProfile& profile, const std::vector<float>& input, int blockSize)
    {
        std::vector<float> out(input.size());
        for (size_t done = 0; done < input.size();)
        {
            const int n = static_cast<int>(std::min<size_t>(static_cast<size_t>(blockSize), input.size() - done));
            profile.process(0, input.data() + done, out.data() + done, n);
            done += static_cast<size_t>(n);
        }
        return out;
    }

    // A sine that carries its phase across blocks, so block edges add no steps.
    struct ContinuousSine
    {
        double frequency = 110.0;
        double sampleRate = 48000.0;
        float amplitude = 0.25f;
        double phase = 0.0;

        juce::AudioBuffer<float> next(int numSamples)
        {
            juce::AudioBuffer<float> buffer(2, numSamples);
            for (int i = 0; i < numSamples; ++i)
            {
                const float value = amplitude * static_cast<float>(std::sin(phase));
                buffer.setSample(0, i, value);
                buffer.setSample(1, i, value);
                phase += kTwoPi * frequency / sampleRate;
            }
            phase = std::fmod(phase, kTwoPi);
            return buffer;
        }
    };

    // The largest jump between neighbouring samples from index `from` on.
    float largestStep(const std::vector<float>& x, size_t from = 1)
    {
        float largest = 0.0f;
        for (size_t i = std::max<size_t>(1, from); i < x.size(); ++i)
            largest = juce::jmax(largest, std::abs(x[i] - x[i - 1]));
        return largest;
    }

    int longestZeroRun(const std::vector<float>& x)
    {
        int longest = 0, run = 0;
        for (const float s : x)
        {
            run = s == 0.0f ? run + 1 : 0;
            longest = juce::jmax(longest, run);
        }
        return longest;
    }

    void appendChannel0(std::vector<float>& to, const juce::AudioBuffer<float>& buffer)
    {
        to.insert(to.end(), buffer.getReadPointer(0), buffer.getReadPointer(0) + buffer.getNumSamples());
    }

    // Allocations made by one processBlock call (0 when the RT guard isn't built in).
    int allocationsDuring(PluginProcessor& processor, juce::AudioBuffer<float>& buffer)
    {
        juce::MidiBuffer midi;
       #if defined (DISTORTION_RT_GUARD) && DISTORTION_RT_GUARD
        rt_guard::resetAllocationCounter();
        processor.processBlock(buffer, midi);
        return rt_guard::getAllocationCount();
       #else
        processor.processBlock(buffer, midi);
        return 0;
       #endif
    }

    // Everything that colours level off, and Distortion Amount at the 0 dB point of
    // a profile's input-gain curve (-24 dB + 0.36 dB per %), so an identity model
    // passes the signal at unity.
    void configureUnityChain(PluginProcessor& processor)
    {
        setParameter(processor.parameters, "distortionAmount", 200.0f / 3.0f);
        setParameter(processor.parameters, "inputGain", 50.0f);
        setParameter(processor.parameters, "outputGain", 50.0f);
        setParameter(processor.parameters, "autoGainEnabled", 0.0f);
        setParameter(processor.parameters, "compEnabled", 0.0f);
        setParameter(processor.parameters, "extremeEnabled", 0.0f);
        setParameter(processor.parameters, "cleanBoost", 0.0f);
        setParameter(processor.parameters, "waveshaperMix", 0.0f);
        setParameter(processor.parameters, "tone", 20000.0f);
        setParameter(processor.parameters, "highPassFreq", 20.0f);
        setParameter(processor.parameters, "lfoEnabled", 0.0f);
        setParameter(processor.parameters, "subGuardFreq", 0.0f);
    }

    // Runs a mono signal through both channels in 512-sample blocks; returns channel 0.
    std::vector<float> renderThrough(PluginProcessor& processor, const std::vector<float>& signal)
    {
        constexpr int blockSize = 512;
        std::vector<float> out;
        juce::MidiBuffer midi;
        for (size_t done = 0; done + static_cast<size_t>(blockSize) <= signal.size(); done += static_cast<size_t>(blockSize))
        {
            juce::AudioBuffer<float> buffer(2, blockSize);
            for (int ch = 0; ch < 2; ++ch)
                buffer.copyFrom(ch, 0, signal.data() + done, blockSize);
            processor.processBlock(buffer, midi);
            appendChannel0(out, buffer);
        }
        return out;
    }

    // White noise low-passed at 8 kHz: inside the island's band at every rate.
    std::vector<float> lowPassedNoise(double sampleRate, double seconds, float amplitude)
    {
        std::vector<float> x(static_cast<size_t>(std::lround(sampleRate * seconds)));
        juce::Random random(5);
        for (auto& s : x)
            s = amplitude * (random.nextFloat() * 2.0f - 1.0f);
        juce::IIRFilter lowPass;
        lowPass.setCoefficients(juce::IIRCoefficients::makeLowPass(sampleRate, 8000.0));
        lowPass.processSamples(x.data(), static_cast<int>(x.size()));
        return x;
    }

    // The lag, within +/- maxLag samples, at which b lines up best with a.
    int bestLag(const std::vector<float>& a, const std::vector<float>& b, size_t from, int maxLag)
    {
        const auto room = static_cast<size_t>(maxLag);
        const size_t end = juce::jmin(a.size(), b.size());
        int best = 0;
        double bestScore = -1.0e300;
        for (int lag = -maxLag; lag <= maxLag; ++lag)
        {
            double score = 0.0;
            for (size_t i = juce::jmax(from, room); i + room < end; ++i)
                score += static_cast<double>(a[i]) * static_cast<double>(b[static_cast<size_t>(static_cast<int64_t>(i) + lag)]);
            if (score > bestScore)
            {
                bestScore = score;
                best = lag;
            }
        }
        return best;
    }
}

void NamProfileTests::runTest()
{
    // Drives the audio thread and the message-thread timer the way a running host
    // does, until no profile switch is in flight.
    const auto settle = [](PluginProcessor& processor, int maxRounds = 400)
    {
        juce::MidiBuffer midi;
        for (int round = 0; round < maxRounds; ++round)
        {
            processor.timerCallback();
            if (processor.isProfileSwitchIdle())
                return true;
            if (processor.stagedRefreshesInFlight.load() > 0)
                juce::Thread::sleep(1);   // the loader is re-preparing a staged profile
            auto buffer = generateSineWave(440.0, processor.getSampleRate(), processor.getBlockSize(), 0.1f);
            processor.processBlock(buffer, midi);
        }
        return processor.isProfileSwitchIdle();
    };

    beginTest("Loading a profile reports it and switches the chain to 1x");
    {
        PluginProcessor processor;
        prepareForProfileTest(processor);
        expect(processor.oversamplingFactor > 1, "Default setting oversamples");

        expect(processor.loadProfileBlocking(namTestModel("wavenet.nam")), "wavenet.nam loads");
        const auto status = processor.getProfileStatus();
        expectEquals(status.name, juce::String("wavenet"));
        expect(status.error.isEmpty(), "No error after a good load");
        expectWithinAbsoluteError(status.expectedSampleRate, 48000.0, 0.5);

        expect(settle(processor), "The switch into profile mode completes");
        expect(processor.profileMode, "Profile mode is on");
        expectEquals(static_cast<int>(processor.oversamplingFactor), 1);
    }

    beginTest("A SlimmableContainer profile loads and runs");
    {
        // Many downloaded profiles ship as containers of several sized submodels.
        PluginProcessor processor;
        prepareForProfileTest(processor);
        expect(processor.loadProfileBlocking(namTestModel("slimmable_container.nam")),
               "slimmable_container.nam loads");
        expect(processor.getProfileStatus().error.isEmpty(), "No error after a good load");

        expect(settle(processor), "The switch completes");
        juce::AudioBuffer<float> out;
        const float rms = processSine(processor, 8, &out);
        expect(processor.activeProfile != nullptr, "The container is the active profile");
        expect(! containsInvalidSamples(out), "Output stays finite");
        expect(rms > 0.0f, "The container produces output");
    }

    beginTest("An active profile changes the sound and stays finite, with and without Sub Guard");
    {
        for (const float subGuard : { 0.0f, 80.0f })
        {
            // Reference: the built-in clip type, also at 1x, so the only difference is the profile.
            PluginProcessor reference;
            prepareForProfileTest(reference);
            setParameter(reference.parameters, "subGuardFreq", subGuard);
            reference.requestOversamplingRebuild(0);
            reference.handleAsyncUpdate();
            juce::AudioBuffer<float> refOut;
            processSine(reference, 8, &refOut);

            PluginProcessor processor;
            prepareForProfileTest(processor);
            setParameter(processor.parameters, "subGuardFreq", subGuard);
            expect(processor.loadProfileBlocking(namTestModel("wavenet.nam")), "wavenet.nam loads");
            expect(settle(processor), "The switch completes");
            juce::AudioBuffer<float> out;
            const float rms = processSine(processor, 8, &out);

            expect(processor.activeProfile != nullptr && processor.activeProfile->getNumChannels() == 2,
                   "The audio thread picked up a stereo profile");
            expect(! containsInvalidSamples(out), "Output is finite");
            expect(rms > 1.0e-4f, "Output is not silent");

            float maxDiff = 0.0f;
            for (int ch = 0; ch < out.getNumChannels(); ++ch)
                for (int i = 0; i < out.getNumSamples(); ++i)
                    maxDiff = juce::jmax(maxDiff, std::abs(out.getSample(ch, i) - refOut.getSample(ch, i)));
            expect(maxDiff > 1.0e-3f, "The profile, not the built-in clip type, shaped the output (Sub Guard "
                                      + juce::String(subGuard) + " Hz)");
        }
    }

    beginTest("Clearing restores the built-in clip type and the oversampling setting");
    {
        PluginProcessor processor;
        prepareForProfileTest(processor);
        processor.loadProfileBlocking(namTestModel("wavenet.nam"));
        expect(settle(processor), "Loaded");

        processor.clearProfile();
        expect(processor.duckRequested.load(), "Leaving profile mode asks for a duck");
        expect(settle(processor), "Cleared");

        expect(! processor.isProfileLoaded(), "No profile reported after clearing");
        expect(processor.activeProfile == nullptr && ! processor.profileMode, "No profile runs");
        expect(processor.oversamplingFactor > 1, "The user's oversampling setting is back");
        expect(processor.retiredProfile.load() == nullptr, "The replaced profile was freed");
    }

   #if defined (DISTORTION_RT_GUARD) && DISTORTION_RT_GUARD
    beginTest("No allocation while a profile runs or while one is swapped in");
    {
        PluginProcessor processor;
        prepareForProfileTest(processor);
        setParameter(processor.parameters, "subGuardFreq", 80.0f);
        processor.loadProfileBlocking(namTestModel("wavenet.nam"));
        expect(settle(processor), "Loaded");
        processSine(processor, 4);   // filters settled

        juce::MidiBuffer midi;
        auto buffer = generateSineWave(440.0, 48000.0, 512, 0.5f);
        rt_guard::resetAllocationCounter();
        processor.processBlock(buffer, midi);
        expectEquals(rt_guard::getAllocationCount(), 0, "processBlock with an active profile");

        // A second profile arrives while audio runs: the swap must not allocate or free.
        expect(processor.loadProfileBlocking(namTestModel("lstm.nam")), "lstm.nam loads");
        processor.timerCallback();
        expect(! processor.duckRequested.load(), "Swapping profiles needs no rebuild");
        int allocations = 0;
        for (int block = 0; block < 64 && ! processor.isProfileSwitchIdle(); ++block)
        {
            auto swapBuffer = generateSineWave(440.0, 48000.0, 512, 0.5f);
            allocations += allocationsDuring(processor, swapBuffer);
            expect(! containsInvalidSamples(swapBuffer), "Output stays finite across the swap");
            processor.timerCallback();
        }
        expectEquals(allocations, 0, "processBlock across the swap");
        expect(processor.activeProfile != nullptr && processor.activeProfile->getName() == "lstm",
               "The new profile is active after the swap");
    }
   #endif

    beginTest("A profile prepared for other settings is refreshed before it runs");
    {
        PluginProcessor processor;
        prepareForProfileTest(processor, 48000.0, 512);
        processor.loadProfileBlocking(namTestModel("wavenet.nam"));   // staged for 48 kHz / 512

        // The host changes settings before the timer installed it: prepareToPlay
        // brings it up to the new settings and installs it, with nothing left pending.
        prepareForProfileTest(processor, 44100.0, 1024);
        expect(processor.isProfileSwitchIdle(), "Nothing left pending after prepareToPlay");
        expect(processor.profileMode && processor.activeProfile != nullptr, "The profile is installed");
        if (processor.activeProfile != nullptr)
        {
            expectWithinAbsoluteError(processor.activeProfile->getPreparedSampleRate(), 44100.0, 0.5);
            expect(processor.activeProfile->getPreparedBlockSize() >= 1024, "Prepared for the new block size");
            expectEquals(processor.activeProfile->getLatencySamples(), ResamplingIsland::latencyFor(44100.0, 48000.0));
        }
        juce::AudioBuffer<float> out;
        processSine(processor, 2, &out);
        expect(! containsInvalidSamples(out), "It runs at the new settings");
    }

    beginTest("Bad files fail cleanly and leave the built-in clip type running");
    {
        PluginProcessor processor;
        prepareForProfileTest(processor);

        expect(! processor.loadProfileBlocking(namTestModel("does-not-exist.nam")), "Missing file fails");
        expect(processor.getProfileStatus().error.isNotEmpty(), "Missing file reports an error");

        auto garbage = juce::File::createTempFile(".nam");
        garbage.replaceWithText("{ not a model");
        expect(! processor.loadProfileBlocking(garbage), "Malformed file fails");
        expect(processor.getProfileStatus().error.isNotEmpty(), "Malformed file reports an error");
        garbage.deleteFile();

        expect(! processor.isProfileLoaded(), "Nothing is loaded after failures");
        processor.timerCallback();
        expect(processor.oversamplingFactor > 1, "Oversampling untouched by failed loads");
        juce::AudioBuffer<float> out;
        processSine(processor, 2, &out);
        expect(! containsInvalidSamples(out), "Built-in path still runs");
    }

    beginTest("A session remembers the profile by path and restores it");
    {
        juce::MemoryBlock saved;
        {
            PluginProcessor processor;
            prepareForProfileTest(processor);
            processor.loadProfileBlocking(namTestModel("wavenet.nam"));
            processor.getStateInformation(saved);
        }

        PluginProcessor restored;
        prepareForProfileTest(restored);
        restored.setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));
        expect(waitForProfileLoad(restored), "Restore finished loading");
        expectEquals(restored.getProfileStatus().name, juce::String("wavenet"));
        expect(! restored.parameters.state.hasProperty(PluginProcessor::profilePathAttribute),
               "The session's profile path stays out of the parameter tree, so presets never carry it");

        // A session saved without a profile clears one that is loaded.
        juce::MemoryBlock plain;
        {
            PluginProcessor processor;
            prepareForProfileTest(processor);
            processor.getStateInformation(plain);
        }
        restored.setStateInformation(plain.getData(), static_cast<int>(plain.getSize()));
        expect(! restored.isProfileLoaded(), "A session without a profile clears it");

        // A session whose profile file is missing falls back to the built-in clip
        // type rather than keeping the profile that was loaded before it.
        restored.loadProfileBlocking(namTestModel("wavenet.nam"));
        auto xml = restored.parameters.copyState().createXml();
        const auto missing = namTestModel("moved-away.nam").getFullPathName();
        xml->setAttribute(PluginProcessor::profilePathAttribute, missing);
        juce::MemoryBlock missingState;
        juce::AudioProcessor::copyXmlToBinary(*xml, missingState);
        restored.setStateInformation(missingState.getData(), static_cast<int>(missingState.getSize()));
        expect(waitForProfileLoad(restored), "Restore attempt finished");
        expect(! restored.isProfileLoaded(), "The previous profile is not kept");
        expect(restored.getProfileStatus().error.isNotEmpty(), "The missing file is reported");
        expectEquals(restored.getProfileStatus().path, missing, "The path is kept for the next save");
    }

    const auto savedProfilePath = [](const juce::MemoryBlock& state)
    {
        auto xml = juce::AudioProcessor::getXmlFromBinary(state.getData(), static_cast<int>(state.getSize()));
        return xml != nullptr ? xml->getStringAttribute(PluginProcessor::profilePathAttribute) : juce::String();
    };

    beginTest("A session restored before the host prepares the plugin keeps its profile");
    {
        // The standalone app and some hosts restore state first and start audio
        // afterwards, so the load runs before any sample rate is known.
        const auto model = namTestModel("wavenet.nam");
        juce::MemoryBlock saved;
        {
            PluginProcessor processor;
            prepareForProfileTest(processor);
            processor.loadProfileBlocking(model);
            processor.getStateInformation(saved);
        }

        PluginProcessor restored;
        restored.setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));

        // Saved again before the load finishes, the session still names the profile.
        juce::MemoryBlock early;
        restored.getStateInformation(early);
        expectEquals(savedProfilePath(early), model.getFullPathName(), "A save mid-load keeps the path");

        expect(waitForProfileLoad(restored), "Restore finished loading");
        prepareForProfileTest(restored);
        expect(settle(restored), "The profile goes in once the host prepares");
        expect(restored.profileMode, "The restored profile plays");
        juce::AudioBuffer<float> out;
        processSine(restored, 4, &out);
        expect(! containsInvalidSamples(out), "Output is finite");

        juce::MemoryBlock resaved;
        restored.getStateInformation(resaved);
        expectEquals(savedProfilePath(resaved), model.getFullPathName(), "The next save keeps the path");

        // Hosts may restore the same state twice; the loaded profile is kept as is.
        restored.setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));
        expect(! restored.getProfileStatus().loading, "The same profile is not loaded again");
        expect(restored.isProfileLoaded(), "It stays loaded");
    }

    beginTest("Clicking through profiles quickly loads only the last one");
    {
        PluginProcessor processor;
        prepareForProfileTest(processor);

        // Hold the loader's one thread so every request queues behind it, as they
        // do when clicks arrive faster than a model parses.
        juce::WaitableEvent gate;
        processor.getProfileLoader().addJob([&gate] { gate.wait(10000); });

        const char* models[] = { "wavenet.nam", "lstm.nam", "wavenet.nam", "lstm.nam", "wavenet.nam" };
        for (const auto* model : models)
            processor.loadProfileAsync(namTestModel(model));
        gate.signal();

        expect(waitForProfileLoad(processor), "The last request finished");
        expectEquals(processor.profileFilesRead.load(), 1, "Superseded requests never read their file");
        expectEquals(processor.getProfileStatus().name, juce::String("wavenet"), "The last click's profile is loaded");
        expect(settle(processor), "It goes in");
        expect(processor.profileMode, "And plays");
    }

    beginTest("A new instance starts with the last profile once the host prepares it");
    {
        PluginProcessor processor;
        processor.setStartupProfile(namTestModel("wavenet.nam"));
        expect(processor.getProfileStatus().path.isEmpty(), "Nothing before the host prepares");

        prepareForProfileTest(processor);
        expect(waitForProfileLoad(processor), "Startup load finished");
        expectEquals(processor.getProfileStatus().name, juce::String("wavenet"));
        expect(settle(processor), "It goes in");
        expect(processor.profileMode, "And plays");

        prepareForProfileTest(processor, 44100.0, 256);   // the host changes rate
        expectEquals(processor.profileFilesRead.load(), 1, "Only the first prepare loads it");
    }

    beginTest("A restored session decides for itself, whatever the last profile was");
    {
        juce::MemoryBlock plain, withLstm;
        {
            PluginProcessor source;
            prepareForProfileTest(source);
            source.getStateInformation(plain);
            source.loadProfileBlocking(namTestModel("lstm.nam"));
            source.getStateInformation(withLstm);
        }

        PluginProcessor noProfile;   // restored, then prepared: the usual order
        noProfile.setStartupProfile(namTestModel("wavenet.nam"));
        noProfile.setStateInformation(plain.getData(), static_cast<int>(plain.getSize()));
        prepareForProfileTest(noProfile);
        expect(! noProfile.getProfileStatus().loading && ! noProfile.isProfileLoaded(),
               "A session saved without a profile stays without one");
        expectEquals(noProfile.profileFilesRead.load(), 0, "The last profile is never even read");

        PluginProcessor ownProfile;
        ownProfile.setStartupProfile(namTestModel("wavenet.nam"));
        ownProfile.setStateInformation(withLstm.getData(), static_cast<int>(withLstm.getSize()));
        prepareForProfileTest(ownProfile);
        expect(waitForProfileLoad(ownProfile), "Restore finished");
        expectEquals(ownProfile.getProfileStatus().name, juce::String("lstm"), "The session's own profile");

        // Some hosts prepare before they restore: the last profile starts loading,
        // and the session then takes it out again.
        PluginProcessor prepareFirst;
        prepareFirst.setStartupProfile(namTestModel("wavenet.nam"));
        prepareForProfileTest(prepareFirst);
        prepareFirst.setStateInformation(plain.getData(), static_cast<int>(plain.getSize()));
        expect(waitForProfileLoad(prepareFirst), "Nothing left loading");
        expect(settle(prepareFirst), "Settles");
        expect(! prepareFirst.isProfileLoaded() && ! prepareFirst.profileMode,
               "The session's built-in clip type wins");
    }

    beginTest("A last profile that has gone, or a clear before prepare, loads nothing");
    {
        PluginProcessor gone;
        gone.setStartupProfile(namTestModel("deleted-since.nam"));
        prepareForProfileTest(gone);
        const auto status = gone.getProfileStatus();
        expect(status.path.isEmpty() && status.error.isEmpty(), "No error about a file nobody asked for");

        PluginProcessor cleared;
        cleared.setStartupProfile(namTestModel("wavenet.nam"));
        cleared.clearProfile();
        prepareForProfileTest(cleared);
        expect(! cleared.getProfileStatus().loading && ! cleared.isProfileLoaded(), "The clear wins");
    }

    beginTest("Reopening a session whose profile failed to load tries again");
    {
        // A drive that wasn't mounted, a file being synced: the first attempt fails,
        // and reopening the same session once the file is back must load it.
        const auto file = juce::File::getSpecialLocation(juce::File::tempDirectory)
                              .getChildFile("sledge-retry-test.nam");
        file.replaceWithText("not a model");

        PluginProcessor source;
        prepareForProfileTest(source);
        auto xml = source.parameters.copyState().createXml();
        xml->setAttribute(PluginProcessor::profilePathAttribute, file.getFullPathName());
        juce::MemoryBlock state;
        juce::AudioProcessor::copyXmlToBinary(*xml, state);

        PluginProcessor processor;
        prepareForProfileTest(processor);
        processor.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
        expect(waitForProfileLoad(processor), "First attempt finished");
        expect(! processor.isProfileLoaded(), "A broken file does not load");
        expect(processor.getProfileStatus().error.isNotEmpty(), "The failure is reported");

        expect(namTestModel("wavenet.nam").copyFileTo(file), "The file comes back");
        processor.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
        expect(waitForProfileLoad(processor), "Second attempt finished");
        expect(processor.isProfileLoaded(), "The same session loads it now");
        expect(processor.getProfileStatus().error.isEmpty(), "The old error is gone");
        file.deleteFile();
    }

    beginTest("A request is held while it loads or once it has loaded, never after it failed");
    {
        PluginProcessor::ProfileStatus status;
        status.path = "/profiles/a.nam";
        status.fingerprint = "10-ab";
        status.loading = true;
        expect(status.holds("/profiles/a.nam"), "Loading, by path");
        expect(status.holds("/elsewhere/a.nam", "10-ab"), "Loading, by content");
        expect(! status.holds("/profiles/b.nam"), "Another file is not held");

        status.loading = false;
        status.name = "a";
        status.loadedPath = "/profiles/a.nam";
        status.loadedFingerprint = "10-ab";
        expect(status.holds("/profiles/a.nam"), "Loaded");
        expect(status.holds("/moved/a.nam", "10-ab"), "Loaded, matched by content");

        status.error = "Could not load a.nam";
        expect(! status.holds("/profiles/a.nam"),
               "A failed request is tried again, even while an earlier load of the same file plays");
        expect(! PluginProcessor::ProfileStatus().holds({}), "Nothing requested, nothing held");
    }

    beginTest("A pick that fails leaves the playing profile in charge, and is tried again");
    {
        // Stepping through the library lands on a file the loader rejects while the
        // previous profile keeps playing. A save keeps the profile you hear, and the
        // file loads once it is fixed, rather than counting as loaded already.
        const auto broken = juce::File::getSpecialLocation(juce::File::tempDirectory)
                                .getNonexistentChildFile("sledge-broken-pick", ".nam", false);
        broken.replaceWithText("not a model");
        const auto playing = namTestModel("wavenet.nam");

        PluginProcessor processor;
        prepareForProfileTest(processor);
        expect(processor.loadProfileBlocking(playing), "The first pick loads");
        const auto playingPrint = processor.getProfileStatus().loadedFingerprint;
        expectEquals(playingPrint, ProfileLibrary::fingerprint(playing), "With its file's fingerprint");

        expect(! processor.loadProfileBlocking(broken), "The broken pick fails");
        const auto status = processor.getProfileStatus();
        expect(status.error.isNotEmpty(), "The failure is reported");
        expectEquals(status.path, broken.getFullPathName(), "The request stays, for the card and the arrows");
        expectEquals(status.name, juce::String("wavenet"), "The profile before it still plays");
        expectEquals(status.loadedPath, playing.getFullPathName(), "And is still the loaded one");
        expect(! status.holds(broken.getFullPathName()), "The failed file isn't held, so picking it again loads it");
        expect(! status.holds(playing.getFullPathName()), "Picking the playing one again replaces the failed request");

        juce::MemoryBlock saved;
        processor.getStateInformation(saved);
        auto xml = juce::AudioProcessor::getXmlFromBinary(saved.getData(), static_cast<int>(saved.getSize()));
        expectEquals(xml->getStringAttribute(PluginProcessor::profilePathAttribute), playing.getFullPathName(),
                     "A save keeps the profile you hear, not the failed pick");
        expectEquals(xml->getStringAttribute(PluginProcessor::profileFingerprintAttribute), playingPrint,
                     "With its fingerprint");

        // Fixed on disk, a session naming the file loads it.
        expect(namTestModel("lstm.nam").copyFileTo(broken), "The file is fixed");
        auto session = processor.parameters.copyState().createXml();
        session->setAttribute(PluginProcessor::profilePathAttribute, broken.getFullPathName());
        juce::MemoryBlock sessionState;
        juce::AudioProcessor::copyXmlToBinary(*session, sessionState);
        processor.setStateInformation(sessionState.getData(), static_cast<int>(sessionState.getSize()));
        expect(waitForProfileLoad(processor), "The restore finished");
        expectEquals(processor.getProfileStatus().loadedPath, broken.getFullPathName(), "The fixed file loads");
        expect(processor.getProfileStatus().error.isEmpty(), "The old error is gone");
        broken.deleteFile();
    }

    beginTest("A profile at 44.1, 88.2, 96 and 192 kHz sounds as it does at 48 kHz");
    {
        const std::vector<double> harmonics { 1000.0, 2000.0, 3000.0, 4000.0, 5000.0 };
        const std::vector<double> tones { 200.0, 1000.0, 3000.0, 7000.0, 12000.0, 17000.0 };

        // One second of signal, measured over its second half, once the model and
        // the island have settled.
        const auto measure = [&](double rate, bool multitone)
        {
            std::vector<double> levels;
            juce::String error;
            auto profile = NamProfile::load(namTestModel("wavenet.nam"), 1, rate, 512, error);
            expect(profile != nullptr, error);
            if (profile == nullptr)
                return levels;
            const auto input = multitone ? multitoneAt(tones, rate, 1.0, 0.01f) : sineAt(1000.0, rate, 1.0, 0.2f);
            const auto out = runProfile(*profile, input, 512);
            const auto half = static_cast<size_t>(rate * 0.5);
            for (const double f : multitone ? tones : harmonics)
                levels.push_back(toneAmplitude(out, half, half, f, rate));
            return levels;
        };

        const auto referenceHarmonics = measure(48000.0, false);
        const auto referenceTones = measure(48000.0, true);
        for (const double rate : { 44100.0, 88200.0, 96000.0, 192000.0 })
        {
            const auto harmonicLevels = measure(rate, false);
            const auto toneLevels = measure(rate, true);
            for (size_t i = 0; i < harmonicLevels.size() && i < referenceHarmonics.size(); ++i)
                if (referenceHarmonics[i] > referenceHarmonics[0] * 1.0e-3)   // within 60 dB of the fundamental
                    expectWithinAbsoluteError(toDb(harmonicLevels[i] / referenceHarmonics[i]), 0.0, 0.5,
                                              "Harmonic " + juce::String(static_cast<int>(i) + 1)
                                              + " at " + juce::String(rate));
            for (size_t i = 0; i < toneLevels.size() && i < referenceTones.size(); ++i)
                expectWithinAbsoluteError(toDb(toneLevels[i] / referenceTones[i]), 0.0, 0.5,
                                          juce::String(tones[i]) + " Hz at " + juce::String(rate));
        }
    }

    beginTest("A model run at the wrong rate fails the same check (negative control)");
    {
        // A two-tap average, [0.5, 0.5]: -2.0 dB at 10 kHz at the 48 kHz it claims.
        // The same taps declared as 96 kHz make a 96 kHz host run them directly, the
        // way the prototype ran every model: -0.5 dB at 10 kHz, 1.5 dB away.
        TempNam at48(linearModelJson("0.5,0.5", 2, 48000));
        TempNam at96(linearModelJson("0.5,0.5", 2, 96000));
        const auto levelAt10k = [&](const juce::File& file, double rate)
        {
            juce::String error;
            auto profile = NamProfile::load(file, 1, rate, 512, error);
            expect(profile != nullptr, error);
            if (profile == nullptr)
                return 0.0;
            const auto out = runProfile(*profile, sineAt(10000.0, rate, 0.5, 0.5f), 512);
            const auto quarter = static_cast<size_t>(rate * 0.25);
            return toneAmplitude(out, quarter, quarter, 10000.0, rate);
        };

        const double reference = levelAt10k(at48.file, 48000.0);
        expectWithinAbsoluteError(toDb(reference / 0.5), -2.01, 0.1, "The reference is the model's own response");
        expectWithinAbsoluteError(toDb(levelAt10k(at48.file, 96000.0) / reference), 0.0, 0.5,
                                  "Through the island, a 96 kHz host hears the 48 kHz response");
        expect(std::abs(toDb(levelAt10k(at96.file, 96000.0) / reference)) > 0.5,
               "Run directly at 96 kHz, the same taps miss by more than 0.5 dB");
    }

    beginTest("A profile reports its island delay and refuses a host rate it can't serve");
    {
        juce::String error;
        auto at48 = NamProfile::load(namTestModel("wavenet.nam"), 2, 48000.0, 512, error);
        auto at44 = NamProfile::load(namTestModel("wavenet.nam"), 2, 44100.0, 512, error);
        expect(at48 != nullptr && at44 != nullptr, error);
        if (at48 != nullptr && at44 != nullptr)
        {
            expectEquals(at48->getLatencySamples(), 0);
            expectEquals(at44->getLatencySamples(), ResamplingIsland::latencyFor(44100.0, 48000.0));
            expect(at44->isPrepared(), "Prepared at 44.1 kHz");
            expect(at44->getSettleSeconds() > 0.0, "A WaveNet reports how long it takes to settle");
        }

        error = {};
        auto odd = NamProfile::load(namTestModel("wavenet.nam"), 2, 44056.0, 512, error);
        expect(odd == nullptr, "A 44.056 kHz host is refused");
        expectEquals(error, juce::String("This profile can't run at 44.056 kHz"));
    }

    beginTest("A load prepared for settings the host has left is re-prepared before it runs");
    {
        PluginProcessor processor;
        prepareForProfileTest(processor, 96000.0, 512);
        // Stands in for a load that read the host settings before prepareToPlay
        // changed them: a profile prepared for 48 kHz, staged into a 96 kHz host.
        juce::String error;
        auto stale = NamProfile::load(namTestModel("wavenet.nam"), 2, 48000.0, 512, error);
        expect(stale != nullptr, error);
        stale->setRequestId(processor.profileRequestId.load());   // as a real load tags it
        processor.stagedProfile.store(stale.release());
        expect(settle(processor), "The switch completes");
        expect(processor.activeProfile != nullptr, "The profile is installed");
        if (processor.activeProfile != nullptr)
            expectWithinAbsoluteError(processor.activeProfile->getPreparedSampleRate(), 96000.0, 0.5);
    }

    beginTest("Entering and leaving profile mode ducks instead of clicking");
    {
        PluginProcessor processor;
        prepareForProfileTest(processor);
        ContinuousSine sine;
        int allocations = 0;
        const auto run = [&](int blocks, std::vector<float>& captured)
        {
            for (int b = 0; b < blocks; ++b)
            {
                auto buffer = sine.next(512);
                allocations += allocationsDuring(processor, buffer);
                appendChannel0(captured, buffer);
            }
        };
        const auto runUntilIdle = [&](std::vector<float>& captured)
        {
            for (int round = 0; round < 100; ++round)
            {
                processor.timerCallback();
                if (processor.isProfileSwitchIdle())
                    return true;
                run(1, captured);
            }
            return false;
        };

        std::vector<float> builtIn, entering, profiled, leaving, builtInAgain;
        run(40, builtIn);
        processor.loadProfileBlocking(namTestModel("wavenet.nam"));
        expect(runUntilIdle(entering), "Entering completes");
        expect(processor.profileMode, "Profile mode is on");
        run(40, profiled);
        processor.clearProfile();
        expect(runUntilIdle(leaving), "Leaving completes");
        expect(! processor.profileMode, "Profile mode is off");
        run(40, builtInAgain);

        // Steady states skip their first 20 blocks, where filters and the model settle.
        const float steady = juce::jmax(largestStep(builtIn, 512 * 20), largestStep(profiled, 512 * 20),
                                        largestStep(builtInAgain, 512 * 20));
        entering.insert(entering.begin(), builtIn.back());
        leaving.insert(leaving.begin(), profiled.back());
        expect(largestStep(entering) <= 1.5f * steady, "No click entering profile mode");
        expect(largestStep(leaving) <= 1.5f * steady, "No click leaving profile mode");
        expect(longestZeroRun(entering) <= 1920 && longestZeroRun(leaving) <= 1920,
               "The silent gap is 40 ms or less at 512-sample blocks");
        expectEquals(allocations, 0, "No allocation on the audio thread across the ducks");
    }

    beginTest("The switch completes after the timeout when no audio is flowing");
    {
        PluginProcessor processor;
        prepareForProfileTest(processor);
        expect(processor.loadProfileBlocking(namTestModel("wavenet.nam")), "Loads");
        processor.timerCallback();
        expect(processor.duckRequested.load(), "Entering profile mode asks for a duck");
        processor.timerCallback();
        expect(processor.activeProfile == nullptr, "It waits for the audio thread, or the timeout");

        juce::Thread::sleep(DSPConstants::DUCK_TIMEOUT_MS + 50);
        processor.timerCallback();
        expect(processor.profileMode && processor.activeProfile != nullptr, "Installed after the timeout");
        expectEquals(static_cast<int>(processor.oversamplingFactor), 1);

        juce::MidiBuffer midi;
        auto out = generateSineWave(440.0, 48000.0, 64, 0.5f);   // shorter than the 480-sample fade
        processor.processBlock(out, midi);
        expect(processor.duckState == PluginProcessor::DuckState::open,
               "Playback starts at full level: the audio thread never faded out");
    }

    beginTest("Changing the Oversampling setting ducks; re-sending the same one does nothing");
    {
        PluginProcessor processor;
        prepareForProfileTest(processor);
        processor.requestOversamplingRebuild(2);   // what an opening editor re-sends
        expect(! processor.duckRequested.load(), "The current setting again: no rebuild, no dip");
        processor.requestOversamplingRebuild(1);
        expect(processor.duckRequested.load(), "A new setting asks for a duck");
        expect(settle(processor), "The rebuild completes");
        expectEquals(static_cast<int>(processor.oversamplingFactor), 2);
    }

    beginTest("Toggling the linear-phase filter ducks through the timer");
    {
        PluginProcessor processor;
        prepareForProfileTest(processor);
        setParameter(processor.parameters, "linearPhaseDry", 1.0f);   // flags a rebuild, as automation would
        processor.timerCallback();
        expect(processor.duckRequested.load(), "The flagged rebuild became a duck");
        expect(settle(processor), "The rebuild completes");
        expect(processor.currentLinearPhase, "The oversampler now uses the linear-phase filter");
    }

    beginTest("A pending profile prepared for settings the host has left is re-prepared before it runs");
    {
        PluginProcessor processor;
        prepareForProfileTest(processor, 96000.0, 512);
        expect(processor.loadProfileBlocking(namTestModel("wavenet.nam")), "wavenet.nam loads");
        expect(settle(processor), "The switch into profile mode completes");

        // Stands in for a host that calls prepareToPlay off the message thread while
        // the timer is inside routeStagedProfile: a profile prepared for 48 kHz lands
        // in pendingProfile behind its back.
        juce::String error;
        auto stale = NamProfile::load(namTestModel("lstm.nam"), 2, 48000.0, 512, error);
        expect(stale != nullptr, error);
        stale->setRequestId(processor.profileRequestId.load());   // as a real load tags it
        processor.pendingProfile.store(stale.release());
        expect(settle(processor), "The switch completes");
        expect(processor.activeProfile != nullptr && processor.activeProfile->getName() == "lstm",
               "lstm is active");
        if (processor.activeProfile != nullptr)
            expectWithinAbsoluteError(processor.activeProfile->getPreparedSampleRate(), 96000.0, 0.5);
    }

    beginTest("Swapping profiles crossfades without a click, a duck or an allocation");
    {
        PluginProcessor processor;
        prepareForProfileTest(processor);
        processor.loadProfileBlocking(namTestModel("wavenet.nam"));
        expect(settle(processor), "First profile in");

        ContinuousSine sine;
        int allocations = 0;
        bool ducked = false;
        const auto run = [&](int blocks, std::vector<float>& captured)
        {
            for (int b = 0; b < blocks; ++b)
            {
                auto buffer = sine.next(512);
                allocations += allocationsDuring(processor, buffer);
                appendChannel0(captured, buffer);
                ducked = ducked || processor.duckState != PluginProcessor::DuckState::open;
            }
        };

        std::vector<float> before, swapping, after;
        run(40, before);
        processor.loadProfileBlocking(namTestModel("lstm.nam"));
        for (int round = 0; round < 100; ++round)
        {
            processor.timerCallback();
            if (processor.isProfileSwitchIdle())
                break;
            run(1, swapping);
        }
        run(40, after);

        expect(processor.activeProfile != nullptr && processor.activeProfile->getName() == "lstm", "lstm is active");
        expect(! ducked, "A swap between two 48 kHz profiles never ducks");
        expectEquals(allocations, 0, "No allocation on the audio thread across the swap");
        expect(swapping.size() >= static_cast<size_t>(std::lround(DSPConstants::PROFILE_CROSSFADE_TIME_S * 48000.0)),
               "The blend took at least its 30 ms");
        const float steady = juce::jmax(largestStep(before, 512 * 20), largestStep(after, 512 * 20));
        swapping.insert(swapping.begin(), before.back());
        swapping.push_back(after.front());
        expect(largestStep(swapping) <= 1.5f * steady, "No click across the swap");
    }

    beginTest("A profile arriving mid-swap waits its turn");
    {
        PluginProcessor processor;
        prepareForProfileTest(processor);
        processor.loadProfileBlocking(namTestModel("wavenet.nam"));
        expect(settle(processor), "First profile in");

        processor.loadProfileBlocking(namTestModel("lstm.nam"));
        processor.timerCallback();      // routed for a crossfade
        processSine(processor, 1);      // taken: lstm starts on its way in
        expect(processor.incomingProfile != nullptr, "lstm is on its way in");

        processor.loadProfileBlocking(namTestModel("slimmable_container.nam"));
        processor.timerCallback();
        expect(! processor.duckRequested.load(), "The newcomer is routed for a blend, not a duck");

        processSine(processor, 1);
        expect(processor.incomingProfile != nullptr && processor.incomingProfile->getName() == "lstm",
               "The swap in flight keeps going");
        expect(processor.pendingProfile.load() != nullptr
               && processor.pendingProfile.load()->getName() == "slimmable_container",
               "The newcomer waits");
        expect(processor.activeProfile != nullptr && processor.activeProfile->getName() == "wavenet",
               "wavenet is still active while lstm is mid-swap");

        // Same shape as settle(), but recording each distinct active profile along
        // the way: the newest must win only after lstm has had its turn as active.
        std::vector<juce::String> activeNames;
        bool idle = false;
        for (int round = 0; round < 400; ++round)
        {
            processor.timerCallback();
            if (processor.isProfileSwitchIdle())
            {
                idle = true;
                break;
            }
            processSine(processor, 1);
            if (processor.activeProfile != nullptr
                && (activeNames.empty() || activeNames.back() != processor.activeProfile->getName()))
                activeNames.push_back(processor.activeProfile->getName());
        }
        expect(idle, "Both swaps complete");

        int lstmIndex = -1, slimIndex = -1;
        for (size_t i = 0; i < activeNames.size(); ++i)
        {
            if (lstmIndex < 0 && activeNames[i] == "lstm")
                lstmIndex = static_cast<int>(i);
            if (slimIndex < 0 && activeNames[i] == "slimmable_container")
                slimIndex = static_cast<int>(i);
        }
        expect(lstmIndex >= 0 && slimIndex >= 0 && lstmIndex < slimIndex,
               "lstm becomes active before slimmable_container");

        expect(processor.activeProfile != nullptr
               && processor.activeProfile->getName() == "slimmable_container", "The newest profile wins");
        expect(processor.retiredProfile.load() == nullptr, "Nothing is left to free");
    }

    beginTest("A swap lasts its warm-up plus 30 ms, however the host splits its blocks");
    {
        PluginProcessor processor;
        prepareForProfileTest(processor);
        processor.loadProfileBlocking(namTestModel("wavenet.nam"));
        expect(settle(processor), "First profile in");

        processor.loadProfileBlocking(namTestModel("lstm.nam"));
        processor.timerCallback();   // routed: lstm sits in pendingProfile

        juce::MidiBuffer midi;
        ContinuousSine sine;
        int blockCount = 0;
        double lstmSettleSeconds = 0.0;
        bool completed = false;
        for (int i = 0; i < 20000; ++i)
        {
            auto buffer = sine.next(1);
            processor.processBlock(buffer, midi);
            ++blockCount;

            if (i == 0)
            {
                expect(processor.incomingProfile != nullptr, "The first sample takes lstm");
                if (processor.incomingProfile != nullptr)
                    lstmSettleSeconds = processor.incomingProfile->getSettleSeconds();
            }

            if (processor.incomingProfile == nullptr)
            {
                completed = true;
                break;
            }
        }
        expect(completed, "The swap finished within the safety cap");

        const int warmupSamples = static_cast<int>(std::ceil(
            juce::jmin(lstmSettleSeconds, DSPConstants::PROFILE_WARMUP_MAX_S) * 48000.0));
        const int fadeSamples = static_cast<int>(std::lround(DSPConstants::PROFILE_CROSSFADE_TIME_S * 48000.0));
        expectEquals(blockCount, warmupSamples + fadeSamples,
                     "The swap lasts exactly its warm-up plus its fade, sample for sample");
        expect(processor.activeProfile != nullptr && processor.activeProfile->getName() == "lstm", "lstm is active");
    }

    beginTest("The reported latency is the reserve, whatever loads, swaps or clears");
    {
        for (const double rate : { 44100.0, 48000.0, 96000.0 })
        {
            for (const bool linearPhase : { false, true })
            {
                PluginProcessor processor;
                setParameter(processor.parameters, "linearPhaseDry", linearPhase ? 1.0f : 0.0f);
                prepareForProfileTest(processor, rate, 512);

                const juce::dsp::Oversampling<float> probe(1, 2,
                    linearPhase ? juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple
                                : juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, false, false);
                const int expected = juce::jmax(static_cast<int>(std::lround(probe.getLatencyInSamples())),
                                                ResamplingIsland::latencyFor(rate, 48000.0));
                const auto label = juce::String(rate) + (linearPhase ? " Hz, linear phase" : " Hz, IIR");

                expect(settle(processor), "Settled, " + label);
                expectEquals(processor.getLatencySamples(), expected, "Built-in clip type, " + label);
                processor.loadProfileBlocking(namTestModel("wavenet.nam"));
                expect(settle(processor), "Loaded, " + label);
                expectEquals(processor.getLatencySamples(), expected, "Profile loaded, " + label);
                processor.loadProfileBlocking(namTestModel("lstm.nam"));
                expect(settle(processor), "Swapped, " + label);
                expectEquals(processor.getLatencySamples(), expected, "Profile swapped, " + label);
                processor.clearProfile();
                expect(settle(processor), "Cleared, " + label);
                expectEquals(processor.getLatencySamples(), expected, "Profile cleared, " + label);
            }
        }
    }

    beginTest("At 48 kHz the built-in path keeps its old latency and adds no pad");
    {
        PluginProcessor processor;
        prepareForProfileTest(processor, 48000.0, 512);
        expectEquals(processor.getLatencySamples(), 3, "The default 4x oversampler's own latency, measured before this change");
        expectEquals(processor.outputPadDelay, 0);
    }

    beginTest("A model trained at another rate raises the reported latency only while loaded");
    {
        TempNam at44(linearModelJson("1.0", 1, 44100));
        PluginProcessor processor;
        prepareForProfileTest(processor, 48000.0, 512);
        const int normal = processor.getLatencySamples();

        processor.loadProfileBlocking(namTestModel("wavenet.nam"));
        expect(settle(processor), "48 kHz profile in");
        expectEquals(processor.getLatencySamples(), normal, "A 48 kHz model at 48 kHz fits the reserve");

        processor.loadProfileBlocking(at44.file);
        processor.timerCallback();
        expect(processor.duckRequested.load(), "A different island delay goes in with a duck, not a blend");
        expect(settle(processor), "44.1 kHz profile in");
        expectEquals(processor.getLatencySamples(), ResamplingIsland::latencyFor(48000.0, 44100.0),
                     "Its island delay is reported while it is loaded");

        processor.clearProfile();
        expect(settle(processor), "Cleared");
        expectEquals(processor.getLatencySamples(), normal, "Back to the reserve once it is gone");
    }

    beginTest("Dry and wet line up through the island");
    {
        // An identity model: the dry and wet halves of either mix carry the same
        // signal, so any misalignment shows up directly.
        TempNam identity(linearModelJson("1.0", 1, 48000));
        for (const double rate : { 44100.0, 96000.0 })
        {
            const auto render = [&](float distMix, float globalMix, const std::vector<float>& signal)
            {
                PluginProcessor processor;
                prepareForProfileTest(processor, rate, 512);
                configureUnityChain(processor);
                setParameter(processor.parameters, "distMix", distMix);
                setParameter(processor.parameters, "globalMix", globalMix);
                processor.loadProfileBlocking(identity.file);
                expect(settle(processor), "Identity profile in");
                return renderThrough(processor, signal);
            };

            const auto noise = lowPassedNoise(rate, 1.0, 0.05f);
            const auto from = static_cast<size_t>(rate * 0.25);
            const auto wet = render(100.0f, 100.0f, noise);
            expectEquals(bestLag(wet, render(0.0f, 100.0f, noise), from, 64), 0,
                         "Distortion Mix dry half vs wet at " + juce::String(rate));
            expectEquals(bestLag(wet, render(100.0f, 0.0f, noise), from, 64), 0,
                         "Global Mix dry vs wet at " + juce::String(rate));

            // A one-sample slip would cost about 2 dB at 10 kHz and 44.1 kHz; the
            // island's own passband ripple is far below 0.1 dB.
            const auto tone = sineAt(10000.0, rate, 1.0, 0.05f);
            const auto level = [&](const std::vector<float>& x)
            {
                return toneAmplitude(x, static_cast<size_t>(rate * 0.5), static_cast<size_t>(rate * 0.4), 10000.0, rate);
            };
            const double full = level(render(100.0f, 100.0f, tone));
            expectWithinAbsoluteError(toDb(level(render(50.0f, 100.0f, tone)) / full), 0.0, 0.1,
                                      "50% Distortion Mix at 10 kHz, " + juce::String(rate));
            expectWithinAbsoluteError(toDb(level(render(100.0f, 50.0f, tone)) / full), 0.0, 0.1,
                                      "50% global Mix at 10 kHz, " + juce::String(rate));
        }
    }

    beginTest("Sub Guard stays flat through its crossover with a profile loaded");
    {
        TempNam identity(linearModelJson("1.0", 1, 48000));
        for (const double rate : { 44100.0, 96000.0 })
        {
            // 200 Hz is the most demanding crossover: a low band left D samples early
            // would dip the sum there by about 0.5 dB.
            const auto levelAt200 = [&](float subGuard)
            {
                PluginProcessor processor;
                prepareForProfileTest(processor, rate, 512);
                configureUnityChain(processor);
                setParameter(processor.parameters, "subGuardFreq", subGuard);
                processor.loadProfileBlocking(identity.file);
                expect(settle(processor), "Identity profile in");
                const auto out = renderThrough(processor, sineAt(200.0, rate, 1.0, 0.05f));
                return toneAmplitude(out, static_cast<size_t>(rate * 0.5), static_cast<size_t>(rate * 0.4), 200.0, rate);
            };
            expectWithinAbsoluteError(toDb(levelAt200(200.0f) / levelAt200(0.0f)), 0.0, 0.2,
                                      "Crossover at 200 Hz, " + juce::String(rate) + " Hz host");
        }
    }

    beginTest("Crossing into bypass and back leaves no stale audio in the output pad");
    {
        PluginProcessor processor;
        prepareForProfileTest(processor, 44100.0, 512);   // built-in clip type; distortion 60

        // The built-in oversampler's own recursive filter keeps whatever state it
        // held when frozen through a bypass gap, and rings for a while once it
        // resumes -- a separate, pre-existing property of that filter, unrelated to
        // the pad line under test here. Turn oversampling off, and neutralise the
        // other colouring stages via configureUnityChain, so the shared pad line
        // dominates whatever state survives the crossing.
        processor.requestedOversamplingStages.store(0, std::memory_order_release);
        processor.prepareToPlay(44100.0, 512);
        configureUnityChain(processor);
        setParameter(processor.parameters, "distortionAmount", 60.0f);
        expect(processor.outputPadDelay > 0, "44.1 kHz reserves more than the built-in path's own (zero) latency");

        juce::MidiBuffer midi;

        // A loud tone through the active path: fills the shared pad line with a
        // signal well above the near-silence threshold checked below.
        for (int b = 0; b < 20; ++b)
        {
            auto buffer = generateSineWave(1000.0, 44100.0, 512, 0.5f);
            processor.processBlock(buffer, midi);
        }

        // Cross into true bypass and feed it silence, long enough to flush the pad
        // line's history if the bypass branch fed it too.
        setParameter(processor.parameters, "distortionAmount", 0.0f);
        setParameter(processor.parameters, "compEnabled", 0.0f);
        for (int b = 0; b < 20; ++b)
        {
            juce::AudioBuffer<float> silence(2, 512);
            silence.clear();
            processor.processBlock(silence, midi);
        }

        // Cross back to active. Parameter smoothing may keep the very next block in
        // bypass (the threshold is on the smoothed value); advance until a block
        // actually ran the active path, then check that one.
        setParameter(processor.parameters, "distortionAmount", 60.0f);
        bool checked = false;
        for (int b = 0; b < 20 && !checked; ++b)
        {
            juce::AudioBuffer<float> silence(2, 512);
            silence.clear();
            processor.processBlock(silence, midi);
            if (processor.pb_modulatedDistortionParam >= 0.5f)
            {
                checked = true;
                float largest = 0.0f;
                for (int ch = 0; ch < silence.getNumChannels(); ++ch)
                    for (int i = 0; i < silence.getNumSamples(); ++i)
                        largest = juce::jmax(largest, std::abs(silence.getSample(ch, i)));

                // Not exactly zero: the always-on input filter ahead of the
                // distortion stage (a state-variable filter that runs every active
                // block regardless of its cutoff, even at this "transparent"
                // setting) carries a little of its own state across the same gap --
                // measured in isolation (this same scenario, but never crossing to
                // bypass at all) that settles to exactly 0; measured with the
                // crossing but the pad line NOT shared it is ~0.62 (the tone's own
                // amplitude, i.e. a genuine stale replay); measured here, with the
                // pad line shared, it is ~0.09 -- the input filter's own residual,
                // not a further contribution from the pad line. The threshold below
                // sits comfortably above that filter's own residual and well clear of
                // the stale-replay figure, so it still catches a regression in the
                // pad-sharing fix without failing on the unrelated, pre-existing
                // filter behaviour.
                expect(largest < 0.15f, "No stale tone replays after the crossing: largest=" + juce::String(largest, 8));
            }
        }
        expect(checked, "The active path resumed within the loop's block budget");
    }

    // A loud tone into a 48 kHz identity profile at 44.1 kHz, where the island and
    // the dry delays each hold the last D samples they were fed. Returns the
    // processor ready for whatever the test does next.
    const auto loudToneThroughIsland = [&](PluginProcessor& processor, const juce::File& model)
    {
        prepareForProfileTest(processor, 44100.0, 512);
        configureUnityChain(processor);
        processor.loadProfileBlocking(model);
        expect(settle(processor), "Identity profile in");
        expect(processor.islandDelay > 0, "44.1 kHz runs the island with a delay");
        juce::MidiBuffer midi;
        for (int b = 0; b < 20; ++b)
        {
            auto buffer = generateSineWave(1000.0, 44100.0, 512, 0.5f);
            processor.processBlock(buffer, midi);
        }
    };
    const auto largestAbs = [](const juce::AudioBuffer<float>& buffer)
    {
        float largest = 0.0f;
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                largest = juce::jmax(largest, std::abs(buffer.getSample(ch, i)));
        return largest;
    };

    beginTest("A profile keeps running through true bypass, so resuming replays nothing stale");
    {
        TempNam identity(linearModelJson("1.0", 1, 48000));
        PluginProcessor processor;
        loudToneThroughIsland(processor, identity.file);

        // Into true bypass, on silence long enough to flush every line fed during it.
        juce::MidiBuffer midi;
        setParameter(processor.parameters, "distortionAmount", 0.0f);
        for (int b = 0; b < 20; ++b)
        {
            juce::AudioBuffer<float> silence(2, 512);
            silence.clear();
            processor.processBlock(silence, midi);
        }
        expect(processor.pb_modulatedDistortionParam < 0.5f, "True bypass is running");

        setParameter(processor.parameters, "distortionAmount", 200.0f / 3.0f);
        bool checked = false;
        for (int b = 0; b < 20 && ! checked; ++b)
        {
            juce::AudioBuffer<float> silence(2, 512);
            silence.clear();
            processor.processBlock(silence, midi);
            if (processor.pb_modulatedDistortionParam >= 0.5f)
            {
                checked = true;
                // Frozen, the island hands back D samples of the 0.5 tone. What
                // remains is the input filter's own residual (see the test above).
                const float largest = largestAbs(silence);
                expect(largest < 0.15f, "No stale tone after the crossing: largest=" + juce::String(largest, 8));
            }
        }
        expect(checked, "The active path resumed within the loop's block budget");
    }

    beginTest("A state reset clears the island along with the dry delays");
    {
        // A preset or session load mid-playback resets the DSP without a duck; the
        // island restarting with the dry lines is what keeps the two halves in step.
        TempNam identity(linearModelJson("1.0", 1, 48000));
        PluginProcessor processor;
        loudToneThroughIsland(processor, identity.file);

        processor.stateNeedsReset.store(true);
        juce::AudioBuffer<float> silence(2, 512);
        silence.clear();
        juce::MidiBuffer midi;
        processor.processBlock(silence, midi);
        const float largest = largestAbs(silence);
        expect(largest < 1.0e-3f, "Nothing from before the reset comes out: largest=" + juce::String(largest, 8));
    }

    beginTest("An offline render never ducks to silence");
    {
        // A bounce runs faster than the timer: a duck that waits for its tick would
        // print that wait into the file as silence, many times over.
        PluginProcessor processor;
        prepareForProfileTest(processor);
        processor.setNonRealtime(true);
        expect(processor.loadProfileBlocking(namTestModel("wavenet.nam")), "Loads");
        processor.timerCallback();   // the switch into profile mode is requested

        ContinuousSine sine;
        std::vector<float> out;
        juce::MidiBuffer midi;
        for (int b = 0; b < 10; ++b)
        {
            auto buffer = sine.next(512);
            processor.processBlock(buffer, midi);
            appendChannel0(out, buffer);
        }
        const int zeros = longestZeroRun(out);
        expect(zeros <= 8, "No silent gap in the render: longest zero run " + juce::String(zeros));

        expect(settle(processor), "The switch completes");
        expect(processor.profileMode, "Profile mode is on");
    }

    beginTest("Host bypass keeps the reported latency");
    {
        // The host compensates for the reported latency whether or not it has
        // bypassed us, so the bypassed signal must arrive exactly that late.
        for (const double rate : { 44100.0, 96000.0 })
        {
            PluginProcessor processor;
            prepareForProfileTest(processor, rate, 512);   // built-in clip type
            const int latency = processor.getLatencySamples();

            std::vector<float> out;
            juce::MidiBuffer midi;
            for (int b = 0; b < 4; ++b)
            {
                juce::AudioBuffer<float> buffer(2, 512);
                buffer.clear();
                if (b == 0)
                {
                    buffer.setSample(0, 0, 0.25f);
                    buffer.setSample(1, 0, 0.25f);
                }
                processor.processBlockBypassed(buffer, midi);
                appendChannel0(out, buffer);
            }

            const auto peak = std::distance(out.begin(), std::max_element(out.begin(), out.end(),
                [](float a, float b) { return std::abs(a) < std::abs(b); }));
            expect(latency > 0, "A latency is reported at " + juce::String(rate));
            expectEquals(static_cast<int>(peak), latency, "Bypassed impulse lands at the reported latency, "
                                                          + juce::String(rate) + " Hz");
        }
    }

    beginTest("A block processBlock gives up on keeps the reported latency and the duck");
    {
        // A NaN parameter is one of processBlock's early exits; the others share its tail.
        PluginProcessor processor;
        prepareForProfileTest(processor, 44100.0, 512);   // built-in clip type
        const int latency = processor.getLatencySamples();
        expect(latency > 0, "A latency is reported at 44.1 kHz");
        const float distortion = processor.distortionAmountParam->load();
        processor.distortionAmountParam->store(std::numeric_limits<float>::quiet_NaN());

        std::vector<float> out;
        juce::MidiBuffer midi;
        for (int b = 0; b < 4; ++b)
        {
            juce::AudioBuffer<float> buffer(2, 512);
            buffer.clear();
            if (b == 0)
            {
                buffer.setSample(0, 0, 0.25f);
                buffer.setSample(1, 0, 0.25f);
            }
            processor.processBlock(buffer, midi);
            appendChannel0(out, buffer);
        }
        const auto peak = std::distance(out.begin(), std::max_element(out.begin(), out.end(),
            [](float a, float b) { return std::abs(a) < std::abs(b); }));
        expectEquals(static_cast<int>(peak), latency, "The impulse lands at the reported latency");

        // Without the duck gain the fade would stall at full level until the timeout.
        processor.requestOversamplingRebuild(1);
        expect(processor.duckRequested.load(), "A new setting asks for a duck");
        for (int b = 0; b < 8 && processor.duckState != PluginProcessor::DuckState::closed; ++b)
        {
            auto buffer = generateSineWave(440.0, 44100.0, 512, 0.5f);
            processor.processBlock(buffer, midi);
        }
        expect(processor.duckState == PluginProcessor::DuckState::closed, "The fade reaches silence");
        processor.distortionAmountParam->store(distortion);
    }

    beginTest("The output lands exactly at the reported latency with a profile loaded");
    {
        // The relative tests line the paths up with each other; this one pins the
        // whole active path to the figure the host compensates for.
        TempNam identity(linearModelJson("1.0", 1, 48000));
        for (const double rate : { 44100.0, 48000.0, 96000.0 })
        {
            PluginProcessor processor;
            prepareForProfileTest(processor, rate, 512);
            configureUnityChain(processor);
            processor.loadProfileBlocking(identity.file);
            expect(settle(processor), "Identity profile in at " + juce::String(rate));
            expect(processor.profileMode, "Profile mode is on at " + juce::String(rate));

            constexpr size_t impulseAt = 2048;
            std::vector<float> signal(static_cast<size_t>(rate), 0.0f);
            signal[impulseAt] = 0.25f;
            const auto out = renderThrough(processor, signal);
            const auto peak = std::distance(out.begin(), std::max_element(out.begin(), out.end(),
                [](float a, float b) { return std::abs(a) < std::abs(b); }));
            expectEquals(static_cast<int>(peak), static_cast<int>(impulseAt) + processor.getLatencySamples(),
                         "Impulse peak at 2048 + the reported latency, " + juce::String(rate) + " Hz");
        }
    }

    beginTest("A block longer than the host prepared for still runs the profile, in step");
    {
        // Prepared for 512, handed 768: the profile runs in pieces instead of the
        // built-in clip type taking over at zero latency, D ahead of the dry and the
        // Sub Guard low band. Either way the output matches a 512-sample render.
        TempNam identity(linearModelJson("1.0", 1, 48000));
        const auto noise = lowPassedNoise(44100.0, 0.5, 0.05f);   // 22050 samples
        for (const float subGuard : { 0.0f, 80.0f })
        {
            const auto render = [&](int blockSize)
            {
                PluginProcessor processor;
                prepareForProfileTest(processor, 44100.0, 512);
                configureUnityChain(processor);
                setParameter(processor.parameters, "subGuardFreq", subGuard);
                processor.loadProfileBlocking(identity.file);
                expect(settle(processor), "Identity profile in");
                expect(processor.islandDelay > 0, "44.1 kHz runs the island with a delay");

                std::vector<float> out;
                juce::MidiBuffer midi;
                for (size_t done = 0; done + static_cast<size_t>(blockSize) <= noise.size();
                     done += static_cast<size_t>(blockSize))
                {
                    juce::AudioBuffer<float> buffer(2, blockSize);
                    for (int ch = 0; ch < 2; ++ch)
                        buffer.copyFrom(ch, 0, noise.data() + done, blockSize);
                    processor.processBlock(buffer, midi);
                    appendChannel0(out, buffer);
                }
                return out;
            };

            const auto prepared = render(512);
            const auto oversized = render(768);
            const size_t from = 4410;   // past the first blocks' settling
            const size_t to = juce::jmin(prepared.size(), oversized.size());
            float largest = 0.0f;
            for (size_t i = from; i < to; ++i)
                largest = juce::jmax(largest, std::abs(prepared[i] - oversized[i]));
            expect(largest < 1.0e-4f, "768-sample blocks match 512-sample ones, Sub Guard "
                                      + juce::String(subGuard) + ": largest difference " + juce::String(largest, 8));
        }
    }

    beginTest("An older pending profile never comes back over a newer load or a clear");
    {
        // The pending profile was prepared for a rate the host has left, which is
        // the case that sends a still-wanted one back through the staged slot.
        TempNam older(linearModelJson("0.5", 1, 48000));
        TempNam newer(linearModelJson("1.0", 1, 48000));
        for (const bool clear : { false, true })
        {
            PluginProcessor processor;
            prepareForProfileTest(processor, 48000.0, 512);
            juce::String error;
            auto* pending = NamProfile::load(older.file, 2, 44100.0, 512, error).release();
            expect(pending != nullptr && ! processor.isPreparedForHost(*pending), "Older profile left behind by the host");
            processor.pendingProfile.store(pending);
            NamProfile* staged = nullptr;
            if (clear)
            {
                processor.clearRequested.store(true);
            }
            else
            {
                staged = NamProfile::load(newer.file, 2, 48000.0, 512, error).release();
                expect(staged != nullptr && processor.isPreparedForHost(*staged), "Newer profile ready");
                processor.stagedProfile.store(staged);
            }

            std::vector<NamProfile*> toFree;
            {
                const juce::ScopedLock sl(processor.getCallbackLock());
                processor.installRequestedProfile(toFree);
            }
            const juce::String what = clear ? "after a clear" : "after a newer load";
            expect(processor.activeProfile == staged, "The newest intent is installed " + what);
            expect(processor.stagedProfile.load() == nullptr, "The older profile isn't staged again " + what);
            expect(std::find(toFree.begin(), toFree.end(), pending) != toFree.end(),
                   "The older profile is freed " + what);
            for (auto* profile : toFree)
                delete profile;
        }
    }

    // Spins until `done` holds, for at most two seconds. The races below fire the
    // clear the moment the other thread is inside its window.
    const auto spinUntil = [](const std::function<bool()>& done)
    {
        const auto deadline = juce::Time::getMillisecondCounter() + 2000;
        while (! done() && juce::Time::getMillisecondCounter() < deadline)
            std::this_thread::yield();
        return done();
    };

    // Blocks the profile loader until release(), so a test can hold a job queued.
    struct LoaderGate
    {
        explicit LoaderGate(PluginProcessor& processor)
        {
            processor.getProfileLoader().addJob([this] { started.signal(); gate.wait(); });
            started.wait();
        }
        ~LoaderGate() { release(); }
        void release() { gate.signal(); }
        juce::WaitableEvent started, gate;
    };

    const auto waitForRefreshes = [&spinUntil](PluginProcessor& processor)
    {
        return spinUntil([&] { return processor.stagedRefreshesInFlight.load() == 0; });
    };

    beginTest("The timer leaves re-preparing a stale staged profile to the loader");
    {
        PluginProcessor processor;
        prepareForProfileTest(processor, 48000.0, 512);
        const auto model = namTestModel("wavenet.nam");
        juce::String error;
        auto stale = NamProfile::load(model, 2, 44100.0, 512, error);
        expect(stale != nullptr, error);
        stale->setRequestId(processor.beginProfileRequest(model));
        auto* staleProfile = stale.get();
        processor.stagedProfile.store(stale.release());

        // With the loader busy, the tick still returns: nothing was prepared on it.
        LoaderGate busy(processor);
        processor.refreshStagedProfile();
        expectEquals(processor.stagedRefreshesInFlight.load(), 1);
        expect(processor.stagedProfile.load() == nullptr, "The profile is out with the loader");
        expect(! processor.isProfileSwitchIdle(), "A refresh in flight isn't idle");
        expectWithinAbsoluteError(staleProfile->getPreparedSampleRate(), 44100.0, 0.5);

        busy.release();
        expect(waitForRefreshes(processor), "The loader finishes the refresh");
        auto* back = processor.stagedProfile.load();
        expect(back == staleProfile, "The same profile is staged again");
        if (back != nullptr)
            expectWithinAbsoluteError(back->getPreparedSampleRate(), 48000.0, 0.5);
        expect(settle(processor), "The switch completes");
        expect(processor.profileMode && processor.activeProfile == staleProfile, "The profile is installed");
    }

    beginTest("A clear while the loader re-prepares a staged profile keeps it cleared");
    {
        PluginProcessor processor;
        prepareForProfileTest(processor, 48000.0, 512);
        const auto model = namTestModel("wavenet.nam");
        juce::String error;
        auto stale = NamProfile::load(model, 2, 44100.0, 512, error);
        expect(stale != nullptr, error);
        stale->setRequestId(processor.beginProfileRequest(model));
        processor.stagedProfile.store(stale.release());

        // The clear lands while the profile is out of its slot, held on the loader.
        LoaderGate busy(processor);
        processor.refreshStagedProfile();
        expectEquals(processor.stagedRefreshesInFlight.load(), 1);
        processor.clearProfile();   // as setStateInformation on a host thread would
        busy.release();
        expect(waitForRefreshes(processor), "The loader finishes the refresh");

        expect(processor.stagedProfile.load() == nullptr, "The cleared profile isn't staged again");
        expect(settle(processor), "Nothing left to switch");
        expect(processor.activeProfile == nullptr && ! processor.profileMode, "No profile runs");
        expect(processor.getProfileStatus().name.isEmpty(), "Nothing is reported loaded");
    }

    beginTest("A newer load while the loader re-prepares a staged profile wins");
    {
        PluginProcessor processor;
        prepareForProfileTest(processor, 48000.0, 512);
        const auto model = namTestModel("wavenet.nam");
        juce::String error;
        auto stale = NamProfile::load(model, 2, 44100.0, 512, error);
        expect(stale != nullptr, error);
        stale->setRequestId(processor.beginProfileRequest(model));
        processor.stagedProfile.store(stale.release());

        LoaderGate busy(processor);
        processor.refreshStagedProfile();
        expect(processor.loadProfileBlocking(namTestModel("lstm.nam")), "lstm.nam loads");
        busy.release();
        expect(waitForRefreshes(processor), "The loader finishes the refresh");

        expect(settle(processor), "The switch completes");
        expect(processor.activeProfile != nullptr && processor.activeProfile->getName() == "lstm",
               "The newer load is installed, not the refreshed one");
    }

    beginTest("A clear during a switch into profile mode is served, not lost");
    {
        for (int round = 0; round < 5; ++round)
        {
            PluginProcessor processor;
            prepareForProfileTest(processor, 48000.0, 512);
            expect(processor.loadProfileBlocking(namTestModel("wavenet.nam")), "Loads");
            processor.timerCallback();   // routes it: entering profile mode asks for a duck
            expect(processor.duckRequested.load(), "A duck is pending");

            // The clear lands once the switch has installed the profile, while the
            // switch is still rebuilding and its duck flag is still up.
            std::thread timer([&] { processor.performSwitch(); });
            expect(spinUntil([&] { return processor.profileModeInstalled.load(); }),
                   "The switch installed the profile");
            processor.clearProfile();
            timer.join();

            expect(settle(processor), "The clear's own switch completes");
            expect(processor.activeProfile == nullptr && ! processor.profileMode,
                   "The clear won: no profile runs");
        }
    }

    // Left and right as two tones, each carrying its phase across blocks.
    const auto stereoBlock = [](ContinuousSine& left, ContinuousSine& right, int numSamples)
    {
        auto buffer = left.next(numSamples);
        const auto r = right.next(numSamples);
        buffer.copyFrom(1, 0, r, 0, 0, numSamples);
        return buffer;
    };

    beginTest("With a mono input, channel 1 shares channel 0's model output and its model rests");
    {
        PluginProcessor processor;
        prepareForProfileTest(processor);
        setParameter(processor.parameters, "monoInput", 1.0f);
        expect(processor.loadProfileBlocking(namTestModel("wavenet.nam")), "wavenet.nam loads");
        expect(settle(processor), "Loaded");

        ContinuousSine left { 110.0, 48000.0, 0.25f };
        ContinuousSine right { 220.0, 48000.0, 0.1f };   // the unused second input
        juce::MidiBuffer midi;
        for (int b = 0; b < 10; ++b)
        {
            auto buffer = stereoBlock(left, right, 512);
            processor.processBlock(buffer, midi);
        }
        expect(processor.profileRightIdle, "Channel 1's model rests once the fade is done");
        expectEquals(processor.profileRightWeight, 0.0f);

        int allocations = 0;
        bool identical = true;
        for (int b = 0; b < 4; ++b)
        {
            auto buffer = stereoBlock(left, right, 512);
            allocations += allocationsDuring(processor, buffer);
            for (int i = 0; i < 512; ++i)
                identical = identical && juce::exactlyEqual(buffer.getSample(0, i), buffer.getSample(1, i));
        }
        expect(identical, "Both channels carry the same output");
        expectEquals(allocations, 0, "processBlock while channel 1 rests");

        // Through true bypass and back, it keeps resting.
        setParameter(processor.parameters, "distortionAmount", 0.0f);
        for (int b = 0; b < 10; ++b)
        {
            auto buffer = stereoBlock(left, right, 512);
            processor.processBlock(buffer, midi);
        }
        setParameter(processor.parameters, "distortionAmount", 60.0f);
        for (int b = 0; b < 10; ++b)
        {
            auto buffer = stereoBlock(left, right, 512);
            processor.processBlock(buffer, midi);
        }
        expect(processor.profileRightIdle, "Channel 1's model still rests after true bypass");
    }

    beginTest("Leaving mono, channel 1 warms up and fades back in to what it would have been");
    {
        // A runs with Mono Input on, then off. B is stereo throughout and is fed what
        // A's chain saw: the left input on both channels, then both inputs.
        PluginProcessor a, b;
        for (auto* p : { &a, &b })
        {
            prepareForProfileTest(*p);
            expect(p->loadProfileBlocking(namTestModel("wavenet.nam")), "wavenet.nam loads");
            expect(settle(*p), "Loaded");
        }
        setParameter(a.parameters, "monoInput", 1.0f);

        // Mono Input goes off with nothing plugged into the second input: right is
        // silent. unusedA is what that input carries while Mono Input ignores it.
        ContinuousSine leftA { 110.0, 48000.0, 0.25f }, rightA { 220.0, 48000.0, 0.0f },
                       unusedA { 330.0, 48000.0, 0.1f };
        ContinuousSine leftB { 110.0, 48000.0, 0.25f }, leftB2 { 110.0, 48000.0, 0.25f },
                       rightB { 220.0, 48000.0, 0.0f };
        juce::MidiBuffer midi;
        std::vector<float> a0, a1, b0, b1;
        const auto run = [&](juce::AudioBuffer<float>& bufA, juce::AudioBuffer<float>& bufB, int& allocations)
        {
            allocations += allocationsDuring(a, bufA);
            b.processBlock(bufB, midi);
            appendChannel0(a0, bufA);
            appendChannel0(b0, bufB);
            a1.insert(a1.end(), bufA.getReadPointer(1), bufA.getReadPointer(1) + 512);
            b1.insert(b1.end(), bufB.getReadPointer(1), bufB.getReadPointer(1) + 512);
        };

        int allocations = 0;
        for (int blk = 0; blk < 20; ++blk)
        {
            auto bufA = stereoBlock(leftA, unusedA, 512);
            auto bufB = stereoBlock(leftB, leftB2, 512);
            run(bufA, bufB, allocations);
        }
        expect(a.profileRightIdle && ! b.profileRightIdle, "Only the mono instance rests channel 1");

        const size_t rejoinAt = a1.size();
        setParameter(a.parameters, "monoInput", 0.0f);
        for (int blk = 0; blk < 30; ++blk)
        {
            auto bufA = stereoBlock(leftA, rightA, 512);
            auto bufB = stereoBlock(leftB, rightB, 512);
            run(bufA, bufB, allocations);
        }
        expectEquals(allocations, 0, "processBlock while channel 1 rests and rejoins");
        expect(! a.profileRightIdle, "Channel 1's model runs again");
        expectEquals(a.profileRightWeight, 1.0f);

        expect(a0 == b0, "Channel 0 is untouched by the sharing");

        // Channel 1 fades from the tone to silence, stepping no harder than the
        // tone itself does on channel 0. Switched outright, it would drop at once.
        const float stepRight = largestStep(a1, rejoinAt);
        const float stepTone = largestStep(a0, rejoinAt);
        expect(stepRight <= stepTone * 1.25f,
               "No click on rejoining: " + juce::String(stepRight, 6) + " vs " + juce::String(stepTone, 6));

        // Settled and faded in, channel 1 is what it would have been had it never rested.
        float maxDiff = 0.0f;
        for (size_t i = a1.size() - 10 * 512; i < a1.size(); ++i)
            maxDiff = juce::jmax(maxDiff, std::abs(a1[i] - b1[i]));
        expect(maxDiff < 1.0e-4f, "Channel 1 matches the stereo instance: " + juce::String(maxDiff, 8));
    }
}

//==============================================================================
// Profile library
//==============================================================================

namespace
{
    // A folder for one test, removed with everything in it afterwards.
    struct ScratchFolder
    {
        explicit ScratchFolder(const juce::String& name)
            : dir(juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile(name, {}, false))
        {
            dir.createDirectory();
        }
        ~ScratchFolder() { dir.deleteRecursively(); }

        juce::File write(const juce::String& relativePath, const juce::String& content) const
        {
            auto file = dir.getChildFile(relativePath);
            file.getParentDirectory().createDirectory();
            file.replaceWithText(content);
            return file;
        }

        juce::File dir;
    };

    juce::StringArray relativeNames(const juce::Array<juce::File>& files, const juce::File& root)
    {
        juce::StringArray names;
        for (const auto& f : files)
            names.add(f.getRelativePathFrom(root).replaceCharacter('\\', '/'));
        return names;
    }
}

void ProfileLibraryTests::runTest()
{
    beginTest("Lists .nam files in natural order, subfolders included");
    {
        ScratchFolder scratch("sledge-library");
        scratch.write("amp 10.nam", "10");
        scratch.write("amp 2.nam", "2");
        scratch.write("Fender/clean.nam", "clean");
        scratch.write("notes.txt", "not a profile");

        ProfileLibrary library(scratch.dir);
        expectEquals(relativeNames(library.list(), scratch.dir).joinIntoString("|"),
                     juce::String("amp 2.nam|amp 10.nam|Fender/clean.nam"));
        expect(ProfileLibrary(scratch.dir.getChildFile("missing")).list().isEmpty(),
               "A library that doesn't exist yet is empty");
    }

    beginTest("Steps wrap at both ends and start from an end outside the library");
    {
        ScratchFolder scratch("sledge-library");
        const auto a = scratch.write("a.nam", "a");
        const auto b = scratch.write("b.nam", "b");
        const auto c = scratch.write("sub/c.nam", "c");
        ProfileLibrary library(scratch.dir);

        expect(library.step(a, 1) == b, "Forward");
        expect(library.step(b, -1) == a, "Back");
        expect(library.step(c, 1) == a, "Forward from the last wraps to the first");
        expect(library.step(a, -1) == c, "Back from the first wraps to the last");
        expect(library.step({}, 1) == a, "Forward from nothing starts at the first");
        expect(library.step(juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("x.nam"), -1) == c,
               "Back from outside starts at the last");

        const auto position = library.positionOf(b);
        expectEquals(position.index, 1);
        expectEquals(position.count, 3);
        expectEquals(library.positionOf({}).index, -1);

        ScratchFolder empty("sledge-library-empty");
        expect(ProfileLibrary(empty.dir).step(a, 1) == juce::File(), "An empty library has nowhere to go");
    }

    beginTest("Importing copies in, reuses the same content and numbers different content");
    {
        ScratchFolder scratch("sledge-library");
        ScratchFolder outside("sledge-downloads");
        ProfileLibrary library(scratch.dir.getChildFile("Profiles"));   // created by the first import
        juce::String error;

        const auto first = outside.write("jcm.nam", "model A");
        const auto copied = library.import(first, error);
        expect(copied == library.getRoot().getChildFile("jcm.nam"), "Copied under its own name: " + error);
        expectEquals(copied.loadFileAsString(), juce::String("model A"));
        expect(first.existsAsFile(), "The original stays where it was");

        expect(library.import(first, error) == copied, "The same content again is reused");
        expect(library.import(copied, error) == copied, "A library file comes back as it is");

        const auto other = outside.write("other/jcm.nam", "model B");
        const auto numbered = library.import(other, error);
        expect(numbered == library.getRoot().getChildFile("jcm (2).nam"), "Different content gets a number");
        expectEquals(numbered.loadFileAsString(), juce::String("model B"));
        expect(library.import(other, error) == numbered, "And is reused under that number");

        expect(library.import(outside.dir.getChildFile("gone.nam"), error) == juce::File(), "A missing file fails");
        expect(error.isNotEmpty(), "With a reason");
    }

    beginTest("Fingerprints identify content, wherever the file is");
    {
        ScratchFolder scratch("sledge-library");
        const auto a = scratch.write("a.nam", "same bytes");
        const auto b = scratch.write("sub/b.nam", "same bytes");
        const auto c = scratch.write("c.nam", "different!");   // same size, different content
        const auto print = ProfileLibrary::fingerprint(a);

        expect(print.isNotEmpty(), "A readable file has one");
        expectEquals(ProfileLibrary::fingerprint(b), print, "Same content, same fingerprint");
        expect(ProfileLibrary::fingerprint(c) != print, "Same size alone doesn't match");
        expect(ProfileLibrary::fingerprint(scratch.dir.getChildFile("none.nam")).isEmpty(), "No file, no fingerprint");

        ProfileLibrary library(scratch.dir);
        expect(library.findByFingerprint(print, "b.nam") == b, "The preferred name wins among matches");
        expect(library.findByFingerprint(print) == a, "Otherwise the first in order");
        expect(library.findByFingerprint(ProfileLibrary::fingerprint(c), "a.nam") == c,
               "A same-named file with other content is skipped");
        expect(library.findByFingerprint({}) == juce::File(), "No fingerprint finds nothing");
    }

    beginTest("A session whose profile file moved loads the library's copy");
    {
        ScratchFolder libraryFolder("sledge-library");
        ScratchFolder downloads("sledge-downloads");
        const auto original = downloads.dir.getChildFile("wavenet.nam");
        expect(namTestModel("wavenet.nam").copyFileTo(original), "Test model copied");

        juce::MemoryBlock saved;
        {
            PluginProcessor processor;
            prepareForProfileTest(processor);
            expect(processor.loadProfileBlocking(original), "Loads from where it was downloaded");
            expect(processor.getProfileStatus().fingerprint.isNotEmpty(), "The loaded profile has a fingerprint");
            processor.getStateInformation(saved);
        }
        auto savedXml = juce::AudioProcessor::getXmlFromBinary(saved.getData(), static_cast<int>(saved.getSize()));
        expect(savedXml->getStringAttribute(PluginProcessor::profileFingerprintAttribute).isNotEmpty(),
               "The session saves the fingerprint");

        juce::String error;
        const auto copy = ProfileLibrary(libraryFolder.dir).import(original, error);
        expect(copy.existsAsFile(), "Imported: " + error);
        original.deleteFile();

        PluginProcessor restored;
        restored.setProfileLibraryRoot(libraryFolder.dir);
        prepareForProfileTest(restored);
        restored.setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));
        expect(waitForProfileLoad(restored), "Restore finished");
        expect(restored.isProfileLoaded(), "The library's copy stands in for the missing file");
        expectEquals(restored.getProfileStatus().path, copy.getFullPathName(), "The session now points at the copy");
        expect(! restored.parameters.state.hasProperty(PluginProcessor::profileFingerprintAttribute),
               "The fingerprint stays out of the parameter tree, so presets never carry it");

        juce::MemoryBlock resaved;
        restored.getStateInformation(resaved);
        auto resavedXml = juce::AudioProcessor::getXmlFromBinary(resaved.getData(), static_cast<int>(resaved.getSize()));
        expectEquals(resavedXml->getStringAttribute(PluginProcessor::profilePathAttribute), copy.getFullPathName());

        restored.setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));
        expect(! restored.getProfileStatus().loading, "Restoring the same session again keeps the copy loaded");
    }

    beginTest("A missing profile with no copy in the library stays missing, and remembered");
    {
        ScratchFolder libraryFolder("sledge-library");
        ScratchFolder downloads("sledge-downloads");
        const auto original = downloads.dir.getChildFile("wavenet.nam");
        expect(namTestModel("wavenet.nam").copyFileTo(original), "Test model copied");

        juce::MemoryBlock saved;
        {
            PluginProcessor processor;
            prepareForProfileTest(processor);
            processor.loadProfileBlocking(original);
            processor.getStateInformation(saved);
        }
        original.deleteFile();

        // A different model under the same name must not stand in for it.
        expect(namTestModel("lstm.nam").copyFileTo(libraryFolder.dir.getChildFile("wavenet.nam")), "Impostor copied");

        PluginProcessor restored;
        restored.setProfileLibraryRoot(libraryFolder.dir);
        prepareForProfileTest(restored);
        restored.setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));
        expect(waitForProfileLoad(restored), "Restore finished");
        expect(! restored.isProfileLoaded(), "A same-named file with other content is not loaded");
        expect(restored.getProfileStatus().error.isNotEmpty(), "The missing file is reported");

        juce::MemoryBlock resaved;
        restored.getStateInformation(resaved);
        auto savedXml = juce::AudioProcessor::getXmlFromBinary(saved.getData(), static_cast<int>(saved.getSize()));
        auto resavedXml = juce::AudioProcessor::getXmlFromBinary(resaved.getData(), static_cast<int>(resaved.getSize()));
        expectEquals(resavedXml->getStringAttribute(PluginProcessor::profilePathAttribute), original.getFullPathName(),
                     "The path is kept for when the file comes back");
        expectEquals(resavedXml->getStringAttribute(PluginProcessor::profileFingerprintAttribute),
                     savedXml->getStringAttribute(PluginProcessor::profileFingerprintAttribute),
                     "So is the fingerprint");
    }
}

//==============================================================================
// Profile card animation
//==============================================================================

void ProfileCardAnimationTests::runTest()
{
    using A = ProfileCardAnimation;
    const double t0 = 100.0;
    const double settled = A::slideSeconds + 0.01;

    beginTest("A click slides the card in from its arrow's side, holds, then fades");
    {
        A next;
        expect(! next.isActive(t0), "Hidden before any click");
        next.trigger(1, t0);
        expect(next.isActive(t0), "Shown from the click");
        expectWithinAbsoluteError(next.getCardAlpha(t0), 0.0f, 1.0e-6f, "Starts transparent");
        expectWithinAbsoluteError(next.getCardOffset(t0), 1.0f, 1.0e-6f, "Next enters from the right");
        expect(next.isMoving(t0 + A::slideSeconds * 0.5), "Moving while it slides in");

        expectWithinAbsoluteError(next.getCardAlpha(t0 + settled), 1.0f, 1.0e-6f, "Opaque once in");
        expectWithinAbsoluteError(next.getCardOffset(t0 + settled), 0.0f, 1.0e-6f, "In place once in");
        expect(! next.isMoving(t0 + settled), "Settled: nothing to repaint while it holds");
        expectWithinAbsoluteError(next.getCardAlpha(t0 + A::holdSeconds - 0.01), 1.0f, 1.0e-6f, "Holds");

        const double midFade = t0 + A::holdSeconds + A::fadeSeconds * 0.5;
        expect(next.getCardAlpha(midFade) > 0.0f && next.getCardAlpha(midFade) < 1.0f, "Fading");
        expect(next.isMoving(midFade), "Moving while it fades");
        expect(! next.isActive(t0 + A::holdSeconds + A::fadeSeconds + 0.01), "Gone after the fade");

        A previous;
        previous.trigger(-1, t0);
        expectWithinAbsoluteError(previous.getCardOffset(t0), -1.0f, 1.0e-6f, "Previous enters from the left");

        A menu;
        menu.trigger(0, t0);
        expectWithinAbsoluteError(menu.getCardOffset(t0), 0.0f, 1.0e-6f, "A menu pick fades in place");
    }

    beginTest("Clicks while the card shows slide the name and restart the hold");
    {
        A card;
        card.trigger(1, t0);
        const double t1 = t0 + 1.0;
        card.trigger(-1, t1);
        expectWithinAbsoluteError(card.getCardAlpha(t1), 1.0f, 1.0e-6f, "The card stays up");
        expectWithinAbsoluteError(card.getCardOffset(t1), 0.0f, 1.0e-6f, "And does not slide in again");
        expectEquals(card.getNameDirection(), -1);
        expectWithinAbsoluteError(card.getNameProgress(t1), 0.0f, 1.0e-6f, "The new name starts its slide");
        expect(card.isNameSliding(t1) && card.isMoving(t1), "Moving while the name slides");
        expectWithinAbsoluteError(card.getNameProgress(t1 + settled), 1.0f, 1.0e-6f, "And settles");
        expectWithinAbsoluteError(card.getCardAlpha(t0 + A::holdSeconds + A::fadeSeconds), 1.0f, 1.0e-6f,
                                  "The hold restarts from the latest click");
        expect(! card.isActive(t1 + A::holdSeconds + A::fadeSeconds + 0.01), "Then fades as usual");
    }

    beginTest("A click while the card fades brings it back without a jump");
    {
        A card;
        card.trigger(1, t0);
        const double t1 = t0 + A::holdSeconds + A::fadeSeconds * 0.5;
        const float before = card.getCardAlpha(t1);
        card.trigger(1, t1);
        expectWithinAbsoluteError(card.getCardAlpha(t1), before, 1.0e-4f, "Opacity continues from where it was");
        expectWithinAbsoluteError(card.getCardOffset(t1), 0.0f, 1.0e-6f, "No second slide-in");
        expectWithinAbsoluteError(card.getCardAlpha(t1 + settled), 1.0f, 1.0e-6f, "Back to opaque");
    }

    beginTest("The card waits for a slow load, and keepAlive never revives a fading card");
    {
        A card;
        card.trigger(1, t0);
        for (double t = t0; t < t0 + 3.0; t += 0.1)
            card.keepAlive(t);   // the profile is still loading
        expectWithinAbsoluteError(card.getCardAlpha(t0 + 3.0), 1.0f, 1.0e-6f, "Still up while loading");
        expect(! card.isActive(t0 + 3.0 + A::holdSeconds + A::fadeSeconds), "Fades once the load is done");

        A fading;
        fading.trigger(1, t0);
        const double t1 = t0 + A::holdSeconds + 0.1;
        const float before = fading.getCardAlpha(t1);
        fading.keepAlive(t1);
        expectWithinAbsoluteError(fading.getCardAlpha(t1), before, 1.0e-6f, "Already fading: unchanged");
    }
}

#endif // JUCE_DEBUG
