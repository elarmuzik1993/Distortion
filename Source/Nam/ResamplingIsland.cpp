#include "ResamplingIsland.h"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace
{
    constexpr double pi = 3.14159265358979323846;

    // sinc(x) * sinc(x / a) inside |x| < a.
    double lanczos(double x)
    {
        constexpr double a = ResamplingIsland::lanczosA;
        if (std::abs(x) >= a)
            return 0.0;
        if (std::abs(x) < 1.0e-12)
            return 1.0;
        const double px = pi * x;
        return a * std::sin(px) * std::sin(px / a) / (px * px);
    }

    // Both rates in whole hertz, divided by their GCD.
    bool reduceRates(double hostRate, double modelRate, int64_t& hostUnits, int64_t& modelUnits)
    {
        const int64_t host = std::llround(hostRate);
        const int64_t model = std::llround(modelRate);
        if (host <= 0 || model <= 0)
            return false;
        const int64_t divisor = std::gcd(host, model);
        hostUnits = host / divisor;
        modelUnits = model / divisor;
        return true;
    }

    // A stage that downsamples stretches its kernel by the rate ratio, so the cutoff
    // lands on the output's Nyquist frequency instead of the input's.
    int halfWidthFor(int64_t inUnits, int64_t outUnits)
    {
        if (inUnits <= outUnits)
            return ResamplingIsland::lanczosA;
        return static_cast<int>((ResamplingIsland::lanczosA * inUnits + outUnits - 1) / outUnits);
    }

    int64_t nextPowerOfTwo(int64_t value)
    {
        int64_t power = 1;
        while (power < value)
            power <<= 1;
        return power;
    }
}

bool ResamplingIsland::supports(double hostRate, double modelRate)
{
    int64_t host = 0, model = 0;
    return reduceRates(hostRate, modelRate, host, model) && host <= maxPhases && model <= maxPhases;
}

int ResamplingIsland::latencyFor(double hostRate, double modelRate)
{
    int64_t host = 0, model = 0;
    if (! reduceRates(hostRate, modelRate, host, model) || host > maxPhases || model > maxPhases)
        return -1;
    if (host == model)
        return 0;

    // Output n sits at model time (n - D) * M / H. Its window reaches toHost's
    // half-width past that, ceil(L2 * H / M) host samples; each model sample in it
    // reaches toModel's half-width L1 past its own position. With D covering both,
    // everything an output needs has arrived by the time it is written.
    const int toModelHalf = halfWidthFor(host, model);
    const int toHostHalf = halfWidthFor(model, host);
    return static_cast<int>((toHostHalf * host + model - 1) / model) + toModelHalf;
}

void ResamplingIsland::Stage::build(int64_t inUnits, int64_t outUnits)
{
    const double scale = inUnits > outUnits ? static_cast<double>(outUnits) / static_cast<double>(inUnits) : 1.0;
    halfWidth = halfWidthFor(inUnits, outUnits);
    numPhases = static_cast<int>(outUnits);
    const auto width = static_cast<size_t>(2 * halfWidth);
    taps.assign(static_cast<size_t>(numPhases) * width, 0.0f);

    std::vector<double> weights(width);
    for (int p = 0; p < numPhases; ++p)
    {
        // Phase p: the output sits p / outUnits of an input sample past input q, and
        // tap i reads input q - halfWidth + 1 + i.
        const double fraction = static_cast<double>(p) / static_cast<double>(outUnits);
        double sum = 0.0;
        for (size_t i = 0; i < width; ++i)
        {
            const double offset = static_cast<double>(halfWidth - 1) - static_cast<double>(i) + fraction;
            weights[i] = scale * lanczos(scale * offset);
            sum += weights[i];
        }
        // Every phase sums to exactly one, so low frequencies pass at unity whatever
        // the fractional position.
        for (size_t i = 0; i < width; ++i)
            taps[static_cast<size_t>(p) * width + i] = static_cast<float>(weights[i] / sum);
    }
}

