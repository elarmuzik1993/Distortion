/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "RTAllocationGuard.h"
#include "FastMath.h"
#include <memory>

namespace
{
void writeFirstOrderHighPassCoeffs(juce::dsp::IIR::Coefficients<float>& dest,
                                   const double sampleRate,
                                   const double cutoffHz) noexcept
{
    const auto n = std::tan(juce::MathConstants<double>::pi * cutoffHz / sampleRate);
    const auto invA0 = 1.0 / (n + 1.0);
    auto* coeffs = dest.getRawCoefficients();

    coeffs[0] = static_cast<float>(invA0);
    coeffs[1] = static_cast<float>(-invA0);
    coeffs[2] = static_cast<float>((n - 1.0) * invA0);
}

void writeFirstOrderLowPassCoeffs(juce::dsp::IIR::Coefficients<float>& dest,
                                  const double sampleRate,
                                  const double cutoffHz) noexcept
{
    const auto n = std::tan(juce::MathConstants<double>::pi * cutoffHz / sampleRate);
    const auto invA0 = 1.0 / (n + 1.0);
    auto* coeffs = dest.getRawCoefficients();

    coeffs[0] = static_cast<float>(n * invA0);
    coeffs[1] = static_cast<float>(n * invA0);
    coeffs[2] = static_cast<float>((n - 1.0) * invA0);
}

void writeSecondOrderLowPassCoeffs(juce::dsp::IIR::Coefficients<float>& dest,
                                   const double sampleRate,
                                   const double cutoffHz,
                                   const double q) noexcept
{
    const auto n = 1.0 / std::tan(juce::MathConstants<double>::pi * cutoffHz / sampleRate);
    const auto nSquared = n * n;
    const auto invQ = 1.0 / q;
    const auto c1 = 1.0 / (1.0 + invQ * n + nSquared);
    auto* coeffs = dest.getRawCoefficients();

    coeffs[0] = static_cast<float>(c1);
    coeffs[1] = static_cast<float>(c1 * 2.0);
    coeffs[2] = static_cast<float>(c1);
    coeffs[3] = static_cast<float>(c1 * 2.0 * (1.0 - nSquared));
    coeffs[4] = static_cast<float>(c1 * (1.0 - invQ * n + nSquared));
}

void writeSecondOrderHighPassCoeffs(juce::dsp::IIR::Coefficients<float>& dest,
                                    const double sampleRate,
                                    const double cutoffHz,
                                    const double q) noexcept
{
    const auto n = std::tan(juce::MathConstants<double>::pi * cutoffHz / sampleRate);
    const auto nSquared = n * n;
    const auto invQ = 1.0 / q;
    const auto c1 = 1.0 / (1.0 + invQ * n + nSquared);
    auto* coeffs = dest.getRawCoefficients();

    coeffs[0] = static_cast<float>(c1);
    coeffs[1] = static_cast<float>(c1 * -2.0);
    coeffs[2] = static_cast<float>(c1);
    coeffs[3] = static_cast<float>(c1 * 2.0 * (nSquared - 1.0));
    coeffs[4] = static_cast<float>(c1 * (1.0 - invQ * n + nSquared));
}

}

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
    subGuardFreqParam = parameters.getRawParameterValue("subGuardFreq");
    clipTypeParam = parameters.getRawParameterValue("clipType");
    lfoRateParam = parameters.getRawParameterValue("lfoRate");
    lfoDepthParam = parameters.getRawParameterValue("lfoDepth");
    lfoWaveformParam = parameters.getRawParameterValue("lfoWaveform");
    lfoEnabledParam = parameters.getRawParameterValue("lfoEnabled");
    lfoDestinationParam = parameters.getRawParameterValue("lfoDestination");
    waveshaperMixParam = parameters.getRawParameterValue("waveshaperMix");
    compPeakReductionParam = parameters.getRawParameterValue("compPeakReduction");
    compMakeupGainParam = parameters.getRawParameterValue("compMakeupGain");
    compRatioParam = parameters.getRawParameterValue("compRatio");
    compEnabledParam = parameters.getRawParameterValue("compEnabled");
    autoGainEnabledParam = parameters.getRawParameterValue("autoGainEnabled");
    extremeEnabledParam = parameters.getRawParameterValue("extremeEnabled");
    globalMixParam = parameters.getRawParameterValue("globalMix");
    distMixParam = parameters.getRawParameterValue("distMix");
    toneParam = parameters.getRawParameterValue("tone");
    waveshaperCleanParam = parameters.getRawParameterValue("waveshaperClean");
    linearPhaseDryParam = parameters.getRawParameterValue("linearPhaseDry");
    // Verify all parameters were found
    jassert(inputGainParam && outputGainParam && distortionAmountParam
        && highPassFreqParam && subGuardFreqParam && clipTypeParam
        && lfoRateParam && lfoDepthParam && lfoWaveformParam && lfoEnabledParam && lfoDestinationParam && waveshaperMixParam
        && compPeakReductionParam && compMakeupGainParam && compRatioParam && compEnabledParam
        && autoGainEnabledParam && extremeEnabledParam && globalMixParam
        && distMixParam && toneParam && waveshaperCleanParam && linearPhaseDryParam);

    // Initialize SmoothedValues with default sample rate to prevent assertions
    // They will be properly re-initialized in prepareToPlay() with actual sample rate
    const double defaultSampleRate = 44100.0;
    smoothedOutputGain.reset(defaultSampleRate, DSPConstants::GAIN_SMOOTH_TIME_S);
    smoothedOutputGain.setCurrentAndTargetValue(1.0f);
    smoothedGlobalMix.reset(defaultSampleRate, 0.02);
    smoothedGlobalMix.setCurrentAndTargetValue(1.0f);
    smoothedSubGuardFreq.reset(defaultSampleRate, DSPConstants::SUBGUARD_FREQ_SMOOTH_TIME_S);
    smoothedSubGuardFreq.setCurrentAndTargetValue(DSPConstants::SUBGUARD_FREQ_DEFAULT);
}

PluginProcessor::~PluginProcessor()
{
}

//==============================================================================
// Studio Distortion DSP Helper Methods
//==============================================================================

