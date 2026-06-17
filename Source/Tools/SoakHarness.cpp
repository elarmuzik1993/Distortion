/*
  ==============================================================================

    Soak / Multi-Instance Harness

    Headless stress tool for the Definition-of-Done "24h stress" and "10+
    instance" shipping gates. Runs N PluginProcessor instances — each loaded
    with a different factory preset — rendering a test signal faster than
    realtime for a chosen duration, watching for non-finite output and
    resident-memory growth (gross leak detection).

    Returns non-zero on failure so it can gate a release.

    Example:
      DistortionSoak --instances 10 --seconds 120

  ==============================================================================
*/

#include <JuceHeader.h>
#include "../PluginProcessor.h"
#include "../FactoryPresets.h"

#include <cmath>
#include <iostream>
#include <memory>
#include <vector>

#if defined(__linux__)
 #include <unistd.h>
 #include <cstdio>
#endif

namespace
{
constexpr double kSampleRate = 44100.0;
constexpr int    kBlockSize  = 512;

// Current resident set size in KB (Linux). Returns 0 where unavailable, in
// which case the leak check is skipped.
long currentResidentKB()
{
#if defined(__linux__)
    long rssPages = 0;
    if (FILE* f = std::fopen("/proc/self/statm", "r"))
    {
        long total = 0;
        if (std::fscanf(f, "%ld %ld", &total, &rssPages) != 2)
            rssPages = 0;
        std::fclose(f);
    }
    return rssPages * (sysconf(_SC_PAGESIZE) / 1024);
#else
    return 0;
#endif
}

int intArg(int argc, char* argv[], const juce::String& key, int fallback)
{
    for (int i = 1; i + 1 < argc; ++i)
        if (key == argv[i])
            return juce::String(argv[i + 1]).getIntValue();
    return fallback;
}
} // namespace

int main(int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    const int numInstances = juce::jmax(1, intArg(argc, argv, "--instances", 10));
    const int seconds      = juce::jmax(1, intArg(argc, argv, "--seconds", 60));

    std::cout << "Soak: " << numInstances << " instances, " << seconds
              << "s of audio @ " << (int) kSampleRate << " Hz, block " << kBlockSize << "\n";

    // One processor per instance, each loaded with a rotating factory preset so
    // varied DSP paths run concurrently; one pre-sized work buffer per instance
    // so the render loop does no allocation (keeps the leak metric clean).
    std::vector<std::unique_ptr<PluginProcessor>> procs;
    std::vector<juce::AudioBuffer<float>>          work;
    const auto& presets = FactoryPresets::all();
    for (int i = 0; i < numInstances; ++i)
    {
        auto p = std::make_unique<PluginProcessor>();
        p->setRateAndBufferSizeDetails(kSampleRate, kBlockSize);
        p->prepareToPlay(kSampleRate, kBlockSize);
        FactoryPresets::apply(p->parameters, presets[(size_t) i % presets.size()].name);
        procs.push_back(std::move(p));
        work.emplace_back(2, kBlockSize);
    }

    const long totalBlocks = (long) std::llround((seconds * kSampleRate) / kBlockSize);
    const long step        = juce::jmax(1L, totalBlocks / 10);
    long warmupRSS         = 0;
    const long warmupBlock = juce::jmax(1L, totalBlocks / 20);  // baseline after ~5%

    juce::AudioBuffer<float> source(2, kBlockSize);
    juce::MidiBuffer midi;
    double phase = 0.0;
    const double inc = juce::MathConstants<double>::twoPi * 110.0 / kSampleRate;

    bool nonFinite = false;
    long badBlock  = -1;

    for (long b = 0; b < totalBlocks && ! nonFinite; ++b)
    {
        // Mild-level sine so the safety limiter isn't permanently pinned.
        for (int s = 0; s < kBlockSize; ++s)
        {
            const float v = 0.4f * (float) std::sin(phase);
            phase += inc;
            if (phase >= juce::MathConstants<double>::twoPi)
                phase -= juce::MathConstants<double>::twoPi;
            source.setSample(0, s, v);
            source.setSample(1, s, v);
        }

        for (int i = 0; i < numInstances && ! nonFinite; ++i)
        {
            work[(size_t) i].makeCopyOf(source);   // same size -> reuses allocation
            procs[(size_t) i]->processBlock(work[(size_t) i], midi);

            for (int ch = 0; ch < work[(size_t) i].getNumChannels() && ! nonFinite; ++ch)
            {
                const float* d = work[(size_t) i].getReadPointer(ch);
                for (int s = 0; s < kBlockSize; ++s)
                    if (! std::isfinite(d[s])) { nonFinite = true; badBlock = b; break; }
            }
        }

        if (b == warmupBlock)
            warmupRSS = currentResidentKB();

        if (b % step == 0)
            std::cout << "  " << (100 * b / juce::jmax(1L, totalBlocks)) << "%  RSS "
                      << currentResidentKB() / 1024 << " MB\r" << std::flush;
    }
    std::cout << "\n";

    const long endRSS   = currentResidentKB();
    const long growthKB = (warmupRSS > 0) ? (endRSS - warmupRSS) : 0;

    std::cout << "Output: " << (nonFinite ? "NON-FINITE" : "all finite") << "\n";
    if (warmupRSS > 0)
        std::cout << "RSS: warmup " << warmupRSS / 1024 << " MB -> end " << endRSS / 1024
                  << " MB (growth " << growthKB / 1024 << " MB)\n";
    else
        std::cout << "RSS: unavailable on this platform (leak check skipped)\n";

    // Fail on non-finite output, or resident growth past a generous leak gate.
    const bool leak = (warmupRSS > 0) && (growthKB > 16 * 1024);  // > 16 MB
    if (nonFinite)
        std::cout << "FAILED: non-finite output at block " << badBlock << "\n";
    if (leak)
        std::cout << "FAILED: resident memory grew " << growthKB / 1024 << " MB (possible leak)\n";

    const bool ok = ! nonFinite && ! leak;
    std::cout << (ok ? "SOAK PASSED\n" : "SOAK FAILED\n");
    return ok ? 0 : 1;
}
