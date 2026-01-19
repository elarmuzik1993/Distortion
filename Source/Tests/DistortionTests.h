#pragma once

#if JUCE_DEBUG

#include <JuceHeader.h>
#include "../PluginProcessor.h"
#include "TestUtilities.h"

//==============================================================================
// Test Categories
//==============================================================================

namespace TestCategories
{
    const juce::String DSP = "DSP";
    const juce::String Parameters = "Parameters";
    const juce::String ProcessBlock = "ProcessBlock";
    const juce::String ThreadSafety = "ThreadSafety";
    const juce::String StateIO = "StateIO";
    const juce::String GoldenAudio = "GoldenAudio";
}

//==============================================================================
// DSP Tests
//==============================================================================

/** Tests for applyStudioDistortion() - all 7 clip types */
class DistortionDSPTests : public juce::UnitTest
{
public:
    DistortionDSPTests() : UnitTest("Distortion DSP Algorithms", TestCategories::DSP) {}
    void runTest() override;

private:
    void testClipType(PluginProcessor& processor, int clipType, const juce::String& name);
    void testEdgeCases(PluginProcessor& processor);
    void testOutputRange(PluginProcessor& processor);
};

/** Tests for applyLA2ACompression() */
class CompressionDSPTests : public juce::UnitTest
{
public:
    CompressionDSPTests() : UnitTest("LA2A Compression", TestCategories::DSP) {}
    void runTest() override;

private:
    void testThresholdMapping();
    void testRatioModes();
    void testMakeupGain();
    void testEnvelopeFollower();
    void testGainReductionMeter();
};

/** Tests for pre-distortion transient tamer compression */
class PreCompressionTests : public juce::UnitTest
{
public:
    PreCompressionTests() : UnitTest("Pre-Distortion Compression", TestCategories::DSP) {}
    void runTest() override;

private:
    void testTransientReduction();
    void testBypassWhenDistortionOff();
    void testEnvelopeAttackRelease();
    void testStereoIndependence();
    void testNoInvalidSamples();
    void testGainReductionRange();
};

/** Tests for sub-linear harmonic density scaling */
class HarmonicDensityTests : public juce::UnitTest
{
public:
    HarmonicDensityTests() : UnitTest("Harmonic Density Scaling", TestCategories::DSP) {}
    void runTest() override;

private:
    void testSubLinearScaling();
    void testClipTypeSpecificity();
    void testNoNaNOrInf();
};

/** Tests for generateLFOWaveform() - all 5 waveforms */
class LFOTests : public juce::UnitTest
{
public:
    LFOTests() : UnitTest("LFO Waveforms", TestCategories::DSP) {}
    void runTest() override;

private:
    void testWaveformShape(PluginProcessor& processor, int waveformType, const juce::String& name);
    void testOutputRange(PluginProcessor& processor);
    void testRandomSampleHold(PluginProcessor& processor);
};

//==============================================================================
// Integration Tests
//==============================================================================

/** Tests for full processBlock() signal chain */
class ProcessBlockTests : public juce::UnitTest
{
public:
    ProcessBlockTests() : UnitTest("Process Block Integration", TestCategories::ProcessBlock) {}
    void runTest() override;

private:
    void testTrueBypass();
    void test808SafeMode();
    void testOversampling();
    void testDCBlocking();
    void testWetDryMix();
    void testEmptyBufferHandling();
    void testOutputGain();
};

/** Tests for all parameters - ranges and smoothing */
class ParameterTests : public juce::UnitTest
{
public:
    ParameterTests() : UnitTest("Parameter Validation", TestCategories::Parameters) {}
    void runTest() override;

private:
    void testParameterRanges(PluginProcessor& processor);
    void testParameterDefaults(PluginProcessor& processor);
    void testParameterSmoothing(PluginProcessor& processor);
    void testParameterPointerValidity(PluginProcessor& processor);
};

/** Tests for multi-sample-rate support */
class SampleRateTests : public juce::UnitTest
{
public:
    SampleRateTests() : UnitTest("Sample Rate Adaptation", TestCategories::ProcessBlock) {}
    void runTest() override;

private:
    void testSampleRate(double sampleRate);
    void testRuntimeSampleRateChange();
};

//==============================================================================
// Safety Tests
//==============================================================================

