#pragma once

#if JUCE_DEBUG

#include <JuceHeader.h>
#include "../PluginProcessor.h"
#include "../FastMath.h"
#include "TestUtilities.h"
#include "../Diagnostics/Report.h"
#include "../Diagnostics/ReportStore.h"
#include "../RTAllocationGuard.h"
#include "../Diagnostics/DiagnosticsSink.h"
#include "../Diagnostics/ReportComposer.h"

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
    void testIirPartialMixCoherence();
    void testBypassLatencyCompensation();
    void testDryOversamplerStaysWarm();
    void testResetClearsWetOversampler();
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

// Verifies the Sub Guard band-split sums flat (no notch/dip) at the crossover for
// every filter order: LR12 (2nd-order LR, needs the high-band polarity flip),
// LR18 (3rd-order Butterworth, Q=1.0), and LR24 (4th-order LR).
class SubGuardCrossoverFlatnessTest : public juce::UnitTest
{
public:
    SubGuardCrossoverFlatnessTest() : UnitTest("Sub Guard Crossover Flatness", TestCategories::DSP) {}
    void runTest() override;
};

// Verifies that sweeping the Sub Guard crossover frequency across the slope-order
// boundaries (92 Hz, 142 Hz) is click-free thanks to the order crossfade.
class SubGuardOrderCrossfadeTest : public juce::UnitTest
{
public:
    SubGuardOrderCrossfadeTest() : UnitTest("Sub Guard Order Crossfade", TestCategories::DSP) {}
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
    void testVersionStamp();
    void testLegacyMigration();
};

//==============================================================================
// Factory Preset Tests
//==============================================================================