float PluginProcessor::applyStudioDistortion(float x, float gain, float drive, int clipType, float harmonicScale, int channel)
{
    x = x * gain;
    float y = 0.0f;

    switch (clipType)
    {
    case 0:  // BRUTAL FUZZ - Aggressive hard clipping with analog noise
    {
        // Heavy drive for maximum grit
        y = x * drive * 2.25f;

        // Hard clip with brutal threshold
        const float threshold = 0.3f;  // Very low threshold for aggressive clipping
        if (y > threshold)
            y = threshold + FastMath::atan((y - threshold) * 2.0f) * 0.2f;
        else if (y < -threshold)
            y = -threshold + FastMath::atan((y + threshold) * 2.0f) * 0.2f;

        // Add input-dependent analog noise (silent on silence, warm with signal)
        float noiseAmount = std::min(std::abs(x) * 0.01f, 0.002f);
        y += distortionRandom.nextFloat() * noiseAmount - (noiseAmount * 0.5f);

        // Final saturation
        y = FastMath::tanh(y * 1.8f);
        break;
    }
    case 1:  // TUBE OVERDRIVE - Asymmetric tube saturation with harmonics
    {
        // Tube-style asymmetric clipping (positive clips harder)
        y = x * drive * 1.4f;

        // Stateful cathode bias envelope: sustained signal shifts positive clip threshold down
        // Simulates cathode capacitor charging under sustained signal
        const int tubeCh = (channel >= 0 && channel < 2) ? channel : 0;
        const float tubeInputLevel = std::abs(y);
        if (tubeInputLevel > tubeBiasEnvelope[tubeCh])
            tubeBiasEnvelope[tubeCh] = tubeBiasAttackCoeff * tubeBiasEnvelope[tubeCh]
                                     + (1.0f - tubeBiasAttackCoeff) * tubeInputLevel;
        else
            tubeBiasEnvelope[tubeCh] = tubeBiasReleaseCoeff * tubeBiasEnvelope[tubeCh]
                                     + (1.0f - tubeBiasReleaseCoeff) * tubeInputLevel;

        // Modulate positive clipping threshold: 0.85 (cold) → 0.70 (hot)
        const float biasShift = juce::jlimit(0.0f, 1.0f, tubeBiasEnvelope[tubeCh]) * DSPConstants::TUBE_BIAS_MOD_DEPTH;
        const float posClipThreshold = 0.85f - biasShift;

        // Asymmetric waveshaping (vintage tube behavior)
        if (y > 0.0f)
        {
            // Positive: harder clipping with even harmonics (threshold modulated by bias)
            y = FastMath::tanh(y * 1.6f) * posClipThreshold;
            y += (0.15f * harmonicScale) * y * y;  // 2nd harmonic (sub-linear scaled)
        }
        else
        {
            // Negative: softer clipping with odd harmonics
            y = FastMath::tanh(y * 1.2f) * 0.9f;
            y += (0.08f * harmonicScale) * y * y * y;  // 3rd harmonic (sub-linear scaled)
        }

        // Add subtle warmth (also scaled to prevent harshness at high levels)
        y = y + (0.05f * harmonicScale) * std::sin(y * juce::MathConstants<float>::pi);
        break;
    }
    case 2:  // BIT CRUSHER - Digital destruction with sample rate reduction
    {
        // Extreme bit reduction for digital grit
        constexpr float maxValue = 32.0f;  // 2^5 = 32 (6-bit depth, avoiding runtime pow)

        y = x * drive * 1.6f;

        // Bit crushing (truncate-toward-zero to avoid the DC offset that std::floor introduces)
        y = (float)(int)(y * maxValue) / maxValue;

        // Add aliasing character
        y = FastMath::tanh(y * 2.5f);

        // Hard clip for extra grit
        y = juce::jlimit(-0.95f, 0.95f, y);
        break;
    }
    case 3:  // TAPE SATURATION - Analog tape with hysteresis
    {
        // Tape-style soft saturation with magnetic hysteresis simulation
        y = x * drive * 1.25f;

        // Stateful magnetic hysteresis envelope: sustained signal stiffens compression
        // Simulates magnetic particle saturation under sustained signal
        const int tapeCh = (channel >= 0 && channel < 2) ? channel : 0;
        const float tapeInputLevel = std::abs(y);
        if (tapeInputLevel > tapeSaturationEnvelope[tapeCh])
            tapeSaturationEnvelope[tapeCh] = tapeHysteresisAttackCoeff * tapeSaturationEnvelope[tapeCh]
                                           + (1.0f - tapeHysteresisAttackCoeff) * tapeInputLevel;
        else
            tapeSaturationEnvelope[tapeCh] = tapeHysteresisReleaseCoeff * tapeSaturationEnvelope[tapeCh]
                                           + (1.0f - tapeHysteresisReleaseCoeff) * tapeInputLevel;

        // Modulate compression knee: 0.7 (cold/loose) → 0.5 (hot/stiff)
        const float hysteresisShift = juce::jlimit(0.0f, 1.0f, tapeSaturationEnvelope[tapeCh]) * DSPConstants::TAPE_HYSTERESIS_MOD_DEPTH;
        const float tapeKnee = 0.7f - hysteresisShift;  // Lower knee = earlier compression onset

        // Tape compression curve (progressive, with modulated knee)
        const float abs_y = std::abs(y);
        const float sign = (y > 0.0f) ? 1.0f : -1.0f;

        if (abs_y < tapeKnee * 0.57f)  // Scale quiet region proportionally
            y = y * 1.05f;  // Slight boost in quiet regions
        else if (abs_y < 1.0f)
            y = sign * (tapeKnee * 0.57f * 1.05f + (abs_y - tapeKnee * 0.57f) * tapeKnee);
        else
            y = sign * (tapeKnee * 0.57f * 1.05f + (1.0f - tapeKnee * 0.57f) * tapeKnee
                + FastMath::tanh((abs_y - 1.0f) * 2.0f) * 0.15f);

        // Add tape warmth (subtle even harmonics, sub-linear scaled)
        y += (0.12f * harmonicScale) * y * y * sign;

        // Final soft saturation
        y = FastMath::tanh(y * 1.3f) * 0.92f;
        break;
    }
    case 4:  // TRANSFORMER SATURATION - Heavy harmonic distortion
    {
        // Transformer-style saturation with rich harmonics
        y = x * drive * 1.5f;

        // Multi-stage waveshaping for complex harmonics
        y = FastMath::tanh(y * 1.5f);

        // Add rich harmonic content (all coefficients sub-linear scaled)
        const float fundamental = y;
        const float harmonic2 = (0.25f * harmonicScale) * fundamental * fundamental * (fundamental > 0.0f ? 1.0f : -1.0f);
        const float harmonic3 = (0.15f * harmonicScale) * fundamental * fundamental * fundamental;
        const float harmonic5 = (0.08f * harmonicScale) * fundamental * fundamental * fundamental * fundamental * fundamental;

        y = fundamental + harmonic2 + harmonic3 + harmonic5;

        // Final limiting
        y = FastMath::tanh(y * 1.4f) * 0.88f;
        break;
    }
    case 5:  // DIODE CLIPPER - Asymmetric diode clipping with grit
    {
        // Asymmetric diode-style clipping (forward/reverse bias difference)
        y = x * drive * 1.9f;

        // Asymmetric clipping (simulating diode forward voltage)
        if (y > 0.5f)
        {
            // Forward bias: hard clip at ~0.7V
            y = 0.5f + FastMath::atan((y - 0.5f) * 4.0f) * 0.15f;
        }
        else if (y < -0.6f)
        {
            // Reverse bias: slightly different threshold
            y = -0.6f + FastMath::atan((y + 0.6f) * 3.5f) * 0.2f;
        }

        // Add crossover distortion character
        if (std::abs(y) < 0.05f)
            y *= 0.7f;  // Dead zone near zero crossing

        // Final saturation
        y = FastMath::tanh(y * 2.2f) * 0.9f;
        break;
    }
    case 6:  // DECIMATOR - Extreme digital destruction
    {
        // Brutal digital destruction with severe aliasing
        y = x * drive * 2.5f;

        // Sample & hold for brutal aliasing
        const float foldback = 4.0f;
        y = std::fmod(y + 2.0f, 2.0f * foldback) - foldback;

        // Hard clip with fold-back (clamped to 16 iterations to prevent infinite loops)
        for (int i = 0; i < 16 && y > 1.0f; ++i) y = 2.0f - y;
        for (int i = 0; i < 16 && y < -1.0f; ++i) y = -2.0f - y;

        // Add harmonic distortion
        y = FastMath::tanh(y * 2.8f);

        // Brutal final limiting
        y = juce::jlimit(-0.9f, 0.9f, y);
        break;
    }
    default:  // Fallback: simple tanh
        y = FastMath::tanh(x * drive) * 0.95f;
        break;
    }

    // Normalize output levels across clip types (prevents volume jumps when switching)
    constexpr float normGains[] = {
        DSPConstants::CLIP_NORM_BRUTAL_FUZZ,       // 0
        DSPConstants::CLIP_NORM_TUBE_OVERDRIVE,    // 1
        DSPConstants::CLIP_NORM_BIT_CRUSHER,       // 2
        DSPConstants::CLIP_NORM_TAPE_SATURATION,   // 3
        DSPConstants::CLIP_NORM_TRANSFORMER,       // 4
        DSPConstants::CLIP_NORM_DIODE_CLIPPER,     // 5
        DSPConstants::CLIP_NORM_DECIMATOR          // 6
    };
    if (clipType >= 0 && clipType < 7)
        y *= normGains[clipType];

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
    // SAFETY: Validate sample rate to prevent NaN in coefficient calculation
    // Valid audio sample rates are typically 8kHz to 384kHz
    if (sampleRate < 1000.0 || sampleRate > 500000.0 || std::isnan(sampleRate) || std::isinf(sampleRate))
    {
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
        compAttackCoeff = 0.9995f;
    }
    if (std::isnan(compReleaseCoeff) || compReleaseCoeff < 0.0f || compReleaseCoeff > 1.0f)
    {
        compReleaseCoeff = 0.99995f;
    }
    if (std::isnan(compRmsHistoryCoeff) || compRmsHistoryCoeff < 0.0f || compRmsHistoryCoeff > 1.0f)
    {
        compRmsHistoryCoeff = 0.99f;
    }

    // Output limiter coefficients (normal sample rate, not oversampled)
    outputLimiterAttackCoeff = std::exp(-1.0f / (DSPConstants::OUTPUT_LIMITER_ATTACK_TIME_S * static_cast<float>(sampleRate)));
    outputLimiterReleaseCoeff = std::exp(-1.0f / (DSPConstants::OUTPUT_LIMITER_RELEASE_TIME_S * static_cast<float>(sampleRate)));

    // SAFETY: Validate output limiter coefficients
    if (std::isnan(outputLimiterAttackCoeff) || outputLimiterAttackCoeff < 0.0f || outputLimiterAttackCoeff > 1.0f)
    {
        outputLimiterAttackCoeff = 0.99f;  // Safe fallback (~0.23ms at 44.1kHz)
    }
    if (std::isnan(outputLimiterReleaseCoeff) || outputLimiterReleaseCoeff < 0.0f || outputLimiterReleaseCoeff > 1.0f)
    {
        outputLimiterReleaseCoeff = 0.9995f;  // Safe fallback (~45ms at 44.1kHz)
    }

    // Store the sample rate to detect changes
    lastSampleRate = sampleRate;
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
    // LA-2A compression has 500ms release time that produces tail
    return DSPConstants::COMP_RELEASE_TIME_S;  // 0.5 seconds
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

    // Output gain is applied AFTER downsampling at normal sample rate
    smoothedOutputGain.reset(sampleRate, DSPConstants::GAIN_SMOOTH_TIME_S);
    smoothedOutputGain.setCurrentAndTargetValue(1.0f);  // Initialize to unity gain

    // Global mix smoothing (for wet/dry crossfade during automation)
    smoothedGlobalMix.reset(sampleRate, 0.02);
    smoothedGlobalMix.setCurrentAndTargetValue(globalMixParam ? globalMixParam->load() / 100.0f : 1.0f);

    compEnvelopeState = 1.0f;
    compRmsHistory = 0.0f;
    tubeWarmth = 0.0f;
    outputLimiterEnvelope = 1.0f;

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
    lastDistortionDrive = 1.0f + (distParam / 100.0f) * 3.0f;  // Match processBlock calculation (reduced range)
    lastDistMix = distMixParam ? distMixParam->load() / 100.0f : 1.0f;  // Match processBlock calculation

    // Update all sample-rate-dependent coefficients (DC blocking, compression, etc.)
    updateSampleRateDependentCoefficients(sampleRate);

    scopeBuffer.setSize(2, DSPConstants::SCOPE_BUFFER_SIZE);
    scopeBuffer.clear();
    scopeFifo.setTotalSize(DSPConstants::SCOPE_BUFFER_SIZE);

    rebuildOversampling(sampleRate, samplesPerBlock);

    // Prepare DSP filters with oversampled sample rate & block size
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate * oversamplingFactor;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock * oversamplingFactor);
    spec.numChannels = static_cast<juce::uint32>(numChannels);

    const int worstCaseOversampledBlockSize = (samplesPerBlock * oversamplingFactor) + 64;

    // CRITICAL: For JUCE IIR ProcessorDuplicator filters, the order MUST be:
    // 1. prepare() - creates the internal state
    // 2. set coefficients - applies to the created state
    // 3. reset() - clears the filter history

    // Pre-highpass filter runs BEFORE upsampling (at base sample rate) for efficiency
    // Prepare a normalSpec for base-rate filters
    juce::dsp::ProcessSpec baseSpec;
    baseSpec.sampleRate = sampleRate;  // Base rate, not oversampled
    baseSpec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    baseSpec.numChannels = static_cast<juce::uint32>(numChannels);

    // Initialize with default frequency at BASE sample rate
    // CRITICAL: Use first-order filter for numerical stability at low frequencies
    preHighPassFilter.prepare(baseSpec);
    *preHighPassFilter.state = *juce::dsp::IIR::Coefficients<float>::makeFirstOrderHighPass(baseSpec.sampleRate, DSPConstants::DEFAULT_HIPASS_FREQ);
    preHighPassFilter.reset();

    // Force filter update on first processBlock (especially important for DAW state restoration)
    lastHighPassFreq = -1.0f;

    // Post-distortion tone filter (oversampled rate) - lowpass for darkness/brightness control
    toneFilter.prepare(spec);
    const float initialToneFreq = toneParam ? toneParam->load() : 20000.0f;
    *toneFilter.state = *juce::dsp::IIR::Coefficients<float>::makeLowPass(spec.sampleRate, initialToneFreq);
    toneFilter.reset();
    lastToneFreq = initialToneFreq;

    // Phase-match mirror of the tone LP for the Sub Guard low branch (same coeffs, own state)
    toneFilterLow.prepare(spec);
    *toneFilterLow.state = *juce::dsp::IIR::Coefficients<float>::makeLowPass(spec.sampleRate, initialToneFreq);
    toneFilterLow.reset();

    // Sub Guard variable-slope crossover filters (oversampled domain)
    const float sgFreq = subGuardFreqParam ? subGuardFreqParam->load() : DSPConstants::SUBGUARD_FREQ_DEFAULT;
    // Use safe frequency for filter initialization when OFF (prevents divide-by-zero)
    const float filterInitFreq = (sgFreq <= 1.0f) ? 60.0f : sgFreq;

    // LR24 filters (4th order = 2 cascaded 2nd-order stages) - PRESERVE mode
    lowPassFilter1.prepare(spec);
    *lowPassFilter1.state = *juce::dsp::IIR::Coefficients<float>::makeLowPass(spec.sampleRate, filterInitFreq);
    lowPassFilter1.reset();

    lowPassFilter2.prepare(spec);
    *lowPassFilter2.state = *juce::dsp::IIR::Coefficients<float>::makeLowPass(spec.sampleRate, filterInitFreq);
    lowPassFilter2.reset();

    highPassFilter1.prepare(spec);
    *highPassFilter1.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass(spec.sampleRate, filterInitFreq);
    highPassFilter1.reset();

    highPassFilter2.prepare(spec);
    *highPassFilter2.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass(spec.sampleRate, filterInitFreq);
    highPassFilter2.reset();

    // LR12 filters (2nd order = single stage, Q=0.5 for true Linkwitz-Riley 2) - AGGRESSIVE mode
    constexpr float lr2Q = 0.5f;
    subGuardLP12.prepare(spec);
    *subGuardLP12.state = *juce::dsp::IIR::Coefficients<float>::makeLowPass(spec.sampleRate, filterInitFreq, lr2Q);
    subGuardLP12.reset();

    subGuardHP12.prepare(spec);
    *subGuardHP12.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass(spec.sampleRate, filterInitFreq, lr2Q);
    subGuardHP12.reset();

    // LR18 filters (1st + 2nd order = 3rd order approximation, Q=0.5 on 2nd-order) - CONTROL mode
    subGuardLP18_1.prepare(spec);
    *subGuardLP18_1.state = *juce::dsp::IIR::Coefficients<float>::makeFirstOrderLowPass(spec.sampleRate, filterInitFreq);
    subGuardLP18_1.reset();

    subGuardLP18_2.prepare(spec);
    *subGuardLP18_2.state = *juce::dsp::IIR::Coefficients<float>::makeLowPass(spec.sampleRate, filterInitFreq, lr2Q);
    subGuardLP18_2.reset();

    subGuardHP18_1.prepare(spec);
    *subGuardHP18_1.state = *juce::dsp::IIR::Coefficients<float>::makeFirstOrderHighPass(spec.sampleRate, filterInitFreq);
    subGuardHP18_1.reset();

    subGuardHP18_2.prepare(spec);
    *subGuardHP18_2.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass(spec.sampleRate, filterInitFreq, lr2Q);
    subGuardHP18_2.reset();

    // Initialize Sub Guard smoothing (use actual parameter value, not filterInitFreq)
    smoothedSubGuardFreq.reset(spec.sampleRate, DSPConstants::SUBGUARD_FREQ_SMOOTH_TIME_S);
    smoothedSubGuardFreq.setCurrentAndTargetValue(sgFreq);
    lastSubGuardFreq = sgFreq;

    // Store oversampled sample rate for change detection
    lastOversampledSampleRate = spec.sampleRate;

    // Pre-distortion compression coefficients (oversampled rate)
    const float oversampledRate = static_cast<float>(spec.sampleRate);
    preCompAttackCoeff = std::exp(-1.0f / (DSPConstants::PRE_COMP_ATTACK_TIME_S * oversampledRate));
    preCompReleaseCoeff = std::exp(-1.0f / (DSPConstants::PRE_COMP_RELEASE_TIME_S * oversampledRate));
    preCompEnvelope[0] = 1.0f;
    preCompEnvelope[1] = 1.0f;

    // Sub-linear harmonic density coefficients (oversampled rate)
    harmonicDensityAttackCoeff = std::exp(-1.0f / (DSPConstants::HARMONIC_DENSITY_ATTACK_TIME_S * oversampledRate));
    harmonicDensityReleaseCoeff = std::exp(-1.0f / (DSPConstants::HARMONIC_DENSITY_RELEASE_TIME_S * oversampledRate));

    // Validate harmonic density coefficients (matching pre-compression validation pattern)
    if (!std::isfinite(harmonicDensityAttackCoeff) || harmonicDensityAttackCoeff < 0.0f || harmonicDensityAttackCoeff > 1.0f)
        harmonicDensityAttackCoeff = 0.99f;  // Safe fallback (~2ms at 44.1kHz*4)
    if (!std::isfinite(harmonicDensityReleaseCoeff) || harmonicDensityReleaseCoeff < 0.0f || harmonicDensityReleaseCoeff > 1.0f)
        harmonicDensityReleaseCoeff = 0.999f;  // Safe fallback (~30ms at 44.1kHz*4)

    harmonicDensityEnvelope[0] = 0.0f;
    harmonicDensityEnvelope[1] = 0.0f;

    // Stateful distortion envelope coefficients (oversampled rate)
    tubeBiasAttackCoeff = std::exp(-1.0f / (DSPConstants::TUBE_BIAS_ATTACK_TIME_S * oversampledRate));
    tubeBiasReleaseCoeff = std::exp(-1.0f / (DSPConstants::TUBE_BIAS_RELEASE_TIME_S * oversampledRate));
    tapeHysteresisAttackCoeff = std::exp(-1.0f / (DSPConstants::TAPE_HYSTERESIS_ATTACK_TIME_S * oversampledRate));
    tapeHysteresisReleaseCoeff = std::exp(-1.0f / (DSPConstants::TAPE_HYSTERESIS_RELEASE_TIME_S * oversampledRate));
    tubeBiasEnvelope[0] = 0.0f;
    tubeBiasEnvelope[1] = 0.0f;
    tapeSaturationEnvelope[0] = 0.0f;
    tapeSaturationEnvelope[1] = 0.0f;

    // Auto-gain compensation coefficients (calculated at oversampled rate)
    autoGainAttackCoeff = std::exp(-1.0f / (DSPConstants::AUTO_GAIN_ATTACK_TIME_S * oversampledRate));
    autoGainReleaseCoeff = std::exp(-1.0f / (DSPConstants::AUTO_GAIN_RELEASE_TIME_S * oversampledRate));
    autoGainInputEnvelope = 0.0f;
    autoGainOutputEnvelope = 0.0f;
    autoGainCompensation = 1.0f;

    // LA-2A compression coefficients for OVERSAMPLED rate
    compAttackCoeffOversampled = std::exp(-1.0f / (DSPConstants::COMP_ATTACK_TIME_S * oversampledRate));
    compReleaseCoeffOversampled = std::exp(-1.0f / (DSPConstants::COMP_RELEASE_TIME_S * oversampledRate));
    compRmsHistoryCoeffOversampled = std::exp(-1.0f / (DSPConstants::COMP_RMS_HISTORY_TIME_S * oversampledRate));

    // Prepare buffers for band-split processing (oversampled size)
    // CRITICAL: JUCE's oversampling can produce variable output sizes depending on:
    // 1. Internal filter latency compensation
    // 2. Sample rate (44.1kHz vs 48kHz have different characteristics)
    // 3. Block size alignment requirements
    const size_t oversamplingLatencySamples = oversampling
        ? static_cast<size_t>(oversampling->getLatencyInSamples()) : 0;

    // Report latency to host for proper delay compensation
    setLatencySamples(static_cast<int>(oversamplingLatencySamples));

    // Capture fractional latency for internal dry-path alignment (compensates the
    // global wet/dry mix; host compensation above remains integer-only).
    dryDelaySamples = oversampling ? oversampling->getLatencyInSamples() : 0.0f;
    if (! std::isfinite(dryDelaySamples) || dryDelaySamples < 0.0f)
        dryDelaySamples = 0.0f;
    const int dryStateLen = static_cast<int>(std::ceil(dryDelaySamples)) + 2;
    dryDelayState.setSize(numChannels, dryStateLen, false, false, true);
    dryDelayState.clear();

    lowBandBuffer.setSize(numChannels, worstCaseOversampledBlockSize, false, false, true);
    highBandBuffer.setSize(numChannels, worstCaseOversampledBlockSize, false, false, true);
    dryBuffer.setSize(numChannels, samplesPerBlock + 64, false, false, true);
}


