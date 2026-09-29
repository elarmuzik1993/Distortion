#pragma once

#include <cstdint>
#include <vector>

// Runs a block-processing callback at a fixed model rate inside a stream at the
// host rate: host rate -> model rate -> callback -> host rate. A NAM model only
// sounds as it was trained at its own rate, so this is how a 48 kHz model runs
// correctly in a 44.1, 88.2, 96 or 192 kHz session.
//
// Each direction resamples with a Lanczos-windowed sinc (a = 12, the kernel NAM
// ships), its cutoff at the lower of the two Nyquist frequencies, so a stage that
// downsamples filters instead of folding. Positions are exact integer fractions,
// so the round trip delays the signal by a whole number of host samples that never
// changes for a pair of rates.
//
// Threading: prepare() allocates and runs off the audio thread. process() is the
// audio-thread entry point; it never allocates, throws or logs.
class ResamplingIsland
{
public:
    // Kernel half-width in zero crossings.
    static constexpr int lanczosA = 12;
    // Most fractional positions (one tap set each) a stage may need. Every standard
    // rate from 8 to 384 kHz needs at most 640 against a 48 kHz model.
    static constexpr int maxPhases = 1024;

    // True when the island can convert between these rates.
    static bool supports(double hostRate, double modelRate);
    // The round-trip delay in host samples, without building an island: 0 when the
    // rates match, -1 when they are unsupported.
    static int latencyFor(double hostRate, double modelRate);

    // Allocates. Returns false, leaving the island unusable, for unsupported rates.
    bool prepare(double hostRate, double modelRate, int maxHostBlock);
    // Clears the signal history and keeps the configuration. No allocation.
    void reset() noexcept;

    int getLatencySamples() const noexcept { return latency; }
    int getMaxModelBlock() const noexcept { return maxModelBlock; }
    bool isPassThrough() const noexcept { return passThrough; }
    bool isPrepared() const noexcept { return prepared; }

    // Audio thread. Writes exactly numSamples to out; in and out may not alias.
    // modelFn(const float* in, float* out, int n) runs the model at the model rate,
    // never on more than getMaxModelBlock() samples at once.
    template <typename ModelFn>
    void process(const float* in, float* out, int numSamples, ModelFn&& modelFn) noexcept
    {
        if (! prepared)
        {
            for (int i = 0; i < numSamples; ++i)
                out[i] = 0.0f;
            return;
        }
        for (int done = 0; done < numSamples;)
        {
            const int count = numSamples - done < maxHostBlock ? numSamples - done : maxHostBlock;
            if (passThrough)
            {
                modelFn(in + done, out + done, count);
            }
            else
            {
                const int modelCount = pushHost(in + done, count);
                if (modelCount > 0)
                    modelFn(modelIn.data(), modelOut.data(), modelCount);
                pullHost(out + done, count, modelCount);
            }
            done += count;
        }
    }

private:
    // One direction of the conversion: output j sits at input position j * in / out.
    struct Stage
    {
        int halfWidth = 0;         // taps either side of the output position
        int numPhases = 1;         // fractional positions, one tap set each
        std::vector<float> taps;   // numPhases rows of 2 * halfWidth
        void build(int64_t inUnits, int64_t outUnits);
        const float* phase(int p) const noexcept
        {
            return taps.data() + static_cast<size_t>(p) * static_cast<size_t>(2 * halfWidth);
        }
    };

    int pushHost(const float* in, int numSamples) noexcept;   // returns model samples ready in modelIn
    void pullHost(float* out, int numSamples, int modelCount) noexcept;

    bool prepared = false;
    bool passThrough = false;
    int latency = 0;
    int maxHostBlock = 1;
    int maxModelBlock = 1;
    int64_t hostUnits = 1, modelUnits = 1;   // the two rates divided by their GCD
    Stage toModel, toHost;

    // Histories are written twice, at i and i + size, so every tap window is contiguous.
    std::vector<float> hostHistory, modelHistory;
    int64_t hostMask = 0, modelMask = 0;
    int64_t hostWritten = 0, modelWritten = 0, outWritten = 0;
    std::vector<float> modelIn, modelOut;
};
