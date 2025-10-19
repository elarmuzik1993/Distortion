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
    scopeFifo(DSPConstants::SCOPE_BUFFER_SIZE)
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
    compWetDryParam = parameters.getRawParameterValue("compWetDry");
    compCrossoverParam = parameters.getRawParameterValue("compCrossover");
    distMixParam = parameters.getRawParameterValue("distMix");
    // Verify all parameters were found
    jassert(inputGainParam && outputGainParam && distortionAmountParam
        && highPassFreqParam && bandSplitEnabledParam && clipTypeParam
        && lfoRateParam && lfoDepthParam
        && compPeakReductionParam && compMakeupGainParam && compRatioParam && compEnabledParam && compWetDryParam && compCrossoverParam
        && distMixParam);
}

PluginProcessor::~PluginProcessor()
{
}

//==============================================================================
// Studio Distortion DSP Helper Methods
//==============================================================================

inline float PluginProcessor::dcBlock(float sample, float& x1, float& y1)
{
    const float R = 0.995f;
    const float output = sample - x1 + R * y1;
    x1 = sample;
    y1 = output;
    return output;
}

inline float PluginProcessor::onePoleLowpass(float sample, float& state, float cutoffHz, float sampleRate)
{
    const float omega = 2.0f * juce::MathConstants<float>::pi * cutoffHz / sampleRate;
    const float alpha = omega / (omega + 1.0f);
    const float output = state + alpha * (sample - state);
    state = output;
    return output;
}