void PluginProcessor::releaseResources()
{
    cancelPendingUpdate();
    oversampling.reset();
    preHighPassFilter.reset();
    toneFilter.reset();
    toneFilterLow.reset();

    // Reset manual DC blocker state
    for (int ch = 0; ch < 2; ++ch)
    {
        manualDCBlockerPrevInput[ch] = 0.0f;
        manualDCBlockerPrevOutput[ch] = 0.0f;
    }

    // Reset pre-compression state
    preCompEnvelope[0] = 1.0f;
    preCompEnvelope[1] = 1.0f;

    // Reset harmonic density envelope state
    harmonicDensityEnvelope[0] = 0.0f;
    harmonicDensityEnvelope[1] = 0.0f;

    // Reset stateful distortion envelopes
    tubeBiasEnvelope[0] = 0.0f;
    tubeBiasEnvelope[1] = 0.0f;
    tapeSaturationEnvelope[0] = 0.0f;
    tapeSaturationEnvelope[1] = 0.0f;
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
            const float tubeSaturation = FastMath::tanh(tubeInput);

            // Blend tube character (subtle 2nd harmonic)
            sampleValue = sampleValue * (1.0f - DSPConstants::COMP_TUBE_BLEND) +
                         tubeSaturation * DSPConstants::COMP_TUBE_BLEND;

            // Apply makeup gain
            sampleValue *= makeupGainLinear;

            // Conditional soft clip (transparent below -1dBFS, prevents overs above)
            {
                const float absSample = std::abs(sampleValue);
                if (absSample > DSPConstants::COMP_SOFT_CLIP_THRESHOLD)
                {
                    const float sign = (sampleValue > 0.0f) ? 1.0f : -1.0f;
                    const float excess = absSample - DSPConstants::COMP_SOFT_CLIP_THRESHOLD;
                    sampleValue = sign * (DSPConstants::COMP_SOFT_CLIP_THRESHOLD
                                + FastMath::tanh(excess * 4.0f) * DSPConstants::COMP_SOFT_CLIP_HEADROOM);
                }
            }

            buffer.setSample(channel, sample, sampleValue);
        }
    }

    // Store the max gain reduction for UI meter display
    currentGainReductionDB.store(maxGainReductionDB, std::memory_order_relaxed);
}

void PluginProcessor::requestOversamplingRebuild(int stages)
{
    requestedOversamplingStages.store(stages, std::memory_order_release);
    triggerAsyncUpdate();
}

void PluginProcessor::handleAsyncUpdate()
{
    const double sampleRate = getSampleRate();
    const int samplesPerBlock = getBlockSize();
    if (sampleRate <= 0.0 || samplesPerBlock <= 0)
        return;

    const juce::ScopedLock callbackLockGuard(getCallbackLock());
    rebuildOversampling(sampleRate, samplesPerBlock);
}

void PluginProcessor::rebuildOversampling(double sampleRate, int samplesPerBlock)
{
    const int stages = requestedOversamplingStages.load(std::memory_order_acquire);
    const bool linearPhase = linearPhaseDryParam && (linearPhaseDryParam->load() > 0.5f);
    const int numChannels = std::max(1, getTotalNumInputChannels());
    const double sr = sampleRate;
    const int currentBlockSize = samplesPerBlock;
    const bool needsRebuild = stages != currentOversamplingStages
        || linearPhase != currentLinearPhase
        || currentNumChannels != numChannels
        || (stages > 0 && !oversampling);

    if (needsRebuild)
    {
        currentOversamplingStages = stages;
        currentLinearPhase = linearPhase;

        if (stages == 0)
        {
            oversampling.reset();
            oversamplingFactor = 1;
        }
        else
        {
            const auto filterType = linearPhase
                ? juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple
                : juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR;
            oversampling = std::make_unique<juce::dsp::Oversampling<float>>(
                numChannels, stages, filterType, false, false
            );
            oversampling->initProcessing(static_cast<size_t>(currentBlockSize));
            oversamplingFactor = oversampling->getOversamplingFactor();
        }
    }
    else if (oversampling)
    {
        oversampling->reset();
        oversampling->initProcessing(static_cast<size_t>(currentBlockSize));
    }
    currentNumChannels = numChannels;

    // Recalculate oversampled-rate-dependent coefficients
    const float oversampledRate = static_cast<float>(sr * oversamplingFactor);
    preCompAttackCoeff = std::exp(-1.0f / (DSPConstants::PRE_COMP_ATTACK_TIME_S * oversampledRate));
    preCompReleaseCoeff = std::exp(-1.0f / (DSPConstants::PRE_COMP_RELEASE_TIME_S * oversampledRate));
    harmonicDensityAttackCoeff = std::exp(-1.0f / (DSPConstants::HARMONIC_DENSITY_ATTACK_TIME_S * oversampledRate));
    harmonicDensityReleaseCoeff = std::exp(-1.0f / (DSPConstants::HARMONIC_DENSITY_RELEASE_TIME_S * oversampledRate));
    tubeBiasAttackCoeff = std::exp(-1.0f / (DSPConstants::TUBE_BIAS_ATTACK_TIME_S * oversampledRate));
    tubeBiasReleaseCoeff = std::exp(-1.0f / (DSPConstants::TUBE_BIAS_RELEASE_TIME_S * oversampledRate));
    tapeHysteresisAttackCoeff = std::exp(-1.0f / (DSPConstants::TAPE_HYSTERESIS_ATTACK_TIME_S * oversampledRate));
    tapeHysteresisReleaseCoeff = std::exp(-1.0f / (DSPConstants::TAPE_HYSTERESIS_RELEASE_TIME_S * oversampledRate));
    autoGainAttackCoeff = std::exp(-1.0f / (DSPConstants::AUTO_GAIN_ATTACK_TIME_S * oversampledRate));
    autoGainReleaseCoeff = std::exp(-1.0f / (DSPConstants::AUTO_GAIN_RELEASE_TIME_S * oversampledRate));
    compAttackCoeffOversampled = std::exp(-1.0f / (DSPConstants::COMP_ATTACK_TIME_S * oversampledRate));
    compReleaseCoeffOversampled = std::exp(-1.0f / (DSPConstants::COMP_RELEASE_TIME_S * oversampledRate));
    compRmsHistoryCoeffOversampled = std::exp(-1.0f / (DSPConstants::COMP_RMS_HISTORY_TIME_S * oversampledRate));

    // Re-prepare filters at new oversampled spec
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sr * oversamplingFactor;
    spec.maximumBlockSize = static_cast<juce::uint32>(currentBlockSize * oversamplingFactor);
    spec.numChannels = static_cast<juce::uint32>(numChannels);

    const float toneFreq = toneParam ? toneParam->load() : 20000.0f;
    toneFilter.prepare(spec);
    *toneFilter.state = *juce::dsp::IIR::Coefficients<float>::makeLowPass(spec.sampleRate, toneFreq);
    toneFilter.reset();
    lastToneFreq = toneFreq;

    toneFilterLow.prepare(spec);
    *toneFilterLow.state = *juce::dsp::IIR::Coefficients<float>::makeLowPass(spec.sampleRate, toneFreq);
    toneFilterLow.reset();

    // Re-prepare sub guard filters
    const float sgFreq = subGuardFreqParam ? subGuardFreqParam->load() : DSPConstants::SUBGUARD_FREQ_DEFAULT;
    const float filterInitFreq = (sgFreq <= 1.0f) ? 60.0f : sgFreq;
    constexpr float lr2Q = 0.5f;

    lowPassFilter1.prepare(spec);
    *lowPassFilter1.state = *juce::dsp::IIR::Coefficients<float>::makeLowPass(spec.sampleRate, filterInitFreq);
    lowPassFilter1.reset();
    lowPassFilter2.prepare(spec);
    *lowPassFilter2.state = *juce::dsp::IIR::Coefficients<float>::makeLowPass(spec.sampleRate, filterInitFreq);
    lowPassFilter2.reset();
    highPassFilter1.prepare(spec);
    *highPassFilter1.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass(spec.sampleRate, filterInitFreq);
    highPassFilter1.reset();
    highPassFilter2.prepare(spec);
    *highPassFilter2.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass(spec.sampleRate, filterInitFreq);
    highPassFilter2.reset();
    subGuardLP12.prepare(spec);
    *subGuardLP12.state = *juce::dsp::IIR::Coefficients<float>::makeLowPass(spec.sampleRate, filterInitFreq, lr2Q);
    subGuardLP12.reset();
    subGuardHP12.prepare(spec);
    *subGuardHP12.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass(spec.sampleRate, filterInitFreq, lr2Q);
    subGuardHP12.reset();
    subGuardLP18_1.prepare(spec);
    *subGuardLP18_1.state = *juce::dsp::IIR::Coefficients<float>::makeFirstOrderLowPass(spec.sampleRate, filterInitFreq);
    subGuardLP18_1.reset();
    subGuardLP18_2.prepare(spec);
    *subGuardLP18_2.state = *juce::dsp::IIR::Coefficients<float>::makeLowPass(spec.sampleRate, filterInitFreq, lr2Q);
    subGuardLP18_2.reset();
    subGuardHP18_1.prepare(spec);
    *subGuardHP18_1.state = *juce::dsp::IIR::Coefficients<float>::makeFirstOrderHighPass(spec.sampleRate, filterInitFreq);
    subGuardHP18_1.reset();
    subGuardHP18_2.prepare(spec);
    *subGuardHP18_2.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass(spec.sampleRate, filterInitFreq, lr2Q);
    subGuardHP18_2.reset();

    lastOversampledSampleRate = spec.sampleRate;

    // Reallocate band-split buffers at new oversampled block size
    const size_t expectedOversampledSize = static_cast<size_t>(currentBlockSize) * oversamplingFactor;
    const size_t latSamples = oversampling
        ? static_cast<size_t>(oversampling->getLatencyInSamples()) : 0;
    const int oversampledBlockSize = static_cast<int>((expectedOversampledSize + latSamples) * 2 + 128);
    lowBandBuffer.setSize(numChannels, oversampledBlockSize, false, false, true);
    highBandBuffer.setSize(numChannels, oversampledBlockSize, false, false, true);

    // Report updated latency
    setLatencySamples(static_cast<int>(latSamples));

    // Refresh fractional dry-delay state for the new oversampling factor
    dryDelaySamples = oversampling ? oversampling->getLatencyInSamples() : 0.0f;
    if (! std::isfinite(dryDelaySamples) || dryDelaySamples < 0.0f)
        dryDelaySamples = 0.0f;
    const int dryStateLen = static_cast<int>(std::ceil(dryDelaySamples)) + 2;
    dryDelayState.setSize(numChannels, dryStateLen, false, false, true);
    dryDelayState.clear();

    // Reset all DSP state
    resetDSPState();
}

void PluginProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    RT_ASSERT_SCOPE();
    juce::ignoreUnused(midiMessages);

    // CRITICAL: Enable flush-to-zero and denormals-are-zero to prevent denormal issues at 44.1kHz
    // Denormals cause massive CPU spikes and audio dropout
    juce::ScopedNoDenormals noDenormals;

    // Early return for empty buffers
    if (buffer.getNumSamples() == 0 || buffer.getNumChannels() == 0)
        return;

    // Thread-safe state reset (triggered by setStateInformation on GUI thread)
    if (stateNeedsReset.exchange(false, std::memory_order_acquire))
    {
        resetDSPState();
    }

    // CRITICAL: Detect sample rate changes and update coefficients
    // Belt-and-suspenders catch for non-compliant hosts that change sample rate without
    // calling prepareToPlay. Standard DAWs (Reaper, Ableton, FL, Bitwig, Cubase, Pro Tools)
    // always call prepareToPlay on SR change, so in normal operation this branch is
    // skipped. Everything below must stay allocation-free for RT safety:
    //   - updateSampleRateDependentCoefficients: only std::exp, no allocation.
    //   - SmoothedValue::reset(double, double) is noexcept and allocation-free in JUCE 7.
    // The branch does NOT rebuild the oversampler or re-prepare filters; that is deferred
    // to a proper prepareToPlay or handleAsyncUpdate on the message thread. Block-size
    // changes without prepareToPlay are NOT supported: the dryBuffer jassert below will
    // fire in debug, and release builds have undefined behaviour if the block grows beyond
    // the last prepared size. No supported DAW exhibits that pattern.
    const double currentSR = getSampleRate();
    if (currentSR > 0 && std::abs(currentSR - lastSampleRate) > 0.1)
    {
        debugHadUnexpectedSampleRateChange.store(true, std::memory_order_relaxed);
        currentSampleRate = static_cast<float>(currentSR);

        // Update time-constant coefficients (DC blocking, compression envelope)
        updateSampleRateDependentCoefficients(currentSR);

        // Update smoothed values for new sample rate (preserve current values)
        auto tempOutputGain = smoothedOutputGain.getCurrentValue();
        smoothedOutputGain.reset(currentSR, DSPConstants::GAIN_SMOOTH_TIME_S);
        smoothedOutputGain.setCurrentAndTargetValue(tempOutputGain);

        auto tempGlobalMix = smoothedGlobalMix.getCurrentValue();
        smoothedGlobalMix.reset(currentSR, 0.02);
        smoothedGlobalMix.setCurrentAndTargetValue(tempGlobalMix);

        // Force update of all dynamic filters on next use
        lastHighPassFreq = -1.0f;  // Force hi-pass filter update
        lastOversampledSampleRate = 0.0;  // Force distortion filter update
    }

    // Load parameters and scale them
    // SAFETY: Validate all parameter loads to prevent NaN propagation
    const auto inGainParam = inputGainParam->load();
    const auto outGainParam = outputGainParam->load();
    const auto distortionParam = distortionAmountParam->load();

    // Emergency NaN detection - if parameters are corrupted, use safe defaults
    if (std::isnan(inGainParam) || std::isnan(outGainParam) || std::isnan(distortionParam))
    {
        debugHadNaN.store(true, std::memory_order_relaxed);
        return;  // Skip this block to prevent NaN propagation
    }
    auto highPassFreq = highPassFreqParam->load();
    const float subGuardFreq = subGuardFreqParam->load();
    const int clipType = static_cast<int>(clipTypeParam->load());
    auto lfoRate = lfoRateParam->load();
    auto lfoDepth = lfoDepthParam->load();
    const int lfoWaveform = static_cast<int>(lfoWaveformParam->load());
    auto compPeakReduction = compPeakReductionParam->load();
    auto compMakeupGain = compMakeupGainParam->load();
    const int compRatioMode = static_cast<int>(compRatioParam->load());
    const bool compEnabled = compEnabledParam->load() > 0.5f;
    const bool autoGainEnabled = autoGainEnabledParam->load() > 0.5f;
    const bool extremeEnabled = extremeEnabledParam->load() > 0.5f;
    auto distMix = distMixParam->load();
    auto globalMix = globalMixParam->load();

    // SAFETY: Validate all parameter values to prevent NaN propagation
    if (std::isnan(highPassFreq)) highPassFreq = DSPConstants::DEFAULT_HIPASS_FREQ;
    if (std::isnan(lfoRate)) lfoRate = 0.0f;
    if (std::isnan(lfoDepth)) lfoDepth = 0.0f;
    if (std::isnan(compPeakReduction)) compPeakReduction = 0.0f;
    if (std::isnan(compMakeupGain)) compMakeupGain = 50.0f;
    if (std::isnan(distMix)) distMix = 100.0f;
    if (std::isnan(globalMix)) globalMix = 100.0f;

    // Set targets for smoothed parameters (prevent zipper noise from automation)
    smoothedGlobalMix.setTargetValue(globalMix / 100.0f);

    // LFO modulation for dynamic distortion effects
    const bool lfoEnabled = lfoEnabledParam->load() > 0.5f;
    const int lfoDestination = static_cast<int>(lfoDestinationParam->load());

    // Determine if this destination needs per-sample LFO (0=dist, 3=mix, 4=gain)
    // or block-level LFO (1=tone, 2=hipass - filter coefficients can't change per-sample)
    const bool perSampleLFO = (lfoDestination == 0 || lfoDestination == 3 || lfoDestination == 4);
    const float lfoPhaseIncrement = (lfoEnabled && lfoRate > 0.0f && currentSampleRate > 0.0f)
                                   ? (lfoRate / currentSampleRate) : 0.0f;

    // For block-level destinations (1,2): compute LFO once and advance phase
    float lfoValue = 0.0f;
    if (lfoEnabled && lfoRate > 0.0f && currentSampleRate > 0.0f && !perSampleLFO)
    {
        lfoValue = generateLFOWaveform(lfoPhase, lfoWaveform);
        if (std::isnan(lfoValue) || std::isinf(lfoValue))
            lfoValue = 0.0f;

        // Advance phase for the entire block
        lfoPhase += lfoPhaseIncrement * buffer.getNumSamples();
        if (lfoPhase >= 0.0f)
            lfoPhase = std::fmod(lfoPhase, 1.0f);
        else
            lfoPhase = 0.0f;

        lfoPhaseForUI.store(lfoPhase, std::memory_order_relaxed);
    }
    // For per-sample destinations (0,3,4): phase is advanced in the sample loops below

    const float lfoModulation = (lfoValue * lfoDepth / 100.0f);  // Block-level only (for dest 1,2)

    // Load tone parameter (not loaded earlier in processBlock)
    const float toneParamValue = toneParam ? toneParam->load() : 20000.0f;

    // Initialize modulated parameter copies (block-level defaults)
    float modulatedDistortionParam = distortionParam;
    float modulatedToneFreq = toneParamValue;
    float modulatedHighPassFreq = highPassFreq;
    float modulatedDistMix = distMix;
    float modulatedOutputGain = outGainParam;

    // Apply block-level modulation for filter destinations only
    if (!std::isnan(lfoModulation) && !std::isinf(lfoModulation))
    {
        switch (lfoDestination)
        {
        case 1:  // Tone Filter (2000-20000 Hz) - Logarithmic for musical sweep
            {
                const float centerFreqLog = std::log2(juce::jmax(2000.0f, toneParamValue));
                const float modulatedFreqLog = juce::jlimit(10.96f, 14.29f,
                    centerFreqLog + (lfoModulation * 2.0f));  // ±2 octaves, pre-clamped
                modulatedToneFreq = std::pow(2.0f, modulatedFreqLog);
            }
            break;

        case 2:  // Hi-Pass Filter (20-500 Hz) - Logarithmic
            {
                const float centerFreqLog = std::log2(juce::jmax(20.0f, highPassFreq));
                const float modulatedFreqLog = juce::jlimit(4.32f, 8.97f,
                    centerFreqLog + (lfoModulation * 1.5f));  // ±1.5 octaves, pre-clamped
                modulatedHighPassFreq = std::pow(2.0f, modulatedFreqLog);
            }
            break;

        default:
            break;  // Per-sample destinations handled in sample loops
        }
    }

    // Publish preamble parameters to block-scope members for stage helpers (PR-8)
    pb_modulatedHighPassFreq    = modulatedHighPassFreq;
    pb_modulatedDistortionParam = modulatedDistortionParam;
    pb_modulatedToneFreq        = modulatedToneFreq;
    pb_distortionParam          = distortionParam;
    pb_distMix                  = distMix;
    pb_extremeEnabled           = extremeEnabled;
    pb_autoGainEnabled          = autoGainEnabled;
    pb_compEnabled              = compEnabled;
    pb_compPeakReduction        = compPeakReduction;
    pb_compMakeupGain           = compMakeupGain;
    pb_compRatioMode            = compRatioMode;
    pb_lfoPhaseIncrement        = lfoPhaseIncrement;
    pb_perSampleLFO             = perSampleLFO;
    pb_lfoEnabled               = lfoEnabled;
    pb_lfoWaveform              = lfoWaveform;
    pb_lfoDepth                 = lfoDepth;
    pb_lfoDestination           = lfoDestination;
    pb_clipType                 = clipType;
    pb_subGuardFreq             = subGuardFreq;

    // Calculate distortion drive from modulated parameter
    float distortionDrive = 1.0f + (modulatedDistortionParam / 100.0f) * 3.0f;

    // Validate distortion drive
    if (std::isnan(distortionDrive) || std::isinf(distortionDrive) || distortionDrive < 1.0f)
        distortionDrive = 1.0f;

    if (extremeEnabled)
        distortionDrive *= 4.0f;

    // Convert modulated gain parameters to processing values
    const float inputGain = std::pow(inGainParam / 50.0f, 1.5f);
    const auto modulatedOutGainDB = (modulatedOutputGain - 50.0f) * 0.18f;
    const auto outGain = juce::Decibels::decibelsToGain(modulatedOutGainDB);

    // Set target value for output gain (consumed at normal rate)
    smoothedOutputGain.setTargetValue(outGain);

    // ========== SAVE DRY BUFFER FOR GLOBAL MIX ==========
    const float globalMixAmount = smoothedGlobalMix.getCurrentValue();
    // Capture dry buffer if mix is not 100% wet OR if it is currently transitioning (to avoid a one-block glitch)
    const bool needsGlobalMix = globalMixAmount < 0.999f || smoothedGlobalMix.isSmoothing();
    if (needsGlobalMix)
    {
        jassert(dryBuffer.getNumSamples() >= buffer.getNumSamples());
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            dryBuffer.copyFrom(ch, 0, buffer, ch, 0, buffer.getNumSamples());
    }

    // Bypass state detection
    const bool bypassed = (modulatedDistortionParam < 0.5f && !compEnabled);

    // TRUE BYPASS MODE: Pass through with output gain only
    if (bypassed)
    {
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            const float currentOutputGain = smoothedOutputGain.getNextValue();
            for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            {
                // Simple pass-through with output gain
                float sample_value = buffer.getSample(channel, sample);
                buffer.setSample(channel, sample, sample_value * currentOutputGain);
            }
        }

        // ========== OUTPUT LIMITER (applies even in bypass mode) ==========
        // Safety limiter at -0.5dBFS to prevent clipping even when bypassed
        {
            const float thresholdLinear = juce::Decibels::decibelsToGain(DSPConstants::OUTPUT_LIMITER_THRESHOLD_DB);
            const float threshDB = DSPConstants::OUTPUT_LIMITER_THRESHOLD_DB;
            const float kneeDB = DSPConstants::OUTPUT_LIMITER_KNEE_DB;

            for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            {
                float peakLevel = 0.0f;
                for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
                {
                    const float absValue = std::abs(buffer.getSample(channel, sample));
                    if (absValue > peakLevel)
                        peakLevel = absValue;
                }

                float targetGain = 1.0f;
                if (peakLevel > thresholdLinear)
                {
                    const float peakDB = juce::Decibels::gainToDecibels(peakLevel + 1e-6f);
                    const float overDB = peakDB - threshDB;

                    float grDB = 0.0f;
                    if (overDB < kneeDB)
                    {
                        const float t = overDB / kneeDB;
                        grDB = overDB * t;
                    }
                    else
                    {
                        grDB = kneeDB + (overDB - kneeDB);
                    }

                    targetGain = juce::Decibels::decibelsToGain(-grDB);
                }

                if (targetGain < outputLimiterEnvelope)
                {
                    outputLimiterEnvelope = outputLimiterAttackCoeff * outputLimiterEnvelope
                                          + (1.0f - outputLimiterAttackCoeff) * targetGain;
                }
                else
                {
                    outputLimiterEnvelope = outputLimiterReleaseCoeff * outputLimiterEnvelope
                                          + (1.0f - outputLimiterReleaseCoeff) * targetGain;
                }

                for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
                {
                    auto* channelData = buffer.getWritePointer(channel);
                    channelData[sample] *= outputLimiterEnvelope;
                }
            }
        }

        // ========== GLOBAL MIX (Bypass path) ==========
        if (needsGlobalMix)
        {
            for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            {
                const float currentMix = smoothedGlobalMix.getNextValue();
                for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
                {
                    auto* channelData = buffer.getWritePointer(channel);
                    const float drySample = dryBuffer.getSample(channel, sample);
                    channelData[sample] = drySample * (1.0f - currentMix) + channelData[sample] * currentMix;
                }
            }
        }
        else
        {
            // Consume smoothed values to keep state in sync
            for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
                smoothedGlobalMix.getNextValue();
        }

        // Update oscilloscope even in bypass mode
        for (int sample = 0; sample < buffer.getNumSamples(); sample += DSPConstants::SCOPE_UPDATE_DECIMATION)
        {
            if (scopeFifo.getFreeSpace() > 0)
            {
                const int next = juce::jmin(sample + 1, buffer.getNumSamples() - 1);
                const float leftSample = (buffer.getSample(0, sample) + buffer.getSample(0, next)) * 0.5f;
                const float rightSample = buffer.getNumChannels() > 1 ?
                    (buffer.getSample(1, sample) + buffer.getSample(1, next)) * 0.5f : leftSample;
                pushSampleToScope(leftSample, rightSample);
            }
        }

        return;  // Skip all DSP processing
    }

    applyPreHighpass(buffer);

    // Check actual oversampled size and validate buffer allocation (always enabled)
    const size_t actualOversampledSamples = pb_oversampledBlock.getNumSamples();

    // CRITICAL: Check for zero samples (would cause division by zero)
    if (actualOversampledSamples == 0)
    {
        return;
    }

    // CRITICAL FIX: Manual linear interpolation for parameters consumed in oversampled domain
    // Calculate step size from LAST block's final value to THIS block's target value
    const float gainDelta = (inputGain - lastInputGain) / static_cast<float>(actualOversampledSamples);
    const float driveDelta = (distortionDrive - lastDistortionDrive) / static_cast<float>(actualOversampledSamples);
    const float targetMixAmount = modulatedDistMix / 100.0f;  // 0.0 to 1.0
    const float mixDelta = (targetMixAmount - lastDistMix) / static_cast<float>(actualOversampledSamples);

    // Start from last block's values
    float currentInputGain = lastInputGain;
    float currentDrive = lastDistortionDrive;
    float currentMixAmount = lastDistMix;

    if (actualOversampledSamples > static_cast<size_t>(lowBandBuffer.getNumSamples()))
    {
        debugHadBufferOverflow.store(true, std::memory_order_relaxed);
    }
    jassert(actualOversampledSamples <= static_cast<size_t>(lowBandBuffer.getNumSamples()));
    jassert(actualOversampledSamples <= static_cast<size_t>(highBandBuffer.getNumSamples()));

    // Get the actual oversampled sample rate
    const double oversampledSR = getSampleRate() * oversamplingFactor;

    // Publish oversampled-domain context to block-scope members for stage helpers
    pb_numSamples       = actualOversampledSamples;
    pb_numChannels      = pb_oversampledBlock.getNumChannels();
    pb_oversampledSR    = oversampledSR;
    pb_gainDelta        = gainDelta;
    pb_driveDelta       = driveDelta;
    pb_mixDelta         = mixDelta;
    pb_currentInputGain = currentInputGain;
    pb_currentDrive     = currentDrive;
    pb_currentMixAmount = currentMixAmount;

    // Sub Guard filter coefficients are updated dynamically in the processing block below
    // No need for static sample rate change detection

    // NOTE: Pre-highpass filter is now applied BEFORE upsampling (see above)

    const size_t numSamples = pb_oversampledBlock.getNumSamples();
    const size_t numChannels = pb_oversampledBlock.getNumChannels();

    applyPreCompression();

    // ========== AUTO-GAIN COMPENSATION: Measure input RMS ==========
    if (autoGainEnabled)
    {
        // Calculate input RMS for auto-gain compensation (before distortion)
        float inputSumSquares = 0.0f;
        for (size_t ch = 0; ch < numChannels; ++ch)
        {
            const float* data = pb_oversampledBlock.getChannelPointer(ch);
            for (size_t i = 0; i < numSamples; ++i)
                inputSumSquares += data[i] * data[i];
        }
        const float inputRms = std::sqrt(inputSumSquares / (numSamples * numChannels));

        // Update input envelope (one-pole filter with asymmetric attack/release)
        if (inputRms > autoGainInputEnvelope)
            autoGainInputEnvelope = autoGainAttackCoeff * autoGainInputEnvelope + (1.0f - autoGainAttackCoeff) * inputRms;
        else
            autoGainInputEnvelope = autoGainReleaseCoeff * autoGainInputEnvelope + (1.0f - autoGainReleaseCoeff) * inputRms;
    }

    // Sub Guard variable-slope crossover processing
    // Check if Sub Guard is OFF (value <= 1.0 Hz to account for smoothing)
    if (subGuardFreq <= 1.0f)
    {
        // === SUB GUARD OFF: Full-range distortion (no band-split) ===

        // Apply distortion to entire oversampledBlock (all frequencies)
        // Per-sample LFO phase increment (oversampled rate: divide by oversamplingFactor)
        const float lfoOversampledPhaseInc = lfoPhaseIncrement / static_cast<float>(oversamplingFactor);

        for (size_t sample = 0; sample < numSamples; ++sample)
        {
            // Per-sample LFO for destinations 0 (distortion) and 3 (dist mix)
            float sampleDrive = currentDrive;
            float sampleMixAmount = currentMixAmount;
            float sampleDistortionParam = modulatedDistortionParam;

            if (perSampleLFO && lfoEnabled && lfoPhaseIncrement > 0.0f)
            {
                float sampleLfoValue = generateLFOWaveform(lfoPhase, lfoWaveform);
                if (std::isnan(sampleLfoValue) || std::isinf(sampleLfoValue))
                    sampleLfoValue = 0.0f;
                const float sampleLfoMod = sampleLfoValue * lfoDepth / 100.0f;

                if (lfoDestination == 0)  // Distortion Amount
                {
                    sampleDistortionParam = juce::jlimit(0.0f, 100.0f,
                        distortionParam + sampleLfoMod * 50.0f);
                    sampleDrive = 1.0f + (sampleDistortionParam / 100.0f) * 3.0f;
                    if (extremeEnabled) sampleDrive *= 4.0f;
                }
                else if (lfoDestination == 3)  // Dist Mix
                {
                    const float modMix = juce::jlimit(0.0f, 100.0f,
                        distMix + sampleLfoMod * 50.0f);
                    sampleMixAmount = modMix / 100.0f;
                }
                // Destination 4 (output gain) handled in the output gain loop below

                // Advance LFO phase per oversampled sample
                lfoPhase += lfoOversampledPhaseInc;
                if (lfoPhase >= 1.0f)
                    lfoPhase -= 1.0f;
            }

            for (size_t channel = 0; channel < numChannels; ++channel)
            {
                auto* channelData = pb_oversampledBlock.getChannelPointer(channel);
                const int ch = static_cast<int>(channel);

                // Read input
                float inputSample = channelData[sample];

                // Bypass distortion if amount is negligible (< 0.5%)
                if (sampleDistortionParam < 0.5f)
                {
                    channelData[sample] = inputSample;  // Pure bypass
                }
                else
                {
                    // Update harmonic density envelope for sub-linear scaling
                    const float inputLevel = std::abs(inputSample);
                    if (inputLevel > harmonicDensityEnvelope[ch])
                        harmonicDensityEnvelope[ch] = harmonicDensityAttackCoeff * harmonicDensityEnvelope[ch]
                                                    + (1.0f - harmonicDensityAttackCoeff) * inputLevel;
                    else
                        harmonicDensityEnvelope[ch] = harmonicDensityReleaseCoeff * harmonicDensityEnvelope[ch]
                                                    + (1.0f - harmonicDensityReleaseCoeff) * inputLevel;

                    // Calculate inverse harmonic scale
                    const float clampedEnv = std::max(0.0f, harmonicDensityEnvelope[ch]);
                    const float harmonicScale = juce::jlimit(DSPConstants::HARMONIC_DENSITY_MIN_SCALE, 1.0f,
                                                            1.0f / (1.0f + std::sqrt(clampedEnv)));

                    // Apply studio distortion with sub-linear harmonic scaling
                    float distorted = applyStudioDistortion(inputSample, currentInputGain, sampleDrive, clipType,
                        extremeEnabled ? 1.0f : harmonicScale, ch);

                    // Wet/Dry mix (per-sample interpolated to prevent zipper noise)
                    channelData[sample] = inputSample * (1.0f - sampleMixAmount) + distorted * sampleMixAmount;
                }
            }

            // Manually step the smoothed parameters AFTER processing all channels
            currentInputGain += gainDelta;
            currentDrive += driveDelta;
            currentMixAmount += mixDelta;
        }
    }
    else
    {
        // === SUB GUARD ACTIVE: Variable-slope clean low + Distorted high ===

        // Update Sub Guard frequency with smoothing
        smoothedSubGuardFreq.setTargetValue(subGuardFreq);
        const float currentSubGuardFreq = smoothedSubGuardFreq.getNextValue();

        // Update filter coefficients if frequency changed significantly
        if (std::abs(currentSubGuardFreq - lastSubGuardFreq) > 0.5f)
        {
            updateSubGuardCoefficients(currentSubGuardFreq, oversampledSR);
            lastSubGuardFreq = currentSubGuardFreq;
        }

        // Determine which filter order to use based on frequency
        const SubGuardFilterOrder filterOrder = determineSubGuardFilterOrder(currentSubGuardFreq);

        // SAFETY CHECK: Ensure we have enough buffer space
        const int requiredBufferSize = static_cast<int>(numSamples);
        if (requiredBufferSize > lowBandBuffer.getNumSamples() || requiredBufferSize > highBandBuffer.getNumSamples())
        {
            debugHadBufferOverflow.store(true, std::memory_order_relaxed);
            // Skip band-split processing to prevent crash
            return;
        }

        // Copy input to both band buffers (with bounds checking)
        // Note: copyFrom overwrites exactly numSamples; tail is never read via getSubBlock(0, numSamples).
        for (size_t channel = 0; channel < numChannels; ++channel)
        {
            const int channelIdx = static_cast<int>(channel);
            const int sampleCount = static_cast<int>(numSamples);

            lowBandBuffer.copyFrom(channelIdx, 0,
                pb_oversampledBlock.getChannelPointer(channel),
                sampleCount);
            highBandBuffer.copyFrom(channelIdx, 0,
                pb_oversampledBlock.getChannelPointer(channel),
                sampleCount);
        }

        // Filter the bands using the appropriate filter order
        auto lowBlock = juce::dsp::AudioBlock<float>(lowBandBuffer).getSubBlock(0, numSamples);
        auto highBlock = juce::dsp::AudioBlock<float>(highBandBuffer).getSubBlock(0, numSamples);

        // Apply filters based on determined order
        switch (filterOrder)
        {
            case SubGuardFilterOrder::LR12:  // AGGRESSIVE - 12dB/octave
                subGuardLP12.process(juce::dsp::ProcessContextReplacing<float>(lowBlock));
                subGuardHP12.process(juce::dsp::ProcessContextReplacing<float>(highBlock));
                break;

            case SubGuardFilterOrder::LR18:  // CONTROL - 18dB/octave
                subGuardLP18_1.process(juce::dsp::ProcessContextReplacing<float>(lowBlock));
                subGuardLP18_2.process(juce::dsp::ProcessContextReplacing<float>(lowBlock));
                subGuardHP18_1.process(juce::dsp::ProcessContextReplacing<float>(highBlock));
                subGuardHP18_2.process(juce::dsp::ProcessContextReplacing<float>(highBlock));
                break;

            case SubGuardFilterOrder::LR24:  // PRESERVE - 24dB/octave
            default:
                lowPassFilter1.process(juce::dsp::ProcessContextReplacing<float>(lowBlock));
                lowPassFilter2.process(juce::dsp::ProcessContextReplacing<float>(lowBlock));
                highPassFilter1.process(juce::dsp::ProcessContextReplacing<float>(highBlock));
                highPassFilter2.process(juce::dsp::ProcessContextReplacing<float>(highBlock));
                break;
        }

        // Apply studio distortion ONLY to the high band (with pre/post LP + DC block)
        // Per-sample LFO phase increment (oversampled rate)
        const float lfoOversampledPhaseIncSG = lfoPhaseIncrement / static_cast<float>(oversamplingFactor);

        for (size_t sample = 0; sample < numSamples; ++sample)
        {
            // Per-sample LFO for destinations 0 (distortion) and 3 (dist mix)
            float sampleDrive = currentDrive;
            float sampleMixAmount = currentMixAmount;
            float sampleDistortionParam = modulatedDistortionParam;

            if (perSampleLFO && lfoEnabled && lfoPhaseIncrement > 0.0f)
            {
                float sampleLfoValue = generateLFOWaveform(lfoPhase, lfoWaveform);
                if (std::isnan(sampleLfoValue) || std::isinf(sampleLfoValue))
                    sampleLfoValue = 0.0f;
                const float sampleLfoMod = sampleLfoValue * lfoDepth / 100.0f;

                if (lfoDestination == 0)  // Distortion Amount
                {
                    sampleDistortionParam = juce::jlimit(0.0f, 100.0f,
                        distortionParam + sampleLfoMod * 50.0f);
                    sampleDrive = 1.0f + (sampleDistortionParam / 100.0f) * 3.0f;
                    if (extremeEnabled) sampleDrive *= 4.0f;
                }
                else if (lfoDestination == 3)  // Dist Mix
                {
                    const float modMix = juce::jlimit(0.0f, 100.0f,
                        distMix + sampleLfoMod * 50.0f);
                    sampleMixAmount = modMix / 100.0f;
                }

                // Advance LFO phase per oversampled sample
                lfoPhase += lfoOversampledPhaseIncSG;
                if (lfoPhase >= 1.0f)
                    lfoPhase -= 1.0f;
            }

            for (size_t channel = 0; channel < numChannels; ++channel)
            {
                auto* highBandData = highBandBuffer.getWritePointer(static_cast<int>(channel));
                const int ch = static_cast<int>(channel);

                // Read input (already band-split)
                float inputSample = highBandData[sample];

                // Bypass distortion if amount is negligible (< 0.5%)
                if (sampleDistortionParam < 0.5f)
                {
                    highBandData[sample] = inputSample;  // Pure bypass
                }
                else
                {
                    // Update harmonic density envelope for sub-linear scaling
                    const float inputLevel = std::abs(inputSample);
                    if (inputLevel > harmonicDensityEnvelope[ch])
                        harmonicDensityEnvelope[ch] = harmonicDensityAttackCoeff * harmonicDensityEnvelope[ch]
                                                    + (1.0f - harmonicDensityAttackCoeff) * inputLevel;
                    else
                        harmonicDensityEnvelope[ch] = harmonicDensityReleaseCoeff * harmonicDensityEnvelope[ch]
                                                    + (1.0f - harmonicDensityReleaseCoeff) * inputLevel;

                    // Calculate inverse harmonic scale: high input → fewer harmonics (prevents harshness)
                    const float clampedEnv = std::max(0.0f, harmonicDensityEnvelope[ch]);
                    const float harmonicScale = juce::jlimit(DSPConstants::HARMONIC_DENSITY_MIN_SCALE, 1.0f,
                                                            1.0f / (1.0f + std::sqrt(clampedEnv)));

                    // Apply studio distortion with sub-linear harmonic scaling
                    float distorted = applyStudioDistortion(inputSample, currentInputGain, sampleDrive, clipType,
                        extremeEnabled ? 1.0f : harmonicScale, ch);

                    // Wet/Dry mix (per-sample interpolated to prevent zipper noise)
                    highBandData[sample] = inputSample * (1.0f - sampleMixAmount) + distorted * sampleMixAmount;
                }
            }

            // Manually step the smoothed parameters AFTER processing all channels
            currentInputGain += gainDelta;
            currentDrive += driveDelta;
            currentMixAmount += mixDelta;
        }

        // Recombine: Clean low + Distorted high
        for (size_t channel = 0; channel < numChannels; ++channel)
        {
            auto* outputData = pb_oversampledBlock.getChannelPointer(channel);
            const auto* lowData = lowBandBuffer.getReadPointer(static_cast<int>(channel));
            const auto* highData = highBandBuffer.getReadPointer(static_cast<int>(channel));

            for (size_t sample = 0; sample < numSamples; ++sample)
            {
                outputData[sample] = lowData[sample] + highData[sample];
            }
        }
    }  // end else (Sub Guard active)

    // ========== SUB GUARD: Remove clean low band before post-distortion processing ==========
    // When Sub Guard is active, subtract the clean low band from oversampledBlock
    // so that waveshaper, tone filter, and compressor only process the high band.
    // The clean low band is added back after all nonlinear processing is complete.
    const bool subGuardActive = (subGuardFreq > 1.0f);
    if (subGuardActive)
    {
        for (size_t channel = 0; channel < numChannels; ++channel)
        {
            auto* outputData = pb_oversampledBlock.getChannelPointer(channel);
            const auto* lowData = lowBandBuffer.getReadPointer(static_cast<int>(channel));

            for (size_t sample = 0; sample < numSamples; ++sample)
            {
                outputData[sample] -= lowData[sample];
            }
        }
    }

    // ========== AUTO-GAIN COMPENSATION: Measure output RMS and apply compensation ==========
    if (autoGainEnabled)
    {
        // Calculate output RMS for auto-gain compensation (after distortion)
        float outputSumSquares = 0.0f;
        for (size_t ch = 0; ch < numChannels; ++ch)
        {
            const float* data = pb_oversampledBlock.getChannelPointer(ch);
            for (size_t i = 0; i < numSamples; ++i)
                outputSumSquares += data[i] * data[i];
        }
        const float outputRms = std::sqrt(outputSumSquares / (numSamples * numChannels));

        // Update output envelope (one-pole filter with asymmetric attack/release)
        if (outputRms > autoGainOutputEnvelope)
            autoGainOutputEnvelope = autoGainAttackCoeff * autoGainOutputEnvelope + (1.0f - autoGainAttackCoeff) * outputRms;
        else
            autoGainOutputEnvelope = autoGainReleaseCoeff * autoGainOutputEnvelope + (1.0f - autoGainReleaseCoeff) * outputRms;

        // Calculate compensation gain (input/output ratio with safety limits)
        if (autoGainOutputEnvelope > 0.0001f)  // Avoid division by near-zero
        {
            const float rawCompensation = autoGainInputEnvelope / autoGainOutputEnvelope;
            autoGainCompensation = juce::jlimit(DSPConstants::AUTO_GAIN_MIN, DSPConstants::AUTO_GAIN_MAX, rawCompensation);
        }
        else
        {
            autoGainCompensation = 1.0f;  // No signal = no compensation
        }

        // Apply auto-gain compensation to oversampled block
        for (size_t ch = 0; ch < numChannels; ++ch)
        {
            float* data = pb_oversampledBlock.getChannelPointer(ch);
            for (size_t i = 0; i < numSamples; ++i)
                data[i] *= autoGainCompensation;
        }
    }
    else
    {
        // Reset state so re-enabling doesn't cause jumps
        autoGainCompensation = 1.0f;
    }

    // Store final parameter values for next block's interpolation
    lastInputGain = currentInputGain;
    lastDistortionDrive = currentDrive;
    lastDistMix = currentMixAmount;

    // Check for corruption after distortion processing, before downsampling
    bool hasNaN = false;
    bool hasInf = false;
    float maxSample = 0.0f;
    const size_t numSamplesCheck = pb_oversampledBlock.getNumSamples();
    const size_t numChannelsCheck = pb_oversampledBlock.getNumChannels();
    for (size_t ch = 0; ch < numChannelsCheck && !hasNaN && !hasInf; ++ch)
    {
        const float* channelData = pb_oversampledBlock.getChannelPointer(ch);
        for (size_t i = 0; i < numSamplesCheck; ++i)
        {
            const float sample = channelData[i];
            if (std::isnan(sample)) hasNaN = true;
            if (std::isinf(sample)) hasInf = true;
            maxSample = std::max(maxSample, std::abs(sample));
        }
    }

    if (hasNaN || hasInf || maxSample > 10.0f)
    {
        debugHadDistortionCorruption.store(true, std::memory_order_relaxed);
    }

    // REMOVED: DC blocking before downsampling causes instability at 44.1kHz
    // The DC blocking filter at 5Hz with 176.4kHz oversampled rate creates
    // extremely resonant poles that interact badly with the downsampler
    // DC blocking is applied AFTER downsampling via manual one-pole DC blocker

    // ========== TONE FILTER & WAVESHAPER (oversampled domain) ==========
    // Order controlled by Clean Mode toggle to optimize aliasing vs character
    const bool cleanMode = waveshaperCleanParam ? (waveshaperCleanParam->load() > 0.5f) : false;
    // Use modulated frequency for LFO-controlled tone sweeps
    const float toneFreq = modulatedToneFreq;
    const float waveshaperMix = *waveshaperMixParam;

    // Update tone filter coefficients if frequency changed (RT-safe in-place write)
    if (std::abs(toneFreq - lastToneFreq) > 1.0f && toneFilter.state != nullptr)
    {
        const double toneSampleRate = currentSampleRate * oversamplingFactor;
        // Clamp frequency to valid range (well below Nyquist)
        const float clampedToneFreq = juce::jlimit(2000.0f, std::min(20000.0f, (float)(toneSampleRate * 0.45)), toneFreq);
        // Butterworth Q matches JUCE's default inverseRootTwo used by makeLowPass(sr, freq).
        constexpr double butterworthQ = 0.7071067811865476;
        // In-place write into the Coefficients object. ProcessorDuplicator's per-channel
        // IIR::Filter holds its own CoefficientsPtr that was initialised from .state in
        // prepare() and aliases the same object — so writing through .state here updates
        // the coefficients read by every channel's Filter on the next process() call.
        // No standby-swap is used for the tone filter: the swap exchanges only .state's
        // pointer, which the per-channel Filter does not track. Audio thread is the sole
        // writer and reader, sequenced, so no torn reads are possible.
        writeSecondOrderLowPassCoeffs(*toneFilter.state, toneSampleRate, clampedToneFreq, butterworthQ);
        if (toneFilterLow.state != nullptr)
            writeSecondOrderLowPassCoeffs(*toneFilterLow.state, toneSampleRate, clampedToneFreq, butterworthQ);
        lastToneFreq = toneFreq;
    }

    // Lambda for tone filter processing
    auto applyToneFilter = [&]() {
        if (toneFreq < 19500.0f && toneFilter.state != nullptr)
        {
            toneFilter.process(juce::dsp::ProcessContextReplacing<float>(pb_oversampledBlock));
        }
    };

    // Lambda for waveshaper processing
    auto applyWaveshaper = [&]() {
        if (waveshaperMix > 0.0f)
    {
        const float wetAmountWS = waveshaperMix / 100.0f;  // 0.0 to 1.0
        const float dryAmountWS = 1.0f - wetAmountWS;

        const size_t wsNumChannels = pb_oversampledBlock.getNumChannels();
        const size_t wsNumSamples = pb_oversampledBlock.getNumSamples();

        for (size_t channel = 0; channel < wsNumChannels; ++channel)
        {
            auto* channelData = pb_oversampledBlock.getChannelPointer(channel);
            for (size_t sample = 0; sample < wsNumSamples; ++sample)
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
                    const float compressed = 0.4f + FastMath::tanh(excess * 1.2f) * 0.4f;
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
                wet = FastMath::tanh(wet * 0.85f);

                // Stage 8: Subtle asymmetry for analog character
                if (wet > 0.0f)
                    wet *= 0.98f;

                // Blend dry and wet signals
                channelData[sample] = dryAmountWS * dry + wetAmountWS * wet;
            }
        }
        }
    };

    // Apply processing in order based on Clean Mode toggle
    if (cleanMode)
    {
        // CLEAN MODE: Waveshaper → Tone Filter
        // Reduces aliasing by generating harmonics before filtering
        applyWaveshaper();
        applyToneFilter();
    }
    else
    {
        // GRITTY MODE: Tone Filter → Waveshaper (original order)
        // Creates edgier character by adding harmonics after darkening
        applyToneFilter();
        applyWaveshaper();
    }

    // ========== LA-2A FULL-BAND COMPRESSION (oversampled domain) ==========
    // Simple full-band compression without crossover or wet/dry blend
    if (compEnabled && compPeakReduction > 0.0f)
    {
        const size_t oversampledNumSamples = pb_oversampledBlock.getNumSamples();
        const int compNumChannels = static_cast<int>(pb_oversampledBlock.getNumChannels());
        const int compNumSamples = static_cast<int>(oversampledNumSamples);

        // Map peak reduction (0-100) to threshold in dB
        const float threshold = DSPConstants::COMP_THRESHOLD_MIN_DB +
                              (compPeakReduction * DSPConstants::COMP_THRESHOLD_RANGE_DB / 100.0f);

        // Ratio: Compress mode = 3:1, Limit mode = 12:1
        const float ratio = (compRatioMode == 0) ? DSPConstants::COMP_RATIO_COMPRESS : DSPConstants::COMP_RATIO_LIMIT;

        // Map makeup gain
        const float makeupGainDB = (compMakeupGain - 50.0f) * (DSPConstants::COMP_MAKEUP_RANGE_DB / 50.0f);
        const float makeupGainLinear = juce::Decibels::decibelsToGain(makeupGainDB);

        // Use OVERSAMPLED coefficients
        const float attackCoeff = compAttackCoeffOversampled;
        const float releaseCoeff = compReleaseCoeffOversampled;

        float maxGainReductionDB = 0.0f;

        for (int sampleIdx = 0; sampleIdx < compNumSamples; ++sampleIdx)
        {
            // Calculate RMS across channels for detection
            float sumSquares = 0.0f;
            for (int ch = 0; ch < compNumChannels; ++ch)
            {
                const float sampleValue = pb_oversampledBlock.getChannelPointer(static_cast<size_t>(ch))[sampleIdx];
                sumSquares += sampleValue * sampleValue;
            }

            const float rms = std::sqrt(sumSquares / compNumChannels);

            // Update RMS history with OVERSAMPLED coefficient
            compRmsHistory = compRmsHistoryCoeffOversampled * compRmsHistory +
                            (1.0f - compRmsHistoryCoeffOversampled) * rms;

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
                    const float kneeRatio = overThresholdDB / kneeWidth;
                    gainReductionDB = overThresholdDB * kneeRatio * (1.0f - 1.0f / ratio);
                }
                else
                {
                    gainReductionDB = kneeWidth * (1.0f - 1.0f / ratio) +
                        (overThresholdDB - kneeWidth) * (1.0f - 1.0f / ratio);
                }
            }

            // Optical cell envelope follower
            const float targetGainReduction = juce::Decibels::decibelsToGain(-gainReductionDB);

            if (targetGainReduction < compEnvelopeState)
            {
                compEnvelopeState = attackCoeff * compEnvelopeState + (1.0f - attackCoeff) * targetGainReduction;
            }
            else
            {
                compEnvelopeState = releaseCoeff * compEnvelopeState + (1.0f - releaseCoeff) * targetGainReduction;
            }

            maxGainReductionDB = juce::jmax(maxGainReductionDB, gainReductionDB);

            // Apply compression and makeup gain to all channels
            for (int ch = 0; ch < compNumChannels; ++ch)
            {
                auto* channelData = pb_oversampledBlock.getChannelPointer(static_cast<size_t>(ch));
                float sampleValue = channelData[sampleIdx];

                // Apply compression
                sampleValue *= compEnvelopeState;

                // Tube harmonic generation
                const float tubeInput = sampleValue * DSPConstants::COMP_TUBE_DRIVE;
                const float tubeSaturation = FastMath::tanh(tubeInput);

                sampleValue = sampleValue * (1.0f - DSPConstants::COMP_TUBE_BLEND) +
                             tubeSaturation * DSPConstants::COMP_TUBE_BLEND;

                sampleValue *= makeupGainLinear;

                // Conditional soft clip (transparent below -1dBFS, prevents overs above)
                {
                    const float absSample = std::abs(sampleValue);
                    if (absSample > DSPConstants::COMP_SOFT_CLIP_THRESHOLD)
                    {
                        const float sign = (sampleValue > 0.0f) ? 1.0f : -1.0f;
                        const float excess = absSample - DSPConstants::COMP_SOFT_CLIP_THRESHOLD;
                        sampleValue = sign * (DSPConstants::COMP_SOFT_CLIP_THRESHOLD
                                    + FastMath::tanh(excess * 4.0f) * DSPConstants::COMP_SOFT_CLIP_HEADROOM);
                    }
                }

                channelData[sampleIdx] = sampleValue;
            }
        }

        currentGainReductionDB.store(maxGainReductionDB, std::memory_order_relaxed);
    }

    // ========== SOFT CLIPPER (ISP Protection) ==========
    // Gentle ceiling at -0.3 dBFS to prevent inter-sample overs from oversampling collapse
    {
        const float thresholdLinear = juce::Decibels::decibelsToGain(DSPConstants::SOFT_CLIP_THRESHOLD_DB);
        const size_t scNumChannels = pb_oversampledBlock.getNumChannels();
        const size_t scNumSamples = pb_oversampledBlock.getNumSamples();

        for (size_t channel = 0; channel < scNumChannels; ++channel)
        {
            auto* data = pb_oversampledBlock.getChannelPointer(channel);
            for (size_t sample = 0; sample < scNumSamples; ++sample)
            {
                const float input = data[sample];
                const float absInput = std::abs(input);

                if (absInput > thresholdLinear)
                {
                    // Soft saturation above threshold
                    const float sign = (input > 0.0f) ? 1.0f : -1.0f;
                    const float excess = absInput - thresholdLinear;
                    const float compressed = thresholdLinear + FastMath::tanh(excess * 2.0f) * (1.0f - thresholdLinear);
                    data[sample] = sign * compressed;
                }
            }
        }
    }

    // ========== SUB GUARD: Phase-match low branch to the high branch's tone filter ==========
    // Apply an identical tone LP (with its own state) to the clean low band so both
    // bands share the same magnitude/phase response at the recombine sum. Gated on the
    // same bypass condition as the high-branch tone filter.
    if (subGuardActive && toneFreq < 19500.0f && toneFilterLow.state != nullptr)
    {
        auto lowBlockTone = juce::dsp::AudioBlock<float>(lowBandBuffer).getSubBlock(0, numSamples);
        toneFilterLow.process(juce::dsp::ProcessContextReplacing<float>(lowBlockTone));
    }

    // ========== SUB GUARD: Add clean low band back after all nonlinear processing ==========
    // The clean sub bypasses: auto-gain, waveshaper, compressor, and soft clipper
    // (tone filter is mirrored above for phase coherence at recombine)
    if (subGuardActive)
    {
        for (size_t channel = 0; channel < numChannels; ++channel)
        {
            auto* outputData = pb_oversampledBlock.getChannelPointer(channel);
            const auto* lowData = lowBandBuffer.getReadPointer(static_cast<int>(channel));

            for (size_t sample = 0; sample < numSamples; ++sample)
            {
                outputData[sample] += lowData[sample];
            }
        }
    }

    // Downsample back into original buffer (skip when oversampling is off)
    if (oversampling)
        oversampling->processSamplesDown(pb_inputBlock);

    // NOTE: Waveshaper and LA-2A compression are now in the oversampled domain (before downsampling)
    // This eliminates aliasing from harmonic generation

    // Manual DC blocker - simple one-pole filter that's extremely stable
    // y[n] = x[n] - x[n-1] + R * y[n-1], where R ≈ 0.9995 for ~3.5Hz cutoff at 44.1kHz
    // Previous value 0.995 (~35Hz) was stealing 2.5dB at 40Hz and 1.2dB at 60Hz
    constexpr float R = 0.9995f;  // ~3.5Hz cutoff: removes DC without touching sub
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
    // Per-sample LFO for destination 4 (output gain / tremolo) at base sample rate
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        float currentOutputGain = smoothedOutputGain.getNextValue();

        // Per-sample LFO modulation of output gain (destination 4)
        if (perSampleLFO && lfoEnabled && lfoPhaseIncrement > 0.0f && lfoDestination == 4)
        {
            float sampleLfoValue = generateLFOWaveform(lfoPhase, lfoWaveform);
            if (std::isnan(sampleLfoValue) || std::isinf(sampleLfoValue))
                sampleLfoValue = 0.0f;
            const float sampleLfoMod = sampleLfoValue * lfoDepth / 100.0f;

            // Modulate output gain: ±25% swing (±4.5dB tremolo)
            const float modOutGainParam = juce::jlimit(0.0f, 100.0f,
                outGainParam + sampleLfoMod * 25.0f);
            const float modOutGainDB = (modOutGainParam - 50.0f) * 0.18f;
            currentOutputGain = juce::Decibels::decibelsToGain(modOutGainDB);

            // Advance LFO phase at base sample rate
            lfoPhase += lfoPhaseIncrement;
            if (lfoPhase >= 1.0f)
                lfoPhase -= 1.0f;
        }

        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            auto* channelData = buffer.getWritePointer(channel);
            channelData[sample] *= currentOutputGain;
        }
    }

    // Update LFO phase for UI display (per-sample destinations update phase in loops above)
    if (perSampleLFO && lfoEnabled)
        lfoPhaseForUI.store(lfoPhase, std::memory_order_relaxed);

    // ========== OUTPUT LIMITER (Final Safety) ==========
    // Stereo-linked soft limiter at -0.5dBFS to prevent clipping
    // Always-on safety net for DAC protection
    {
        const float thresholdLinear = juce::Decibels::decibelsToGain(DSPConstants::OUTPUT_LIMITER_THRESHOLD_DB);
        const float threshDB = DSPConstants::OUTPUT_LIMITER_THRESHOLD_DB;
        const float kneeDB = DSPConstants::OUTPUT_LIMITER_KNEE_DB;

        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            // Stereo-linked peak detection (max of both channels)
            float peakLevel = 0.0f;
            for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            {
                const float absValue = std::abs(buffer.getSample(channel, sample));
                if (absValue > peakLevel)
                    peakLevel = absValue;
            }

            // Calculate target gain reduction with soft knee
            float targetGain = 1.0f;
            if (peakLevel > thresholdLinear)
            {
                const float peakDB = juce::Decibels::gainToDecibels(peakLevel + 1e-6f);
                const float overDB = peakDB - threshDB;

                // Soft knee limiting (1dB transition zone)
                float grDB = 0.0f;
                if (overDB < kneeDB)
                {
                    // Inside knee: quadratic curve for smooth onset
                    const float t = overDB / kneeDB;
                    grDB = overDB * t;
                }
                else
                {
                    // Above knee: brick-wall limiting
                    grDB = kneeDB + (overDB - kneeDB);
                }

                targetGain = juce::Decibels::decibelsToGain(-grDB);
            }

            // Envelope follower with asymmetric attack/release
            if (targetGain < outputLimiterEnvelope)
            {
                // Attack phase (reducing gain to catch transients)
                outputLimiterEnvelope = outputLimiterAttackCoeff * outputLimiterEnvelope
                                      + (1.0f - outputLimiterAttackCoeff) * targetGain;
            }
            else
            {
                // Release phase (returning to unity gain)
                outputLimiterEnvelope = outputLimiterReleaseCoeff * outputLimiterEnvelope
                                      + (1.0f - outputLimiterReleaseCoeff) * targetGain;
            }

            // Apply gain reduction to all channels (stereo-linked)
            for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            {
                auto* channelData = buffer.getWritePointer(channel);
                channelData[sample] *= outputLimiterEnvelope;
            }
        }
    }

    // ========== GLOBAL MIX (Active path) ==========
    // Delay the dry path by the oversampler's (fractional) latency so it aligns
    // with the wet path at the mix point. Without this, non-zero oversampling
    // latency causes comb filtering at any mix < 100% wet.
    if (needsGlobalMix)
    {
        const float delay = dryDelaySamples;
        const int numSamp = buffer.getNumSamples();
        const int numCh = buffer.getNumChannels();

        if (delay < 1.0e-4f)
        {
            // No oversampling latency — direct mix, no interpolation needed.
            for (int sample = 0; sample < numSamp; ++sample)
            {
                const float currentMix = smoothedGlobalMix.getNextValue();
                for (int channel = 0; channel < numCh; ++channel)
                {
                    auto* channelData = buffer.getWritePointer(channel);
                    const float drySample = dryBuffer.getSample(channel, sample);
                    channelData[sample] = drySample * (1.0f - currentMix) + channelData[sample] * currentMix;
                }
            }
        }
        else
        {
            const float intPart = std::floor(delay);
            const float frac = delay - intPart;
            const int intDelay = static_cast<int>(intPart);
            const int stateLen = dryDelayState.getNumSamples();

            // Linear interpolation between the integer-delay sample and the one
            // before it. Negative positions read from dryDelayState (the tail of
            // the previous block's dry signal).
            for (int sample = 0; sample < numSamp; ++sample)
            {
                const float currentMix = smoothedGlobalMix.getNextValue();
                const int readBase = sample - intDelay;

                for (int channel = 0; channel < numCh; ++channel)
                {
                    auto* channelData = buffer.getWritePointer(channel);

                    auto readDry = [&] (int pos) -> float
                    {
                        if (pos >= 0)
                            return dryBuffer.getSample(channel, pos);
                        const int statePos = stateLen + pos;
                        return (statePos >= 0) ? dryDelayState.getSample(channel, statePos) : 0.0f;
                    };

                    const float s0 = readDry(readBase);
                    const float sM1 = readDry(readBase - 1);
                    const float drySample = s0 * (1.0f - frac) + sM1 * frac;

                    channelData[sample] = drySample * (1.0f - currentMix) + channelData[sample] * currentMix;
                }
            }

            // Update dry-delay state with the last `stateLen` samples of the dry
            // signal so the next block can read across the boundary.
            const int srcStart = numSamp - stateLen;
            for (int channel = 0; channel < numCh; ++channel)
            {
                for (int i = 0; i < stateLen; ++i)
                {
                    const int src = srcStart + i;
                    const float v = (src >= 0)
                        ? dryBuffer.getSample(channel, src)
                        : dryDelayState.getSample(channel, stateLen + src);
                    dryDelayState.setSample(channel, i, v);
                }
            }
        }
    }
    else
    {
        // Consume smoothed values to keep state in sync
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            smoothedGlobalMix.getNextValue();
    }

    // Calculate phase correlation for UI meter
    {
        float sumLR = 0.0f, sumLL = 0.0f, sumRR = 0.0f;
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            float pcL = buffer.getSample(0, i);
            float pcR = buffer.getNumChannels() > 1 ? buffer.getSample(1, i) : pcL;
            sumLR += pcL * pcR;
            sumLL += pcL * pcL;
            sumRR += pcR * pcR;
        }
        float denom = std::sqrt(sumLL * sumRR);
        phaseCorrelation.store(
            denom > 1e-8f ? juce::jlimit(-1.0f, 1.0f, sumLR / denom) : 1.0f,
            std::memory_order_relaxed);
    }

    // Push samples to oscilloscope (after all processing)
    for (int sample = 0; sample < buffer.getNumSamples(); sample += DSPConstants::SCOPE_UPDATE_DECIMATION)
    {
        // Check if there's space in the FIFO before writing
        if (scopeFifo.getFreeSpace() > 0)
        {
            const int next = juce::jmin(sample + 1, buffer.getNumSamples() - 1);
            const float leftSample = (buffer.getSample(0, sample) + buffer.getSample(0, next)) * 0.5f;
            const float rightSample = buffer.getNumChannels() > 1 ?
                (buffer.getSample(1, sample) + buffer.getSample(1, next)) * 0.5f : leftSample;
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
    // Sliding-window scope: destBuffer is a persistent rolling buffer owned
    // by the Oscilloscope. Each frame we shift existing samples left and
    // append newly-arrived FIFO samples on the right. This guarantees the
    // buffer always holds a contiguous stream of the most recent audio at
    // any sample rate / block size / oversampling combo, with no seams or
    // padding artifacts on the right edge.

    const int bufferSize = destBuffer.getNumSamples();
    const int numChannels = destBuffer.getNumChannels();
    const int available = scopeFifo.getNumReady();

    if (available <= 0 || bufferSize <= 0)
        return;

    // Drain any samples beyond what we can display in one frame so the
    // FIFO doesn't lag behind real time.
    if (available > bufferSize)
    {
        const int excess = available - bufferSize;
        int s1, sz1, s2, sz2;
        scopeFifo.prepareToRead(excess, s1, sz1, s2, sz2);
        scopeFifo.finishedRead(sz1 + sz2);
    }

    const int toRead = juce::jmin(bufferSize, scopeFifo.getNumReady());
    if (toRead <= 0)
        return;

    // Shift existing samples left by `toRead` so new samples land flush
    // against the right edge.
    const int keep = bufferSize - toRead;
    if (keep > 0)
    {
        for (int ch = 0; ch < numChannels; ++ch)
        {
            float* data = destBuffer.getWritePointer(ch);
            std::memmove(data, data + toRead, (size_t) keep * sizeof(float));
        }
    }

    int start1, size1, start2, size2;
    scopeFifo.prepareToRead(toRead, start1, size1, start2, size2);

    const int writeOffset = keep;
    if (size1 > 0)
    {
        for (int ch = 0; ch < numChannels; ++ch)
            destBuffer.copyFrom(ch, writeOffset, scopeBuffer, ch, start1, size1);
    }
    if (size2 > 0)
    {
        for (int ch = 0; ch < numChannels; ++ch)
            destBuffer.copyFrom(ch, writeOffset + size1, scopeBuffer, ch, start2, size2);
    }

    scopeFifo.finishedRead(toRead);
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

        // BACKWARD COMPATIBILITY: Migrate old bandSplitEnabled (bool) to subGuardFreq (float)
        if (xmlState->hasAttribute("bandSplitEnabled"))
        {
            bool oldBandSplitEnabled = xmlState->getBoolAttribute("bandSplitEnabled", false);
            // Map old boolean to new frequency range with OFF position:
            // false (OFF) → 0.0 Hz (OFF - no band-split, full-range distortion)
            // true (ON) → 150.0 Hz (AGGRESSIVE - old 808-Safe behavior)
            float migratedFreq = oldBandSplitEnabled ? 150.0f : 0.0f;

            if (auto* param = parameters.getParameter("subGuardFreq"))
            {
                float normalized = param->convertTo0to1(migratedFreq);
                param->setValueNotifyingHost(normalized);
            }
        }

        // Signal audio thread to reset state (thread-safe handoff)
        stateNeedsReset.store(true, std::memory_order_release);
    }
}

