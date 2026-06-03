#pragma once

#if JUCE_DEBUG

#include <JuceHeader.h>
#include "../PluginProcessor.h"
#include "../FastMath.h"
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

/** Tests for Clean Boost (pre-emphasis toggle in front of the distortion) */
class CleanBoostTests : public juce::UnitTest
{
public:
    CleanBoostTests() : UnitTest("Clean Boost", TestCategories::DSP) {}
    void runTest() override;

private:
    void testNoInvalidSamplesAcrossSampleRates();
    void testBoostRaisesLevel();
    void testBypassWhenDistortionOff();
    void testStateRoundTrip();
    void testNoAllocationWhenToggling();
    void testRuntimeOversamplingChange();
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

/** Tests for LFO destination routing */
class LFODestinationTests : public juce::UnitTest
{
public:
    LFODestinationTests() : UnitTest("LFO Destination Routing", TestCategories::DSP) {}
    void runTest() override;

private:
    void testDestination(int destIndex, const juce::String& paramID, float centerValue);
};

/** Tests for LFO BPM sync toggle and division parameter */
class LFOBpmSyncTests : public juce::UnitTest
{
public:
    LFOBpmSyncTests() : UnitTest("LFO BPM Sync", TestCategories::DSP) {}
    void runTest() override;
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

/** Null-test gate for PR-8 processBlock decomposition — bit-exact determinism check */
class ProcessBlockDecompTest : public juce::UnitTest
{
public:
    ProcessBlockDecompTest() : UnitTest("ProcessBlock Decomposition", TestCategories::ProcessBlock) {}
    void runTest() override;
};

/** Tests for dry/wet alignment across the oversampling boundary */
class DryWetAlignmentTests : public juce::UnitTest
{
public:
    DryWetAlignmentTests() : UnitTest("Dry/Wet Alignment", TestCategories::ProcessBlock) {}
    void runTest() override;

private:
    void testDryOnlyLatency();
    void testNoCombFiltering();
    void testFullyWetPathUnaffected();
    void testOversamplingReinit();
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
    void testScopeDrainExcess();
    void testAtomicGainReduction();
    void testMultiInstanceIndependence();
    void testPerInstanceRandomGenerators();
};

/** Tests for the debug RT-allocation guard (PR-0 instrumentation). */
class RTAllocationGuardTest : public juce::UnitTest
{
public:
    RTAllocationGuardTest() : UnitTest("RT Allocation Guard", TestCategories::ThreadSafety) {}
    void runTest() override;
};

class RTBufferPreallocTest : public juce::UnitTest
{
public:
    RTBufferPreallocTest() : UnitTest("RT Buffer Preallocation", TestCategories::ThreadSafety) {}
    void runTest() override;
};

class RTCleanPreHighPassTest : public juce::UnitTest
{
public:
    RTCleanPreHighPassTest() : UnitTest("RT Clean Pre HighPass", TestCategories::ThreadSafety) {}
    void runTest() override;
};

class RTCleanSubGuardTest : public juce::UnitTest
{
public:
    RTCleanSubGuardTest() : UnitTest("RT Clean Sub Guard", TestCategories::ThreadSafety) {}
    void runTest() override;
};

class RTCleanOversamplingTest : public juce::UnitTest
{
public:
    RTCleanOversamplingTest() : UnitTest("RT Clean Oversampling", TestCategories::ThreadSafety) {}
    void runTest() override;
};

class RTCleanToneSweepTest : public juce::UnitTest
{
public:
    RTCleanToneSweepTest() : UnitTest("RT Clean Tone Sweep", TestCategories::ThreadSafety) {}
    void runTest() override;
};

class RTCleanSampleRateDriftTest : public juce::UnitTest
{
public:
    RTCleanSampleRateDriftTest() : UnitTest("RT Clean Sample Rate Drift", TestCategories::ThreadSafety) {}
    void runTest() override;
};

// PR-6: Linear-Phase Dry toggle — FIR oversampling path
class LinearPhaseDryTest : public juce::UnitTest
{
public:
    LinearPhaseDryTest() : UnitTest("Linear Phase Dry", TestCategories::DSP) {}
    void runTest() override;
};

// PR-14: verify coefficient updates actually reach the per-channel Filter.
class CoefficientPropagationTest : public juce::UnitTest
{
public:
    CoefficientPropagationTest() : UnitTest("Coefficient Propagation", TestCategories::DSP) {}
    void runTest() override;
};

// PR-7: verify FastMath approximation accuracy and plugin tolerance with fast math.
class FastMathAccuracyTest : public juce::UnitTest
{
public:
    FastMathAccuracyTest() : UnitTest("Fast Math Accuracy", TestCategories::DSP) {}
    void runTest() override;
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
// Normalization Tests (clip type level matching)
//==============================================================================

/** Tests that all clip types produce similar RMS output levels */
class NormalizationTests : public juce::UnitTest
{
public:
    NormalizationTests() : UnitTest("Clip Type Normalization", TestCategories::DSP) {}
    void runTest() override;

private:
    void testClipTypeLevelMatching();
};

//==============================================================================
// Stateful Distortion Tests (Tube/Tape memory)
//==============================================================================

/** Tests that Tube and Tape algorithms have signal-history-dependent output */
class StatefulDistortionTests : public juce::UnitTest
{
public:
    StatefulDistortionTests() : UnitTest("Stateful Distortion", TestCategories::DSP) {}
    void runTest() override;

private:
    void testTubeBiasShift();
    void testTapeHysteresis();
};

//==============================================================================
// Sub Guard / Input Filter Spectral Tests
//==============================================================================

/** Sub Guard crossover sums flat: same noise through Sub Guard OFF vs ON should
    leave the spectrum unchanged (no dip/null at the crossover). Migrated from the
    render harness's --verify-subguard so it runs automatically in CI. */
class SubGuardFlatnessTest : public juce::UnitTest
{
public:
    SubGuardFlatnessTest() : UnitTest("Sub Guard Crossover Flatness", TestCategories::DSP) {}
    void runTest() override;
};

/** Input multimode filter shapes correctly in true bypass (distortion + comp off):
    high-pass cuts lows, low-pass cuts highs, band-pass cuts both. Migrated from the
    render harness's --verify-filter. Also guards the bypass-path filter behaviour. */
class InputFilterModeTest : public juce::UnitTest
{
public:
    InputFilterModeTest() : UnitTest("Input Filter Modes", TestCategories::DSP) {}
    void runTest() override;
};

//==============================================================================
// Test Runner Function
//==============================================================================

//==============================================================================
// Sanity Test (no PluginProcessor dependency)
//==============================================================================

/** Tests for CyclingComboBox::cycleSelection — wrap + excluded-ID skipping */
class CyclingComboBoxTests : public juce::UnitTest
{
public:
    CyclingComboBoxTests() : juce::UnitTest("CyclingComboBox", "UI") {}
    void runTest() override;
};

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
    static CyclingComboBoxTests cyclingComboBoxTests;