float PluginProcessor::applyStudioDistortion(float x, float gain, float drive, int clipType)
{
    x = x * gain;
    float y = 0.0f;

    switch (clipType)
    {
    case 0:  // Enhanced Tanh with asymmetric bias
    {
        y = std::tanh(x * drive * 1.5f);
        y = (y > 0.0f) ? y * (1.0f - 0.12f * y) : y * (1.0f - 0.05f * y);
        y *= 0.9f;
        break;
    }
    case 1:  // Soft knee with compression above threshold
    {
        y = x * drive * 1.3f;
        const float abs_y = std::abs(y);
        if (abs_y > 0.5f)
        {
            const float sign = (y > 0.0f) ? 1.0f : -1.0f;
            const float over = abs_y - 0.5f;
            const float compressed = 0.5f + over * 0.5f;
            y = sign * std::tanh(compressed) * 0.92f;
        }
        else
        {
            y = std::tanh(y) * 0.95f;
        }
        break;
    }
    case 2:  // Dynamic ratio compression
    {
        y = x * drive * 1.8f;
        const float abs_y = std::abs(y);
        if (abs_y > 0.35f)
        {
            const float sign = (y > 0.0f) ? 1.0f : -1.0f;
            const float ratio = 2.5f + abs_y * 2.0f;
            const float over = abs_y - 0.35f;
            const float compressed = 0.35f + over / ratio;
            y = sign * compressed * 0.95f;
        }
        else
        {
            y *= 0.98f;
        }
        y = std::tanh(y);
        break;
    }
    case 3:  // Multi-stage hard clipping
    {
        y = x * drive * 1.2f;
        const float abs_y = std::abs(y);
        float compressed;
        if (abs_y < 0.5f)
        {
            compressed = abs_y;
        }
        else if (abs_y < 1.2f)
        {
            compressed = 0.5f + (abs_y - 0.5f) * 0.6f;
        }
        else
        {
            compressed = 0.92f + (abs_y - 1.2f) * 0.05f;
            compressed = juce::jmin(compressed, 0.98f);
        }
        y = (x > 0.0f ? 1.0f : -1.0f) * compressed;
        break;
    }
    case 4:  // Tanh with 2nd harmonic boost
    {
        y = std::tanh(x * drive * 1.4f);
        y = y + 0.08f * y * y * (y > 0.0f ? 1.0f : -1.0f);
        y = std::tanh(y * 1.2f) * 0.9f;
        break;
    }
    case 5:  // Asymmetric clipping with different thresholds
    {
        y = x * drive * 2.0f;
        if (y > 0.6f)
        {
            y = 0.6f + std::tanh((y - 0.6f) * 3.0f) * 0.3f;
        }
        else if (y < -0.7f)
        {
            y = -0.7f + std::tanh((y + 0.7f) * 2.5f) * 0.25f;
        }
        y *= 0.95f;
        break;
    }
    case 6:  // Hard limiting with soft transition
    {
        y = x * drive * 2.5f;
        if (y > 0.8f)
        {
            y = 0.8f + (y - 0.8f) * 0.1f;
        }
        else if (y < -0.8f)
        {
            y = -0.8f + (y + 0.8f) * 0.1f;
        }
        y = juce::jlimit(-0.9f, 0.9f, y);
        break;
    }
    default:  // Fallback: simple tanh
        y = std::tanh(x * drive) * 0.95f;
        break;
    }

    return y;
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
    // Store sample rate for LFO calculations
    currentSampleRate = static_cast<float>(sampleRate);
    lfoPhase = 0.0f;  // Reset LFO phase
    
    const int numChannels = std::max(1, getTotalNumInputChannels());
    smoothedInputGain.reset(sampleRate, DSPConstants::GAIN_SMOOTH_TIME_S);
    smoothedOutputGain.reset(sampleRate, DSPConstants::GAIN_SMOOTH_TIME_S);
    smoothedDistortion.reset(sampleRate, DSPConstants::DISTORTION_SMOOTH_TIME_S);
    compEnvelopeState = 0.0f;
    compRmsHistory = 0.0f;
    tubeWarmth = 0.0f;

    // Initialize studio distortion state vectors (per-channel)
    dc_x1.resize(numChannels, 0.0f);
    dc_y1.resize(numChannels, 0.0f);
    pre_lp_z.resize(numChannels, 0.0f);
    post_lp_z.resize(numChannels, 0.0f);

    // Initialize smoothed gain reduction with slow release (LA-2A style)
    smoothedGainReduction.reset(sampleRate, DSPConstants::COMP_GR_SMOOTH_TIME_S);
    smoothedGainReduction.setCurrentAndTargetValue(1.0f);  // Start at unity (no reduction)


    scopeBuffer.setSize(2, DSPConstants::SCOPE_BUFFER_SIZE);
    scopeBuffer.clear();
    scopeFifo.setTotalSize(DSPConstants::SCOPE_BUFFER_SIZE);



    // Oversampling
if (!oversampling || currentNumChannels != numChannels) {
    oversampling = std::make_unique<juce::dsp::Oversampling<float>>(
        numChannels, DSPConstants::OVERSAMPLING_STAGES,
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

    // Initialize with default frequency, but DON'T cache it to lastHighPassFreq
    // This allows the first processBlock to set the correct frequency after state restoration
    *preHighPassFilter.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass(spec.sampleRate, DSPConstants::DEFAULT_HIPASS_FREQ);
    preHighPassFilter.prepare(spec);
    preHighPassFilter.reset();

    // Force filter update on first processBlock (especially important for DAW state restoration)
    lastHighPassFreq = -1.0f;

    // DC BLOCKING 1
    *dcBlockingFilter.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass(spec.sampleRate, DSPConstants::DC_BLOCKING_FREQ);
    dcBlockingFilter.prepare(spec);
    dcBlockingFilter.reset();

    // 808-Safe distortion filters (Linkwitz-Riley crossover, oversampled domain)
    const float crossoverFreq = DSPConstants::DISTORTION_CROSSOVER_FREQ;

    *lowPassFilter1.state = *juce::dsp::IIR::Coefficients<float>::makeLowPass(spec.sampleRate, crossoverFreq);
    lowPassFilter1.prepare(spec);
    lowPassFilter1.reset();

    *lowPassFilter2.state = *juce::dsp::IIR::Coefficients<float>::makeLowPass(spec.sampleRate, crossoverFreq);
    lowPassFilter2.prepare(spec);
    lowPassFilter2.reset();

    *highPassFilter1.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass(spec.sampleRate, crossoverFreq);
    highPassFilter1.prepare(spec);
    highPassFilter1.reset();

    *highPassFilter2.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass(spec.sampleRate, crossoverFreq);
    highPassFilter2.prepare(spec);
    highPassFilter2.reset();

    //  DC BLOCK #2 (Normal Rate - After Compression)
    juce::dsp::ProcessSpec normalSpec;
    normalSpec.sampleRate = sampleRate;  // Normal rate, not oversampled
    normalSpec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    normalSpec.numChannels = static_cast<juce::uint32>(numChannels);

    *dcBlockingFilter2.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass(normalSpec.sampleRate, DSPConstants::DC_BLOCKING_FREQ);
    dcBlockingFilter2.prepare(normalSpec);
    dcBlockingFilter2.reset();

    // Compression crossover filters (normal sample rate)
    const float compCrossoverFreq = compCrossoverParam ? compCrossoverParam->load() : DSPConstants::DEFAULT_COMP_CROSSOVER;

    *compLowPassFilter1.state = *juce::dsp::IIR::Coefficients<float>::makeLowPass(normalSpec.sampleRate, compCrossoverFreq);
    compLowPassFilter1.prepare(normalSpec);
    compLowPassFilter1.reset();

    *compLowPassFilter2.state = *juce::dsp::IIR::Coefficients<float>::makeLowPass(normalSpec.sampleRate, compCrossoverFreq);
    compLowPassFilter2.prepare(normalSpec);
    compLowPassFilter2.reset();

    *compHighPassFilter1.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass(normalSpec.sampleRate, compCrossoverFreq);
    compHighPassFilter1.prepare(normalSpec);
    compHighPassFilter1.reset();

    *compHighPassFilter2.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass(normalSpec.sampleRate, compCrossoverFreq);
    compHighPassFilter2.prepare(normalSpec);
    compHighPassFilter2.reset();


    // Prepare buffers for band-split processing (oversampled size)
    const int oversampledBlockSize = samplesPerBlock * static_cast<int>(oversamplingFactor);
    lowBandBuffer.setSize(numChannels, oversampledBlockSize);
    highBandBuffer.setSize(numChannels, oversampledBlockSize);

    // Compression buffers (normal sample rate)
    compLowBandBuffer.setSize(numChannels, samplesPerBlock);
    compHighBandBuffer.setSize(numChannels, samplesPerBlock);
    compDryBuffer.setSize(numChannels, samplesPerBlock);
}



void PluginProcessor::releaseResources()
{
    oversampling.reset();
    preHighPassFilter.reset();
    dcBlockingFilter.reset();
    dcBlockingFilter2.reset();
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
    const float threshold = DSPConstants::COMP_THRESHOLD_MIN_DB +
                          (peakReduction * DSPConstants::COMP_THRESHOLD_RANGE_DB / 100.0f);

    // Ratio: Compress mode = 3:1, Limit mode = 12:1 (LA-2A style)
    const float ratio = (ratioMode == 0) ? DSPConstants::COMP_RATIO_COMPRESS : DSPConstants::COMP_RATIO_LIMIT;

    // Map makeup gain (0-100, 50=unity) to dB
    const float makeupGainDB = (makeupGain - 50.0f) * (DSPConstants::COMP_MAKEUP_RANGE_DB / 50.0f);
    const float makeupGainLinear = juce::Decibels::decibelsToGain(makeupGainDB);

    // Optical cell timing (program-dependent)
    const float attackCoeff = DSPConstants::COMP_ATTACK_COEFF;
    const float releaseCoeff = DSPConstants::COMP_RELEASE_COEFF;

    float maxGainReductionDB = 0.0f;  // Track max gain reduction for meter display

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
        compRmsHistory = DSPConstants::COMP_RMS_HISTORY_COEFF * compRmsHistory +
                        (1.0f - DSPConstants::COMP_RMS_HISTORY_COEFF) * rms;

        // Convert to dB
        const float inputLevelDB = juce::Decibels::gainToDecibels(rms + 0.00001f);

        // Calculate gain reduction needed
        float gainReductionDB = 0.0f;
        if (inputLevelDB > threshold)
        {
            const float overThresholdDB = inputLevelDB - threshold;

            // Soft knee for smooth LA-2A character
            const float kneeWidth = DSPConstants::COMP_KNEE_WIDTH_DB;
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

        // Track maximum gain reduction for meter display
        maxGainReductionDB = juce::jmax(maxGainReductionDB, gainReductionDB);

        // Apply compression and makeup gain to all channels
        for (int channel = 0; channel < numChannels; ++channel)
        {
            float sampleValue = buffer.getSample(channel, sample);

            // Apply compression
            sampleValue *= compEnvelopeState;

            // Tube harmonic generation (even harmonics for warmth)
            const float tubeInput = sampleValue * DSPConstants::COMP_TUBE_DRIVE;
            const float tubeSaturation = std::tanh(tubeInput);

            // Blend tube character (subtle 2nd harmonic)
            sampleValue = sampleValue * (1.0f - DSPConstants::COMP_TUBE_BLEND) +
                         tubeSaturation * DSPConstants::COMP_TUBE_BLEND;

            // Apply makeup gain
            sampleValue *= makeupGainLinear;

            // Soft clip output (prevent overs from makeup gain)
            sampleValue = std::tanh(sampleValue * 0.9f) * 1.1f;

            buffer.setSample(channel, sample, sampleValue);
        }
    }

    // Store the max gain reduction for UI meter display
    currentGainReductionDB.store(maxGainReductionDB, std::memory_order_relaxed);
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
    const float compWetDry = compWetDryParam->load();
    const float compCrossover = compCrossoverParam->load();
    const float distMix = distMixParam->load();

    // Scale to actual ranges for processing (studio distortion style)
    const float inputGain = std::pow(inGainParam / 50.0f, 1.5f) * 0.7f;
    const auto outGainDB = (outGainParam - 50.0f) * 0.24f;  // Maps 0->-12dB, 50->0dB, 100->+12dB
    const auto outGain = juce::Decibels::decibelsToGain(outGainDB);

    // LFO modulation for dynamic distortion effects
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
    const float lfoModulation = (lfoValue * lfoDepth / 100.0f);  // -1 to +1 scaled by depth
    const float modulatedDistortionParam = juce::jlimit(0.0f, 100.0f, distortionParam + lfoModulation * 50.0f);
    const float distortionDrive = 1.0f + (modulatedDistortionParam / 100.0f) * 3.5f;  // Studio style: 1.0 to 4.5

    // Mix amount for wet/dry blend
    const float mixAmount = distMix / 100.0f;  // 0.0 to 1.0

    smoothedInputGain.setTargetValue(inputGain);
    smoothedOutputGain.setTargetValue(outGain);
    smoothedDistortion.setTargetValue(distortionDrive);


    // Wrap original buffer into an AudioBlock
    auto inputBlock = juce::dsp::AudioBlock<float>(buffer);
    auto oversampledBlock = oversampling->processSamplesUp(inputBlock);

    // Calculate oversampled sample rate for one-pole filters
    const float oversampledSampleRate = currentSampleRate * static_cast<float>(oversamplingFactor);

    // Update pre-filter coefficients if frequency changed
    if (!preHighPassFilter.state || std::abs(highPassFreq - lastHighPassFreq) > 0.01f)
    {
        // Update coefficients IN-PLACE (dereference both sides)
        *preHighPassFilter.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass(
            getSampleRate() * oversamplingFactor, highPassFreq);

        lastHighPassFreq = highPassFreq;
    }

    // Pre-filtering
    preHighPassFilter.process(juce::dsp::ProcessContextReplacing<float>(oversampledBlock));


    const size_t numSamples = oversampledBlock.getNumSamples();
    const size_t numChannels = oversampledBlock.getNumChannels();

    // Distortion processing with optional band-split (808-Safe mode)
    if (bandSplitEnabled)
    {
        // === BAND-SPLIT MODE: Clean low + Distorted high ===

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

        // Apply studio distortion ONLY to the high band (with pre/post LP + DC block)
        for (size_t sample = 0; sample < numSamples; ++sample)
        {
            const float currentInputGain = smoothedInputGain.getNextValue();
            const float currentDrive = smoothedDistortion.getNextValue();

            for (size_t channel = 0; channel < numChannels; ++channel)
            {
                auto* highBandData = highBandBuffer.getWritePointer(static_cast<int>(channel));
                const int channelIdx = static_cast<int>(channel);

                // Read input (already band-split)
                float inputSample = highBandData[sample];
                const float drySample = inputSample;  // Store for mix

                // Pre-distortion lowpass at 12kHz (oversampled rate)
                inputSample = onePoleLowpass(inputSample, pre_lp_z[channelIdx], 12000.0f, oversampledSampleRate);

                // Apply studio distortion
                float distorted = applyStudioDistortion(inputSample, currentInputGain, currentDrive, clipType);

                // Post-distortion lowpass at 10kHz (oversampled rate)
                distorted = onePoleLowpass(distorted, post_lp_z[channelIdx], 10000.0f, oversampledSampleRate);

                // DC blocking
                distorted = dcBlock(distorted, dc_x1[channelIdx], dc_y1[channelIdx]);

                // Wet/Dry mix
                const float mixed = drySample * (1.0f - mixAmount) + distorted * mixAmount;

                // Final output saturation
                highBandData[sample] = std::tanh(mixed * 1.2f) * 0.95f;
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
        // === NORMAL MODE: Full-range studio distortion (with pre/post LP + DC block) ===
        for (size_t sample = 0; sample < numSamples; ++sample)
        {
            const float currentInputGain = smoothedInputGain.getNextValue();
            const float currentDrive = smoothedDistortion.getNextValue();

            for (size_t channel = 0; channel < numChannels; ++channel)
            {
                auto* channelData = oversampledBlock.getChannelPointer(channel);
                const int channelIdx = static_cast<int>(channel);

                // Read input
                float inputSample = channelData[sample];
                const float drySample = inputSample;  // Store for mix

                // Pre-distortion lowpass at 12kHz (oversampled rate)
                inputSample = onePoleLowpass(inputSample, pre_lp_z[channelIdx], 12000.0f, oversampledSampleRate);

                // Apply studio distortion
                float distorted = applyStudioDistortion(inputSample, currentInputGain, currentDrive, clipType);

                // Post-distortion lowpass at 10kHz (oversampled rate)
                distorted = onePoleLowpass(distorted, post_lp_z[channelIdx], 10000.0f, oversampledSampleRate);

                // DC blocking
                distorted = dcBlock(distorted, dc_x1[channelIdx], dc_y1[channelIdx]);

                // Wet/Dry mix
                const float mixed = drySample * (1.0f - mixAmount) + distorted * mixAmount;

                // Final output saturation
                channelData[sample] = std::tanh(mixed * 1.2f) * 0.95f;
            }
        }
    }

    // DC blocking AFTER all distortion processing, BEFORE downsampling
    dcBlockingFilter.process(juce::dsp::ProcessContextReplacing<float>(oversampledBlock));

    // Downsample back into original buffer
    oversampling->processSamplesDown(inputBlock);

    // Parallel compression (LA-2A style, normal sample rate)
    if (compEnabled && compPeakReduction > 0.0f)
    {
        const int normalNumSamples = buffer.getNumSamples();
        const int normalNumChannels = buffer.getNumChannels();

        // Store DRY signal for parallel blending
        for (int channel = 0; channel < normalNumChannels; ++channel)
        {
            compDryBuffer.copyFrom(channel, 0, buffer, channel, 0, normalNumSamples);
        }

        // Create WET signal: Split → Compress low only → Recombine
        compLowBandBuffer.clear();
        compHighBandBuffer.clear();

        // Copy main buffer to both bands
        for (int channel = 0; channel < normalNumChannels; ++channel)
        {
            compLowBandBuffer.copyFrom(channel, 0, buffer, channel, 0, normalNumSamples);
            compHighBandBuffer.copyFrom(channel, 0, buffer, channel, 0, normalNumSamples);
        }

        // Update crossover frequency dynamically
        const double normalSampleRate = getSampleRate();
        *compLowPassFilter1.state = *juce::dsp::IIR::Coefficients<float>::makeLowPass(normalSampleRate, compCrossover);
        *compLowPassFilter2.state = *juce::dsp::IIR::Coefficients<float>::makeLowPass(normalSampleRate, compCrossover);
        *compHighPassFilter1.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass(normalSampleRate, compCrossover);
        *compHighPassFilter2.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass(normalSampleRate, compCrossover);

        // Create blocks for filtering
        auto lowBlock = juce::dsp::AudioBlock<float>(compLowBandBuffer).getSubBlock(0, static_cast<size_t>(normalNumSamples));
        auto highBlock = juce::dsp::AudioBlock<float>(compHighBandBuffer).getSubBlock(0, static_cast<size_t>(normalNumSamples));

        // Apply Linkwitz-Riley crossover (4th order)
        compLowPassFilter1.process(juce::dsp::ProcessContextReplacing<float>(lowBlock));
        compLowPassFilter2.process(juce::dsp::ProcessContextReplacing<float>(lowBlock));

        compHighPassFilter1.process(juce::dsp::ProcessContextReplacing<float>(highBlock));
        compHighPassFilter2.process(juce::dsp::ProcessContextReplacing<float>(highBlock));

        // Compress LOW BAND ONLY
        applyLA2ACompression(compLowBandBuffer, compPeakReduction, compMakeupGain, compRatioMode);

        // Recombine: Compressed low + Clean high = WET signal (store in main buffer)
        for (int channel = 0; channel < normalNumChannels; ++channel)
        {
            auto* mainData = buffer.getWritePointer(channel);
            const auto* lowData = compLowBandBuffer.getReadPointer(channel);
            const auto* highData = compHighBandBuffer.getReadPointer(channel);

            for (int sample = 0; sample < normalNumSamples; ++sample)
            {
                mainData[sample] = lowData[sample] + highData[sample];  // WET signal
            }
        }

        // Parallel blend: Mix dry and wet signals
        const float wetAmount = compWetDry / 100.0f;  // 0.0 to 1.0
        const float dryAmount = 1.0f - wetAmount;

        for (int channel = 0; channel < normalNumChannels; ++channel)
        {
            auto* mainData = buffer.getWritePointer(channel);
            const auto* dryData = compDryBuffer.getReadPointer(channel);

            for (int sample = 0; sample < normalNumSamples; ++sample)
            {
                mainData[sample] = dryAmount * dryData[sample] + wetAmount * mainData[sample];
            }
        }
    }

    // DC blocking filter #2 (after compression, normal sample rate)
    auto normalBlock = juce::dsp::AudioBlock<float>(buffer);
    dcBlockingFilter2.process(juce::dsp::ProcessContextReplacing<float>(normalBlock));

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
    for (int sample = 0; sample < buffer.getNumSamples(); sample += DSPConstants::SCOPE_UPDATE_DECIMATION)
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
    const juce::SpinLock::ScopedLockType lock(scopeLock);
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
    const juce::SpinLock::ScopedLockType lock(scopeLock);
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
    {
        parameters.replaceState(juce::ValueTree::fromXml(*xmlState));

        // Reset all filters when loading state to prevent stale coefficients/state
        if (preHighPassFilter.state)
            preHighPassFilter.reset();
        if (dcBlockingFilter.state)
            dcBlockingFilter.reset();
        if (dcBlockingFilter2.state)
            dcBlockingFilter2.reset();
        if (lowPassFilter1.state)
            lowPassFilter1.reset();
        if (lowPassFilter2.state)
            lowPassFilter2.reset();
        if (highPassFilter1.state)
            highPassFilter1.reset();
        if (highPassFilter2.state)
            highPassFilter2.reset();
        if (compLowPassFilter1.state)
            compLowPassFilter1.reset();
        if (compLowPassFilter2.state)
            compLowPassFilter2.reset();
        if (compHighPassFilter1.state)
            compHighPassFilter1.reset();
        if (compHighPassFilter2.state)
            compHighPassFilter2.reset();

        // Force filter coefficient update on next processBlock by invalidating cache
        lastHighPassFreq = -1.0f;
    }
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
        DSPConstants::DEFAULT_HIPASS_FREQ));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{ "bandSplitEnabled", 1 },
        "808-Safe Mode",
        false));  // Default is OFF (normal distortion mode)

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ "clipType", 1 },
        "Clip Type",
        juce::StringArray{
            "Studio Tanh",           // 0: Enhanced Tanh with asymmetric bias
            "Soft Knee",             // 1: Soft knee with compression
            "Dynamic Compress",      // 2: Dynamic ratio compression
            "Multi-Stage",           // 3: Multi-stage hard clipping
            "Harmonic",              // 4: Tanh with 2nd harmonic boost
            "Asymmetric",            // 5: Asymmetric clipping
            "Hard Limit"             // 6: Hard limiting
        },
        0));

    // LFO parameters
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

  
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "compWetDry", 1 },
        "Comp Wet/Dry",
        juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f),
        50.0f));  // Default 50 = 50% dry, 50% wet (parallel blend)

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "compCrossover", 1 },
        "Comp Crossover",
        juce::NormalisableRange<float>(150.0f, 350.0f, 1.0f),
        DSPConstants::DEFAULT_COMP_CROSSOVER));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "distMix", 1 },
        "Distortion Mix",
        juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f),
        100.0f));  // Default 100 = 100% wet

    return { params.begin(), params.end() };
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}