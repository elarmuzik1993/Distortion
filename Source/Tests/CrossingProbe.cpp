// Measurement probe, not a pass/fail test: what a listener hears when processBlock
// switches between its true-bypass branch and the active chain (Distortion Amount
// crossing 0.5%), and when the host bypass engages or releases.
//
// Every crossing is compared against two twins fed the same input: one that stays
// on the old route and one that stays on the new route the whole time. From them:
//   fed only   = a hard switch between the warm twins (every element kept running, no fade)
//   fade only  = a 10 ms crossfade from the old twin into the real output (faded, not kept running)
//   both       = a 10 ms crossfade between the warm twins (kept running and faded)
// "today" is the real output. today - (fed only) is purely the state the switch left behind.
// A click's size depends on where in the waveform the switch lands, so each crossing
// runs at 8 positions and the table gives the median and the worst.
//
// A full test run leaves it out (category "Probe"); name it to run it (about 40 s):
//   DistortionTests "Crossing Probe"
// With CROSSING_PROBE_OUT set to a folder, it also writes listening reels there.

#include <JuceHeader.h>
#include "../PluginProcessor.h"
#include "TestUtilities.h"

#include <algorithm>
#include <cstdio>
#include <vector>

namespace
{
using Buffer = juce::AudioBuffer<float>;

enum class Route { active, bypass, hostBypass };
struct Segment { Route route; int blocks; };

constexpr int    kBlock        = 512;
constexpr int    kPhases       = 8;
constexpr float  kActiveAmount = 0.6f;   // just above the 0.5% true-bypass threshold
constexpr float  kBypassAmount = 0.4f;   // just below it
constexpr double kFadeMs       = 10.0;

struct Settings
{
    double sampleRate = 48000.0;
    int clipType = 0;
    bool linearPhase = false;
    const char* profile = nullptr;   // a NAM Core example model, or the built-in clip type
    juce::String label() const
    {
        auto s = juce::String(sampleRate / 1000.0, 1) + "k ";
        if (profile != nullptr)
            return s + juce::String(profile).upToFirstOccurrenceOf(".", false, false);
        return s + "clip" + juce::String(clipType) + (linearPhase ? " FIR" : "");
    }
};

int blocksFor(double seconds, double sr) { return juce::roundToInt(seconds * sr / kBlock); }

// A held, bright note: 110 Hz with 8 harmonics at 1/k, peak -12 dBFS, both channels.
Buffer makeInput(double sr, int numSamples)
{
    Buffer b(2, numSamples);
    auto* l = b.getWritePointer(0);
    float peak = 0.0f;
    for (int i = 0; i < numSamples; ++i)
    {
        double x = 0.0;
        for (int k = 1; k <= 8; ++k)
            x += std::sin(juce::MathConstants<double>::twoPi * 110.0 * k * i / sr) / k;
        l[i] = static_cast<float>(x);
        peak = juce::jmax(peak, std::abs(l[i]));
    }
    b.applyGain(0, 0, numSamples, juce::Decibels::decibelsToGain(-12.0f) / peak);
    b.copyFrom(1, 0, b, 0, 0, numSamples);
    return b;
}

int totalBlocks(const std::vector<Segment>& segments)
{
    int n = 0;
    for (const auto& s : segments)
        n += s.blocks;
    return n;
}

Buffer render(const Settings& s, const std::vector<Segment>& segments, const Buffer& input,
              int* reportedLatency = nullptr)
{
    PluginProcessor p;
    auto amountFor = [](Route r) { return r == Route::bypass ? kBypassAmount : kActiveAmount; };
    TestUtilities::setParameter(p.parameters, "clipType", static_cast<float>(s.clipType));
    TestUtilities::setParameter(p.parameters, "linearPhaseDry", s.linearPhase ? 1.0f : 0.0f);
    TestUtilities::setParameter(p.parameters, "distortionAmount", amountFor(segments.front().route));
    p.setRateAndBufferSizeDetails(s.sampleRate, kBlock);
    p.prepareToPlay(s.sampleRate, kBlock);
    if (s.profile != nullptr)
    {
        // prepareToPlay installs a staged profile the way the timer would, minus the fade.
        const bool loaded = p.loadProfileBlocking(
            juce::File(DISTORTION_NAM_TEST_MODELS_DIR).getChildFile(s.profile));
        p.prepareToPlay(s.sampleRate, kBlock);
        jassert(loaded && p.isProfileSwitchIdle());
        juce::ignoreUnused(loaded);
    }
    if (reportedLatency != nullptr)
        *reportedLatency = p.getLatencySamples();

    Buffer out(input);
    juce::MidiBuffer midi;
    int pos = 0;
    for (const auto& seg : segments)
    {
        TestUtilities::setParameter(p.parameters, "distortionAmount", amountFor(seg.route));
        for (int b = 0; b < seg.blocks; ++b)
        {
            Buffer view(out.getArrayOfWritePointers(), 2, pos, kBlock);
            if (seg.route == Route::hostBypass)
                p.processBlockBypassed(view, midi);
            else
                p.processBlock(view, midi);
            pos += kBlock;
        }
    }
    jassert(pos == out.getNumSamples());
    return out;
}

// Channel 0 through a 4 kHz high-pass (RBJ biquad, Q 0.707): the treble a click lives in.
std::vector<float> treble(const Buffer& b, double sr)
{
    const double w0 = juce::MathConstants<double>::twoPi * 4000.0 / sr;
    const double alpha = std::sin(w0) / (2.0 * 0.7071);
    const double c = std::cos(w0);
    const double a0 = 1.0 + alpha;
    const double b0 = (1.0 + c) / 2.0 / a0, b1 = -(1.0 + c) / a0, b2 = b0;
    const double a1 = -2.0 * c / a0, a2 = (1.0 - alpha) / a0;
    std::vector<float> y(static_cast<size_t>(b.getNumSamples()));
    double x1 = 0, x2 = 0, y1 = 0, y2 = 0;
    const float* x = b.getReadPointer(0);
    for (size_t i = 0; i < y.size(); ++i)
    {
        const double out = b0 * x[i] + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
        x2 = x1; x1 = x[i]; y2 = y1; y1 = out;
        y[i] = static_cast<float>(out);
    }
    return y;
}

float windowRms(const std::vector<float>& v, int start, int len)
{
    double sum = 0.0;
    for (int i = start; i < start + len; ++i)
        sum += static_cast<double>(v[static_cast<size_t>(i)]) * v[static_cast<size_t>(i)];
    return static_cast<float>(std::sqrt(sum / len));
}

float rmsRange(const Buffer& b, int start, int len)
{
    double sum = 0.0;
    const float* x = b.getReadPointer(0);
    for (int i = start; i < start + len; ++i)
        sum += static_cast<double>(x[i]) * x[i];
    return static_cast<float>(std::sqrt(sum / len));
}

float db(float ratio) { return 20.0f * std::log10(juce::jmax(ratio, 1.0e-9f)); }

struct Crossing
{
    const char* name;
    std::vector<std::pair<Route, double>> schedule;   // route, seconds
    Route oldRoute, newRoute;
    int measuredSwitch;   // which route change is measured (0 = the first)
};

struct Measurement
{
    float today, fedOnly, fadeOnly, both, leftPeak, leftRms;
    int latency, padOffset;
    Buffer reel;
};

Measurement measure(const Settings& s, const Crossing& c, int phase)
{
    // The lead-in shifts every switch by `phase` blocks, so it lands elsewhere in the waveform.
    std::vector<Segment> segments;
    for (size_t i = 0; i < c.schedule.size(); ++i)
        segments.push_back({ c.schedule[i].first,
                             blocksFor(c.schedule[i].second, s.sampleRate) + (i == 0 ? phase : 0) });
    int n0 = 0;
    for (int i = 0; i <= c.measuredSwitch; ++i)
        n0 += segments[static_cast<size_t>(i)].blocks * kBlock;
    const int numSamples = totalBlocks(segments) * kBlock;
    const Buffer input = makeInput(s.sampleRate, numSamples);

    Measurement m {};
    const Buffer real = render(s, segments, input, &m.latency);
    const Buffer oldTwin = render(s, { { c.oldRoute, totalBlocks(segments) } }, input);
    const Buffer newTwin = render(s, { { c.newRoute, totalBlocks(segments) } }, input);

    const auto ms = [&](double t) { return juce::roundToInt(t * 0.001 * s.sampleRate); };

    // The route changes at n0, but the shared output pad still holds the old route's
    // last few samples, so the output switches a pad's length later. Find that point:
    // the first sample where the real output leaves the old route's twin.
    {
        const float threshold = 0.05f * rmsRange(oldTwin, n0 - ms(600.0), ms(500.0));
        const auto* r = real.getReadPointer(0);
        const auto* o = oldTwin.getReadPointer(0);
        int i = n0;
        const int routeSwitch = n0;
        while (i < n0 + m.latency + 64 && std::abs(r[i] - o[i]) <= threshold)
            ++i;
        n0 = i < n0 + m.latency + 64 ? i : n0;
        m.padOffset = n0 - routeSwitch;
    }
    const int fadeLen = ms(kFadeMs);
    Buffer fedOnly(oldTwin), both(oldTwin), fadeOnly(real);
    for (int ch = 0; ch < 2; ++ch)
    {
        const auto* o = oldTwin.getReadPointer(ch);
        const auto* n = newTwin.getReadPointer(ch);
        const auto* r = real.getReadPointer(ch);
        auto* h = fedOnly.getWritePointer(ch);
        auto* x = both.getWritePointer(ch);
        auto* f = fadeOnly.getWritePointer(ch);
        for (int i = n0; i < numSamples; ++i)
        {
            const float w = juce::jmin(1.0f, static_cast<float>(i - n0 + 1) / static_cast<float>(fadeLen));
            h[i] = n[i];
            x[i] = o[i] * (1.0f - w) + n[i] * w;
            f[i] = o[i] * (1.0f - w) + r[i] * w;
        }
    }

    // Treble burst: the loudest 1 ms of >4 kHz content within 30 ms of the switch,
    // against the loudest 1 ms of treble either steady twin has within 100 ms of it.
    const int win = ms(1.0), hop = juce::jmax(1, ms(0.25));
    const auto tOld = treble(oldTwin, s.sampleRate);
    const auto tNew = treble(newTwin, s.sampleRate);
    float masker = 1.0e-6f;
    for (int i = n0 - ms(100.0); i < n0 + ms(100.0); i += hop)
        masker = juce::jmax(masker, windowRms(tOld, i, win), windowRms(tNew, i, win));
    const auto burst = [&](const Buffer& b) {
        const auto t = treble(b, s.sampleRate);
        float peak = 0.0f;
        for (int i = n0 - ms(2.0); i < n0 + ms(30.0); i += hop)
            peak = juce::jmax(peak, windowRms(t, i, win));
        return db(peak / masker);
    };
    m.today = burst(real);
    m.fedOnly = burst(fedOnly);
    m.fadeOnly = burst(fadeOnly);
    m.both = burst(both);

    // Leftover: today minus the warm hard switch, against the steady signal's RMS.
    const float signalRms = juce::jmax(rmsRange(newTwin, n0 + ms(100.0), ms(500.0)),
                                       rmsRange(oldTwin, n0 - ms(600.0), ms(500.0)));
    float leftPeak = 0.0f;
    double leftSum = 0.0;
    const auto* r = real.getReadPointer(0);
    const auto* h = fedOnly.getReadPointer(0);
    for (int i = n0; i < n0 + ms(30.0); ++i)
        leftPeak = juce::jmax(leftPeak, std::abs(r[i] - h[i]));
    const int settleEnd = juce::jmin(numSamples, n0 + ms(1000.0));
    for (int i = n0 + ms(30.0); i < settleEnd; ++i)
        leftSum += static_cast<double>(r[i] - h[i]) * (r[i] - h[i]);
    m.leftPeak = db(leftPeak / signalRms);
    m.leftRms = db(static_cast<float>(std::sqrt(leftSum / (settleEnd - n0 - ms(30.0)))) / signalRms);

    // A listening reel: today, fade only, fed only, both; 1.2 s around the switch each.
    const int pre = ms(600.0), len = ms(1200.0), gap = ms(400.0);
    m.reel.setSize(2, 4 * (len + gap));
    m.reel.clear();
    const Buffer* order[] = { &real, &fadeOnly, &fedOnly, &both };
    for (int k = 0; k < 4; ++k)
        for (int ch = 0; ch < 2; ++ch)
            m.reel.copyFrom(ch, k * (len + gap), *order[k], ch, n0 - pre,
                            juce::jmin(len, numSamples - (n0 - pre)));
    return m;
}

float median(std::vector<float> v)
{
    std::sort(v.begin(), v.end());
    return v[v.size() / 2];
}

float worst(const std::vector<float>& v) { return *std::max_element(v.begin(), v.end()); }

} // namespace

