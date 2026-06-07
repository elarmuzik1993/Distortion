/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <memory>

#include <JuceHeader.h>
#include <juce_dsp/juce_dsp.h>
#include "LinearRamp.h"

//==============================================================================
// DSP Constants - Centralized configuration for audio processing algorithms
//==============================================================================
namespace DSPConstants
{
    // Sub Guard crossover configuration (replaces 808-Safe mode)
    constexpr float SUBGUARD_FREQ_MAX = 200.0f;               // Maximum crossover frequency
    constexpr float SUBGUARD_FREQ_DEFAULT = 60.0f;            // Default to 60Hz LR24 (sub-preserving, Saturn 2 style)
    constexpr float SUBGUARD_CROSSFADE_TIME_S = 0.010f;       // 10ms order transition crossfade
    constexpr float SUBGUARD_FREQ_SMOOTH_TIME_S = 0.050f;     // 50ms frequency smoothing

    // Manual one-pole DC blocker corner (applied after downsampling, at base rate).
    // Kept constant across sample rates by deriving R = exp(-2*pi*fc/fs) per rate.
    constexpr float DC_BLOCKER_CUTOFF_HZ = 3.5f;             // ~R=0.9995 at 44.1kHz

    // LA-2A Compressor optical cell simulation (time constants in seconds)
    constexpr float COMP_ATTACK_TIME_S = 0.010f;              // 10ms attack (fast optical response)
    constexpr float COMP_RELEASE_TIME_S = 0.500f;             // 500ms release (slow optical decay)
    constexpr float COMP_KNEE_WIDTH_DB = 2.0f;                // Soft knee for smooth compression
    constexpr float COMP_THRESHOLD_MIN_DB = -60.0f;           // Minimum threshold
    constexpr float COMP_THRESHOLD_RANGE_DB = 60.0f;          // Full range: -60dB to 0dB
    constexpr float COMP_MAKEUP_RANGE_DB = 12.0f;             // ±12dB makeup gain range
    constexpr float COMP_RATIO_COMPRESS = 3.0f;               // 3:1 compression ratio
    constexpr float COMP_RATIO_LIMIT = 12.0f;                 // 12:1 limiting ratio
    constexpr float COMP_RMS_HISTORY_TIME_S = 0.100f;         // 100ms RMS smoothing for program-dependent behavior
    constexpr float COMP_TUBE_BLEND = 0.15f;                  // 15% tube harmonic blend
    constexpr float COMP_TUBE_DRIVE = 1.5f;                   // Tube saturation drive amount

    // Compressor output soft clip (replaces blanket tanh)
    constexpr float COMP_SOFT_CLIP_THRESHOLD = 0.891f;        // -1dBFS threshold (transparent below)
    constexpr float COMP_SOFT_CLIP_HEADROOM = 0.109f;         // Remaining headroom to 1.0

    // Default parameter values
    constexpr float DEFAULT_HIPASS_FREQ = 20.0f;              // Default hi-pass filter frequency (subsonic only)

    // Oscilloscope configuration
    constexpr int SCOPE_BUFFER_SIZE = 2048;                   // Circular buffer size for waveform display
    constexpr int SCOPE_UPDATE_DECIMATION = 2;                // Update every 2 samples to reduce CPU
    constexpr int SCOPE_DISPLAY_POINTS = 512;                 // Number of points to draw
    constexpr int SCOPE_TRIGGER_MARGIN = 256;                 // Extra samples for trigger search
    constexpr int SCOPE_REFRESH_RATE_HZ = 30;                 // UI refresh rate

    // Gain reduction meter
    constexpr int METER_REFRESH_RATE_HZ = 30;                 // UI refresh rate
    constexpr float METER_MAX_DB = 20.0f;                     // Maximum gain reduction display range

    // Parameter smoothing times (in seconds)
    constexpr double GAIN_SMOOTH_TIME_S = 0.02;               // 20ms for gain changes

    // Pre-distortion transient tamer (hardcoded, always-on)
    constexpr float PRE_COMP_ATTACK_TIME_S = 0.001f;         // 1ms attack (catches transients)
    constexpr float PRE_COMP_RELEASE_TIME_S = 0.050f;        // 50ms release (preserves punch)
    constexpr float PRE_COMP_THRESHOLD_DB = -12.0f;          // -12dB threshold
    constexpr float PRE_COMP_RATIO = 2.5f;                   // 2.5:1 ratio (light compression)
    constexpr float PRE_COMP_KNEE_DB = 6.0f;                 // 6dB soft knee

