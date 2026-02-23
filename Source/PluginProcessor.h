/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <juce_dsp/juce_dsp.h>
#include <memory>

//==============================================================================
// DSP Constants - Centralized configuration for audio processing algorithms
//==============================================================================
namespace DSPConstants
{
    // Oversampling configuration
    constexpr int OVERSAMPLING_FACTOR = 4;                    // 4x oversampling to prevent aliasing
    constexpr int OVERSAMPLING_STAGES = 2;                    // 2 stages for polyphase IIR

    // Sub Guard crossover configuration (replaces 808-Safe mode)
    constexpr float SUBGUARD_FREQ_OFF = 0.0f;                 // OFF sentinel value (no band-split)
    constexpr float SUBGUARD_FREQ_MIN = 50.0f;                // Minimum crossover frequency
    constexpr float SUBGUARD_FREQ_MAX = 200.0f;               // Maximum crossover frequency
    constexpr float SUBGUARD_FREQ_DEFAULT = 0.0f;             // Default to OFF (full-range distortion)
    constexpr float SUBGUARD_SNAP_TOLERANCE = 8.0f;           // ±8 Hz snap zone
    constexpr float SUBGUARD_SNAP_PRESERVE = 60.0f;           // LR24 - steepest slope
    constexpr float SUBGUARD_SNAP_CONTROL = 100.0f;           // LR18 - balanced
    constexpr float SUBGUARD_SNAP_AGGRESSIVE = 150.0f;        // LR12 - gentle slope
    constexpr float SUBGUARD_CROSSFADE_TIME_S = 0.010f;       // 10ms order transition crossfade
    constexpr float SUBGUARD_FREQ_SMOOTH_TIME_S = 0.050f;     // 50ms frequency smoothing

    // Distortion gain scaling
    constexpr float DISTORTION_INPUT_SCALE = 0.6f;            // Pre-distortion gain attenuation
    constexpr float DISTORTION_DRIVE_SCALE = 1.2f;            // Secondary drive multiplier

    // DC blocking filter frequency
    // Note: 5Hz was too low and caused numerical instability in IIR filters
    // 20Hz provides good DC removal while being numerically stable
    constexpr float DC_BLOCKING_FREQ = 20.0f;                 // Remove DC offset at 20Hz (subsonic)

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

    // Default parameter values
    constexpr float DEFAULT_HIPASS_FREQ = 20.0f;              // Default hi-pass filter frequency (subsonic only)

    // Oscilloscope configuration
    constexpr int SCOPE_BUFFER_SIZE = 2048;                   // Circular buffer size for waveform display
    constexpr int SCOPE_UPDATE_DECIMATION = 2;                // Update every 2 samples to reduce CPU
    constexpr int SCOPE_DISPLAY_POINTS = 512;                 // Number of points to draw
    constexpr int SCOPE_REFRESH_RATE_HZ = 30;                 // UI refresh rate

    // Gain reduction meter
    constexpr int METER_REFRESH_RATE_HZ = 30;                 // UI refresh rate
    constexpr float METER_MAX_DB = 20.0f;                     // Maximum gain reduction display range
    constexpr float METER_SMOOTHING = 0.7f;                   // Visual smoothing coefficient

    // Parameter smoothing times (in seconds)
    constexpr double GAIN_SMOOTH_TIME_S = 0.02;               // 20ms for gain changes
    constexpr double DISTORTION_SMOOTH_TIME_S = 0.15;         // 150ms for distortion (slower to avoid zipper)
    constexpr double COMP_GR_SMOOTH_TIME_S = 0.5;             // 500ms for gain reduction display

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
class LFOTests;
class ProcessBlockTests;
class ParameterTests;
class SampleRateTests;
class ThreadSafetyTests;
class StateIOTests;
class GoldenAudioTests;
class HarmonicDensityTests;
class OutputLimiterTests;
#endif

class PluginProcessor : public juce::AudioProcessor
{
#if JUCE_DEBUG
    // Grant test classes access to private members for unit testing
    friend class DistortionDSPTests;
    friend class CompressionDSPTests;
    friend class LFOTests;
    friend class ProcessBlockTests;
    friend class ParameterTests;
    friend class SampleRateTests;
    friend class ThreadSafetyTests;
    friend class StateIOTests;
    friend class GoldenAudioTests;
    friend class HarmonicDensityTests;
    friend class OutputLimiterTests;
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

    //==============================================================================
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    juce::AudioProcessorValueTreeState parameters;
    void fillScopeBuffer(juce::AudioBuffer<float>& destBuffer);
    void pushSampleToScope(float left, float right);
    void applyLA2ACompression(juce::AudioBuffer<float>& buffer,
        float peakReduction,
        float makeupGain,
        int ratioMode);

    // Atomic gain reduction for UI meter (in dB) - public for UI access
    std::atomic<float> currentGainReductionDB{ 0.0f };
    std::atomic<float> phaseCorrelation{ 1.0f };  // Phase correlation for UI meter (-1.0 to +1.0)
    std::atomic<float> lfoPhaseForUI{ 0.0f };     // LFO phase (0-1) for UI arc animation

    // Real-time safe debug flags (atomic, no logging in audio thread) - public for test access
    std::atomic<bool> debugHadNaN{false};
    std::atomic<bool> debugHadBufferOverflow{false};
    std::atomic<bool> debugHadDistortionCorruption{false};

