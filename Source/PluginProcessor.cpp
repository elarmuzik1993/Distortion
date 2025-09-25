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
    ), parameters(*this, nullptr, "Parameters", createParameterLayout())
{
    // Ensure parameters exist before storing pointers
    inputGainParam = parameters.getRawParameterValue("inputGain");
    outputGainParam = parameters.getRawParameterValue("outputGain");
    distortionAmountParam = parameters.getRawParameterValue("distortionAmount");

    // Verify all parameters were found
    jassert(inputGainParam && outputGainParam && distortionAmountParam);
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
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
    // so this should be at least 1, even if you're not really implementing programs.
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
    // Ensure we have valid channel count
    const int numChannels = std::max(1, getTotalNumInputChannels());

    // Recreate oversampling if channel count changed or doesn't exist
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

    preHighPassFilter.state = juce::dsp::IIR::Coefficients<float>::makeHighPass(spec.sampleRate, 120.0f);
    preHighPassFilter.prepare(spec);
    preHighPassFilter.reset();
    // DC BLOCKING FILTER AT OUTPUT
    dcBlockingFilter.state = juce::dsp::IIR::Coefficients<float>::makeHighPass(spec.sampleRate, 20.0f);
    dcBlockingFilter.prepare(spec);
    dcBlockingFilter.reset();
}

void PluginProcessor::releaseResources()
{
    if (oversampling)
        oversampling.reset();
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

    // Load parameters once
    const auto inGain = parameters.getRawParameterValue("inputGain")->load();
    const auto outGain = parameters.getRawParameterValue("outputGain")->load();
    const auto distortionAmount = parameters.getRawParameterValue("distortionAmount")->load();

    // Pre-calculate coefficients
    const float gain1 = inGain * distortionAmount * 0.6f;
    const float drive2 = distortionAmount * 1.2f;

    // Wrap original buffer into an AudioBlock
    auto inputBlock = juce::dsp::AudioBlock<float>(buffer);
    auto oversampledBlock = oversampling->processSamplesUp(inputBlock);

    // Pre-filtering
    preHighPassFilter.process(juce::dsp::ProcessContextReplacing<float>(oversampledBlock));

    // Distortion processing on oversampled block
    for (size_t channel = 0; channel < oversampledBlock.getNumChannels(); ++channel)
    {
        auto* channelData = oversampledBlock.getChannelPointer(channel);
        const size_t numSamples = oversampledBlock.getNumSamples();

        for (size_t sample = 0; sample < numSamples; ++sample)
        {
            const float input = channelData[sample];
            const float driveSample = input * gain1;
            const float stage1 = std::tanh(driveSample);
            const float stage2 = (stage1 > 0.0f) ?
                1.0f - std::exp(-stage1 * drive2) :
                -1.0f + std::exp(stage1 * drive2);

            channelData[sample] = juce::jlimit(-1.0f, 1.0f, stage2);
        }
    }

    // DC blocking AFTER all distortion processing, BEFORE downsampling
    dcBlockingFilter.process(juce::dsp::ProcessContextReplacing<float>(oversampledBlock));

    // Downsample back into original buffer
    oversampling->processSamplesDown(inputBlock);

    // Apply output gain to the final downsampled result
    buffer.applyGain(outGain);
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

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Use ParameterID for future-proofing
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "inputGain", 1 },
        "Input Gain",
        juce::NormalisableRange<float>(0.0f, 2.0f),
        1.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "outputGain", 1 },
        "Output Gain",
        juce::NormalisableRange<float>(0.0f, 2.0f),
        1.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "distortionAmount", 1 },
        "Distortion Amount",
        juce::NormalisableRange<float>(1.0f, 30.0f),
        1.0f));

    return { params.begin(), params.end() };
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}