bool ResamplingIsland::prepare(double hostRate, double modelRate, int maxHostBlockSize)
{
    prepared = false;
    if (maxHostBlockSize < 1 || ! supports(hostRate, modelRate))
        return false;

    reduceRates(hostRate, modelRate, hostUnits, modelUnits);
    maxHostBlock = maxHostBlockSize;
    passThrough = hostUnits == modelUnits;
    latency = latencyFor(hostRate, modelRate);

    if (passThrough)
    {
        maxModelBlock = maxHostBlock;
        hostHistory.clear();
        modelHistory.clear();
        modelIn.clear();
        modelOut.clear();
        prepared = true;
        return true;
    }

    toModel.build(hostUnits, modelUnits);
    toHost.build(modelUnits, hostUnits);
    maxModelBlock = static_cast<int>((maxHostBlock * modelUnits + hostUnits - 1) / hostUnits) + 1;

    // Room for one block plus a window either side, and a little slack.
    const int64_t hostSize = nextPowerOfTwo(maxHostBlock + 2 * toModel.halfWidth + 2);
    const int64_t modelSize = nextPowerOfTwo(maxModelBlock + 2 * toHost.halfWidth
                                             + (modelUnits + hostUnits - 1) / hostUnits + 8);
    hostMask = hostSize - 1;
    modelMask = modelSize - 1;
    hostHistory.assign(static_cast<size_t>(2 * hostSize), 0.0f);
    modelHistory.assign(static_cast<size_t>(2 * modelSize), 0.0f);
    modelIn.assign(static_cast<size_t>(maxModelBlock), 0.0f);
    modelOut.assign(static_cast<size_t>(maxModelBlock), 0.0f);

    prepared = true;
    reset();
    return true;
}

void ResamplingIsland::reset() noexcept
{
    if (! prepared || passThrough)
        return;

    std::fill(hostHistory.begin(), hostHistory.end(), 0.0f);
    std::fill(modelHistory.begin(), modelHistory.end(), 0.0f);

    // Start as if silence had been flowing for a while, so every window the first
    // real samples touch reads zeros at non-negative positions.
    const int64_t toModelHalf = toModel.halfWidth;
    const int64_t prime = latency + 2 * toModelHalf
                        + (toHost.halfWidth * hostUnits + modelUnits - 1) / modelUnits + 2;
    hostWritten = prime;
    outWritten = prime;
    modelWritten = ((prime - toModelHalf) * modelUnits + hostUnits - 1) / hostUnits;
}

int ResamplingIsland::pushHost(const float* in, int numSamples) noexcept
{
    const auto size = static_cast<size_t>(hostMask + 1);
    for (int i = 0; i < numSamples; ++i)
    {
        const auto slot = static_cast<size_t>((hostWritten + i) & hostMask);
        hostHistory[slot] = in[i];
        hostHistory[slot + size] = in[i];
    }
    hostWritten += numSamples;

    // Model sample k sits at host position k * H / M and is ready once its window,
    // halfWidth host samples past that position, has arrived.
    const int half = toModel.halfWidth;
    const int width = 2 * half;
    const int64_t ready = ((hostWritten - half) * modelUnits + hostUnits - 1) / hostUnits;
    const auto count = static_cast<int>(ready - modelWritten);   // at most maxModelBlock - 1

    for (int j = 0; j < count; ++j)
    {
        const int64_t position = (modelWritten + j) * hostUnits;   // k * H: host position times M
        const int64_t whole = position / modelUnits;
        const auto phase = static_cast<int>(position - whole * modelUnits);
        const float* x = hostHistory.data() + static_cast<size_t>((whole - half + 1) & hostMask);
        const float* w = toModel.phase(phase);
        float sum = 0.0f;
        for (int t = 0; t < width; ++t)
            sum += x[t] * w[t];
        modelIn[static_cast<size_t>(j)] = sum;
    }
    return count;
}

void ResamplingIsland::pullHost(float* out, int numSamples, int modelCount) noexcept
{
    const auto size = static_cast<size_t>(modelMask + 1);
    for (int j = 0; j < modelCount; ++j)
    {
        const auto slot = static_cast<size_t>((modelWritten + j) & modelMask);
        modelHistory[slot] = modelOut[static_cast<size_t>(j)];
        modelHistory[slot + size] = modelOut[static_cast<size_t>(j)];
    }
    modelWritten += modelCount;

    // Host output n sits at model position (n - D) * M / H: always D behind its input.
    const int half = toHost.halfWidth;
    const int width = 2 * half;
    for (int i = 0; i < numSamples; ++i)
    {
        const int64_t position = (outWritten + i - latency) * modelUnits;   // model position times H
        const int64_t whole = position / hostUnits;
        const auto phase = static_cast<int>(position - whole * hostUnits);
        const float* z = modelHistory.data() + static_cast<size_t>((whole - half + 1) & modelMask);
        const float* w = toHost.phase(phase);
        float sum = 0.0f;
        for (int t = 0; t < width; ++t)
            sum += z[t] * w[t];
        out[i] = sum;
    }
    outWritten += numSamples;
}
