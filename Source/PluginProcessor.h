/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <juce_dsp/juce_dsp.h>
#include <memory>

class PluginProcessor : public juce::AudioProcessor
{
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
    // ========== COMPRESSOR FUNCTION DECLARATION (INSERT HERE) ==========
    void applyLA2ACompression(juce::AudioBuffer<float>& buffer,
        float peakReduction,
        float makeupGain,
        int ratioMode);
private:
    
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
    double oversamplingFactor = 4.0;
    int currentNumChannels = 0;

    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
    juce::dsp::IIR::Coefficients<float>> preHighPassFilter;
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
    juce::dsp::IIR::Coefficients<float>> dcBlockingFilter;

    // ========== SPLIT FILTER HERE ==========

    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
        juce::dsp::IIR::Coefficients<float>> lowPassFilter1;
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
        juce::dsp::IIR::Coefficients<float>> lowPassFilter2;
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
        juce::dsp::IIR::Coefficients<float>> highPassFilter1;
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
        juce::dsp::IIR::Coefficients<float>> highPassFilter2;

    juce::AudioBuffer<float> lowBandBuffer;   // For clean low frequencies
    juce::AudioBuffer<float> highBandBuffer;  // For distorted high frequencies


    juce::SmoothedValue<float> smoothedInputGain, smoothedOutputGain, smoothedDistortion;

    std::atomic<float>* inputGainParam = nullptr;
    std::atomic<float>* outputGainParam = nullptr;
    std::atomic<float>* distortionAmountParam = nullptr;
    std::atomic<float>* highPassFreqParam = nullptr;
    std::atomic<float>* clipTypeParam = nullptr;
    std::atomic<float>* bandSplitEnabledParam = nullptr;
   
    std::atomic<float>* lfoRateParam = nullptr;
    std::atomic<float>* lfoDepthParam = nullptr;

    // ========== COMPRESSOR PARAMETERS (INSERT HERE) ==========

    std::atomic<float>* compPeakReductionParam = nullptr;
    std::atomic<float>* compMakeupGainParam = nullptr;
    std::atomic<float>* compRatioParam = nullptr;  
    std::atomic<float>* compEnabledParam = nullptr;
    

    // ========== COMPRESSOR STATE VARIABLES (INSERT HERE) ==========
    // Optical cell envelope follower (LA-2A T4 cell simulation)
    float compEnvelopeState = 0.0f;


    // RMS detection for program-dependent behavior
    float compRmsHistory = 0.0f;

    // Smoothed gain reduction for visual/smooth compression
    juce::SmoothedValue<float> smoothedGainReduction;

    // Tube harmonic state
    float tubeWarmth = 0.0f;

    float lfoPhase = 0.0f;
    float currentSampleRate = 44100.0f;

    juce::AudioBuffer<float> scopeBuffer;
    juce::AbstractFifo scopeFifo;
    static constexpr int SCOPE_BUFFER_SIZE = 2048;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginProcessor)
};