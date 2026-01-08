/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <memory>
#include <iostream>

// Include test header in debug builds (tests run from separate test runner)
#if JUCE_DEBUG
#include "Tests/DistortionTests.h"
#endif

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
    lfoWaveformParam = parameters.getRawParameterValue("lfoWaveform");
    waveshaperMixParam = parameters.getRawParameterValue("waveshaperMix");
    compPeakReductionParam = parameters.getRawParameterValue("compPeakReduction");
    compMakeupGainParam = parameters.getRawParameterValue("compMakeupGain");
    compRatioParam = parameters.getRawParameterValue("compRatio");
    compEnabledParam = parameters.getRawParameterValue("compEnabled");
    compWetDryParam = parameters.getRawParameterValue("compWetDry");
    compCrossoverParam = parameters.getRawParameterValue("compCrossover");
    distMixParam = parameters.getRawParameterValue("distMix");
    toneParam = parameters.getRawParameterValue("tone");
    // Verify all parameters were found
    jassert(inputGainParam && outputGainParam && distortionAmountParam
        && highPassFreqParam && bandSplitEnabledParam && clipTypeParam
        && lfoRateParam && lfoDepthParam && lfoWaveformParam && waveshaperMixParam
        && compPeakReductionParam && compMakeupGainParam && compRatioParam && compEnabledParam && compWetDryParam && compCrossoverParam
        && distMixParam && toneParam);

}

PluginProcessor::~PluginProcessor()
{
}

//==============================================================================
// Studio Distortion DSP Helper Methods
//==============================================================================