// =============================================================================
// PR-8: processBlock stage helpers
// =============================================================================

void PluginProcessor::applyPreHighpass(juce::AudioBuffer<float>& buffer)
{
    pb_inputBlock = juce::dsp::AudioBlock<float>(buffer);

    // Update filter coefficients if frequency changed (at BASE sample rate).
    // In-place write through .state — see docs/Architecture Contract.md.
    const double baseSampleRate = getSampleRate();
    if (std::abs(pb_modulatedHighPassFreq - lastHighPassFreq) > 0.5f)
    {
        if (baseSampleRate >= 1000.0 && baseSampleRate <= 500000.0 &&
            pb_modulatedHighPassFreq >= 1.0f &&
            pb_modulatedHighPassFreq <= (baseSampleRate / 2.0f) &&
            preHighPassFilter.state != nullptr)
        {
            writeFirstOrderHighPassCoeffs(*preHighPassFilter.state, baseSampleRate, pb_modulatedHighPassFreq);
            lastHighPassFreq = pb_modulatedHighPassFreq;
        }
    }

    if (preHighPassFilter.state)
        preHighPassFilter.process(juce::dsp::ProcessContextReplacing<float>(pb_inputBlock));

    pb_oversampledBlock = oversampling
        ? oversampling->processSamplesUp(pb_inputBlock)
        : pb_inputBlock;
}

