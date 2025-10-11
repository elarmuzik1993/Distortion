/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <memory>

//==============================================================================
PluginProcessor::PluginProcessor()
    : AudioProcessor(BusesProperties()
#if ! JucePlugin_IsMidiEffect
#if ! JucePlugin_IsSynth
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
#endif
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)
#endif
    ), parameters(*this, nullptr, "Parameters", createParameterLayout()),
    scopeFifo(SCOPE_BUFFER_SIZE)
{
    // Initialize Parameter Pointer in Constructor

    inputGainParam = parameters.getRawParameterValue("inputGain");
    outputGainParam = parameters.getRawParameterValue("outputGain");
    distortionAmountParam = parameters.getRawParameterValue("distortionAmount");
    highPassFreqParam = parameters.getRawParameterValue("highPassFreq");

    // Verify all parameters were found
    jassert(inputGainParam && outputGainParam && distortionAmountParam && highPassFreqParam);
}

PluginProcessor::~PluginProcessor()
{
}

//==============================================================================
const juce::String PluginProcessor::getName() const
{
    return JucePlugin_Name;
}

bool PluginProcessor::acceptsMidi() const
{
#if JucePlugin_WantsMidiInput
    return true;
#else
    return false;
#endif
}

bool PluginProcessor::producesMidi() const
{
#if JucePlugin_ProducesMidiOutput
    return true;
#else
    return false;
#endif
}

bool PluginProcessor::isMidiEffect() const
{
#if JucePlugin_IsMidiEffect
    return true;
#else
    return false;
#endif
}

double PluginProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int PluginProcessor::getNumPrograms()
{
    return 1;   
}

int PluginProcessor::getCurrentProgram()
{
    return 0;
}

void PluginProcessor::setCurrentProgram(int index)
{
    juce::ignoreUnused(index);
}

const juce::String PluginProcessor::getProgramName(int index)
{
    juce::ignoreUnused(index);
    return {};
}

void PluginProcessor::changeProgramName(int index, const juce::String& newName)
{
    juce::ignoreUnused(index, newName);
}

//==============================================================================
void PluginProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    
    const int numChannels = std::max(1, getTotalNumInputChannels());
    smoothedInputGain.reset(sampleRate, 0.02);      // 20 ms
    smoothedOutputGain.reset(sampleRate, 0.02);     // 20 ms
    smoothedDistortion.reset(sampleRate, 0.15);     // 150 ms slower ramp
    scopeBuffer.setSize(2, SCOPE_BUFFER_SIZE);
    scopeBuffer.clear();
    scopeFifo.setTotalSize(SCOPE_BUFFER_SIZE);



    // Oversampling
if (!oversampling || currentNumChannels != numChannels) {
    oversampling = std::make_unique<juce::dsp::Oversampling<float>>(
        numChannels, 2,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
        false, false
    );
    currentNumChannels = numChannels;
}
    
    // Initialize oversampling processing block size
    oversampling->initProcessing(static_cast<size_t>(samplesPerBlock));

    // Cache the oversampling factor
    oversamplingFactor = oversampling->getOversamplingFactor();

    // Prepare DSP filters with oversampled sample rate & block size
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate * oversamplingFactor;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock * oversamplingFactor);
    spec.numChannels = static_cast<juce::uint32>(numChannels);

    preHighPassFilter.state = juce::dsp::IIR::Coefficients<float>::makeHighPass(spec.sampleRate, highPassFreqParam->load());
    preHighPassFilter.prepare(spec);
    preHighPassFilter.reset();
 
    // DC BLOCKING FILTER AT OUTPUT
    dcBlockingFilter.state = juce::dsp::IIR::Coefficients<float>::makeHighPass(spec.sampleRate, 20.0f);
    dcBlockingFilter.prepare(spec);
    dcBlockingFilter.reset();
}


void PluginProcessor::releaseResources()
{
    oversampling.reset();
    preHighPassFilter.reset();
    dcBlockingFilter.reset();
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool PluginProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
#if JucePlugin_IsMidiEffect
    juce::ignoreUnused(layouts);
    return true;
#else
    // This is the place where you check if the layout is supported.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
#if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
#endif

    return true;
#endif
}
#endif

void PluginProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused(midiMessages);


    // Early return for empty buffers
    if (buffer.getNumSamples() == 0 || buffer.getNumChannels() == 0)
        return;

    // Ensure oversampling exists
    if (!oversampling)
        return;

    // Load parameters and scale them 
    const auto inGainParam = inputGainParam->load();
    const auto outGainParam = outputGainParam->load();
    const auto distortionParam = distortionAmountParam->load();
    const auto highPassFreq = highPassFreqParam->load();

    // Scale to actual ranges for processing
    const auto inGain = inGainParam / 50.0f;  
    const auto outGain = outGainParam / 50.0f;  
    const auto distortionAmount = 1.0f + (distortionParam / 100.0f) * 29.0f;  

    smoothedInputGain.setTargetValue(inGain);
    smoothedOutputGain.setTargetValue(outGain);
    smoothedDistortion.setTargetValue(distortionAmount);


    // Wrap original buffer into an AudioBlock
    auto inputBlock = juce::dsp::AudioBlock<float>(buffer);
    auto oversampledBlock = oversampling->processSamplesUp(inputBlock);

    *preHighPassFilter.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass(
        getSampleRate() * oversamplingFactor, highPassFreq);

    // Pre-filtering
    preHighPassFilter.process(juce::dsp::ProcessContextReplacing<float>(oversampledBlock));

    // Distortion Loop
    const size_t numSamples = oversampledBlock.getNumSamples();
    const size_t numChannels = oversampledBlock.getNumChannels();

    for (size_t sample = 0; sample < numSamples; ++sample)
    {
        const float currentInputGain = smoothedInputGain.getNextValue();
        const float currentDistortion = smoothedDistortion.getNextValue();

        const float gain1 = currentInputGain * currentDistortion * 0.6f;
        const float drive2 = currentDistortion * 1.2f;

        for (size_t channel = 0; channel < numChannels; ++channel)
        {
            auto* channelData = oversampledBlock.getChannelPointer(channel);
            const float input = channelData[sample];
            const float driveSample = input * gain1;
            const float stage1 = std::tanh(driveSample);
            const float stage2 = (stage1 > 0.0f)
                ? 1.0f - std::exp(-stage1 * drive2)
                : -1.0f + std::exp(stage1 * drive2);

            channelData[sample] = juce::jlimit(-1.0f, 1.0f, stage2);
        }
    }

    // DC blocking AFTER all distortion processing, BEFORE downsampling
    dcBlockingFilter.process(juce::dsp::ProcessContextReplacing<float>(oversampledBlock));

    // Downsample back into original buffer
    oversampling->processSamplesDown(inputBlock);

    // Apply output gain to the final downsampled result
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        const float currentOutputGain = smoothedOutputGain.getNextValue();
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            auto* channelData = buffer.getWritePointer(channel);
            channelData[sample] *= currentOutputGain;
        }
    }
    // Push samples to oscilloscope (after all processing)
    const int scopeUpdateRate = 2;  // Update every 2 samples
    for (int sample = 0; sample < buffer.getNumSamples(); sample += scopeUpdateRate)
    {
        // Check if there's space in the FIFO before writing
        if (scopeFifo.getFreeSpace() > 0)
        {
            const float leftSample = buffer.getSample(0, sample);
            const float rightSample = buffer.getNumChannels() > 1 ?
                buffer.getSample(1, sample) : leftSample;
            pushSampleToScope(leftSample, rightSample);
        }
    }
}

void PluginProcessor::pushSampleToScope(float left, float right)
{
    int start1, size1, start2, size2;
    scopeFifo.prepareToWrite(1, start1, size1, start2, size2);

    if (size1 > 0) {
        scopeBuffer.setSample(0, start1, left);
        scopeBuffer.setSample(1, start1, right);
    }
    if (size2 > 0) {
        scopeBuffer.setSample(0, start2, left);
        scopeBuffer.setSample(1, start2, right);
    }

    scopeFifo.finishedWrite(size1 + size2);
}
void PluginProcessor::fillScopeBuffer(juce::AudioBuffer<float>& destBuffer)
{
    const int numSamples = destBuffer.getNumSamples();
    const int availableSamples = scopeFifo.getNumReady();

    // Only read what's available
    const int samplesToRead = juce::jmin(numSamples, availableSamples);

    if (samplesToRead == 0)
    {
        // No new data available
        return;
    }

    int start1, size1, start2, size2;
    scopeFifo.prepareToRead(samplesToRead, start1, size1, start2, size2);

    // Copy first section
    if (size1 > 0)
    {
        for (int channel = 0; channel < destBuffer.getNumChannels(); ++channel)
        {
            destBuffer.copyFrom(channel, 0, scopeBuffer, channel, start1, size1);
        }
    }

    // Copy second section (wrap-around)
    if (size2 > 0)
    {
        for (int channel = 0; channel < destBuffer.getNumChannels(); ++channel)
        {
            destBuffer.copyFrom(channel, size1, scopeBuffer, channel, start2, size2);
        }
    }

    // If we read less than requested, clear the remainder
    if (samplesToRead < numSamples)
    {
        const int remaining = numSamples - samplesToRead;
        for (int channel = 0; channel < destBuffer.getNumChannels(); ++channel)
        {
            destBuffer.clear(channel, samplesToRead, remaining);
        }
    }

    scopeFifo.finishedRead(samplesToRead);
}
//==============================================================================
bool PluginProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new PluginEditor(*this);
}

//==============================================================================
void PluginProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void PluginProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr)
        parameters.replaceState(juce::ValueTree::fromXml(*xmlState));
}

//Add Parameter Definition Here
juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "inputGain", 1 },
        "Input Gain",
        juce::NormalisableRange<float>(0.0f, 100.0f),
        50.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "outputGain", 1 },
        "Output Gain",
        juce::NormalisableRange<float>(0.0f, 100.0f),
        50.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "distortionAmount", 1 },
        "Distortion Amount",
        juce::NormalisableRange<float>(0.0f, 100.0f),
        0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "highPassFreq", 1 },
        "High Pass Frequency",
        juce::NormalisableRange<float>(20.0f, 500.0f, 1.0f),
        120.0f));

    return { params.begin(), params.end() };
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}