float PluginProcessor::applyStudioDistortion(float x, float gain, float drive, int clipType)
{
    x = x * gain;
    float y = 0.0f;

    switch (clipType)
    {
    case 0:  // BRUTAL FUZZ - Aggressive hard clipping with analog noise
    {
        // Heavy drive for maximum grit
        y = x * drive * 4.5f;

        // Hard clip with brutal threshold
        const float threshold = 0.3f;  // Very low threshold for aggressive clipping
        if (y > threshold)
            y = threshold + std::atan((y - threshold) * 2.0f) * 0.2f;
        else if (y < -threshold)
            y = -threshold + std::atan((y + threshold) * 2.0f) * 0.2f;

        // Add subtle analog noise for warmth (using instance member, not static)
        y += distortionRandom.nextFloat() * 0.005f - 0.0025f;

        // Final saturation
        y = std::tanh(y * 1.8f);
        break;
    }
    case 1:  // TUBE OVERDRIVE - Asymmetric tube saturation with harmonics
    {
        // Tube-style asymmetric clipping (positive clips harder)
        y = x * drive * 2.8f;

        // Asymmetric waveshaping (vintage tube behavior)
        if (y > 0.0f)
        {
            // Positive: harder clipping with even harmonics
            y = std::tanh(y * 1.6f) * 0.85f;
            y += 0.15f * y * y;  // 2nd harmonic
        }
        else
        {
            // Negative: softer clipping with odd harmonics
            y = std::tanh(y * 1.2f) * 0.9f;
            y += 0.08f * y * y * y;  // 3rd harmonic
        }

        // Add subtle warmth
        y = y + 0.05f * std::sin(y * juce::MathConstants<float>::pi);
        break;
    }
    case 2:  // BIT CRUSHER - Digital destruction with sample rate reduction
    {
        // Extreme bit reduction for digital grit
        const float bits = 6.0f;  // Brutal bit depth
        const float maxValue = std::pow(2.0f, bits - 1.0f);

        y = x * drive * 3.2f;

        // Bit crushing
        y = std::floor(y * maxValue) / maxValue;

        // Add aliasing character
        y = std::tanh(y * 2.5f);

        // Hard clip for extra grit
        y = juce::jlimit(-0.95f, 0.95f, y);
        break;
    }
    case 3:  // TAPE SATURATION - Analog tape with hysteresis
    {
        // Tape-style soft saturation with magnetic hysteresis simulation
        y = x * drive * 2.5f;

        // Tape compression curve (progressive)
        const float abs_y = std::abs(y);
        const float sign = (y > 0.0f) ? 1.0f : -1.0f;

        if (abs_y < 0.4f)
            y = y * 1.05f;  // Slight boost in quiet regions
        else if (abs_y < 1.0f)
            y = sign * (0.42f + (abs_y - 0.4f) * 0.7f);
        else
            y = sign * (0.84f + std::tanh((abs_y - 1.0f) * 2.0f) * 0.15f);

        // Add tape warmth (subtle even harmonics)
        y += 0.12f * y * y * sign;

        // Final soft saturation
        y = std::tanh(y * 1.3f) * 0.92f;
        break;
    }
    case 4:  // TRANSFORMER SATURATION - Heavy harmonic distortion
    {
        // Transformer-style saturation with rich harmonics
        y = x * drive * 3.0f;

        // Multi-stage waveshaping for complex harmonics
        y = std::tanh(y * 1.5f);

        // Add rich harmonic content
        const float fundamental = y;
        const float harmonic2 = 0.25f * fundamental * fundamental * (fundamental > 0.0f ? 1.0f : -1.0f);
        const float harmonic3 = 0.15f * fundamental * fundamental * fundamental;
        const float harmonic5 = 0.08f * std::pow(std::abs(fundamental), 5.0f) * (fundamental > 0.0f ? 1.0f : -1.0f);

        y = fundamental + harmonic2 + harmonic3 + harmonic5;

        // Final limiting
        y = std::tanh(y * 1.4f) * 0.88f;
        break;
    }
    case 5:  // DIODE CLIPPER - Asymmetric diode clipping with grit
    {
        // Asymmetric diode-style clipping (forward/reverse bias difference)
        y = x * drive * 3.8f;

        // Asymmetric clipping (simulating diode forward voltage)
        if (y > 0.5f)
        {
            // Forward bias: hard clip at ~0.7V
            y = 0.5f + std::atan((y - 0.5f) * 4.0f) * 0.15f;
        }
        else if (y < -0.6f)
        {
            // Reverse bias: slightly different threshold
            y = -0.6f + std::atan((y + 0.6f) * 3.5f) * 0.2f;
        }

        // Add crossover distortion character
        if (std::abs(y) < 0.05f)
            y *= 0.7f;  // Dead zone near zero crossing

        // Final saturation
        y = std::tanh(y * 2.2f) * 0.9f;
        break;
    }
    case 6:  // DECIMATOR - Extreme digital destruction
    {
        // Brutal digital destruction with severe aliasing
        y = x * drive * 5.0f;

        // Sample & hold for brutal aliasing
        const float foldback = 4.0f;
        y = std::fmod(y + 2.0f, 2.0f * foldback) - foldback;

        // Hard clip with fold-back
        while (y > 1.0f) y = 2.0f - y;
        while (y < -1.0f) y = -2.0f - y;

        // Add harmonic distortion
        y = std::tanh(y * 2.8f);

        // Brutal final limiting
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
// LFO Waveform Generator
// Generates different waveform shapes from 0-1 phase input
// Returns values in -1 to +1 range for modulation
//==============================================================================
float PluginProcessor::generateLFOWaveform(float phase, int waveformType)
{
    float value = 0.0f;

    switch (waveformType)
    {
    case 0:  // Sine wave
        value = std::sin(phase * 2.0f * juce::MathConstants<float>::pi);
        break;

    case 1:  // Triangle wave
        if (phase < 0.25f)
            value = phase * 4.0f;  // Rising 0 to 1
        else if (phase < 0.75f)
            value = 2.0f - (phase * 4.0f);  // Falling 1 to -1
        else
            value = -4.0f + (phase * 4.0f);  // Rising -1 to 0
        break;

    case 2:  // Square wave
        value = (phase < 0.5f) ? 1.0f : -1.0f;
        break;

    case 3:  // Sawtooth wave (rising)
        value = (phase * 2.0f) - 1.0f;  // Linear ramp from -1 to +1
        break;

    case 4:  // Random (sample & hold)
    {
        // Detect phase reset (when phase wraps from ~1 to ~0)
        // Using instance member variables instead of static to support multi-instance
        if (phase < lfoLastPhase)
        {
            lfoRandomValue = distortionRandom.nextFloat() * 2.0f - 1.0f;  // -1 to +1
        }
        lfoLastPhase = phase;
        value = lfoRandomValue;
        break;
    }
    default:  // Fallback to sine
        value = std::sin(phase * 2.0f * juce::MathConstants<float>::pi);
        break;
    }

    return value;
}

//==============================================================================
// Update all sample-rate-dependent coefficients
// This should be called whenever sample rate changes
// Ensures accurate time constants at any sample rate: 44.1, 48, 88.2, 96, 176.4, 192 kHz
//==============================================================================
void PluginProcessor::updateSampleRateDependentCoefficients(double sampleRate)
{
    juce::Logger::writeToLog("--- Updating sample-rate-dependent coefficients ---");
    juce::Logger::writeToLog("Base sample rate: " + juce::String(sampleRate, 1) + " Hz");

    // SAFETY: Validate sample rate to prevent NaN in coefficient calculation
    // Valid audio sample rates are typically 8kHz to 384kHz
    if (sampleRate < 1000.0 || sampleRate > 500000.0 || std::isnan(sampleRate) || std::isinf(sampleRate))
    {
        juce::Logger::writeToLog("ERROR: Invalid sample rate " + juce::String(sampleRate) + " Hz! Using 44100 Hz fallback.");
        sampleRate = 44100.0;  // Safe fallback
    }

    // Calculate sample-rate-dependent compression coefficients
    // Formula: coeff = exp(-1.0 / (timeConstant * sampleRate))
    // These work at normal sample rate (after downsampling)
    compAttackCoeff = std::exp(-1.0f / (DSPConstants::COMP_ATTACK_TIME_S * static_cast<float>(sampleRate)));
    compReleaseCoeff = std::exp(-1.0f / (DSPConstants::COMP_RELEASE_TIME_S * static_cast<float>(sampleRate)));
    compRmsHistoryCoeff = std::exp(-1.0f / (DSPConstants::COMP_RMS_HISTORY_TIME_S * static_cast<float>(sampleRate)));

    // SAFETY: Validate calculated coefficients (should always be between 0 and 1)
    if (std::isnan(compAttackCoeff) || compAttackCoeff < 0.0f || compAttackCoeff > 1.0f)
    {
        juce::Logger::writeToLog("ERROR: Invalid compAttackCoeff! Using fallback 0.9995");
        compAttackCoeff = 0.9995f;
    }
    if (std::isnan(compReleaseCoeff) || compReleaseCoeff < 0.0f || compReleaseCoeff > 1.0f)
    {
        juce::Logger::writeToLog("ERROR: Invalid compReleaseCoeff! Using fallback 0.99995");
        compReleaseCoeff = 0.99995f;
    }
    if (std::isnan(compRmsHistoryCoeff) || compRmsHistoryCoeff < 0.0f || compRmsHistoryCoeff > 1.0f)
    {
        juce::Logger::writeToLog("ERROR: Invalid compRmsHistoryCoeff! Using fallback 0.99");
        compRmsHistoryCoeff = 0.99f;
    }

    juce::Logger::writeToLog("Compression coefficients - Attack: " + juce::String(compAttackCoeff, 6)
        + ", Release: " + juce::String(compReleaseCoeff, 6)
        + ", RMS History: " + juce::String(compRmsHistoryCoeff, 6));

    // Store the sample rate to detect changes
    lastSampleRate = sampleRate;

    juce::Logger::writeToLog("Coefficient update complete");
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
    // Log sample rate initialization for diagnostics
    juce::Logger::writeToLog("=== prepareToPlay called ===");
    juce::Logger::writeToLog("Sample rate: " + juce::String(sampleRate) + " Hz");
    juce::Logger::writeToLog("Samples per block: " + juce::String(samplesPerBlock));

    // Store sample rate for LFO calculations
    currentSampleRate = static_cast<float>(sampleRate);
    lfoPhase = 0.0f;  // Reset LFO phase

    const int numChannels = std::max(1, getTotalNumInputChannels());

    // Output gain is applied AFTER downsampling at normal sample rate
    smoothedOutputGain.reset(sampleRate, DSPConstants::GAIN_SMOOTH_TIME_S);
    smoothedOutputGain.setCurrentAndTargetValue(1.0f);  // Initialize to unity gain
    compEnvelopeState = 0.0f;
    compRmsHistory = 0.0f;
    tubeWarmth = 0.0f;

    // Reset manual DC blocker state
    for (int ch = 0; ch < 2; ++ch)
    {
        manualDCBlockerPrevInput[ch] = 0.0f;
        manualDCBlockerPrevOutput[ch] = 0.0f;
    }

    // Initialize parameter interpolation state to current parameter values
    const auto inGainParam = inputGainParam ? inputGainParam->load() : 50.0f;
    const auto distParam = distortionAmountParam ? distortionAmountParam->load() : 0.0f;
    lastInputGain = std::pow(inGainParam / 50.0f, 1.5f);  // Match processBlock calculation
    lastDistortionDrive = 1.0f + (distParam / 100.0f) * 7.0f;  // Match processBlock calculation (brutal range)

    // Update all sample-rate-dependent coefficients (DC blocking, compression, etc.)
    updateSampleRateDependentCoefficients(sampleRate);

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

    // CRITICAL: For JUCE IIR ProcessorDuplicator filters, the order MUST be:
    // 1. prepare() - creates the internal state
    // 2. set coefficients - applies to the created state
    // 3. reset() - clears the filter history

    // Initialize with default frequency, but DON'T cache it to lastHighPassFreq
    // This allows the first processBlock to set the correct frequency after state restoration
    // CRITICAL: Use first-order filter for numerical stability at low frequencies in oversampled domain
    // Second-order (biquad) filters become unstable at very low frequency ratios (e.g., 20Hz at 176.4kHz)
    preHighPassFilter.prepare(spec);
    *preHighPassFilter.state = *juce::dsp::IIR::Coefficients<float>::makeFirstOrderHighPass(spec.sampleRate, DSPConstants::DEFAULT_HIPASS_FREQ);
    preHighPassFilter.reset();

    // Force filter update on first processBlock (especially important for DAW state restoration)
    lastHighPassFreq = -1.0f;

    // DC BLOCKING 1 (oversampled rate)
    // CRITICAL: Use first-order filter for numerical stability at 5Hz with oversampled rate (~176kHz)
    dcBlockingFilter.prepare(spec);
    *dcBlockingFilter.state = *juce::dsp::IIR::Coefficients<float>::makeFirstOrderHighPass(spec.sampleRate, DSPConstants::DC_BLOCKING_FREQ);
    dcBlockingFilter.reset();

    // Post-distortion tone filter (oversampled rate) - lowpass for darkness/brightness control
    toneFilter.prepare(spec);
    const float initialToneFreq = toneParam ? toneParam->load() : 20000.0f;
    *toneFilter.state = *juce::dsp::IIR::Coefficients<float>::makeLowPass(spec.sampleRate, initialToneFreq);
    toneFilter.reset();
    lastToneFreq = initialToneFreq;

    // 808-Safe distortion filters (Linkwitz-Riley crossover, oversampled domain)
    const float crossoverFreq = DSPConstants::DISTORTION_CROSSOVER_FREQ;

    lowPassFilter1.prepare(spec);
    *lowPassFilter1.state = *juce::dsp::IIR::Coefficients<float>::makeLowPass(spec.sampleRate, crossoverFreq);
    lowPassFilter1.reset();

    lowPassFilter2.prepare(spec);
    *lowPassFilter2.state = *juce::dsp::IIR::Coefficients<float>::makeLowPass(spec.sampleRate, crossoverFreq);
    lowPassFilter2.reset();

    highPassFilter1.prepare(spec);
    *highPassFilter1.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass(spec.sampleRate, crossoverFreq);
    highPassFilter1.reset();

    highPassFilter2.prepare(spec);
    *highPassFilter2.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass(spec.sampleRate, crossoverFreq);
    highPassFilter2.reset();

    // Store oversampled sample rate for change detection
    lastOversampledSampleRate = spec.sampleRate;

    //  DC BLOCK #2 (Normal Rate - After Compression)
    juce::dsp::ProcessSpec normalSpec;
    normalSpec.sampleRate = sampleRate;  // Normal rate, not oversampled
    normalSpec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    normalSpec.numChannels = static_cast<juce::uint32>(numChannels);

    // Compression crossover filters (normal sample rate)
    const float compCrossoverFreq = compCrossoverParam ? compCrossoverParam->load() : DSPConstants::DEFAULT_COMP_CROSSOVER;

    compLowPassFilter1.prepare(normalSpec);
    *compLowPassFilter1.state = *juce::dsp::IIR::Coefficients<float>::makeLowPass(normalSpec.sampleRate, compCrossoverFreq);
    compLowPassFilter1.reset();

    compLowPassFilter2.prepare(normalSpec);
    *compLowPassFilter2.state = *juce::dsp::IIR::Coefficients<float>::makeLowPass(normalSpec.sampleRate, compCrossoverFreq);
    compLowPassFilter2.reset();

    compHighPassFilter1.prepare(normalSpec);
    *compHighPassFilter1.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass(normalSpec.sampleRate, compCrossoverFreq);
    compHighPassFilter1.reset();

    compHighPassFilter2.prepare(normalSpec);
    *compHighPassFilter2.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass(normalSpec.sampleRate, compCrossoverFreq);
    compHighPassFilter2.reset();


    // Prepare buffers for band-split processing (oversampled size)
    // CRITICAL: JUCE's oversampling can produce variable output sizes depending on:
    // 1. Internal filter latency compensation
    // 2. Sample rate (44.1kHz vs 48kHz have different characteristics)
    // 3. Block size alignment requirements
    // SOLUTION: Allocate 2x the expected size to handle all edge cases safely
    const size_t expectedOversampledSize = static_cast<size_t>(samplesPerBlock) * oversamplingFactor;
    const size_t oversamplingLatencySamples = static_cast<size_t>(oversampling->getLatencyInSamples());
    // Use 2x multiplier + latency + 128 sample safety margin for absolute safety
    const int oversampledBlockSize = static_cast<int>((expectedOversampledSize + oversamplingLatencySamples) * 2 + 128);
    lowBandBuffer.setSize(numChannels, oversampledBlockSize, false, false, true);
    highBandBuffer.setSize(numChannels, oversampledBlockSize, false, false, true);

    // Log critical values for debugging 44.1kHz issues (always enabled for diagnostics)
    juce::Logger::writeToLog("Distortion prepareToPlay - sampleRate: " + juce::String(sampleRate)
        + ", samplesPerBlock: " + juce::String(samplesPerBlock)
        + ", oversamplingFactor: " + juce::String((int)oversamplingFactor)
        + ", oversamplingLatency: " + juce::String((int)oversamplingLatencySamples)
        + ", allocatedBufferSize: " + juce::String(oversampledBlockSize));

    // Compression buffers (normal sample rate)
    compLowBandBuffer.setSize(numChannels, samplesPerBlock, false, false, true);
    compHighBandBuffer.setSize(numChannels, samplesPerBlock, false, false, true);
    compDryBuffer.setSize(numChannels, samplesPerBlock, false, false, true);
}



void PluginProcessor::releaseResources()
{
    oversampling.reset();
    preHighPassFilter.reset();
    dcBlockingFilter.reset();
    toneFilter.reset();

    // Reset manual DC blocker state
    for (int ch = 0; ch < 2; ++ch)
    {
        manualDCBlockerPrevInput[ch] = 0.0f;
        manualDCBlockerPrevOutput[ch] = 0.0f;
    }
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

    // Optical cell timing (program-dependent, sample-rate-dependent coefficients)
    const float attackCoeff = compAttackCoeff;
    const float releaseCoeff = compReleaseCoeff;

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

        // Update RMS history (program-dependent behavior, sample-rate-independent)
        compRmsHistory = compRmsHistoryCoeff * compRmsHistory +
                        (1.0f - compRmsHistoryCoeff) * rms;

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

    // CRITICAL: Enable flush-to-zero and denormals-are-zero to prevent denormal issues at 44.1kHz
    // Denormals cause massive CPU spikes and audio dropout
    juce::ScopedNoDenormals noDenormals;

    // Early return for empty buffers
    if (buffer.getNumSamples() == 0 || buffer.getNumChannels() == 0)
        return;

    // Ensure oversampling exists
    if (!oversampling)
        return;

    // DEBUG: Log every 100th block with detailed diagnostics
    const bool shouldLog = (++debugBlockCounter % 100 == 0);

    // CRITICAL: Detect sample rate changes and update coefficients
    // Some DAWs can change sample rate without calling prepareToPlay
    // This ensures all sample-rate-dependent processing works correctly at any rate (44.1, 48, 88.2, 96, 192 kHz)
    const double currentSR = getSampleRate();
    if (std::abs(currentSR - lastSampleRate) > 0.1)
    {
        juce::Logger::writeToLog("=== RUNTIME SAMPLE RATE CHANGE DETECTED ===");
        juce::Logger::writeToLog("Changed from " + juce::String(lastSampleRate, 1)
            + " Hz to " + juce::String(currentSR, 1) + " Hz");
        currentSampleRate = static_cast<float>(currentSR);

        // Update time-constant coefficients (DC blocking, compression envelope)
        updateSampleRateDependentCoefficients(currentSR);

        // Update smoothed values for new sample rate
        smoothedOutputGain.reset(currentSR, DSPConstants::GAIN_SMOOTH_TIME_S);
        smoothedGainReduction.reset(currentSR, DSPConstants::COMP_GR_SMOOTH_TIME_S);

        // Force update of all dynamic filters on next use
        lastCompCrossoverFreq = -1.0f;
        lastHighPassFreq = -1.0f;  // Force hi-pass filter update
        lastOversampledSampleRate = 0.0;  // Force distortion filter update

        juce::Logger::writeToLog("All coefficients updated successfully");
    }

    // Load parameters and scale them
    // SAFETY: Validate all parameter loads to prevent NaN propagation
    const auto inGainParam = inputGainParam->load();
    const auto outGainParam = outputGainParam->load();
    const auto distortionParam = distortionAmountParam->load();

    // Emergency NaN detection - if parameters are corrupted, use safe defaults
    if (std::isnan(inGainParam) || std::isnan(outGainParam) || std::isnan(distortionParam))
    {
        juce::Logger::writeToLog("CRITICAL: NaN detected in core parameters!");
        juce::Logger::writeToLog("inGainParam=" + juce::String(inGainParam)
            + ", outGainParam=" + juce::String(outGainParam)
            + ", distortionParam=" + juce::String(distortionParam));
        return;  // Skip this block to prevent NaN propagation
    }
    auto highPassFreq = highPassFreqParam->load();
    const bool bandSplitEnabled = bandSplitEnabledParam->load() > 0.5f;
    const int clipType = static_cast<int>(clipTypeParam->load());
    auto lfoRate = lfoRateParam->load();
    auto lfoDepth = lfoDepthParam->load();
    const int lfoWaveform = static_cast<int>(lfoWaveformParam->load());
    auto compPeakReduction = compPeakReductionParam->load();
    auto compMakeupGain = compMakeupGainParam->load();
    const int compRatioMode = static_cast<int>(compRatioParam->load());
    const bool compEnabled = compEnabledParam->load() > 0.5f;
    auto compWetDry = compWetDryParam->load();
    auto compCrossover = compCrossoverParam->load();
    auto distMix = distMixParam->load();

    // SAFETY: Validate all parameter values to prevent NaN propagation
    if (std::isnan(highPassFreq)) highPassFreq = DSPConstants::DEFAULT_HIPASS_FREQ;
    if (std::isnan(lfoRate)) lfoRate = 0.0f;
    if (std::isnan(lfoDepth)) lfoDepth = 0.0f;
    if (std::isnan(compPeakReduction)) compPeakReduction = 0.0f;
    if (std::isnan(compMakeupGain)) compMakeupGain = 50.0f;
    if (std::isnan(compWetDry)) compWetDry = 100.0f;
    if (std::isnan(compCrossover)) compCrossover = DSPConstants::DEFAULT_COMP_CROSSOVER;
    if (std::isnan(distMix)) distMix = 100.0f;

    // Scale to actual ranges for processing (studio distortion style)
    const float inputGain = std::pow(inGainParam / 50.0f, 1.5f);  // Unity at 50, range 0-2.83
    const auto outGainDB = (outGainParam - 50.0f) * 0.24f;  // Maps 0->-12dB, 50->0dB, 100->+12dB
    const auto outGain = juce::Decibels::decibelsToGain(outGainDB);

    if (shouldLog)
    {
        juce::Logger::writeToLog("=== BLOCK " + juce::String(debugBlockCounter) + " ===");
        juce::Logger::writeToLog("SR: " + juce::String(getSampleRate(), 0)
            + " | BlockSize: " + juce::String(buffer.getNumSamples())
            + " | InputGain: " + juce::String(inputGain, 3)
            + " | DistParam: " + juce::String(distortionParam, 1)
            + " | CompEnabled: " + juce::String(compEnabled ? "YES" : "NO"));
    }

    // LFO modulation for dynamic distortion effects
    // Calculate LFO value using selected waveform (output -1 to +1)
    float lfoValue = 0.0f;
    if (lfoRate > 0.0f && currentSampleRate > 0.0f)  // Only compute LFO if rate > 0 AND sample rate is valid
    {
        // Generate waveform based on selected type
        lfoValue = generateLFOWaveform(lfoPhase, lfoWaveform);

        // SAFETY: Validate LFO output
        if (std::isnan(lfoValue) || std::isinf(lfoValue))
            lfoValue = 0.0f;

        // Update phase for next block (sample-rate and block-size independent)
        // Phase increment per sample = frequency / sampleRate
        const float phaseIncrementPerSample = lfoRate / currentSampleRate;
        const float totalPhaseIncrement = phaseIncrementPerSample * buffer.getNumSamples();
        lfoPhase += totalPhaseIncrement;

        // Keep phase in 0-1 range (handle multiple wraps for safety)
        if (lfoPhase >= 0.0f)  // Only fmod if phase is valid
            lfoPhase = std::fmod(lfoPhase, 1.0f);
        else
            lfoPhase = 0.0f;
    }

    // Apply LFO modulation to distortion amount
    const float lfoModulation = (lfoValue * lfoDepth / 100.0f);  // -1 to +1 scaled by depth

    // SAFETY: Validate modulation calculation
    float modulatedDistortionParam = distortionParam;
    if (!std::isnan(lfoModulation) && !std::isinf(lfoModulation))
        modulatedDistortionParam = juce::jlimit(0.0f, 100.0f, distortionParam + lfoModulation * 50.0f);

    float distortionDrive = 1.0f + (modulatedDistortionParam / 100.0f) * 7.0f;  // Brutal range: 1.0 to 8.0

    // SAFETY: Final validation of distortion drive (critical parameter)
    if (std::isnan(distortionDrive) || std::isinf(distortionDrive) || distortionDrive < 1.0f)
    {
        juce::Logger::writeToLog("WARNING: Invalid distortionDrive=" + juce::String(distortionDrive) +
                                 ", modulatedDistortionParam=" + juce::String(modulatedDistortionParam) +
                                 ", using fallback 1.0");
        distortionDrive = 1.0f;  // Safe fallback
    }

    // Mix amount for wet/dry blend
    const float mixAmount = distMix / 100.0f;  // 0.0 to 1.0

    // Set target value for output gain (consumed at normal rate)
    smoothedOutputGain.setTargetValue(outGain);

    // TRUE BYPASS MODE: Skip all processing when distortion is off
    if (modulatedDistortionParam < 0.5f && !compEnabled)
    {
        // Apply only output gain (for volume matching)
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            const float currentOutputGain = smoothedOutputGain.getNextValue();
            for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            {
                buffer.setSample(channel, sample,
                    buffer.getSample(channel, sample) * currentOutputGain);
            }
        }

        // Update oscilloscope even in bypass mode
        for (int sample = 0; sample < buffer.getNumSamples(); sample += DSPConstants::SCOPE_UPDATE_DECIMATION)
        {
            if (scopeFifo.getFreeSpace() > 0)
            {
                const float leftSample = buffer.getSample(0, sample);
                const float rightSample = buffer.getNumChannels() > 1 ?
                    buffer.getSample(1, sample) : leftSample;
                pushSampleToScope(leftSample, rightSample);
            }
        }

        return;  // Skip all DSP processing
    }

    // Check input buffer for corruption BEFORE processing
    float maxInputSample = 0.0f;
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            maxInputSample = juce::jmax(maxInputSample, std::abs(buffer.getSample(ch, i)));

    if (shouldLog)
        juce::Logger::writeToLog("Input peak: " + juce::String(maxInputSample, 4));

    // Wrap original buffer into an AudioBlock
    auto inputBlock = juce::dsp::AudioBlock<float>(buffer);
    auto oversampledBlock = oversampling->processSamplesUp(inputBlock);

    // Check actual oversampled size and validate buffer allocation (always enabled)
    const size_t actualOversampledSamples = oversampledBlock.getNumSamples();

    // CRITICAL: Check for zero samples (would cause division by zero)
    if (actualOversampledSamples == 0)
    {
        juce::Logger::writeToLog("ERROR: Oversampling produced ZERO samples! Cannot process.");
        return;
    }

    // CRITICAL FIX: Manual linear interpolation for parameters consumed in oversampled domain
    // Calculate step size from LAST block's final value to THIS block's target value
    const float gainDelta = (inputGain - lastInputGain) / static_cast<float>(actualOversampledSamples);
    const float driveDelta = (distortionDrive - lastDistortionDrive) / static_cast<float>(actualOversampledSamples);

    // Start from last block's values
    float currentInputGain = lastInputGain;
    float currentDrive = lastDistortionDrive;

    // DEBUG: Log the delta calculations
    if (++deltaLogCounter % 100 == 0)
    {
        juce::Logger::writeToLog("Deltas - gainDelta: " + juce::String(gainDelta, 6)
            + ", driveDelta: " + juce::String(driveDelta, 6)
            + ", actualSamples: " + juce::String((int)actualOversampledSamples)
            + ", inputGain: " + juce::String(inputGain, 3)
            + ", lastInputGain: " + juce::String(lastInputGain, 3));
    }
    if (actualOversampledSamples > static_cast<size_t>(lowBandBuffer.getNumSamples()))
    {
        juce::Logger::writeToLog("CRITICAL BUFFER OVERFLOW! oversampledBlock: " + juce::String((int)actualOversampledSamples)
            + ", lowBandBuffer size: " + juce::String(lowBandBuffer.getNumSamples())
            + ", sampleRate: " + juce::String(getSampleRate())
            + ", blockSize: " + juce::String(buffer.getNumSamples())
            + ", oversamplingFactor: " + juce::String((int)oversamplingFactor));

        // Emergency resize to prevent crash (this should never happen after fix)
        lowBandBuffer.setSize(buffer.getNumChannels(), static_cast<int>(actualOversampledSamples + 64),
                              false, false, true);
        highBandBuffer.setSize(buffer.getNumChannels(), static_cast<int>(actualOversampledSamples + 64),
                               false, false, true);
    }

    // Get the actual oversampled sample rate
    const double oversampledSR = getSampleRate() * oversamplingFactor;

    // Update distortion crossover filters if sample rate changed
    // This handles sample rate changes that may not trigger prepareToPlay
    if (std::abs(oversampledSR - lastOversampledSampleRate) > 0.1)
    {
        const float crossoverFreq = DSPConstants::DISTORTION_CROSSOVER_FREQ;
        *lowPassFilter1.state = *juce::dsp::IIR::Coefficients<float>::makeLowPass(oversampledSR, crossoverFreq);
        *lowPassFilter2.state = *juce::dsp::IIR::Coefficients<float>::makeLowPass(oversampledSR, crossoverFreq);
        *highPassFilter1.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass(oversampledSR, crossoverFreq);
        *highPassFilter2.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass(oversampledSR, crossoverFreq);
        *dcBlockingFilter.state = *juce::dsp::IIR::Coefficients<float>::makeFirstOrderHighPass(oversampledSR, DSPConstants::DC_BLOCKING_FREQ);
        lastOversampledSampleRate = oversampledSR;

        juce::Logger::writeToLog("Updated distortion filters for oversampled rate: " + juce::String(oversampledSR));
    }

    // Update pre-filter coefficients if frequency changed
    if (std::abs(highPassFreq - lastHighPassFreq) > 0.01f)
    {
        // SAFETY: Validate oversampledSR and highPassFreq before creating filter
        if (oversampledSR < 1000.0 || oversampledSR > 1000000.0 || std::isnan(oversampledSR))
        {
            juce::Logger::writeToLog("ERROR: Invalid oversampledSR=" + juce::String(oversampledSR) + "! Cannot create filter coefficients.");
        }
        else if (highPassFreq < 1.0f || highPassFreq > (oversampledSR / 2.0f) || std::isnan(highPassFreq))
        {
            juce::Logger::writeToLog("ERROR: Invalid highPassFreq=" + juce::String(highPassFreq) +
                                     " for sampleRate=" + juce::String(oversampledSR) + "! Cannot create filter coefficients.");
        }
        else if (preHighPassFilter.state)
        {
            // Update coefficients IN-PLACE (dereference both sides)
            // CRITICAL: Use first-order filter for numerical stability at low frequencies
            *preHighPassFilter.state = *juce::dsp::IIR::Coefficients<float>::makeFirstOrderHighPass(oversampledSR, highPassFreq);
            lastHighPassFreq = highPassFreq;
        }
        else
        {
            juce::Logger::writeToLog("ERROR: preHighPassFilter.state is null! Cannot update coefficients.");
        }
    }

    // Pre-filtering (only if state is valid)
    if (preHighPassFilter.state)
        preHighPassFilter.process(juce::dsp::ProcessContextReplacing<float>(oversampledBlock));
    else
        juce::Logger::writeToLog("ERROR: Skipping pre-highpass filter - state is null!");

    const size_t numSamples = oversampledBlock.getNumSamples();
    const size_t numChannels = oversampledBlock.getNumChannels();

    // Distortion processing with optional band-split (808-Safe mode)
    if (bandSplitEnabled)
    {
        // === BAND-SPLIT MODE: Clean low + Distorted high ===

        // SAFETY CHECK: Ensure we have enough buffer space
        const int requiredBufferSize = static_cast<int>(numSamples);
        if (requiredBufferSize > lowBandBuffer.getNumSamples() || requiredBufferSize > highBandBuffer.getNumSamples())
        {
            juce::Logger::writeToLog("CRITICAL: Band-split buffer too small! Required: " + juce::String(requiredBufferSize)
                + ", lowBandBuffer: " + juce::String(lowBandBuffer.getNumSamples())
                + ", highBandBuffer: " + juce::String(highBandBuffer.getNumSamples()));
            // Skip band-split processing to prevent crash
            return;
        }

        lowBandBuffer.clear();
        highBandBuffer.clear();

        // Copy input to both band buffers (with bounds checking)
        for (size_t channel = 0; channel < numChannels; ++channel)
        {
            const int channelIdx = static_cast<int>(channel);
            const int sampleCount = static_cast<int>(numSamples);

            lowBandBuffer.copyFrom(channelIdx, 0,
                oversampledBlock.getChannelPointer(channel),
                sampleCount);
            highBandBuffer.copyFrom(channelIdx, 0,
                oversampledBlock.getChannelPointer(channel),
                sampleCount);
        }

        // Filter the bands (using safe subblocks)
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
            for (size_t channel = 0; channel < numChannels; ++channel)
            {
                auto* highBandData = highBandBuffer.getWritePointer(static_cast<int>(channel));

                // Read input (already band-split)
                float inputSample = highBandData[sample];

                // Bypass distortion if amount is negligible (< 0.5%)
                if (modulatedDistortionParam < 0.5f)
                {
                    highBandData[sample] = inputSample;  // Pure bypass
                }
                else
                {
                    // Apply studio distortion
                    float distorted = applyStudioDistortion(inputSample, currentInputGain, currentDrive, clipType);

                    // Wet/Dry mix (DC blocking handled by dcBlockingFilter after distortion)
                    highBandData[sample] = inputSample * (1.0f - mixAmount) + distorted * mixAmount;
                }
            }

            // Manually step the smoothed parameters AFTER processing all channels
            currentInputGain += gainDelta;
            currentDrive += driveDelta;
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
            for (size_t channel = 0; channel < numChannels; ++channel)
            {
                auto* channelData = oversampledBlock.getChannelPointer(channel);

                // Read input
                float inputSample = channelData[sample];

                // Bypass distortion if amount is negligible (< 0.5%)
                if (modulatedDistortionParam < 0.5f)
                {
                    channelData[sample] = inputSample;  // Pure bypass
                }
                else
                {
                    // Apply studio distortion
                    float distorted = applyStudioDistortion(inputSample, currentInputGain, currentDrive, clipType);

                    // Wet/Dry mix (DC blocking handled by dcBlockingFilter after distortion)
                    channelData[sample] = inputSample * (1.0f - mixAmount) + distorted * mixAmount;
                }
            }

            // Manually step the smoothed parameters AFTER processing all channels
            currentInputGain += gainDelta;
            currentDrive += driveDelta;
        }
    }

    // Store final parameter values for next block's interpolation
    lastInputGain = currentInputGain;
    lastDistortionDrive = currentDrive;

    // DEBUG: Check for corruption AFTER our processing, BEFORE downsampling
    bool hasNaN = false;
    bool hasInf = false;
    float maxSample = 0.0f;
    const size_t numSamplesCheck = oversampledBlock.getNumSamples();
    const size_t numChannelsCheck = oversampledBlock.getNumChannels();
    for (size_t ch = 0; ch < numChannelsCheck && !hasNaN && !hasInf; ++ch)
    {
        const float* channelData = oversampledBlock.getChannelPointer(ch);
        for (size_t i = 0; i < numSamplesCheck; ++i)
        {
            const float sample = channelData[i];
            if (std::isnan(sample)) hasNaN = true;
            if (std::isinf(sample)) hasInf = true;
            maxSample = std::max(maxSample, std::abs(sample));
        }
    }

    if (shouldLog)
        juce::Logger::writeToLog("After distortion peak: " + juce::String(maxSample, 4));

    if (hasNaN || hasInf || maxSample > 10.0f)
    {
        juce::Logger::writeToLog("!!! CORRUPTED AFTER DISTORTION! maxSample: " + juce::String(maxSample, 3)
            + ", NaN: " + juce::String(hasNaN ? "true" : "false")
            + ", Inf: " + juce::String(hasInf ? "true" : "false")
            + ", inputGain: " + juce::String(inputGain, 3)
            + ", finalInputGain: " + juce::String(currentInputGain, 3)
            + ", finalDrive: " + juce::String(currentDrive, 3)
            + ", gainDelta: " + juce::String(gainDelta, 6));
    }

    // REMOVED: DC blocking before downsampling causes instability at 44.1kHz
    // The DC blocking filter at 5Hz with 176.4kHz oversampled rate creates
    // extremely resonant poles that interact badly with the downsampler
    // DC blocking is applied AFTER downsampling via manual one-pole DC blocker

    // ========== POST-DISTORTION TONE FILTER (oversampled domain) ==========
    // Apply lowpass filter for darkness/brightness control (2-20kHz)
    const float toneFreq = toneParam ? toneParam->load() : 20000.0f;

    // Update tone filter coefficients if frequency changed
    if (std::abs(toneFreq - lastToneFreq) > 1.0f && toneFilter.state != nullptr)
    {
        const double toneSampleRate = currentSampleRate * oversamplingFactor;
        // Clamp frequency to valid range (well below Nyquist)
        const float clampedToneFreq = juce::jlimit(2000.0f, std::min(20000.0f, (float)(toneSampleRate * 0.45)), toneFreq);
        *toneFilter.state = *juce::dsp::IIR::Coefficients<float>::makeLowPass(toneSampleRate, clampedToneFreq);
        lastToneFreq = toneFreq;
    }

    // Apply tone filter (only if not at maximum/bypass)
    if (toneFreq < 19500.0f && toneFilter.state != nullptr)
    {
        toneFilter.process(juce::dsp::ProcessContextReplacing<float>(oversampledBlock));
    }

    // Downsample back into original buffer
    oversampling->processSamplesDown(inputBlock);

    // Check buffer after downsampling
    if (shouldLog)
    {
        float maxAfterDownsample = 0.0f;
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                maxAfterDownsample = juce::jmax(maxAfterDownsample, std::abs(buffer.getSample(ch, i)));
        juce::Logger::writeToLog("After downsample peak: " + juce::String(maxAfterDownsample, 4));
    }

    // ========== WAVESHAPER (before compression for more musical interaction) ==========
    // Smooth harmonics with buttery fuzz and pleasant hiss
    const float waveshaperMix = *waveshaperMixParam;
    if (waveshaperMix > 0.0f)
    {
        const float wetAmount = waveshaperMix / 100.0f;  // 0.0 to 1.0
        const float dryAmount = 1.0f - wetAmount;

        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            auto* channelData = buffer.getWritePointer(channel);
            for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            {
                const float dry = channelData[sample];
                float wet = dry;

                // Stage 1: Gentle pre-emphasis for detail (3x instead of 8x)
                wet *= 3.0f;

                // Stage 2: Smooth tube-like saturation with even harmonics
                const float x2 = wet * wet;  // 2nd harmonic
                wet = wet + (x2 * 0.15f * juce::dsp::FastMathApproximations::tanh(wet));

                // Stage 3: Buttery soft-knee saturation
                const float absWet = std::abs(wet);
                if (absWet > 0.4f)
                {
                    const float excess = absWet - 0.4f;
                    const float compressed = 0.4f + std::tanh(excess * 1.2f) * 0.4f;
                    wet = (wet > 0.0f ? compressed : -compressed);
                }

                // Stage 4: Add smooth 3rd harmonic for richness
                wet = wet + std::sin(wet * 3.0f) * 0.08f;

                // Stage 5: Pleasant tape-like hiss (subtle high-frequency enhancement)
                const float hiss = waveshaperRandom.nextFloat() * 0.003f - 0.0015f;
                wet += hiss * absWet;

                // Stage 6: Gentle wave folding for silky harmonics
                wet = wet + std::sin(wet * 1.5f) * 0.12f;

                // Stage 7: Final smooth saturation
                wet = std::tanh(wet * 0.85f);

                // Stage 8: Subtle asymmetry for analog character
                if (wet > 0.0f)
                    wet *= 0.98f;

                // Blend dry and wet signals
                channelData[sample] = dryAmount * dry + wetAmount * wet;
            }
        }
    }

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

        // Update crossover frequency only when it changes (avoid redundant coefficient calculations)
        const double normalSampleRate = getSampleRate();
        if (std::abs(compCrossover - lastCompCrossoverFreq) > 0.01f || lastCompCrossoverFreq < 0.0f)
        {
            *compLowPassFilter1.state = *juce::dsp::IIR::Coefficients<float>::makeLowPass(normalSampleRate, compCrossover);
            *compLowPassFilter2.state = *juce::dsp::IIR::Coefficients<float>::makeLowPass(normalSampleRate, compCrossover);
            *compHighPassFilter1.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass(normalSampleRate, compCrossover);
            *compHighPassFilter2.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass(normalSampleRate, compCrossover);
            lastCompCrossoverFreq = compCrossover;
        }

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

    // Manual DC blocker - simple one-pole filter that's extremely stable
    // y[n] = x[n] - x[n-1] + R * y[n-1], where R ≈ 0.995 for ~35Hz cutoff at 44.1kHz
    constexpr float R = 0.995f;  // Higher = lower cutoff, more stable
    // CRITICAL: Clamp to 2 channels max to prevent array out-of-bounds access
    // (manualDCBlockerPrevInput/Output arrays are fixed size [2])
    const int dcBlockerChannels = juce::jmin(buffer.getNumChannels(), 2);
    for (int ch = 0; ch < dcBlockerChannels; ++ch)
    {
        auto* data = buffer.getWritePointer(ch);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const float input = data[i];
            const float output = input - manualDCBlockerPrevInput[ch] + R * manualDCBlockerPrevOutput[ch];

            // Safety check for NaN/Inf (should never happen with this simple filter)
            if (std::isnan(output) || std::isinf(output))
            {
                data[i] = 0.0f;
                manualDCBlockerPrevOutput[ch] = 0.0f;
            }
            else
            {
                data[i] = output;
                manualDCBlockerPrevOutput[ch] = output;
            }
            manualDCBlockerPrevInput[ch] = input;
        }
    }

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

    // Check final output
    float maxFinalOutput = 0.0f;
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            maxFinalOutput = juce::jmax(maxFinalOutput, std::abs(buffer.getSample(ch, i)));

    if (shouldLog)
    {
        juce::Logger::writeToLog("Final output peak: " + juce::String(maxFinalOutput, 4)
            + " | OutputGain: " + juce::String(outGain, 3));
        juce::Logger::writeToLog("=========================================\n");
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
        juce::NormalisableRange<float>(0.1f, 50.0f, 0.1f),  // Increased from 10Hz to 50Hz for tremolo/ring mod
        0.0f));  // Default 0 = LFO off

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "lfoDepth", 1 },
        "LFO Depth",
        juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f),
        0.0f));  // Default 0 = no modulation

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ "lfoWaveform", 1 },
        "LFO Waveform",
        juce::StringArray{ "Sine", "Triangle", "Square", "Saw", "Random" },
        0));  // Default 0 = Sine wave

    // Waveshaper parameter (mix knob 0-100%)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "waveshaperMix", 1 },
        "Waveshaper Mix",
        juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f),
        0.0f));  // Default 0 = no waveshaping

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

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "tone", 1 },
        "Tone",
        juce::NormalisableRange<float>(2000.0f, 20000.0f, 1.0f, 0.5f),  // Skew for better low-end control
        20000.0f));  // Default 20kHz = bright/bypass

    return { params.begin(), params.end() };
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}