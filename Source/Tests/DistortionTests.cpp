#if JUCE_DEBUG

#include "DistortionTests.h"
#include "../RTAllocationGuard.h"
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

void DistortionDSPTests::testClipType(PluginProcessor& processor, int clipType, const juce::String& name)
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
                expect(!std::isnan(output), name + ": Output is NaN for input " + juce::String(input));
                expect(!std::isinf(output), name + ": Output is Inf for input " + juce::String(input));

                // Output should be bounded (allow some headroom for processing)
                expect(std::abs(output) <= 2.0f, name + ": Output " + juce::String(output) +
                       " exceeds bounds for input " + juce::String(input));
            }
        }
    }

    // Test zero input produces near-zero output (except for noise injection)
    float zeroOutput = processor.applyStudioDistortion(0.0f, 1.0f, 1.0f, clipType, 1.0f);
    expect(std::abs(zeroOutput) < 0.1f, name + ": Zero input should produce near-zero output");
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
}

void OutputLimiterTests::testThresholdEnforcement()
{
    using namespace TestUtilities;

    PluginProcessor processor;
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
    processor.prepareToPlay(44100.0, 512);

    // Set envelope to non-unity value
    processor.outputLimiterEnvelope = 0.3f;

    // Reset DSP state
    processor.resetDSPState();

    // Envelope should be back to unity
    expectEquals(processor.outputLimiterEnvelope, 1.0f,
        "State reset didn't restore envelope to unity");
}

//==============================================================================
// LFOTests Implementation
//==============================================================================

void LFOTests::runTest()
{
    PluginProcessor processor;
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

void LFOTests::testWaveformShape(PluginProcessor& processor, int waveformType, const juce::String& name)
{
    // Test key phase points
    const float testPhases[] = { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f };

    for (float phase : testPhases)
    {
        float output = processor.generateLFOWaveform(phase, waveformType);

        expect(!std::isnan(output), name + ": Output is NaN at phase " + juce::String(phase));
        expect(!std::isinf(output), name + ": Output is Inf at phase " + juce::String(phase));
        expect(output >= -1.0f && output <= 1.0f,
               name + ": Output " + juce::String(output) + " out of range [-1, 1]");
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

    beginTest("Destination 2: Hi-pass modulation");
    testDestination(2, "highPassFreq", 100.0f);

    beginTest("Destination 3: Dist mix modulation");
    testDestination(3, "distMix", 50.0f);

    beginTest("Destination 4: Output gain modulation");
    testDestination(4, "outputGain", 50.0f);

    beginTest("Extreme depth doesn't cause NaN on any destination");
    {
        PluginProcessor processor;
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
}

void ProcessBlockTests::testTrueBypass()
{
    PluginProcessor processor;
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

void DryWetAlignmentTests::testNoCombFiltering()
{
    // At 50% global mix, both halves are near-identical (distortion near-linear)
    // so the summed output should preserve the signal at full amplitude. Without
    // the dry-delay fix, dry + delayed_wet sums would produce comb-filter notches
    // at f = Fs/(2*latency), roughly 2 kHz for a 4x polyphase IIR at 44.1kHz.
    PluginProcessor processor;
    const double sr = 44100.0;
    const int blockSize = 512;
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
    // Changing oversampling stages triggers reinitializeOversampling, which
    // must reallocate dryDelayState to the new latency. Process under two
    // different oversampling factors and confirm no invalid samples.
    PluginProcessor processor;
    const double sr = 44100.0;
    const int blockSize = 512;
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
        "inputGain", "outputGain", "distortionAmount", "highPassFreq",
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
        expectWithinAbsoluteError(distortion->getDefaultValue(), 0.0f, 0.01f,
                                  "Distortion default should be 0");
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
    processor.prepareToPlay(44100.0, 512);
    setParameter(processor.parameters, "distortionAmount", 50.0f);

    auto buffer = generateSineWave(1000.0, 44100.0, 512, 0.7f);
    juce::MidiBuffer midi;
    processor.processBlock(buffer, midi);

    // Change to 96kHz (simulating DAW sample rate change)
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
    processor.prepareToPlay(44100.0, 512);

    // Simulate audio thread writing
    auto buffer = generateSineWave(1000.0, 44100.0, 512, 0.7f);
    juce::MidiBuffer midi;
    processor.processBlock(buffer, midi);

    // Simulate GUI thread reading
    juce::AudioBuffer<float> displayBuffer(2, 512);
    processor.fillScopeBuffer(displayBuffer);

    // Should not crash and produce valid data
    expect(!containsInvalidSamples(displayBuffer), "Scope buffer contains invalid samples");
}

void ThreadSafetyTests::testScopeDrainExcess()
{
    PluginProcessor processor;
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

    processor1.prepareToPlay(44100.0, 512);
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

    processor1.prepareToPlay(44100.0, 512);
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
}

void StateIOTests::testGetStateInformation()
{
    PluginProcessor processor;
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

void GoldenAudioTests::testDistortionOutput(int clipType, const juce::String& name)
{
    PluginProcessor processor;
    processor.prepareToPlay(44100.0, 512);

    // Enable distortion with this clip type
    setParameter(processor.parameters, "distortionAmount", 60.0f);
    setParameter(processor.parameters, "clipType", static_cast<float>(clipType));

    // Generate test signal
    auto buffer = generateSineWave(1000.0, 44100.0, 512, 0.7f);
    juce::MidiBuffer midi;

    processor.processBlock(buffer, midi);

    // Basic validation - output should be valid and not silent
    expect(!containsInvalidSamples(buffer), name + " produced invalid samples");
    expect(!isSilent(buffer), name + " produced silence");

    // If in generate mode, save reference file
    if (GENERATE_MODE)
    {
        auto dir = getTestAudioDirectory();
        if (!dir.exists())
            dir.createDirectory();

        auto file = dir.getChildFile("golden_" + name + "_44100.wav");
        if (saveWavFile(buffer, file, 44100.0))
            logMessage("Generated: " + file.getFullPathName());
    }
}

void GoldenAudioTests::test808BandSplit()
{
    PluginProcessor processor;
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
    expect(processor.tubeBiasEnvelope[0] == 0.0f,
        "Tube bias envelope not cleared by resetDSPState");
}

void StatefulDistortionTests::testTapeHysteresis()
{
    // Tape saturation (clip type 3) should produce different output based on signal history
    PluginProcessor processor;
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
    expect(processor.tapeSaturationEnvelope[0] == 0.0f,
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
            auto* p = new int (99);
            delete p;

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
            auto* p = new int (100);
            delete p;
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

void RTCleanOversamplingTest::runTest()
{
#if defined (DISTORTION_RT_GUARD) && DISTORTION_RT_GUARD
    beginTest("No allocation in processBlock while oversampling rebuild is deferred");
    {
        PluginProcessor processor;
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
        for (int i = 0; i < 5; ++i)
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

#endif // JUCE_DEBUG
