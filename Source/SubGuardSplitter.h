#pragma once

#include <JuceHeader.h>
#include <array>
#include <vector>

// Sub Guard crossover configuration (replaces 808-Safe mode). Extends the processor's
// DSPConstants namespace here so this header stands alone (see ShapeFilter.h).
namespace DSPConstants
{
    constexpr float SUBGUARD_FREQ_MAX = 200.0f;               // Maximum crossover frequency
    constexpr float SUBGUARD_FREQ_DEFAULT = 60.0f;            // Default to 60Hz (sub-preserving, Saturn 2 style)
    constexpr float SUBGUARD_FREQ_SMOOTH_TIME_S = 0.050f;     // 50ms frequency smoothing
    constexpr float SUBGUARD_SWITCH_TIME_S = 0.015f;          // 15ms on/off weight ramp
}

/**
    The Sub Guard crossover: one 8th-order Linkwitz-Riley (LR48) split around the drive,
    in the oversampled domain, at every frequency.

    An LR48 is a 4th-order Butterworth applied twice. The low band gets both low-pass
    halves at once (process). The high band gets one high-pass half before the drive
    (process) and the other after the high band's nonlinear stages (processPostDrive).
    With nothing nonlinear between the halves, low + high is exactly an LR48 sum, an
    allpass (flat magnitude, same polarity); with the drive between them, the second half
    also cuts what the drive created below the crossover. The frequency is smoothed once
    per block (SUBGUARD_FREQ_SMOOTH_TIME_S).

    On/off is a weight g per sample (0 off, 1 on, ramped over SUBGUARD_SWITCH_TIME_S),
    which the processor applies wherever the split acts. The splitter runs on every active
    block, Sub Guard on or off, so switching on never starts a cold filter: while off it
    holds the last frequency it was on at (60 Hz if never), and g alone decides whether
    the split is heard.

    Every section runs in double, state and coefficients, from one coefficient writer
    (writeSection) used by prepare and at runtime alike: at 4x oversampling a 60 Hz cutoff
    is fc/fs below 1e-3, where float sections drift (section 3 of
    docs/SUB_GUARD_TRANSITION_REVIEW.md).

    The splitter only splits. The processor drives the high band, recombines, and owns
    what follows: the low band's island delay, its tone-filter mirror and the add-back.

    Thread use: prepare / startTracking from prepareToPlay, and prepare again from an
    oversampling rebuild (under the callback lock); everything else on the audio thread.
    Nothing allocates after prepare.
*/
class SubGuardSplitter
{
public:
    SubGuardSplitter();

    // Stores the oversampled rate, writes the coefficients for freqParam (60 Hz while
    // Sub Guard is off), clears every section's state and sizes the band buffers to
    // spec.numChannels x bandBufferSamples. The tracking state is not touched.
    void prepare(const juce::dsp::ProcessSpec& spec, float freqParam, int bandBufferSamples);

    // Seeds the frequency smoother (advanced once per block, so it runs at blockRate)
    // and the on/off weight from freqParam, so the first active block neither glides
    // nor ramps.
    void startTracking(float freqParam, double blockRate);

    // Clears every section and forces the next block to rewrite the coefficients.
    void reset() noexcept;

    // Sets g straight to its target (a state load is a discontinuity anyway).
    void snapWeight() noexcept;

    // The Sub Guard parameter: above 1 Hz, on at that frequency; otherwise off, holding
    // the last on frequency.
    void setTargetFrequency(float freqParam) noexcept;

    // Splits the first numChannels x numSamples of input into the low band (both
    // low-pass halves) and the high band (the first high-pass half), and writes this
    // block's weights. Returns false, touching no band buffer, when they are too small;
    // the frequency, coefficient and weight state still advance for that block.
    bool process(const juce::dsp::AudioBlock<float>& input,
                 size_t numChannels, size_t numSamples) noexcept;

    // The second high-pass half, in place, on the high band after its nonlinear stages
    // (the drive output, or a profile island's output). Uses the coefficients of the
    // last process() call.
    void processPostDrive(juce::dsp::AudioBlock<float> highBand) noexcept;

    // The last process() call's on/off weight per sample, and whether any of it was above
    // 0 (it ramps monotonically, so checking the two ends is enough).
    const float* getWeights() const noexcept { return weights.data(); }
    bool isEngaged() const noexcept { return engaged; }

    juce::AudioBuffer<float>& getLowBand() noexcept  { return lowBand; }
    juce::AudioBuffer<float>& getHighBand() noexcept { return highBand; }

    // The crossover frequency the coefficients were last written for.
    float getCoefficientFrequency() const noexcept { return lastFreq; }

private:
    friend class CoefficientPropagationTest;

    // One biquad section, transposed direct form II, a0-normalised.
    struct Coeffs { double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0; };
    struct State  { double s1 = 0.0, s2 = 0.0; };

    // A 4th-order Butterworth is two sections with these Qs (1 / (2 cos(k pi / 8)),
    // k = 1, 3); an LR48 is that pair twice.
    static constexpr int kSectionsPerHalf = 2;
    using Half = std::array<State, kSectionsPerHalf>;

    static void writeSection(Coeffs& dest, double sampleRate, double cutoffHz, double q, bool highPass) noexcept;
    void updateCoefficients(float freq) noexcept;
    static void runHalf(const std::array<Coeffs, kSectionsPerHalf>& c, Half& state,
                        float* data, size_t numSamples) noexcept;

    double sampleRate = 44100.0;
    std::array<Coeffs, kSectionsPerHalf> lowCoeffs, highCoeffs;

    // Per channel: the low band's two halves, and the high band's pre- and post-drive halves.
    std::vector<Half> lowStateA, lowStateB, highPreState, highPostState;

    juce::SmoothedValue<float> smoothedFreq;
    float lastFreq = -1.0f;
    float heldFreq = DSPConstants::SUBGUARD_FREQ_DEFAULT;   // the last on frequency

    juce::SmoothedValue<float> weight;   // g, per sample at the oversampled rate
    std::vector<float> weights;
    bool engaged = false;

    juce::AudioBuffer<float> lowBand;    // clean low frequencies
    juce::AudioBuffer<float> highBand;   // the band the processor drives
};