/** Tests for thread safety - scope buffer, atomics */
class ThreadSafetyTests : public juce::UnitTest
{
public:
    ThreadSafetyTests() : UnitTest("Thread Safety", TestCategories::ThreadSafety) {}
    void runTest() override;

private:
    void testScopeBufferAccess();
    void testAtomicGainReduction();
    void testMultiInstanceIndependence();
    void testPerInstanceRandomGenerators();
};

/** Tests for state save/load */
class StateIOTests : public juce::UnitTest
{
public:
    StateIOTests() : UnitTest("State Save/Load", TestCategories::StateIO) {}
    void runTest() override;

private:
    void testGetStateInformation();
    void testSetStateInformation();
    void testRoundTrip();
    void testInvalidDataHandling();
};

//==============================================================================
// Golden Audio Tests
//==============================================================================

/** Tests comparing output to golden reference files */
class GoldenAudioTests : public juce::UnitTest
{
public:
    GoldenAudioTests() : UnitTest("Golden Audio Reference", TestCategories::GoldenAudio) {}
    void runTest() override;

private:
    // Set to true to generate new reference files instead of comparing
    static constexpr bool GENERATE_MODE = false;

    juce::File getTestAudioDirectory();
    void testSilencePassthrough();
    void testDistortionOutput(int clipType, const juce::String& name);
    void test808BandSplit();
};

//==============================================================================
// Output Limiter Tests
//==============================================================================

/** Tests for final output soft limiter */
class OutputLimiterTests : public juce::UnitTest
{
public:
    OutputLimiterTests() : UnitTest("Output Limiter", TestCategories::DSP) {}
    void runTest() override;

private:
    void testThresholdEnforcement();
    void testTransparencyBelowThreshold();
    void testStereoLinking();
    void testSoftKnee();
    void testEnvelopeAttackRelease();
    void testStateReset();
};

//==============================================================================
// Test Runner Function
//==============================================================================

//==============================================================================
// Sanity Test (no PluginProcessor dependency)
//==============================================================================

class SanityTests : public juce::UnitTest
{
public:
    SanityTests() : juce::UnitTest("Sanity Tests", "Distortion") {}

    void runTest() override
    {
        beginTest("Basic Math");
        expect(1 + 1 == 2, "Basic addition failed");
        expect(2 * 3 == 6, "Basic multiplication failed");

        beginTest("Audio Buffer Creation");
        juce::AudioBuffer<float> buffer(2, 512);
        expect(buffer.getNumChannels() == 2, "Channel count mismatch");
        expect(buffer.getNumSamples() == 512, "Sample count mismatch");
        buffer.clear();
        expect(buffer.getMagnitude(0, 512) == 0.0f, "Buffer not cleared");
    }
};

class MinimalProcessorTest : public juce::UnitTest
{
public:
    MinimalProcessorTest() : juce::UnitTest("Minimal Processor Test", "Distortion") {}

    void runTest() override
    {
        beginTest("Create Processor");
        {
            PluginProcessor processor;
            expect(true, "Processor created");
        }

        beginTest("Prepare To Play");
        {
            PluginProcessor processor;
            processor.prepareToPlay(44100.0, 512);
            expect(true, "PrepareToPlay completed");
        }

        beginTest("Process Block");
        {
            PluginProcessor processor;
            processor.prepareToPlay(44100.0, 512);

            juce::AudioBuffer<float> buffer(2, 512);
            buffer.clear();
            juce::MidiBuffer midi;

            processor.processBlock(buffer, midi);
            expect(true, "ProcessBlock completed");
        }
    }
};

// Force static test registration
inline void registerAllTests()
{
    static bool registered = false;
    if (registered) return;
    registered = true;

    // Start with sanity test to verify framework works
    static SanityTests sanityTests;

    // Minimal processor test to verify basic PluginProcessor functionality
    static MinimalProcessorTest minimalProcessorTest;

    // Working tests
    static DistortionDSPTests distortionDSPTests;
    static CompressionDSPTests compressionDSPTests;
    static PreCompressionTests preCompressionTests;
    static HarmonicDensityTests harmonicDensityTests;
    static OutputLimiterTests outputLimiterTests;
    static LFOTests lfoTests;
    static ProcessBlockTests processBlockTests;
    static ParameterTests parameterTests;
    static SampleRateTests sampleRateTests;
    static ThreadSafetyTests threadSafetyTests;
    static StateIOTests stateIOTests;
    static GoldenAudioTests goldenAudioTests;
}

#endif // JUCE_DEBUG