void PluginProcessor::applyPreCompression()
{
    // Light compression to even out dynamics before distortion
    if (pb_modulatedDistortionParam < 0.5f || pb_extremeEnabled)
        return;

    const float thresholdLinear = juce::Decibels::decibelsToGain(DSPConstants::PRE_COMP_THRESHOLD_DB);
    const float ratio = DSPConstants::PRE_COMP_RATIO;
    const float kneeDB = DSPConstants::PRE_COMP_KNEE_DB;
    const float threshDB = DSPConstants::PRE_COMP_THRESHOLD_DB;

    for (size_t sample = 0; sample < pb_numSamples; ++sample)
    {
        for (size_t channel = 0; channel < pb_numChannels && channel < 2; ++channel)
        {
            float* data = pb_oversampledBlock.getChannelPointer(channel);
            const float input = data[sample];
            const float level = std::abs(input);

            float targetGain = 1.0f;
            if (level > thresholdLinear)
            {
                const float inputDB = juce::Decibels::gainToDecibels(level + 1e-6f);
                const float overDB = inputDB - threshDB;

                float grDB = 0.0f;
                if (overDB < kneeDB)
                    grDB = overDB * (overDB / kneeDB) * (1.0f - 1.0f / ratio);
                else
                    grDB = kneeDB * (1.0f - 1.0f / ratio) + (overDB - kneeDB) * (1.0f - 1.0f / ratio);

                targetGain = juce::Decibels::decibelsToGain(-grDB);
            }

            const int ch = static_cast<int>(channel);
            if (targetGain < preCompEnvelope[ch])
                preCompEnvelope[ch] = preCompAttackCoeff * preCompEnvelope[ch] + (1.0f - preCompAttackCoeff) * targetGain;
            else
                preCompEnvelope[ch] = preCompReleaseCoeff * preCompEnvelope[ch] + (1.0f - preCompReleaseCoeff) * targetGain;

            data[sample] = input * preCompEnvelope[ch];
        }
    }
}