    // Soft clipper for ISP protection (before downsampling)
    constexpr float SOFT_CLIP_THRESHOLD_DB = -0.3f;          // -0.3 dBFS ceiling
    constexpr float SOFT_CLIP_KNEE_DB = 0.5f;                // 0.5 dB soft knee

    // Sub-linear harmonic density control (always-on for clip types 1,3,4)
    // Prevents 2-5 kHz harshness at high input levels
    constexpr float HARMONIC_DENSITY_ATTACK_TIME_S = 0.002f;   // 2ms attack (fast response)
    constexpr float HARMONIC_DENSITY_RELEASE_TIME_S = 0.030f;  // 30ms release (smooth decay)
    constexpr float HARMONIC_DENSITY_EPSILON = 0.01f;          // Minimum threshold for sqrt
    constexpr float HARMONIC_DENSITY_MIN_SCALE = 0.1f;         // 10% minimum harmonic strength

    // Auto-gain compensation (maintains consistent perceived loudness)
    constexpr float AUTO_GAIN_ATTACK_TIME_S = 0.005f;          // 5ms attack (track transients)
    constexpr float AUTO_GAIN_RELEASE_TIME_S = 0.100f;         // 100ms release (prevent pumping)
    constexpr float AUTO_GAIN_MIN = 0.1f;                      // -20dB minimum compensation
    constexpr float AUTO_GAIN_MAX = 4.0f;                      // +12dB maximum compensation

    // Clean Boost (toggle): level lift + high-shelf pre-emphasis in front of the
    // distortion, with a complementary high-shelf de-emphasis after the clipper.
    // Suppressing lows entering the nonlinearity reduces intermodulation distortion;
    // the de-emphasis restores spectral balance so only the saturation character changes.
    constexpr float CLEAN_BOOST_DB = 6.0f;                     // broadband level lift into the clipper
    constexpr float CLEAN_BOOST_EMPH_FREQ = 700.0f;            // high-shelf corner (Tube-Screamer territory)
    constexpr float CLEAN_BOOST_EMPH_DB = 5.0f;                // shelf tilt depth (pre boost / post cut)
    constexpr float CLEAN_BOOST_Q = 0.7071067811865476f;       // shelf Q
    constexpr double CLEAN_BOOST_SMOOTH_TIME_S = 0.02;         // 20ms click-free toggle morph

    // Tube bias envelope (simulates cathode bias shift under sustained signal)
    constexpr float TUBE_BIAS_ATTACK_TIME_S = 0.003f;         // 3ms attack
    constexpr float TUBE_BIAS_RELEASE_TIME_S = 0.080f;        // 80ms release
    constexpr float TUBE_BIAS_MOD_DEPTH = 0.15f;              // Modulates clip threshold 0.85→0.70

    // Tape hysteresis envelope (simulates magnetic saturation stiffening)
    constexpr float TAPE_HYSTERESIS_ATTACK_TIME_S = 0.005f;   // 5ms attack
    constexpr float TAPE_HYSTERESIS_RELEASE_TIME_S = 0.100f;  // 100ms release
    constexpr float TAPE_HYSTERESIS_MOD_DEPTH = 0.2f;         // Modulates compression knee 0.7→0.5

    // Clip type output normalization (calibrated for equal RMS at drive=3.0, input=0.5)
    // These values compensate for level differences between algorithms
    constexpr float CLIP_NORM_BRUTAL_FUZZ = 0.82f;        // Type 0: loudest, needs attenuation
    constexpr float CLIP_NORM_TUBE_OVERDRIVE = 1.05f;     // Type 1: slightly quiet
    constexpr float CLIP_NORM_BIT_CRUSHER = 0.90f;        // Type 2: moderate
    constexpr float CLIP_NORM_TAPE_SATURATION = 1.00f;    // Type 3: reference level
    constexpr float CLIP_NORM_TRANSFORMER = 0.88f;        // Type 4: slightly hot
    constexpr float CLIP_NORM_DIODE_CLIPPER = 1.30f;      // Type 5: quietest, needs boost
    constexpr float CLIP_NORM_DECIMATOR = 0.95f;          // Type 6: close to reference

