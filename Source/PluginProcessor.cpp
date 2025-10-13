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
    bandSplitEnabledParam = parameters.getRawParameterValue("bandSplitEnabled");
    clipTypeParam = parameters.getRawParameterValue("clipType");
    lfoRateParam = parameters.getRawParameterValue("lfoRate");
    lfoDepthParam = parameters.getRawParameterValue("lfoDepth");
    compPeakReductionParam = parameters.getRawParameterValue("compPeakReduction");
    compMakeupGainParam = parameters.getRawParameterValue("compMakeupGain");
    compRatioParam = parameters.getRawParameterValue("compRatio");
    compEnabledParam = parameters.getRawParameterValue("compEnabled");

    // Verify all parameters were found
    jassert(inputGainParam && outputGainParam && distortionAmountParam
        && highPassFreqParam && bandSplitEnabledParam && clipTypeParam
        && lfoRateParam && lfoDepthParam
        && compPeakReductionParam && compMakeupGainParam && compRatioParam && compEnabledParam);
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
    // ========== STORE SAMPLE RATE FOR LFO (INSERT HERE) ==========
    currentSampleRate = static_cast<float>(sampleRate);
    lfoPhase = 0.0f;  // Reset LFO phase
    
    const int numChannels = std::max(1, getTotalNumInputChannels());
    smoothedInputGain.reset(sampleRate, 0.02);      // 20 ms
    smoothedOutputGain.reset(sampleRate, 0.02);     // 20 ms
    smoothedDistortion.reset(sampleRate, 0.15);     // 150 ms slower ramp
    compEnvelopeState = 0.0f;
    compRmsHistory = 0.0f;
    tubeWarmth = 0.0f;

    // Initialize smoothed gain reduction with slow release (LA-2A style)
    smoothedGainReduction.reset(sampleRate, 0.5);  // 500ms for smooth, slow compression
    smoothedGainReduction.setCurrentAndTargetValue(1.0f);  // Start at unity (no reduction)
    

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

    // ========== BAND-SPLIT FILTER PREPARATION BELOW ==========
    // Prepare band-split filters (Linkwitz-Riley at 150Hz)
    // Using cascaded Butterworth 2nd order for 4th order LR crossover

    const float crossoverFreq = 150.0f;

    lowPassFilter1.state = juce::dsp::IIR::Coefficients<float>::makeLowPass(spec.sampleRate, crossoverFreq);
    lowPassFilter1.prepare(spec);
    lowPassFilter1.reset();

    lowPassFilter2.state = juce::dsp::IIR::Coefficients<float>::makeLowPass(spec.sampleRate, crossoverFreq);
    lowPassFilter2.prepare(spec);
    lowPassFilter2.reset();

    highPassFilter1.state = juce::dsp::IIR::Coefficients<float>::makeHighPass(spec.sampleRate, crossoverFreq);
    highPassFilter1.prepare(spec);
    highPassFilter1.reset();

    highPassFilter2.state = juce::dsp::IIR::Coefficients<float>::makeHighPass(spec.sampleRate, crossoverFreq);
    highPassFilter2.prepare(spec);
    highPassFilter2.reset();

    // Prepare buffers for band-split processing (oversampled size)
    const int oversampledBlockSize = samplesPerBlock * static_cast<int>(oversamplingFactor);
    lowBandBuffer.setSize(numChannels, oversampledBlockSize);
    highBandBuffer.setSize(numChannels, oversampledBlockSize);
    // =============================================================
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
// LA-2A Style Optical Compressor (processes low band only)
void PluginProcessor::applyLA2ACompression(juce::AudioBuffer<float>& buffer,
    float peakReduction,
    float makeupGain,
    int ratioMode)
{
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    if (numSamples == 0 || numChannels == 0)
        return;

    // Map peak reduction (0-100) to threshold in dB
    const float threshold = -60.0f + (peakReduction * 0.6f);  // -60dB to 0dB range

    // Ratio: Compress mode = 3:1, Limit mode = 12:1 (LA-2A style)
    const float ratio = (ratioMode == 0) ? 3.0f : 12.0f;

    // Map makeup gain (0-100, 50=unity) to dB
    const float makeupGainDB = (makeupGain - 50.0f) * 0.24f;  // ±12dB range
    const float makeupGainLinear = juce::Decibels::decibelsToGain(makeupGainDB);

    // Optical cell timing (program-dependent)
    const float attackCoeff = 0.9995f;   // ~10ms attack (slow optical response)
    const float releaseCoeff = 0.99995f; // ~500ms release (optical cell decay)

    for (int sample = 0; sample < numSamples; ++sample)
    {
        // Calculate RMS across channels for detection
        float sumSquares = 0.0f;
        for (int channel = 0; channel < numChannels; ++channel)
        {
            const float sampleValue = buffer.getSample(channel, sample);
            sumSquares += sampleValue * sampleValue;
        }

        const float rms = std::sqrt(sumSquares / numChannels);

        // Update RMS history (program-dependent behavior)
        compRmsHistory = 0.99f * compRmsHistory + 0.01f * rms;

        // Convert to dB
        const float inputLevelDB = juce::Decibels::gainToDecibels(rms + 0.00001f);

        // Calculate gain reduction needed
        float gainReductionDB = 0.0f;
        if (inputLevelDB > threshold)
        {
            const float overThresholdDB = inputLevelDB - threshold;

            // Soft knee (2dB knee width for smooth LA-2A character)
            const float kneeWidth = 2.0f;
            if (overThresholdDB < kneeWidth)
            {
                // Soft knee curve
                const float kneeRatio = overThresholdDB / kneeWidth;
                gainReductionDB = overThresholdDB * kneeRatio * (1.0f - 1.0f / ratio);
            }
            else
            {
                // Above knee - standard compression
                gainReductionDB = kneeWidth * (1.0f - 1.0f / ratio) +
                    (overThresholdDB - kneeWidth) * (1.0f - 1.0f / ratio);
            }
        }

        // Optical cell envelope follower (T4 cell simulation)
        const float targetGainReduction = juce::Decibels::decibelsToGain(-gainReductionDB);

        if (targetGainReduction < compEnvelopeState)
        {
            // Attack - compression increasing (fast)
            compEnvelopeState = attackCoeff * compEnvelopeState + (1.0f - attackCoeff) * targetGainReduction;
        }
        else
        {
            // Release - compression decreasing (slow, optical decay)
            compEnvelopeState = releaseCoeff * compEnvelopeState + (1.0f - releaseCoeff) * targetGainReduction;
        }

        // Apply compression and makeup gain to all channels
        for (int channel = 0; channel < numChannels; ++channel)
        {
            float sampleValue = buffer.getSample(channel, sample);

            // Apply compression
            sampleValue *= compEnvelopeState;

            // Tube harmonic generation (even harmonics for warmth)
            const float tubeInput = sampleValue * 1.5f;
            const float tubeSaturation = std::tanh(tubeInput);

            // Blend tube character (subtle 2nd harmonic)
            sampleValue = sampleValue * 0.85f + tubeSaturation * 0.15f;

            // Apply makeup gain
            sampleValue *= makeupGainLinear;

            // Soft clip output (prevent overs from makeup gain)
            sampleValue = std::tanh(sampleValue * 0.9f) * 1.1f;

            buffer.setSample(channel, sample, sampleValue);
        }
    }
}


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
    const bool bandSplitEnabled = bandSplitEnabledParam->load() > 0.5f;
    const int clipType = static_cast<int>(clipTypeParam->load());
    const float lfoRate = lfoRateParam->load();
    const float lfoDepth = lfoDepthParam->load();
    const float compPeakReduction = compPeakReductionParam->load();
    const float compMakeupGain = compMakeupGainParam->load();
    const int compRatioMode = static_cast<int>(compRatioParam->load());
    const bool compEnabled = compEnabledParam->load() > 0.5f;

    // Scale to actual ranges for processing
    const auto inGain = inGainParam / 50.0f;
    const auto outGainDB = (outGainParam - 50.0f) * 0.24f;  // Maps 0->-12dB, 50->0dB, 100->+12dB
    const auto outGain = juce::Decibels::decibelsToGain(outGainDB);

    // ========== LFO MODULATION (INSERT HERE) ==========
    // Calculate LFO value (sine wave from -1 to +1)
    float lfoValue = 0.0f;
    if (lfoRate > 0.0f)  // Only compute LFO if rate > 0
    {
        lfoValue = std::sin(lfoPhase * 2.0f * juce::MathConstants<float>::pi);

        // Update phase for next block
        const float phaseIncrement = lfoRate / currentSampleRate * buffer.getNumSamples();
        lfoPhase += phaseIncrement;

        // Keep phase in 0-1 range
        if (lfoPhase >= 1.0f)
            lfoPhase -= 1.0f;
    }

    // Apply LFO modulation to distortion amount
    // lfoValue ranges -1 to +1, lfoDepth is 0-100
    const float lfoModulation = (lfoValue * lfoDepth / 100.0f);  // -1 to +1 scaled by depth
    const float modulatedDistortionParam = juce::jlimit(0.0f, 100.0f, distortionParam + lfoModulation * 50.0f);
    const auto distortionAmount = 1.0f + (modulatedDistortionParam / 100.0f) * 50.0f;
    // ==================================================

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

    // ========== REPLACE THE DISTORTION LOOP WITH THIS BAND-SPLIT LOGIC ==========
    const size_t numSamples = oversampledBlock.getNumSamples();
    const size_t numChannels = oversampledBlock.getNumChannels();

    if (bandSplitEnabled)
    {
        // === BAND-SPLIT MODE: Clean low + Distorted high ===

        // Clear the band buffers
        lowBandBuffer.clear();
        highBandBuffer.clear();

        // Copy input to both band buffers
        for (size_t channel = 0; channel < numChannels; ++channel)
        {
            lowBandBuffer.copyFrom(static_cast<int>(channel), 0,
                oversampledBlock.getChannelPointer(channel),
                static_cast<int>(numSamples));
            highBandBuffer.copyFrom(static_cast<int>(channel), 0,
                oversampledBlock.getChannelPointer(channel),
                static_cast<int>(numSamples));
        }

        // Filter the bands
        auto lowBlock = juce::dsp::AudioBlock<float>(lowBandBuffer).getSubBlock(0, numSamples);
        auto highBlock = juce::dsp::AudioBlock<float>(highBandBuffer).getSubBlock(0, numSamples);

        // Apply cascaded low-pass filters for Linkwitz-Riley
        lowPassFilter1.process(juce::dsp::ProcessContextReplacing<float>(lowBlock));
        lowPassFilter2.process(juce::dsp::ProcessContextReplacing<float>(lowBlock));

        // Apply cascaded high-pass filters for Linkwitz-Riley
        highPassFilter1.process(juce::dsp::ProcessContextReplacing<float>(highBlock));
        highPassFilter2.process(juce::dsp::ProcessContextReplacing<float>(highBlock));

        // ========== APPLY COMPRESSION TO LOW BAND (INSERT HERE) ==========
        // Apply LA-2A compression to low band if enabled
        if (compEnabled && compPeakReduction > 0.0f)
        {
            // Create a temporary buffer view for the low band at correct size
            juce::AudioBuffer<float> lowBandView(
                lowBandBuffer.getArrayOfWritePointers(),
                static_cast<int>(numChannels),
                static_cast<int>(numSamples)
            );

            applyLA2ACompression(lowBandView, compPeakReduction, compMakeupGain, compRatioMode);
        }
        // =================================================================

        
        // Apply distortion ONLY to the high band
        for (size_t sample = 0; sample < numSamples; ++sample)
        {
            const float currentInputGain = smoothedInputGain.getNextValue();
            const float currentDistortion = smoothedDistortion.getNextValue();
            const float gain1 = currentInputGain * currentDistortion * 0.6f;
            const float drive2 = currentDistortion * 1.2f;


            for (size_t channel = 0; channel < numChannels; ++channel)
            {
                auto* highBandData = highBandBuffer.getWritePointer(static_cast<int>(channel));
                const float input = highBandData[sample];
                const float driveSample = input * gain1;

                float output = 0.0f;

                // Switch between clip types
                switch (clipType)
                {
                case 0: // Soft Clip (original tanh + exponential)
                {
                    const float stage1 = std::tanh(driveSample);
                    output = (stage1 > 0.0f)
                        ? 1.0f - std::exp(-stage1 * drive2)
                        : -1.0f + std::exp(stage1 * drive2);
                    break;
                }
                case 1: // Hard Clip
                {
                    output = juce::jlimit(-1.0f, 1.0f, driveSample);
                    break;
                }
                case 2: // Tube Warmth (tanh + asymmetric)
                {
                    const float stage1 = std::tanh(driveSample);
                    output = stage1 + 0.3f * stage1 * stage1 * stage1;
                    break;
                }
                case 3: // Fuzz (aggressive cubic)
                {
                    const float x = juce::jlimit(-1.5f, 1.5f, driveSample);
                    output = x - (x * x * x) / 3.0f;
                    break;
                }
                case 4: // Asymmetric (different curves for +/-)
                {
                    const float stage1 = std::tanh(driveSample * 1.5f);
                    output = (stage1 > 0.0f)
                        ? stage1 * 0.9f
                        : stage1 * 1.2f;
                    break;
                }
                default:
                    output = std::tanh(driveSample);
                    break;
                }

                highBandData[sample] = juce::jlimit(-1.0f, 1.0f, output);
            }
        }

        // Recombine: Clean low + Distorted high
        for (size_t channel = 0; channel < numChannels; ++channel)
        {
            auto* outputData = oversampledBlock.getChannelPointer(channel);
            const auto* lowData = lowBandBuffer.getReadPointer(static_cast<int>(channel));
            const auto* highData = highBandBuffer.getReadPointer(static_cast<int>(channel));

            for (size_t sample = 0; sample < numSamples; ++sample)
            {
                outputData[sample] = lowData[sample] + highData[sample];
            }
        }
    }
    else
    {
        // === NORMAL MODE: Full-range distortion ===
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

                float output = 0.0f;

                // Switch between clip types
                switch (clipType)
                {
                case 0: // Soft Clip (original tanh + exponential)
                {
                    const float stage1 = std::tanh(driveSample);
                    output = (stage1 > 0.0f)
                        ? 1.0f - std::exp(-stage1 * drive2)
                        : -1.0f + std::exp(stage1 * drive2);
                    break;
                }
                case 1: // Hard Clip
                {
                    output = juce::jlimit(-1.0f, 1.0f, driveSample);
                    break;
                }
                case 2: // Tube Warmth (tanh + asymmetric)
                {
                    const float stage1 = std::tanh(driveSample);
                    output = stage1 + 0.3f * stage1 * stage1 * stage1;
                    break;
                }
                case 3: // Fuzz (aggressive cubic)
                {
                    const float x = juce::jlimit(-1.5f, 1.5f, driveSample);
                    output = x - (x * x * x) / 3.0f;
                    break;
                }
                case 4: // Asymmetric (different curves for +/-)
                {
                    const float stage1 = std::tanh(driveSample * 1.5f);
                    output = (stage1 > 0.0f)
                        ? stage1 * 0.9f
                        : stage1 * 1.2f;
                    break;
                }
                default:
                    output = std::tanh(driveSample);
                    break;
                }

                channelData[sample] = juce::jlimit(-1.0f, 1.0f, output);
            }
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

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{ "bandSplitEnabled", 1 },
        "808-Safe Mode",
        false));  // Default is OFF (normal distortion mode)

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ "clipType", 1 },
        "Clip Type",
        juce::StringArray{ "Soft Clip", "Hard Clip", "Tube Warmth", "Fuzz", "Asymmetric" },
        0));  
    
    
    // ========== LFO PARAMETERS (INSERT HERE) ==========
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "lfoRate", 1 },
        "LFO Rate",
        juce::NormalisableRange<float>(0.1f, 10.0f, 0.1f),
        0.0f));  // Default 0 = LFO off

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "lfoDepth", 1 },
        "LFO Depth",
        juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f),
        0.0f));  // Default 0 = no modulation

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "compPeakReduction", 1 },
        "Peak Reduction",
        juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f),
        0.0f));  // Default 0 = no compression

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "compMakeupGain", 1 },
        "Comp Makeup Gain",
        juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f),
        50.0f));  // Default 50 = unity gain

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ "compRatio", 1 },
        "Compress/Limit",
        juce::StringArray{ "Compress", "Limit" },
        0));  // Default 0 = Compress (3:1), 1 = Limit (12:1)

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{ "compEnabled", 1 },
        "Compressor Enable",
        false));  // Default OFF

    return { params.begin(), params.end() };
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}