    // Sub Guard filter order enum (public for method signatures)
    enum class SubGuardFilterOrder { LR12, LR18, LR24 };

private:
    // Sub Guard helper methods
    SubGuardFilterOrder determineSubGuardFilterOrder(float freq) const;
    void updateSubGuardCoefficients(float freq, double sampleRate);

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
    size_t oversamplingFactor = 4;
    int currentNumChannels = 0;

    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
    juce::dsp::IIR::Coefficients<float>> preHighPassFilter;

    // Manual DC blocker state (simple one-pole, extremely stable)
    // y[n] = x[n] - x[n-1] + R * y[n-1], where R ≈ 0.995 for ~35Hz cutoff at 44.1kHz
    float manualDCBlockerPrevInput[2] = { 0.0f, 0.0f };
    float manualDCBlockerPrevOutput[2] = { 0.0f, 0.0f };

    // Sub Guard LR24 filters (4th order = 2 cascaded 2nd-order stages)
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

    // Post-distortion tone filter (oversampled rate)
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
        juce::dsp::IIR::Coefficients<float>> toneFilter;

    juce::AudioBuffer<float> lowBandBuffer;   // For clean low frequencies
    juce::AudioBuffer<float> highBandBuffer;  // For distorted high frequencies

    juce::SmoothedValue<float> smoothedOutputGain;  // Only output gain uses SmoothedValue (normal rate)
    juce::SmoothedValue<float> bypassRamp;  // Bypass crossfade to prevent clicks (10ms)
    bool wasBypassed = true;  // Track bypass state for crossfade detection

    // Smoothed parameters for automation (prevent zipper noise)
    juce::SmoothedValue<float> smoothedLfoDepth;
    juce::SmoothedValue<float> smoothedDistMix;
    juce::SmoothedValue<float> smoothedToneParam;

    // Thread safety for state persistence
    std::atomic<bool> stateNeedsReset{false};

    std::atomic<float>* inputGainParam = nullptr;
    std::atomic<float>* outputGainParam = nullptr;
    std::atomic<float>* distortionAmountParam = nullptr;
    std::atomic<float>* highPassFreqParam = nullptr;
    std::atomic<float>* clipTypeParam = nullptr;
    std::atomic<float>* subGuardFreqParam = nullptr;  // Sub Guard crossover frequency (50-200Hz)
    std::atomic<float>* lfoRateParam = nullptr;
    std::atomic<float>* lfoDepthParam = nullptr;
    std::atomic<float>* lfoWaveformParam = nullptr;  // LFO waveform type
    std::atomic<float>* lfoEnabledParam = nullptr;   // LFO on/off toggle
    std::atomic<float>* lfoDestinationParam = nullptr;  // LFO destination (0-4: Dist, Tone, Hi-Pass, Mix, Gain)
    std::atomic<float>* waveshaperMixParam = nullptr;  // Waveshaper wet/dry mix (0-100)

    // Compressor parameters
    std::atomic<float>* compPeakReductionParam = nullptr;
    std::atomic<float>* compMakeupGainParam = nullptr;
    std::atomic<float>* compRatioParam = nullptr;  
    std::atomic<float>* compEnabledParam = nullptr;
    std::atomic<float>* autoGainEnabledParam = nullptr;

    std::atomic<float>* distMixParam = nullptr;         // Distortion wet/dry mix (0-100)
    std::atomic<float>* toneParam = nullptr;            // Post-distortion tone (2000-20000Hz)
    std::atomic<float>* waveshaperCleanParam = nullptr; // 0=Gritty (tone→waveshaper), 1=Clean (waveshaper→tone)

    // Compressor state variables (LA-2A optical cell simulation)
    // Optical cell envelope follower (T4 cell)
    float compEnvelopeState = 0.0f;


    // RMS detection for program-dependent behavior
    float compRmsHistory = 0.0f;

    // Smoothed gain reduction for visual/smooth compression
    juce::SmoothedValue<float> smoothedGainReduction;

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

    // Debug counters (per-instance, not static to avoid multi-instance bugs)
    int debugBlockCounter = 0;
    int deltaLogCounter = 0;

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

    // Cached filter parameters to avoid unnecessary coefficient updates
    float lastHighPassFreq = -1.0f;
    float lastToneFreq = -1.0f;
    double lastSampleRate = 0.0;  // Track sample rate changes
    double lastOversampledSampleRate = 0.0;  // Track oversampled rate for distortion filters

    // Manual parameter smoothing for oversampled domain (to avoid SmoothedValue issues)
    float lastInputGain = 1.0f;
    float lastDistortionDrive = 1.0f;

    juce::AudioBuffer<float> scopeBuffer;
    juce::AbstractFifo scopeFifo;
    mutable juce::SpinLock scopeLock;

    // Helper methods for studio distortion DSP
    float applyStudioDistortion(float x, float gain, float drive, int clipType, float harmonicScale);
    void updateSampleRateDependentCoefficients(double sampleRate);
    float generateLFOWaveform(float phase, int waveformType);  // Generate LFO waveforms
    void resetDSPState();  // Thread-safe DSP state reset (called from audio thread)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginProcessor)
};