    // Output limiter (final safety, always-on, stereo-linked)
    constexpr float OUTPUT_LIMITER_THRESHOLD_DB = -0.5f;       // -0.5 dBFS ceiling (safe headroom)
    constexpr float OUTPUT_LIMITER_ATTACK_TIME_S = 0.0005f;    // 0.5ms attack (catch transients)
    constexpr float OUTPUT_LIMITER_RELEASE_TIME_S = 0.050f;    // 50ms release (preserve punch)
    constexpr float OUTPUT_LIMITER_KNEE_DB = 1.0f;             // 1dB soft knee (transparent onset)
}

// Forward declarations for test classes
#if JUCE_DEBUG
class DistortionDSPTests;
class CompressionDSPTests;
class CleanBoostTests;
class LFOTests;
class ProcessBlockTests;
class ParameterTests;
class SampleRateTests;
class ThreadSafetyTests;
class StateIOTests;
class GoldenAudioTests;
class HarmonicDensityTests;
class OutputLimiterTests;
class NormalizationTests;
class StatefulDistortionTests;
class DryWetAlignmentTests;
class RTCleanOversamplingTest;
class CoefficientPropagationTest;
class ProcessBlockDecompTest;
#endif

class PluginProcessor : public juce::AudioProcessor,
                        private juce::AsyncUpdater,
                        private juce::AudioProcessorValueTreeState::Listener
{
#if JUCE_DEBUG
    // Grant test classes access to private members for unit testing
    friend class DistortionDSPTests;
    friend class CompressionDSPTests;
    friend class CleanBoostTests;
    friend class LFOTests;
    friend class LFOBpmSyncTests;
    friend class ProcessBlockTests;
    friend class ParameterTests;
    friend class SampleRateTests;
    friend class ThreadSafetyTests;
    friend class StateIOTests;
    friend class GoldenAudioTests;
    friend class HarmonicDensityTests;
    friend class OutputLimiterTests;
    friend class NormalizationTests;
    friend class StatefulDistortionTests;
    friend class DryWetAlignmentTests;
    friend class RTCleanOversamplingTest;
    friend class CoefficientPropagationTest;
    friend class ProcessBlockDecompTest;
#endif

public:
    //==============================================================================
    PluginProcessor();
    ~PluginProcessor() override;

    //==============================================================================
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;


#ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
#endif

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // State schema versioning. Bump currentStateVersion whenever the serialized
    // layout changes in a way that needs a migration, and add the corresponding
    // step in migrateState(). State saved before this stamp existed carries no
    // attribute and is treated as version 0 (the bare/legacy format).
    static constexpr const char* stateVersionAttribute = "stateVersion";
    static constexpr int currentStateVersion = 1;

    // Reads the schema version from a freshly-restored state element and applies
    // any migrations needed to bring the live parameter tree up to
    // currentStateVersion, then stamps the tree with the current version. Must be
    // called AFTER replaceState so the parameters being migrated exist.
    void migrateState(const juce::XmlElement& xmlState);

    //==============================================================================
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    juce::AudioProcessorValueTreeState parameters;
    void fillScopeBuffer(juce::AudioBuffer<float>& destBuffer);
    void pushSampleToScope(float left, float right);
    void applyLA2ACompression(juce::AudioBuffer<float>& buffer,
        float peakReduction,
        float makeupGain,
        int ratioMode);
    void requestOversamplingRebuild(int stages);

    // Atomic gain reduction for UI meter (in dB) - public for UI access
    std::atomic<float> currentGainReductionDB{ 0.0f };
    std::atomic<float> phaseCorrelation{ 1.0f };  // Phase correlation for UI meter (-1.0 to +1.0)
    std::atomic<float> lfoPhaseForUI{ 0.0f };     // LFO phase (0-1) for UI arc animation

    // Real-time safe debug flags (atomic, no logging in audio thread) - public for test access
    std::atomic<bool> debugHadNaN{false};
    std::atomic<bool> debugHadBufferOverflow{false};
    std::atomic<bool> debugHadDistortionCorruption{false};
    std::atomic<bool> debugHadUnexpectedSampleRateChange{false};

    // Sub Guard filter order enum (public for method signatures)
    enum class SubGuardFilterOrder { LR12, LR18, LR24 };

private:
    // Sub Guard helper methods
    SubGuardFilterOrder determineSubGuardFilterOrder(float freq) const;
    void updateSubGuardCoefficients(float freq, double sampleRate);
    // Filter lowBuf/highBuf in place through the given order's bank and bake the
    // LR12 flat-sum polarity (inverts the high band for 2nd-order Linkwitz-Riley).
    void filterSubGuardBands(SubGuardFilterOrder order,
                             juce::AudioBuffer<float>& lowBuf,
                             juce::AudioBuffer<float>& highBuf);
    // Clear just one order's filter state (incoming bank at a crossfade start).
    void resetSubGuardOrderFilters(SubGuardFilterOrder order);

    // Clean Boost helpers
    void updateCleanBoostCoefficients(float depth, double sampleRate);
    void applyCleanBoostEmphasis();
    void applyCleanBoostDeEmphasis();

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
    size_t oversamplingFactor = 4;
    int currentNumChannels = 0;

    // Input multimode filter (high-pass / low-pass / band-pass). State-variable TPT
    // topology: one structure switches type cleanly and stays stable under LFO sweeps.
    juce::dsp::StateVariableTPTFilter<float> inputFilter;

    // Manual DC blocker state (simple one-pole, extremely stable)
    // y[n] = x[n] - x[n-1] + R * y[n-1]; R is derived from the sample rate so the
    // ~3.5Hz corner stays constant at every rate (see updateSampleRateDependentCoefficients).
    float manualDCBlockerPrevInput[2] = { 0.0f, 0.0f };
    float manualDCBlockerPrevOutput[2] = { 0.0f, 0.0f };
    float dcBlockerR = 0.9995f;  // ~3.5Hz at 44.1kHz; recomputed per sample rate

    // Sub Guard LR24 filters (4th order = 2 cascaded 2nd-order stages).
    // Coefficient updates mutate .state in place (see updateSubGuardCoefficients).
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
        juce::dsp::IIR::Coefficients<float>> lowPassFilter1;
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
        juce::dsp::IIR::Coefficients<float>> lowPassFilter2;
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
        juce::dsp::IIR::Coefficients<float>> highPassFilter1;
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
        juce::dsp::IIR::Coefficients<float>> highPassFilter2;

    // Sub Guard LR12 filters (2nd order = single stage)
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
        juce::dsp::IIR::Coefficients<float>> subGuardLP12;
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
        juce::dsp::IIR::Coefficients<float>> subGuardHP12;

    // Sub Guard LR18 filters (1st + 2nd order cascaded = 3rd order approximation)
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
        juce::dsp::IIR::Coefficients<float>> subGuardLP18_1;  // First order
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
        juce::dsp::IIR::Coefficients<float>> subGuardLP18_2;  // Second order
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
        juce::dsp::IIR::Coefficients<float>> subGuardHP18_1;  // First order
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
        juce::dsp::IIR::Coefficients<float>> subGuardHP18_2;  // Second order

    // Sub Guard state tracking
    SubGuardFilterOrder currentSubGuardOrder = SubGuardFilterOrder::LR24;
    juce::SmoothedValue<float> smoothedSubGuardFreq;
    float lastSubGuardFreq = -1.0f;

    // Sub Guard order-change crossfade. When the slope order switches (zone boundary),
    // both the outgoing and incoming banks run for SUBGUARD_CROSSFADE_TIME_S and their
    // band-split outputs are linearly blended, so the switch is click-free.
    bool  sgCrossfadeActive = false;
    int   sgCrossfadePos    = 0;           // samples elapsed in the current crossfade
    SubGuardFilterOrder sgFromOrder = SubGuardFilterOrder::LR24;
    SubGuardFilterOrder sgToOrder   = SubGuardFilterOrder::LR24;
    bool  sgWasActive       = false;       // Sub Guard active last block? (OFF->ON snaps, no crossfade)

    // Post-distortion tone filter (oversampled rate)
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
        juce::dsp::IIR::Coefficients<float>> toneFilter;

    // Phase-matching mirror of toneFilter applied to the Sub Guard low branch
    // before recombine, so both bands share the same magnitude/phase response.
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
        juce::dsp::IIR::Coefficients<float>> toneFilterLow;

    // Clean Boost pre-emphasis (high-shelf boost) before distortion and the
    // complementary de-emphasis (high-shelf cut) after. Coefficients mutate
    // .state in place from a smoothed depth (see updateCleanBoostCoefficients).
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
        juce::dsp::IIR::Coefficients<float>> emphasisFilter;
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
        juce::dsp::IIR::Coefficients<float>> deEmphasisFilter;
    juce::SmoothedValue<float> smoothedBoostDepth;  // 0=off, 1=full boost (morphs the toggle)
    float lastBoostDepth = -1.0f;

    juce::AudioBuffer<float> lowBandBuffer;   // For clean low frequencies
    juce::AudioBuffer<float> highBandBuffer;  // For distorted high frequencies
    // Secondary band buffers — hold the incoming order's split during an order crossfade.
    juce::AudioBuffer<float> lowBandBufferB;
    juce::AudioBuffer<float> highBandBufferB;
    juce::AudioBuffer<float> dryBuffer;       // For global wet/dry mix

    // Phase-matched dry path for the global wet/dry mix. The dry is routed through a
    // second oversampler (identical config, no inner processing: up then down) so it
    // picks up the SAME allpass phase and latency as the wet path. A plain fractional
    // delay cannot align the minimum-phase IIR oversampler and comb-filters the blend
    // at partial mix; routing the dry through a matched oversampler aligns every
    // frequency, for both the IIR and FIR (linear-phase) oversampling modes.
    std::unique_ptr<juce::dsp::Oversampling<float>> dryOversampling;

    // Re-imposes the reported oversampler latency onto the cheap true-bypass branch
    // (which skips the oversampler). Without it the host's PDC plays the bypassed
    // signal early, and crossing the bypass<->active threshold jumps in time. None
    // interpolation makes it a pure integer delay — bit-transparent, only
    // time-shifted. Sized/updated in rebuildOversampling (message thread).
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None> bypassLatencyDelay;
    int bypassLatencySamples = 0;

    juce::SmoothedValue<float> smoothedOutputGain;  // Only output gain uses SmoothedValue (normal rate)
    juce::SmoothedValue<float> smoothedGlobalMix;

    // Thread safety for state persistence
    std::atomic<bool> stateNeedsReset{false};

public:
    // Oversampling runtime control (set by editor, read by audio thread)
    std::atomic<int> requestedOversamplingStages{2};     // 0=Off, 1=2x, 2=4x

private:
    int currentOversamplingStages = 2;  // Track current stages for comparison
    bool currentLinearPhase = false;   // Track filter type for needsRebuild check
    void rebuildOversampling(double sampleRate, int samplesPerBlock);
    void handleAsyncUpdate() override;

    // APVTS listener: linearPhaseDry changes the oversampler filter type, so a
    // change from any source (host automation, preset load, UI) must rebuild the
    // oversampler — not just a manual editor click. Called synchronously on the
    // thread that changes the parameter (often the audio thread), so the handler
    // must stay RT-safe and only defer work via triggerAsyncUpdate().
    void parameterChanged(const juce::String& parameterID, float newValue) override;

    std::atomic<float>* inputGainParam = nullptr;
    std::atomic<float>* outputGainParam = nullptr;
    std::atomic<float>* distortionAmountParam = nullptr;
    std::atomic<float>* highPassFreqParam = nullptr;
    std::atomic<float>* filterModeParam = nullptr;
    std::atomic<float>* clipTypeParam = nullptr;
    std::atomic<float>* subGuardFreqParam = nullptr;  // Sub Guard crossover frequency (50-200Hz)
    std::atomic<float>* lfoRateParam = nullptr;
    std::atomic<float>* lfoDepthParam = nullptr;
    std::atomic<float>* lfoWaveformParam = nullptr;  // LFO waveform type
    std::atomic<float>* lfoEnabledParam = nullptr;   // LFO on/off toggle
    std::atomic<float>* lfoDestinationParam = nullptr;  // LFO destination (0-4: Dist, Tone, Hi-Pass, Mix, Gain)
    std::atomic<float>* lfoBpmSyncParam = nullptr;       // BPM sync toggle
    std::atomic<float>* lfoBpmDivisionParam = nullptr;   // Note division when BPM sync is ON
    std::atomic<float>* lfoInvertParam = nullptr;        // Invert LFO polarity
    std::atomic<float>* waveshaperMixParam = nullptr;  // Waveshaper wet/dry mix (0-100)

    // Compressor parameters
    std::atomic<float>* compPeakReductionParam = nullptr;
    std::atomic<float>* compMakeupGainParam = nullptr;
    std::atomic<float>* compRatioParam = nullptr;
    std::atomic<float>* compEnabledParam = nullptr;
    std::atomic<float>* autoGainEnabledParam = nullptr;
    std::atomic<float>* extremeEnabledParam = nullptr;
    std::atomic<float>* globalMixParam = nullptr;

    std::atomic<float>* distMixParam = nullptr;         // Distortion wet/dry mix (0-100)
    std::atomic<float>* toneParam = nullptr;            // Post-distortion tone (2000-20000Hz)
    std::atomic<float>* waveshaperCleanParam = nullptr; // 0=Gritty (tone→waveshaper), 1=Clean (waveshaper→tone)
    std::atomic<float>* linearPhaseDryParam = nullptr;  // FIR linear-phase oversampling toggle (default OFF)
    std::atomic<float>* cleanBoostParam = nullptr;      // Clean boost on/off (pre-emphasis into distortion)

    // Compressor state variables (LA-2A optical cell simulation)
    // Optical cell envelope follower (T4 cell)
    float compEnvelopeState = 1.0f;


    // RMS detection for program-dependent behavior
    float compRmsHistory = 0.0f;

    // Tube harmonic state
    float tubeWarmth = 0.0f;

    float lfoPhase = 0.0f;
    float currentSampleRate = 44100.0f;

    // Per-instance random generators (NOT static to avoid multi-instance bugs)
    juce::Random distortionRandom;
    juce::Random waveshaperRandom;

    // LFO Random waveform state (per-instance, not static)
    float lfoRandomValue = 0.0f;
    float lfoLastPhase = 1.0f;

    // Compression optical cell coefficients (sample-rate-dependent)
    float compAttackCoeff = 0.9995f;
    float compReleaseCoeff = 0.99995f;
    float compRmsHistoryCoeff = 0.99f;

    // LA-2A coefficients for OVERSAMPLED domain
    float compAttackCoeffOversampled = 0.9995f;
    float compReleaseCoeffOversampled = 0.99995f;
    float compRmsHistoryCoeffOversampled = 0.99f;

    // Pre-distortion compression state (per-channel for stereo imaging)
    float preCompEnvelope[2] = { 1.0f, 1.0f };
    float preCompAttackCoeff = 0.0f;
    float preCompReleaseCoeff = 0.0f;

    // Sub-linear harmonic density envelope (per-channel for stereo imaging)
    float harmonicDensityEnvelope[2] = { 0.0f, 0.0f };
    float harmonicDensityAttackCoeff = 0.0f;
    float harmonicDensityReleaseCoeff = 0.0f;

    // Auto-gain compensation envelopes (RMS tracking)
    float autoGainInputEnvelope = 0.0f;    // Smoothed input RMS
    float autoGainOutputEnvelope = 0.0f;   // Smoothed output RMS
    float autoGainCompensation = 1.0f;     // Current compensation gain
    float autoGainAttackCoeff = 0.0f;      // Attack coefficient
    float autoGainReleaseCoeff = 0.0f;     // Release coefficient

    // Output limiter state (stereo-linked for image preservation)
    float outputLimiterEnvelope = 1.0f;    // Gain reduction envelope (1.0 = no limiting)
    float outputLimiterAttackCoeff = 0.0f; // Attack coefficient
    float outputLimiterReleaseCoeff = 0.0f; // Release coefficient

    // Stateful distortion envelopes (per-channel for stereo imaging)
    float tubeBiasEnvelope[2] = { 0.0f, 0.0f };           // Tube cathode bias shift
    float tapeSaturationEnvelope[2] = { 0.0f, 0.0f };     // Tape magnetic hysteresis
    float tubeBiasAttackCoeff = 0.0f;
    float tubeBiasReleaseCoeff = 0.0f;
    float tapeHysteresisAttackCoeff = 0.0f;
    float tapeHysteresisReleaseCoeff = 0.0f;

    // Cached filter parameters to avoid unnecessary coefficient updates
    float lastHighPassFreq = -1.0f;
    float lastToneFreq = -1.0f;
    double lastSampleRate = 0.0;  // Track sample rate changes
    double lastOversampledSampleRate = 0.0;  // Track oversampled rate for distortion filters

    // Per-sample linear interpolators for the oversampled-domain hot loop
    // (replace the old lastFoo/fooDelta/currentFoo triads — see LinearRamp.h).
    LinearRamp<float> inputGainRamp { 1.0f };
    LinearRamp<float> driveRamp     { 1.0f };
    LinearRamp<float> distMixRamp   { 1.0f };

    juce::AudioBuffer<float> scopeBuffer;
    juce::AbstractFifo scopeFifo;

    // -------------------------------------------------------------------------
    // Block-scope transient state — set in processBlock preamble; consumed by
    // the stage helper methods.  Valid only during a processBlock call.
    // -------------------------------------------------------------------------
    juce::dsp::AudioBlock<float> pb_inputBlock;          // view into the host buffer
    juce::dsp::AudioBlock<float> pb_oversampledBlock;    // view into the oversampler's buffer
    size_t pb_numSamples  = 0;
    size_t pb_numChannels = 0;
    double pb_oversampledSR = 0.0;

    float pb_modulatedHighPassFreq    = 0.0f;
    int   pb_filterMode               = 0;   // 0=High Pass, 1=Low Pass, 2=Band Pass
    float pb_modulatedDistortionParam = 0.0f;
    float pb_modulatedToneFreq        = 0.0f;
    float pb_distortionParam          = 0.0f;  // raw (pre-LFO) for per-sample modulation
    float pb_distMix                  = 0.0f;  // raw (pre-LFO) for per-sample modulation
    bool  pb_extremeEnabled           = false;
    bool  pb_autoGainEnabled          = false;
    bool  pb_compEnabled              = false;
    float pb_compPeakReduction        = 0.0f;
    float pb_compMakeupGain           = 0.0f;
    int   pb_compRatioMode            = 0;
    float pb_lfoPhaseIncrement        = 0.0f;
    bool  pb_perSampleLFO             = false;
    bool  pb_lfoEnabled               = false;
    int   pb_lfoWaveform              = 0;
    float pb_lfoDepth                 = 0.0f;
    int   pb_lfoDestination           = 0;
    float pb_lfoSign                  = 1.0f;  // 1 or -1 depending on lfoInvert
    int   pb_clipType                 = 0;
    float pb_subGuardFreq             = 0.0f;
    float pb_outGainParam             = 50.0f;  // raw, for output gain LFO modulation
    bool  pb_subGuardActive           = false;  // set by applySubGuardSplit
    bool  pb_cleanBoostOn             = false;  // boost requested this block (gated with distortion active)
    bool  pb_boostProcessedThisBlock  = false;  // emphasis ran → de-emphasis must run too (pairs the shelves)
    // (Per-sample interpolation moved to inputGainRamp/driveRamp/distMixRamp — see LinearRamp.h)

    // Stage helper methods extracted from processBlock (PR-8)
    void applyPreHighpass(juce::AudioBuffer<float>& buffer);
    void applyInputFilter(juce::dsp::AudioBlock<float>& block);  // base-rate multimode filter
    bool isInputFilterActive() const;                            // true when not transparent
    void applyPreCompression();
    bool applySubGuardSplit();  // returns false to abort processBlock (band buffer overflow)
    // Per-sample distortion: harmonic-density envelope + studio distortion + wet/dry mix.
    // Called from both applySubGuardSplit branches (OFF: full-range; ACTIVE: high band only).
    // All per-sample params are explicit so the caller controls ramp advancement.
    float applyDistortionStage(float inputSample, int channel,
                               float currentGain, float sampleDrive,
                               float sampleMixAmount, float sampleDistortionParam);
    void applyAutoGainAndISP(juce::AudioBuffer<float>& buffer);
    void applyLA2A();
    // Stereo-linked soft safety limiter at -0.5dBFS. Must run as the LAST gain stage
    // on the output buffer (after the global dry/wet blend) so a hot dry signal can't
    // push the blended output past the ceiling. Shared by the bypass and active paths.
    void applyFinalLimiter(juce::AudioBuffer<float>& buffer);

    // Helper methods for studio distortion DSP
    float applyStudioDistortion(float x, float gain, float drive, int clipType, float harmonicScale, int channel = 0);
    void updateSampleRateDependentCoefficients(double sampleRate);
    float generateLFOWaveform(float phase, int waveformType);  // Generate LFO waveforms
    void resetDSPState();  // Thread-safe DSP state reset (called from audio thread)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginProcessor)
};