/** Tests for the shared factory preset bank (FactoryPresets.h) */
class FactoryPresetTests : public juce::UnitTest
{
public:
    FactoryPresetTests() : UnitTest("Factory Presets", TestCategories::StateIO) {}
    void runTest() override;

private:
    void testBankSize();
    void testParamIdsExist();
    void testPresetsProduceFiniteOutput();
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
    void testGlobalMixCeiling();
    void testFirstSampleCeiling();
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

class DiagReportJsonTest : public juce::UnitTest
{
public:
    DiagReportJsonTest() : juce::UnitTest ("Diagnostics Report JSON", "Diagnostics") {}
    void runTest() override
    {
        beginTest ("round-trips all fields");
        diag::Report r;
        r.trigger = "user"; r.installId = "abc-123"; r.pluginVersion = "v9.9";
        r.os = "Linux"; r.hostWrapper = "VST3"; r.hostName = "Reaper";
        r.sampleRate = 48000.0; r.blockSize = 512;
        r.nonFiniteBlocks = 3; r.totalBlocks = 1000;
        r.message = "it broke"; r.createdUtc = "2026-06-17T00:00:00Z";

        bool ok = false;
        auto back = diag::Report::fromJson (r.toJson(), ok);
        expect (ok, "fromJson failed to parse");
        expectEquals (back.trigger, r.trigger);
        expectEquals (back.installId, r.installId);
        expectEquals (back.sampleRate, r.sampleRate);
        expectEquals ((int) back.nonFiniteBlocks, (int) r.nonFiniteBlocks);
        expectEquals (back.message, r.message);
        expectEquals (back.schema, r.schema);
        expectEquals (back.pluginVersion, r.pluginVersion);
        expectEquals (back.os, r.os);
        expectEquals (back.hostWrapper, r.hostWrapper);
        expectEquals (back.hostName, r.hostName);
        expectEquals (back.blockSize, r.blockSize);
        expectEquals ((juce::int64) back.totalBlocks, (juce::int64) r.totalBlocks);
        expectEquals (back.createdUtc, r.createdUtc);

        beginTest ("garbage input fails cleanly");
        bool ok2 = true;
        diag::Report::fromJson ("not json {", ok2);
        expect (! ok2, "garbage should set ok=false");
    }
};

class DiagReportStoreTest : public juce::UnitTest
{
public:
    DiagReportStoreTest() : juce::UnitTest ("Diagnostics ReportStore", "Diagnostics") {}
    void runTest() override
    {
        auto tmp = juce::File::createTempFile ("diagq");
        tmp.deleteFile();
        tmp.createDirectory();

        diag::ReportStore store (tmp);

        beginTest ("enqueue writes a pending file");
        diag::Report r; r.trigger = "auto"; r.installId = "x";
        auto f = store.enqueue (r);
        expect (f.existsAsFile(), "enqueue did not create file");
        expectEquals (store.listPending().size(), 1);

        beginTest ("claim hides file from listPending, revert restores");
        auto claimed = store.claim (f);
        expect (claimed.existsAsFile(), "claim target missing");
        expectEquals (store.listPending().size(), 0);
        store.revert (claimed);
        expectEquals (store.listPending().size(), 1);

        beginTest ("remove deletes");
        store.remove (store.listPending()[0]);
        expectEquals (store.listPending().size(), 0);

        beginTest ("prune enforces max file cap (drops oldest)");
        for (int i = 0; i < diag::ReportStore::maxFiles + 10; ++i)
            store.enqueue (r);
        expect (store.listPending().size() <= diag::ReportStore::maxFiles,
                "prune did not cap the queue");

        beginTest ("recoverStaleClaims reverts orphaned .sending files");
        for (auto& g : store.listPending()) store.remove (g);   // clear
        auto orphan = store.enqueue (r);
        store.claim (orphan);                                    // .sending with no drainer
        expectEquals (store.listPending().size(), 0);
        store.recoverStaleClaims();
        expectEquals (store.listPending().size(), 1);

        beginTest ("prune drops files older than maxAgeDays");
        for (auto& g : store.listPending()) store.remove (g);          // clear
        auto old = store.enqueue (r);
        old.setLastModificationTime (juce::Time::getCurrentTime()
                                       - juce::RelativeTime::days (diag::ReportStore::maxAgeDays + 1));
        auto fresh = store.enqueue (r);   // enqueue() runs prune(), which should drop `old`
        expect (! old.existsAsFile(),   "stale file should be pruned by age cap");
        expect (fresh.existsAsFile(),   "fresh file should survive age cap");

        tmp.deleteRecursively();
    }
};

class DiagSinkComposerTest : public juce::UnitTest
{
public:
    DiagSinkComposerTest() : juce::UnitTest ("Diagnostics Sink+Composer", "Diagnostics") {}
    void runTest() override
    {
        beginTest ("noteBlock is allocation-free (RT-safe)");
        diag::DiagnosticsSink sink;
        rt_guard::resetAllocationCounter();
        {
            rt_guard::ScopedRTAssert scope;
            for (int i = 0; i < 1000; ++i)
                sink.noteBlock (i % 100 != 0);   // 1% non-finite
        }
        expectEquals (rt_guard::getAllocationCount(), 0, "noteBlock allocated on audio thread");
        expect (sink.hasAnomalies(), "should have flagged anomalies");
        expectEquals ((int) sink.snapshot().nonFiniteBlocks, 10);
        expectEquals ((int) sink.snapshot().totalBlocks, 1000);

        beginTest ("reset clears counters");
        sink.reset();
        expect (! sink.hasAnomalies(), "reset failed");
        expectEquals ((int) sink.snapshot().totalBlocks, 0);

        beginTest ("composer populates report from snapshot");
        diag::DiagnosticsSink::Summary sum { 3, 500 };
        auto r = diag::ReportComposer::compose ("auto", "", sum, 48000.0, 256,
                                                juce::AudioProcessor::wrapperType_VST3, "id-1");
        expectEquals (r.trigger, juce::String ("auto"));
        expectEquals (r.installId, juce::String ("id-1"));
        expectEquals (r.sampleRate, 48000.0);
        expectEquals ((int) r.nonFiniteBlocks, 3);
        expect (r.os.isNotEmpty(), "os should be filled");
        expect (r.createdUtc.isNotEmpty(), "timestamp should be filled");
        expectEquals (r.hostWrapper, juce::String ("VST3"));

        beginTest ("loadOrCreateInstallId is stable across calls");
        auto idFile = juce::File::createTempFile ("iid");
        idFile.deleteFile();
        auto id1 = diag::ReportComposer::loadOrCreateInstallId (idFile);
        auto id2 = diag::ReportComposer::loadOrCreateInstallId (idFile);
        expect (id1.isNotEmpty(), "id should be generated");
        expectEquals (id1, id2);
        idFile.deleteFile();
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
    static SubGuardCrossoverFlatnessTest subGuardCrossoverFlatnessTest;
    static SubGuardOrderCrossfadeTest subGuardOrderCrossfadeTest;
    static RTCleanOversamplingTest rtCleanOversamplingTest;
    static RTCleanToneSweepTest rtCleanToneSweepTest;
    static RTCleanSampleRateDriftTest rtCleanSampleRateDriftTest;
    static LinearPhaseDryTest linearPhaseDryTest;
    static CoefficientPropagationTest coefficientPropagationTest;
    static FastMathAccuracyTest fastMathAccuracyTest;
    static ProcessBlockDecompTest processBlockDecompTest;
    static StateIOTests stateIOTests;
    static FactoryPresetTests factoryPresetTests;
    static GoldenAudioTests goldenAudioTests;
    static NormalizationTests normalizationTests;
    static StatefulDistortionTests statefulDistortionTests;
    static SubGuardFlatnessTest subGuardFlatnessTest;
    static InputFilterModeTest inputFilterModeTest;
    static DiagReportJsonTest diagReportJsonTest;
    static DiagReportStoreTest diagReportStoreTest;
    static DiagSinkComposerTest diagSinkComposerTest;
}

#endif // JUCE_DEBUG
