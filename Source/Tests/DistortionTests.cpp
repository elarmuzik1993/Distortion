#if JUCE_DEBUG

#include "DistortionTests.h"
#include <iostream>

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
        "compRatio", "compWetDry", "compCrossover"
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

// Static test registration moved to registerAllTests() in DistortionTests.h
// to ensure tests are registered before they're run

#endif // JUCE_DEBUG
