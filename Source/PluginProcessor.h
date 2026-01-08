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

    // Distortion band-split crossover (808-Safe mode)
    constexpr float DISTORTION_CROSSOVER_FREQ = 150.0f;       // Preserve sub-bass below 150Hz

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
    constexpr float DEFAULT_COMP_CROSSOVER = 250.0f;          // Default compression crossover

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

private:
    
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
    size_t oversamplingFactor = 4;
    int currentNumChannels = 0;

    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
    juce::dsp::IIR::Coefficients<float>> preHighPassFilter;
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
    juce::dsp::IIR::Coefficients<float>> dcBlockingFilter;

    // Manual DC blocker state (simple one-pole, extremely stable)
    // y[n] = x[n] - x[n-1] + R * y[n-1], where R ≈ 0.995 for ~35Hz cutoff at 44.1kHz
    float manualDCBlockerPrevInput[2] = { 0.0f, 0.0f };
    float manualDCBlockerPrevOutput[2] = { 0.0f, 0.0f };

    // Distortion band-split filters (808-Safe mode)
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
        juce::dsp::IIR::Coefficients<float>> lowPassFilter1;
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
        juce::dsp::IIR::Coefficients<float>> lowPassFilter2;
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
        juce::dsp::IIR::Coefficients<float>> highPassFilter1;
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
        juce::dsp::IIR::Coefficients<float>> highPassFilter2;

    // Post-distortion tone filter (oversampled rate)
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
        juce::dsp::IIR::Coefficients<float>> toneFilter;

    // Compression band-split filters (normal sample rate)
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
        juce::dsp::IIR::Coefficients<float>> compLowPassFilter1;
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
        juce::dsp::IIR::Coefficients<float>> compLowPassFilter2;
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
        juce::dsp::IIR::Coefficients<float>> compHighPassFilter1;
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
        juce::dsp::IIR::Coefficients<float>> compHighPassFilter2;

    juce::AudioBuffer<float> lowBandBuffer;   // For clean low frequencies
    juce::AudioBuffer<float> highBandBuffer;  // For distorted high frequencies
    juce::AudioBuffer<float> compLowBandBuffer;   // Low band for compression split
    juce::AudioBuffer<float> compHighBandBuffer;  // High band for compression split
    juce::AudioBuffer<float> compDryBuffer;       // Dry signal for parallel blend

    juce::SmoothedValue<float> smoothedOutputGain;  // Only output gain uses SmoothedValue (normal rate)

    std::atomic<float>* inputGainParam = nullptr;
    std::atomic<float>* outputGainParam = nullptr;
    std::atomic<float>* distortionAmountParam = nullptr;
    std::atomic<float>* highPassFreqParam = nullptr;
    std::atomic<float>* clipTypeParam = nullptr;
    std::atomic<float>* bandSplitEnabledParam = nullptr;
    std::atomic<float>* lfoRateParam = nullptr;
    std::atomic<float>* lfoDepthParam = nullptr;
    std::atomic<float>* lfoWaveformParam = nullptr;  // LFO waveform type
    std::atomic<float>* waveshaperMixParam = nullptr;  // Waveshaper wet/dry mix (0-100)

    // Compressor parameters
    std::atomic<float>* compPeakReductionParam = nullptr;
    std::atomic<float>* compMakeupGainParam = nullptr;
    std::atomic<float>* compRatioParam = nullptr;  
    std::atomic<float>* compEnabledParam = nullptr;

    std::atomic<float>* compWetDryParam = nullptr;      // 0=100% dry, 100=100% wet
    std::atomic<float>* compCrossoverParam = nullptr;   // 150-350Hz adjustable split
    std::atomic<float>* distMixParam = nullptr;         // Distortion wet/dry mix (0-100)
    std::atomic<float>* toneParam = nullptr;            // Post-distortion tone (2000-20000Hz)

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

    // Cached filter parameters to avoid unnecessary coefficient updates
    float lastHighPassFreq = -1.0f;
    float lastCompCrossoverFreq = -1.0f;
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
    float applyStudioDistortion(float x, float gain, float drive, int clipType);
    void updateSampleRateDependentCoefficients(double sampleRate);
    float generateLFOWaveform(float phase, int waveformType);  // Generate LFO waveforms

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginProcessor)
};