class CrossingProbe : public juce::UnitTest
{
public:
    CrossingProbe() : juce::UnitTest("Crossing Probe", "Probe") {}

    void runTest() override
    {
        beginTest("Bypass <-> active crossings");

        using R = Route;
        const std::vector<Crossing> crossings {
            { "switch on, first time",  { { R::bypass, 1.0 }, { R::active, 1.0 } },                      R::bypass,     R::active,     0 },
            { "switch off, first time", { { R::active, 1.0 }, { R::bypass, 1.0 } },                      R::active,     R::bypass,     0 },
            { "switch on again",        { { R::active, 1.0 }, { R::bypass, 1.0 }, { R::active, 1.0 } },  R::bypass,     R::active,     1 },
            { "switch off again",       { { R::bypass, 1.0 }, { R::active, 1.0 }, { R::bypass, 1.0 } },  R::active,     R::bypass,     1 },
            { "host bypass released",   { { R::active, 1.0 }, { R::hostBypass, 1.0 }, { R::active, 1.0 } }, R::hostBypass, R::active, 1 },
            { "host bypass engaged",    { { R::hostBypass, 1.0 }, { R::active, 1.0 }, { R::hostBypass, 1.0 } }, R::active, R::hostBypass, 1 },
        };
        const std::vector<Settings> settings {
            { 48000.0, 0, false, nullptr }, { 48000.0, 1, false, nullptr },
            { 44100.0, 0, false, nullptr }, { 44100.0, 1, false, nullptr },
            { 48000.0, 1, true,  nullptr },
            { 48000.0, 0, false, "wavenet.nam" }, { 44100.0, 0, false, "wavenet.nam" },
        };

        std::printf("\nTreble burst at the switch, in dB above the loudest treble the steady sound has\n"
                    "nearby (> 0 dB sticks out: a click). Median / worst of %d switch positions.\n"
                    "Leftover = today minus a hard switch between warm twins, in dB re the signal RMS:\n"
                    "peak in the first 30 ms, RMS from 30 ms to 1 s. Worst of the positions.\n\n", kPhases);
        std::printf("%-23s %-14s %3s %3s | %-13s %-13s %-13s %6s | %7s %7s\n", "crossing", "settings", "lat", "pad",
                    "today", "fed only", "fade only", "both", "left pk", "left rms");

        const auto outPath = juce::SystemStats::getEnvironmentVariable("CROSSING_PROBE_OUT", {});
        const auto outDir = juce::File::getCurrentWorkingDirectory().getChildFile(outPath);
        if (outPath.isNotEmpty())
            outDir.createDirectory();

        for (const auto& s : settings)
        {
            for (const auto& c : crossings)
            {
                std::vector<float> today, fedOnly, fadeOnly, both, leftPeak, leftRms;
                std::vector<Measurement> runs;
                for (int phase = 0; phase < kPhases; ++phase)
                {
                    runs.push_back(measure(s, c, phase));
                    const auto& m = runs.back();
                    today.push_back(m.today);
                    fedOnly.push_back(m.fedOnly);
                    fadeOnly.push_back(m.fadeOnly);
                    both.push_back(m.both);
                    leftPeak.push_back(m.leftPeak);
                    leftRms.push_back(m.leftRms);
                }
                std::printf("%-23s %-14s %3d %3d | %+5.1f / %+5.1f %+5.1f / %+5.1f %+5.1f / %+5.1f %+6.1f | %+7.1f %+7.1f\n",
                            c.name, s.label().toRawUTF8(), runs.front().latency, runs.front().padOffset,
                            median(today), worst(today), median(fedOnly), worst(fedOnly),
                            median(fadeOnly), worst(fadeOnly), worst(both),
                            worst(leftPeak), worst(leftRms));

                // Listening copies for the default sound at 48 kHz and the profile at 44.1 kHz,
                // at the switch position whose "today" burst is the median.
                const bool defaultSound = juce::exactlyEqual(s.sampleRate, 48000.0) && s.clipType == 0
                                       && ! s.linearPhase && s.profile == nullptr;
                const bool profileSound = s.profile != nullptr && juce::exactlyEqual(s.sampleRate, 44100.0);
                if (outPath.isNotEmpty() && (defaultSound || profileSound))
                {
                    const float target = median(today);
                    const auto pick = std::min_element(runs.begin(), runs.end(), [&](const auto& a, const auto& b) {
                        return std::abs(a.today - target) < std::abs(b.today - target);
                    });
                    const auto fileName = s.label().replaceCharacters(". ", "--") + "_"
                                        + juce::String(c.name).replaceCharacters(" ,", "__") + ".wav";
                    TestUtilities::saveWavFile(pick->reel, outDir.getChildFile(fileName), s.sampleRate);
                }
                // The one check: a smooth switch between warm twins must read as no burst,
                // or the measure itself is off.
                expect(worst(both) < 3.0f, juce::String(c.name) + " " + s.label()
                       + ": a smooth switch reads " + juce::String(worst(both), 1) + " dB");
            }
        }
        if (outPath.isNotEmpty())
            std::printf("\nListening reels (today, fade only, fed only, both): %s\n",
                        outDir.getFullPathName().toRawUTF8());
    }
};

static CrossingProbe crossingProbe;
