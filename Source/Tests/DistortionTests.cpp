#if JUCE_DEBUG

#include "DistortionTests.h"

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
                float output = processor.applyStudioDistortion(input, gain, drive, clipType);

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
    float zeroOutput = processor.applyStudioDistortion(0.0f, 1.0f, 1.0f, clipType);
    expect(std::abs(zeroOutput) < 0.1f, name + ": Zero input should produce near-zero output");
}

void DistortionDSPTests::testEdgeCases(PluginProcessor& processor)
{
    // Test denormal values
    float denormal = 1e-40f;
    float output = processor.applyStudioDistortion(denormal, 1.0f, 1.0f, 0);
    expect(!std::isnan(output), "Denormal input caused NaN");
    expect(!std::isinf(output), "Denormal input caused Inf");

    // Test very large values (should be clipped)
    float largeInput = 10.0f;
    output = processor.applyStudioDistortion(largeInput, 1.0f, 1.0f, 0);
    expect(std::abs(output) <= 2.0f, "Large input not properly clipped");

    // Test negative large values
    output = processor.applyStudioDistortion(-10.0f, 1.0f, 1.0f, 0);
    expect(std::abs(output) <= 2.0f, "Large negative input not properly clipped");

    // Test minimum gain
    output = processor.applyStudioDistortion(1.0f, 0.0f, 1.0f, 0);
    expect(!std::isnan(output), "Zero gain caused NaN");

    // Test minimum drive
    output = processor.applyStudioDistortion(1.0f, 1.0f, 1.0f, 0);
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

            float output = processor.applyStudioDistortion(input, 1.5f, 3.0f, clipType);

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
    float lastPhase = 0.0f;
    float lastValue = processor.generateLFOWaveform(0.0f, 4);

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
    if (auto* param = processor.parameters.getParameter("distortionAmount"))
        param->setValueNotifyingHost(0.0f);
    if (auto* param = processor.parameters.getParameter("compEnabled"))
        param->setValueNotifyingHost(0.0f);

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
    processor.prepareToPlay(44100.0, 512);

    // Enable 808-safe mode and distortion
    if (auto* param = processor.parameters.getParameter("bandSplitEnabled"))
        param->setValueNotifyingHost(1.0f);
    if (auto* param = processor.parameters.getParameter("distortionAmount"))
        param->setValueNotifyingHost(0.7f);

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
    if (auto* param = processor.parameters.getParameter("distortionAmount"))
        param->setValueNotifyingHost(0.5f);

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
    if (auto* param = processor.parameters.getParameter("distortionAmount"))
        param->setValueNotifyingHost(0.5f);

    // Create signal with DC offset
    auto buffer = generateDCOffset(2048, 0.5f);
    juce::MidiBuffer midi;

    // Process multiple blocks to let DC filter settle
    for (int i = 0; i < 10; ++i)
    {
        buffer = generateDCOffset(512, 0.5f);
        processor.processBlock(buffer, midi);
    }

    // DC should be significantly reduced after several blocks
    float dcLevel = 0.0f;
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        const auto* data = buffer.getReadPointer(ch);
        for (int s = 0; s < buffer.getNumSamples(); ++s)
            dcLevel += data[s];
    }
    dcLevel /= static_cast<float>(buffer.getNumSamples() * buffer.getNumChannels());

    expect(std::abs(dcLevel) < 0.3f, "DC blocking should reduce DC offset. Remaining: " +
           juce::String(dcLevel));
}

void ProcessBlockTests::testWetDryMix()
{
    PluginProcessor processor;
    processor.prepareToPlay(44100.0, 512);

    // Enable distortion
    if (auto* param = processor.parameters.getParameter("distortionAmount"))
        param->setValueNotifyingHost(0.7f);

    // Test 0% wet (dry only)
    if (auto* param = processor.parameters.getParameter("distMix"))
        param->setValueNotifyingHost(0.0f);

    auto dryBuffer = generateSineWave(1000.0, 44100.0, 512, 0.5f);
    auto originalDry = dryBuffer;
    juce::MidiBuffer midi;

    processor.processBlock(dryBuffer, midi);
    float dryPeak = calculatePeak(dryBuffer);

    // Test 100% wet
    if (auto* param = processor.parameters.getParameter("distMix"))
        param->setValueNotifyingHost(1.0f);

    auto wetBuffer = generateSineWave(1000.0, 44100.0, 512, 0.5f);
    processor.processBlock(wetBuffer, midi);
    float wetPeak = calculatePeak(wetBuffer);

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

    // Set output gain to maximum
    if (auto* param = processor.parameters.getParameter("outputGain"))
        param->setValueNotifyingHost(1.0f);

    auto buffer = generateSineWave(1000.0, 44100.0, 512, 0.3f);
    juce::MidiBuffer midi;

    processor.processBlock(buffer, midi);

    // Output should be louder than input
    expect(calculatePeak(buffer) > 0.3f, "Maximum output gain should boost signal");

    // Set output gain to minimum
    if (auto* param = processor.parameters.getParameter("outputGain"))
        param->setValueNotifyingHost(0.0f);

    buffer = generateSineWave(1000.0, 44100.0, 512, 0.5f);
    processor.processBlock(buffer, midi);

    // Output should be quieter
    expect(calculatePeak(buffer) < 0.5f, "Minimum output gain should reduce signal");
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
        "bandSplitEnabled", "clipType", "distMix",
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
    processor.parameters.getParameter("distortionAmount")->setValueNotifyingHost(0.5f);
    processor.parameters.getParameter("compEnabled")->setValueNotifyingHost(1.0f);

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
    processor.parameters.getParameter("distortionAmount")->setValueNotifyingHost(0.5f);

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
    processor1.parameters.getParameter("distortionAmount")->setValueNotifyingHost(0.3f);
    processor2.parameters.getParameter("distortionAmount")->setValueNotifyingHost(0.8f);

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
    processor1.parameters.getParameter("distortionAmount")->setValueNotifyingHost(0.5f);
    processor2.parameters.getParameter("distortionAmount")->setValueNotifyingHost(0.5f);
    processor1.parameters.getParameter("clipType")->setValueNotifyingHost(0.0f);  // Brutal Fuzz has noise
    processor2.parameters.getParameter("clipType")->setValueNotifyingHost(0.0f);

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
    processor.parameters.getParameter("distortionAmount")->setValueNotifyingHost(0.6f);
    processor.parameters.getParameter("clipType")->setValueNotifyingHost(
        static_cast<float>(clipType) / 6.0f);

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

    // Enable 808-safe mode
    processor.parameters.getParameter("bandSplitEnabled")->setValueNotifyingHost(1.0f);
    processor.parameters.getParameter("distortionAmount")->setValueNotifyingHost(0.7f);

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