// Helper method called from audio thread to safely reset DSP state
void PluginProcessor::resetDSPState()
{
    // Reset all filters when loading state to prevent stale coefficients/state
    if (preHighPassFilter.state)
        preHighPassFilter.reset();
    if (lowPassFilter1.state)
        lowPassFilter1.reset();
    if (lowPassFilter2.state)
        lowPassFilter2.reset();
    if (highPassFilter1.state)
        highPassFilter1.reset();
    if (highPassFilter2.state)
        highPassFilter2.reset();
    if (toneFilter.state)
        toneFilter.reset();
    if (toneFilterLow.state)
        toneFilterLow.reset();

    // Reset envelope states
    preCompEnvelope[0] = 1.0f;
    preCompEnvelope[1] = 1.0f;
    harmonicDensityEnvelope[0] = 0.0f;
    harmonicDensityEnvelope[1] = 0.0f;
    tubeBiasEnvelope[0] = 0.0f;
    tubeBiasEnvelope[1] = 0.0f;
    tapeSaturationEnvelope[0] = 0.0f;
    tapeSaturationEnvelope[1] = 0.0f;
    compEnvelopeState = 1.0f;
    compRmsHistory = 0.0f;
    tubeWarmth = 0.0f;

    // Reset auto-gain compensation state
    autoGainInputEnvelope = 0.0f;
    autoGainOutputEnvelope = 0.0f;
    autoGainCompensation = 1.0f;

    // Reset output limiter state
    outputLimiterEnvelope = 1.0f;  // 1.0 = no gain reduction

    // Reset manual DC blocker
    for (int ch = 0; ch < 2; ++ch)
    {
        manualDCBlockerPrevInput[ch] = 0.0f;
        manualDCBlockerPrevOutput[ch] = 0.0f;
    }

    // Clear fractional dry-delay history to avoid replaying stale samples
    if (dryDelayState.getNumSamples() > 0)
        dryDelayState.clear();

    // Force filter coefficient update on next processBlock by invalidating cache
    lastHighPassFreq = -1.0f;
    lastToneFreq = -1.0f;
    lastSubGuardFreq = -1.0f;
}

