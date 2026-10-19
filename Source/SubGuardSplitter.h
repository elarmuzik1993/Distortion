#pragma once

#include <JuceHeader.h>

// Sub Guard crossover configuration (replaces 808-Safe mode). Extends the processor's
// DSPConstants namespace here so this header stands alone (see ShapeFilter.h).
namespace DSPConstants
{
    constexpr float SUBGUARD_FREQ_MAX = 200.0f;               // Maximum crossover frequency
    constexpr float SUBGUARD_FREQ_DEFAULT = 60.0f;            // Default to 60Hz LR24 (sub-preserving, Saturn 2 style)
    constexpr float SUBGUARD_CROSSFADE_TIME_S = 0.010f;       // 10ms order transition crossfade
    constexpr float SUBGUARD_FREQ_SMOOTH_TIME_S = 0.050f;     // 50ms frequency smoothing
}

/**
    The Sub Guard crossover: splits a block (oversampled domain) into a clean low band
    and a high band for the drive. The slope depends on the frequency zone (LR24 up to
    ~92 Hz, LR18 up to ~142 Hz, LR12 above), and a slope change crossfades the two
    banks' splits over SUBGUARD_CROSSFADE_TIME_S. The frequency is smoothed once per
    block (SUBGUARD_FREQ_SMOOTH_TIME_S).

    The splitter only splits. The processor drives the high band, recombines, and owns
    what follows: the low band's island delay, its tone-filter mirror and the add-back.

    Thread use: prepare / startTracking from prepareToPlay, and prepare again from an
    oversampling rebuild (under the callback lock); everything else on the audio thread. Nothing allocates after prepare.
*/
class SubGuardSplitter
{
public:
    enum class Order { LR12, LR18, LR24 };

    SubGuardSplitter();

    // Builds all three banks for the oversampled spec, with coefficients for freqParam
    // (60 Hz while Sub Guard is off), and sizes the band buffers to spec.numChannels x
    // bandBufferSamples. Filter state is cleared; the tracking state is not.
    void prepare(const juce::dsp::ProcessSpec& spec, float freqParam, int bandBufferSamples);

    // Seeds the frequency smoother (advanced once per block, so it runs at blockRate)
    // and the slope order from freqParam, so the first active block neither glides
    // nor crossfades.
    void startTracking(float freqParam, double blockRate);

    // Clears every bank, aborts any slope crossfade (the next active block snaps to
    // its order) and forces the next block to rewrite the coefficients.
    void reset() noexcept;

    // A block with Sub Guard off: the next active block snaps to its order.
    void setInactive() noexcept;

    void setTargetFrequency(float hz) noexcept;

    // Splits the first numChannels x numSamples of input into the low and high band
    // buffers. Returns false, touching no band buffer, when they are too small; the
    // order, crossfade and coefficient state still advance for that block.
    bool process(const juce::dsp::AudioBlock<float>& input,
                 size_t numChannels, size_t numSamples, double sampleRate) noexcept;

    // True when the last process() call came after an off block (or a reset), i.e.
    // Sub Guard has just switched on.
    bool justActivated() const noexcept { return activatedThisBlock; }

    juce::AudioBuffer<float>& getLowBand() noexcept  { return lowBand; }
    juce::AudioBuffer<float>& getHighBand() noexcept { return highBand; }

    static Order orderForFrequency(float freq) noexcept;

private:
    friend class CoefficientPropagationTest;

    using Filter = juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                                  juce::dsp::IIR::Coefficients<float>>;

    void updateCoefficients(float freq, double sampleRate) noexcept;
    // Filter lowBuf/highBuf in place through the given order's bank and bake the
    // LR12 flat-sum polarity (inverts the high band for 2nd-order Linkwitz-Riley).
    void filterBands(Order order, juce::AudioBuffer<float>& lowBuf, juce::AudioBuffer<float>& highBuf,
                     size_t numChannels, size_t numSamples) noexcept;
    // Clear just one order's filter state (incoming bank at a crossfade start).
    void resetOrderFilters(Order order) noexcept;

    // LR24 (4th order = 2 cascaded 2nd-order stages).
    // Coefficient updates mutate .state in place (see updateCoefficients).
    Filter lr24Low1, lr24Low2, lr24High1, lr24High2;
    // LR12 (2nd order = single stage)
    Filter lr12Low, lr12High;
    // LR18 (1st + 2nd order cascaded = 3rd-order Butterworth)
    Filter lr18Low1, lr18Low2, lr18High1, lr18High2;   // *1 first order, *2 second order

    Order currentOrder = Order::LR24;
    juce::SmoothedValue<float> smoothedFreq;
    float lastFreq = -1.0f;

    // Order-change crossfade. When the slope order switches (zone boundary), both the
    // outgoing and incoming banks run for SUBGUARD_CROSSFADE_TIME_S and their band-split
    // outputs are linearly blended, so the switch is click-free.
    bool  crossfadeActive = false;
    int   crossfadePos    = 0;           // samples elapsed in the current crossfade
    Order fromOrder = Order::LR24;
    Order toOrder   = Order::LR24;
    bool  wasActive          = false;    // Sub Guard active last block? (OFF->ON snaps, no crossfade)
    bool  activatedThisBlock = false;

    juce::AudioBuffer<float> lowBand;    // clean low frequencies
    juce::AudioBuffer<float> highBand;   // the band the processor drives
    // Secondary band buffers: the incoming order's split during an order crossfade.
    juce::AudioBuffer<float> lowBandB;
    juce::AudioBuffer<float> highBandB;
};