    // Working tests
    static DryWetAlignmentTests dryWetAlignmentTests;
    static DistortionDSPTests distortionDSPTests;
    static CompressionDSPTests compressionDSPTests;
    static PreCompressionTests preCompressionTests;
    static CleanBoostTests cleanBoostTests;
    static HarmonicDensityTests harmonicDensityTests;
    static OutputLimiterTests outputLimiterTests;
    static LFOTests lfoTests;
    static LFODestinationTests lfoDestinationTests;
    static LFOBpmSyncTests lfoBpmSyncTests;
    static ProcessBlockTests processBlockTests;
    static ParameterTests parameterTests;
    static SampleRateTests sampleRateTests;
    static ThreadSafetyTests threadSafetyTests;
    static RTAllocationGuardTest rtAllocationGuardTest;
    static RTBufferPreallocTest rtBufferPreallocTest;
    static RTCleanPreHighPassTest rtCleanPreHighPassTest;
    static RTCleanSubGuardTest rtCleanSubGuardTest;
    static RTCleanOversamplingTest rtCleanOversamplingTest;
    static RTCleanToneSweepTest rtCleanToneSweepTest;
    static RTCleanSampleRateDriftTest rtCleanSampleRateDriftTest;
    static LinearPhaseDryTest linearPhaseDryTest;
    static CoefficientPropagationTest coefficientPropagationTest;
    static FastMathAccuracyTest fastMathAccuracyTest;
    static ProcessBlockDecompTest processBlockDecompTest;
    static StateIOTests stateIOTests;
    static GoldenAudioTests goldenAudioTests;
    static NormalizationTests normalizationTests;
    static StatefulDistortionTests statefulDistortionTests;
    static SubGuardFlatnessTest subGuardFlatnessTest;
    static InputFilterModeTest inputFilterModeTest;
}

#endif // JUCE_DEBUG