//==============================================================================
// Sub Guard Helper Methods
//==============================================================================

PluginProcessor::SubGuardFilterOrder PluginProcessor::determineSubGuardFilterOrder(float freq) const
{
    // Determine filter order based on frequency zones with hysteresis
    // PRESERVE zone (60 Hz): LR24 - steepest slope for maximum sub protection
    if (freq <= 80.0f)
        return SubGuardFilterOrder::LR24;

    // CONTROL zone (100 Hz): LR18 - balanced response
    if (freq >= 92.0f && freq <= 125.0f)
        return SubGuardFilterOrder::LR18;

    // AGGRESSIVE zone (150 Hz): LR12 - gentle slope, more frequency overlap
    if (freq >= 142.0f)
        return SubGuardFilterOrder::LR12;

    // Transition zones - use nearest neighbor
    if (freq < 92.0f)
        return SubGuardFilterOrder::LR24;  // Between PRESERVE and CONTROL
    else
        return SubGuardFilterOrder::LR18;  // Between CONTROL and AGGRESSIVE
}

void PluginProcessor::updateSubGuardCoefficients(float freq, double sampleRate)
{
    // RT-safe in-place coefficient update. Every per-channel IIR::Filter was
    // bound to the Coefficients object pointed to by .state at prepare() time,
    // so writing through .state here updates what each channel's process()
    // reads on the next block. No allocation; no standby swap. See
    // docs/Architecture Contract.md "RT-safety" for the rationale.
    constexpr double butterworthQ = 0.7071067811865476;

    // LR24 filters (2 cascaded 2nd-order stages)
    if (lowPassFilter1.state != nullptr)
        writeSecondOrderLowPassCoeffs(*lowPassFilter1.state, sampleRate, freq, butterworthQ);
    if (lowPassFilter2.state != nullptr)
        writeSecondOrderLowPassCoeffs(*lowPassFilter2.state, sampleRate, freq, butterworthQ);
    if (highPassFilter1.state != nullptr)
        writeSecondOrderHighPassCoeffs(*highPassFilter1.state, sampleRate, freq, butterworthQ);
    if (highPassFilter2.state != nullptr)
        writeSecondOrderHighPassCoeffs(*highPassFilter2.state, sampleRate, freq, butterworthQ);

    // LR12 filters (single 2nd-order stage with Q=0.5 for true Linkwitz-Riley 2)
    // Default Butterworth Q=0.707 causes +3dB boost at crossover; LR2 Q=0.5 sums flat
    constexpr float lr2Q = 0.5f;
    if (subGuardLP12.state != nullptr)
        writeSecondOrderLowPassCoeffs(*subGuardLP12.state, sampleRate, freq, lr2Q);
    if (subGuardHP12.state != nullptr)
        writeSecondOrderHighPassCoeffs(*subGuardHP12.state, sampleRate, freq, lr2Q);

    // LR18 filters (1st + 2nd order cascaded, Q=0.5 on 2nd-order stage for flat sum)
    if (subGuardLP18_1.state != nullptr)
        writeFirstOrderLowPassCoeffs(*subGuardLP18_1.state, sampleRate, freq);
    if (subGuardLP18_2.state != nullptr)
        writeSecondOrderLowPassCoeffs(*subGuardLP18_2.state, sampleRate, freq, lr2Q);
    if (subGuardHP18_1.state != nullptr)
        writeFirstOrderHighPassCoeffs(*subGuardHP18_1.state, sampleRate, freq);
    if (subGuardHP18_2.state != nullptr)
        writeSecondOrderHighPassCoeffs(*subGuardHP18_2.state, sampleRate, freq, lr2Q);
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

    // Sub Guard continuous crossover frequency (replaces 808-Safe toggle)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "subGuardFreq", 1 },
        "Sub Guard Frequency",
        juce::NormalisableRange<float>(
            0.0f,                                // 0 Hz = OFF (no band-split)
            DSPConstants::SUBGUARD_FREQ_MAX,
            0.1f,  // 0.1 Hz step
            0.5f   // Logarithmic skew for better low-end control
        ),
        DSPConstants::SUBGUARD_FREQ_DEFAULT));  // Default 60 Hz (LR24 sub-preserving)

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ "clipType", 1 },
        "Clip Type",
        juce::StringArray{
            "Brutal Fuzz",            // 0: Aggressive hard clipping with analog noise
            "Tube Overdrive",         // 1: Asymmetric tube saturation with harmonics
            "Bit Crusher",            // 2: Digital destruction with sample rate reduction
            "Tape Saturation",        // 3: Analog tape with hysteresis
            "Transformer Saturation", // 4: Heavy harmonic distortion
            "Diode Clipper",          // 5: Asymmetric diode clipping with grit
            "Decimator"               // 6: Extreme digital destruction
        },
        0));

    // LFO parameters
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "lfoRate", 1 },
        "LFO Rate",
        // Range starts at 0.0 so the default "off" (0.0) is inside the layout. The LFO is
        // gated by lfoEnabled + lfoRate > 0.0f, so any rate > 0 engages it.
        juce::NormalisableRange<float>(0.0f, 10.0f, 0.01f, 0.35f),
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

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{ "lfoEnabled", 1 },
        "LFO Enable",
        false));  // Default OFF

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ "lfoDestination", 1 },
        "LFO Destination",
        juce::StringArray{
            "Distortion",   // 0 (default for backward compatibility)
            "Tone Filter",  // 1
            "Hi-Pass",      // 2
            "Dist Mix",     // 3
            "Output Gain"   // 4
        },
        0));  // Default to Distortion

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
        juce::ParameterID{ "distMix", 1 },
        "Distortion Mix",
        juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f),
        100.0f));  // Default 100 = 100% wet

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "tone", 1 },
        "Tone",
        juce::NormalisableRange<float>(2000.0f, 20000.0f, 1.0f, 0.5f),  // Skew for better low-end control
        20000.0f));  // Default 20kHz = bright/bypass

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{ "waveshaperClean", 1 },
        "Clean Mode",
        false));  // Default false = Gritty mode (tone→waveshaper, current behavior)

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{ "autoGainEnabled", 1 },
        "Auto Gain",
        true));  // Default ON for backward compatibility

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{ "extremeEnabled", 1 },
        "Extreme",
        false));  // Default OFF

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "globalMix", 1 },
        "Global Mix",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f),
        100.0f));  // Default 100% wet

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{ "linearPhaseDry", 1 },
        "Linear Phase Dry",
        false));  // Default OFF — preserves existing IIR character and sessions

    return { params.begin(), params.end() };
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}
