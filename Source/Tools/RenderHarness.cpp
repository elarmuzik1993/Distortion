/*
  ==============================================================================

    Offline Render Harness

    A headless console tool that drives the real PluginProcessor faster than
    realtime — no DAW, no audio device. It can:

      * render a test signal (or an input WAV) through the plugin to a WAV file
        so you can A/B settings by ear, and
      * verify the Sub Guard crossover is magnitude-flat by rendering the SAME
        noise through Sub Guard OFF vs ON and reporting the per-band difference
        (a dip or null in the crossover shows up immediately).

    Examples:
      DistortionRender --signal kickbass --subguard 60 --drive 70 --out sg_on.wav
      DistortionRender --in loop.wav --subguard off --out sg_off.wav
      DistortionRender --verify-subguard            # flatness check, exits 0/1

  ==============================================================================
*/

#include <JuceHeader.h>
#include "../PluginProcessor.h"

#include <array>
#include <cmath>
#include <iostream>
#include <map>
#include <string>
#include <vector>

namespace
{
constexpr double kSampleRate = 44100.0;
constexpr int    kBlockSize  = 512;

//==============================================================================
// Minimal CLI parsing: --key value pairs plus bare --flags.
struct Args
{
    std::map<juce::String, juce::String> values;

    bool has(const juce::String& key) const { return values.count(key) > 0; }

    juce::String str(const juce::String& key, const juce::String& fallback) const
    {
        auto it = values.find(key);
        return it == values.end() ? fallback : it->second;
    }

    float num(const juce::String& key, float fallback) const
    {
        auto it = values.find(key);
        return it == values.end() ? fallback : it->second.getFloatValue();
    }
};

Args parseArgs(int argc, char* argv[])
{
    Args a;
    for (int i = 1; i < argc; ++i)
    {
        juce::String token(argv[i]);
        if (! token.startsWith("--"))
            continue;

        const auto key = token.substring(2);
        if (i + 1 < argc && ! juce::String(argv[i + 1]).startsWith("--"))
            a.values[key] = juce::String(argv[++i]);
        else
            a.values[key] = "true";   // bare flag
    }
    return a;
}

//==============================================================================
// Set a parameter by ID using its 0..1 normalisation (same path the tests use).
void setParam(juce::AudioProcessorValueTreeState& apvts, const juce::String& id, float value)
{
    if (auto* p = apvts.getParameter(id))
        p->setValueNotifyingHost(p->convertTo0to1(value));
}

//==============================================================================
// Test-signal generators (stereo, identical channels).
juce::AudioBuffer<float> makeBuffer(int numSamples)
{
    juce::AudioBuffer<float> b(2, numSamples);
    b.clear();
    return b;
}

juce::AudioBuffer<float> generateWhiteNoise(int numSamples, float amp, juce::Random& rng)
{
    auto b = makeBuffer(numSamples);
    for (int n = 0; n < numSamples; ++n)
    {
        const float s = amp * (rng.nextFloat() * 2.0f - 1.0f);
        b.setSample(0, n, s);
        b.setSample(1, n, s);
    }
    return b;
}

juce::AudioBuffer<float> generateSineSweep(int numSamples, double sr, float amp,
                                           double f0, double f1)
{
    auto b = makeBuffer(numSamples);
    const double T = numSamples / sr;
    const double k = std::log(f1 / f0);
    for (int n = 0; n < numSamples; ++n)
    {
        const double t = n / sr;
        const double phase = 2.0 * juce::MathConstants<double>::pi * f0 * T / k
                           * (std::exp(t / T * k) - 1.0);
        const float s = amp * static_cast<float>(std::sin(phase));
        b.setSample(0, n, s);
        b.setSample(1, n, s);
    }
    return b;
}

// Synthetic 808-style kick + sustained bass: lots of sub energy to stress Sub Guard.
juce::AudioBuffer<float> generateKickBass(int numSamples, double sr, float amp)
{
    auto b = makeBuffer(numSamples);
    const double twoPi = 2.0 * juce::MathConstants<double>::pi;
    for (int n = 0; n < numSamples; ++n)
    {
        const double t = n / sr;
        // Decaying 50 Hz body with a short pitch drop (the "808").
        const double env  = std::exp(-t * 4.0);
        const double freq = 50.0 + 40.0 * std::exp(-t * 30.0);
        const double kick = env * std::sin(twoPi * freq * t);
        // Sustained 80 Hz bass with a little 2nd harmonic for mid content.
        const double bass = 0.6 * std::sin(twoPi * 80.0 * t)
                          + 0.2 * std::sin(twoPi * 160.0 * t);
        const float s = amp * static_cast<float>(0.6 * kick + 0.4 * bass);
        b.setSample(0, n, s);
        b.setSample(1, n, s);
    }
    return b;
}

//==============================================================================
// Run a buffer through the plugin in place, block by block.
void runPlugin(PluginProcessor& proc, juce::AudioBuffer<float>& buffer)
{
    juce::MidiBuffer midi;
    const int total = buffer.getNumSamples();
    for (int start = 0; start < total; start += kBlockSize)
    {
        const int len = std::min(kBlockSize, total - start);
        juce::AudioBuffer<float> block(buffer.getArrayOfWritePointers(),
                                       buffer.getNumChannels(), start, len);
        midi.clear();
        proc.processBlock(block, midi);
    }
}

//==============================================================================
bool writeWav(const juce::File& file, const juce::AudioBuffer<float>& buffer, double sr)
{
    file.deleteFile();
    juce::WavAudioFormat fmt;
    std::unique_ptr<juce::FileOutputStream> stream(file.createOutputStream());
    if (stream == nullptr)
        return false;

    std::unique_ptr<juce::AudioFormatWriter> writer(
        fmt.createWriterFor(stream.get(), sr,
                            static_cast<unsigned int>(buffer.getNumChannels()),
                            24, {}, 0));
    if (writer == nullptr)
        return false;

    stream.release();   // writer owns the stream now
    writer->writeFromAudioSampleBuffer(buffer, 0, buffer.getNumSamples());
    return true;
}

juce::AudioBuffer<float> readWav(const juce::File& file, double& srOut)
{
    juce::AudioFormatManager mgr;
    mgr.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(mgr.createReaderFor(file));
    if (reader == nullptr)
    {
        srOut = kSampleRate;
        return makeBuffer(0);
    }

    srOut = reader->sampleRate;
    juce::AudioBuffer<float> b(2, static_cast<int>(reader->lengthInSamples));
    b.clear();
    reader->read(&b, 0, static_cast<int>(reader->lengthInSamples), 0, true, true);
    if (reader->numChannels == 1)
        b.copyFrom(1, 0, b, 0, 0, b.getNumSamples());   // mono -> dual mono
    return b;
}

//==============================================================================
// Mono-sum magnitude spectrum (linear) of the first power-of-two window.
std::vector<float> magnitudeSpectrum(const juce::AudioBuffer<float>& buffer, int fftOrder)
{
    const int fftSize = 1 << fftOrder;
    juce::dsp::FFT fft(fftOrder);

    std::vector<float> data(static_cast<size_t>(fftSize) * 2, 0.0f);
    const int avail = std::min(fftSize, buffer.getNumSamples());
    for (int n = 0; n < avail; ++n)
    {
        const float mono = 0.5f * (buffer.getSample(0, n) + buffer.getSample(1, n));
        const float w = 0.5f - 0.5f * std::cos(2.0f * juce::MathConstants<float>::pi
                                               * n / (fftSize - 1));   // Hann
        data[static_cast<size_t>(n)] = mono * w;
    }

    fft.performFrequencyOnlyForwardTransform(data.data());
    data.resize(static_cast<size_t>(fftSize / 2));
    return data;
}

//==============================================================================
// Configure a processor for a clean, isolated pass (no nonlinear coloration),
// so the only variable is the Sub Guard crossover.
void configureClean(PluginProcessor& proc)
{
    auto& p = proc.parameters;
    // Inputs/outputs at unity, fully wet.
    setParam(p, "inputGain", 50.0f);      // unity (gain curve centres at 50)
    setParam(p, "outputGain", 50.0f);     // unity
    setParam(p, "globalMix", 100.0f);     // fully wet
    setParam(p, "tone", 20000.0f);        // tone LP wide open

    // No nonlinear coloration: distortion and waveshaper fully dry/off.
    setParam(p, "distortionAmount", 0.0f);
    setParam(p, "distMix", 0.0f);
    setParam(p, "waveshaperMix", 0.0f);
    setParam(p, "autoGainEnabled", 0.0f);
    setParam(p, "cleanBoost", 0.0f);
    setParam(p, "extremeEnabled", 0.0f);
    setParam(p, "lfoEnabled", 0.0f);

    // CRITICAL: the plugin true-bypasses (skipping the input filter AND Sub Guard)
    // when distortionAmount < 0.5 and the compressor is off. Enable the compressor
    // with zero peak reduction: it returns early (no gain change) but keeps the full
    // DSP chain running so the filter/crossover are actually exercised.
    setParam(p, "compEnabled", 1.0f);
    setParam(p, "compPeakReduction", 0.0f);
}

//==============================================================================
// Differential flatness check: same noise through Sub Guard OFF vs ON.
int verifySubGuard(float crossoverHz)
{
    juce::Random rng(20240603);
    const int numSamples = 1 << 16;   // 65536 samples
    const auto noise = generateWhiteNoise(numSamples, 0.25f, rng);

    auto render = [&](float sgFreq)
    {
        PluginProcessor proc;
        proc.setRateAndBufferSizeDetails(kSampleRate, kBlockSize);
        proc.prepareToPlay(kSampleRate, kBlockSize);
        configureClean(proc);
        setParam(proc.parameters, "subGuardFreq", sgFreq);

        juce::AudioBuffer<float> buf(2, numSamples);
        for (int ch = 0; ch < 2; ++ch)
            buf.copyFrom(ch, 0, noise, ch, 0, numSamples);
        runPlugin(proc, buf);
        return buf;
    };

    const auto outOff = render(0.0f);
    const auto outOn  = render(crossoverHz);

    constexpr int fftOrder = 15;            // 32768-point
    const auto specOff = magnitudeSpectrum(outOff, fftOrder);
    const auto specOn  = magnitudeSpectrum(outOn,  fftOrder);
    const int  fftSize = 1 << fftOrder;
    const double binHz = kSampleRate / fftSize;

    // Report the Sub Guard ON / OFF ratio across log-spaced bands. The same noise
    // realisation feeds both, so this ratio reflects only the crossover.
    const float bandEdges[] = { 20, 30, 40, 50, 60, 80, 100, 130, 160, 200, 300, 500, 1000, 2000 };
    const int numBands = static_cast<int>(std::size(bandEdges)) - 1;

    std::cout << "\n  Sub Guard flatness check  (crossover = "
              << static_cast<int>(crossoverHz) << " Hz)\n";
    std::cout << "  Same white noise through Sub Guard OFF vs ON; 0 dB = perfectly flat.\n\n";
    std::cout << "    Band (Hz)        ON-vs-OFF\n";
    std::cout << "    -----------      ---------\n";

    float worstAbsDb = 0.0f;
    float worstFreq  = 0.0f;
    for (int band = 0; band < numBands; ++band)
    {
        const float lo = bandEdges[band];
        const float hi = bandEdges[band + 1];
        double sumOn = 0.0, sumOff = 0.0;
        int count = 0;
        for (size_t bin = 1; bin < specOn.size(); ++bin)
        {
            const double f = bin * binHz;
            if (f >= lo && f < hi)
            {
                sumOn  += specOn[bin]  * specOn[bin];
                sumOff += specOff[bin] * specOff[bin];
                ++count;
            }
        }
        if (count == 0)
            continue;

        const double rmsOn  = std::sqrt(sumOn  / count);
        const double rmsOff = std::sqrt(sumOff / count);
        const float db = (rmsOff > 1.0e-12)
                       ? juce::Decibels::gainToDecibels(static_cast<float>(rmsOn / rmsOff))
                       : 0.0f;

        if (std::abs(db) > std::abs(worstAbsDb))
        {
            worstAbsDb = db;
            worstFreq  = 0.5f * (lo + hi);
        }

        char line[128];
        std::snprintf(line, sizeof(line), "    %5.0f - %-5.0f      %+6.2f dB\n", lo, hi, db);
        std::cout << line;
    }

    constexpr float toleranceDb = 0.5f;
    const bool pass = std::abs(worstAbsDb) <= toleranceDb;

    std::cout << "\n  Worst deviation: " << juce::String(worstAbsDb, 2)
              << " dB near " << static_cast<int>(worstFreq) << " Hz"
              << "  (tolerance +/-" << toleranceDb << " dB)\n";
    std::cout << (pass ? "  RESULT: PASS - crossover sums flat\n"
                       : "  RESULT: FAIL - crossover is not flat\n");
    return pass ? 0 : 1;
}

//==============================================================================
// Verify the input multimode filter shapes the spectrum as expected.
int verifyFilter()
{
    juce::Random rng(20240603);
    const int numSamples = 1 << 16;
    const auto noise = generateWhiteNoise(numSamples, 0.25f, rng);
    const float cutoff = 1000.0f;

    auto bandEnergyDb = [&](int modeIdx) -> std::array<float, 3>
    {
        PluginProcessor proc;
        proc.setRateAndBufferSizeDetails(kSampleRate, kBlockSize);
        proc.prepareToPlay(kSampleRate, kBlockSize);
        configureClean(proc);
        // Force the plugin's TRUE-BYPASS state (no distortion, no compressor) to prove
        // the input filter now works as a standalone HP/LP/BP with everything else off.
        setParam(proc.parameters, "compEnabled", 0.0f);
        setParam(proc.parameters, "filterMode", static_cast<float>(modeIdx));
        setParam(proc.parameters, "highPassFreq", cutoff);

        juce::AudioBuffer<float> buf(2, numSamples);
        for (int ch = 0; ch < 2; ++ch)
            buf.copyFrom(ch, 0, noise, ch, 0, numSamples);
        runPlugin(proc, buf);

        constexpr int fftOrder = 15;
        const auto spec = magnitudeSpectrum(buf, fftOrder);
        const double binHz = kSampleRate / (1 << fftOrder);
        auto band = [&](double lo, double hi)
        {
            double sum = 0.0; int n = 0;
            for (size_t b = 1; b < spec.size(); ++b)
            {
                const double f = b * binHz;
                if (f >= lo && f < hi) { sum += spec[b] * spec[b]; ++n; }
            }
            return n ? juce::Decibels::gainToDecibels(static_cast<float>(std::sqrt(sum / n))) : -120.0f;
        };
        return { band(150, 250), band(900, 1100), band(4000, 6000) };   // low / mid / high
    };

    const char* names[] = { "High Pass", "Low Pass", "Band Pass" };
    std::cout << "\n  Input filter check  (cutoff = 1000 Hz, white noise)\n\n";
    std::cout << "    Mode         ~200Hz    ~1kHz    ~5kHz\n";
    std::cout << "    ---------    ------    -----    -----\n";

    std::array<std::array<float, 3>, 3> e;
    for (int m = 0; m < 3; ++m)
    {
        e[m] = bandEnergyDb(m);
        char line[128];
        std::snprintf(line, sizeof(line), "    %-9s   %+6.1f   %+6.1f   %+6.1f\n",
                      names[m], e[m][0], e[m][1], e[m][2]);
        std::cout << line;
    }

    // Expectations: HP cuts lows, LP cuts highs, BP cuts both vs centre. The margin
    // is 10 dB — a 2nd-order band-pass rejects ~11-12 dB at 2.5 octaves from centre.
    const bool hpOk = (e[0][1] - e[0][0]) > 10.0f;
    const bool lpOk = (e[1][1] - e[1][2]) > 10.0f;
    const bool bpOk = (e[2][1] - e[2][0]) > 10.0f && (e[2][1] - e[2][2]) > 10.0f;
    const bool pass = hpOk && lpOk && bpOk;

    std::cout << "\n    High Pass cuts lows:   " << (hpOk ? "PASS" : "FAIL") << "\n";
    std::cout << "    Low Pass cuts highs:   " << (lpOk ? "PASS" : "FAIL") << "\n";
    std::cout << "    Band Pass cuts both:   " << (bpOk ? "PASS" : "FAIL") << "\n";
    std::cout << (pass ? "  RESULT: PASS - all modes shape correctly\n"
                       : "  RESULT: FAIL\n");
    return pass ? 0 : 1;
}

//==============================================================================
int renderToFile(const Args& args)
{
    const auto signal   = args.str("signal", "kickbass");
    const auto inPath   = args.str("in", "");
    const auto outPath  = args.str("out", "render.wav");
    const float sgFreq  = args.num("subguard", 0.0f);   // 0 / "off" => OFF
    const float drive   = args.num("drive", 0.0f);
    const float mix     = args.num("mix", 100.0f);
    const float seconds = args.num("seconds", 3.0f);

    double sr = kSampleRate;
    juce::AudioBuffer<float> input;

    if (inPath.isNotEmpty())
    {
        input = readWav(juce::File::getCurrentWorkingDirectory().getChildFile(inPath), sr);
        if (input.getNumSamples() == 0)
        {
            std::cerr << "Could not read input WAV: " << inPath << "\n";
            return 1;
        }
    }
    else
    {
        const int numSamples = static_cast<int>(seconds * kSampleRate);
        juce::Random rng(1);
        if (signal == "noise")      input = generateWhiteNoise(numSamples, 0.25f, rng);
        else if (signal == "sweep") input = generateSineSweep(numSamples, sr, 0.5f, 20.0, 20000.0);
        else                        input = generateKickBass(numSamples, sr, 0.8f);
    }

    PluginProcessor proc;
    proc.setRateAndBufferSizeDetails(sr, kBlockSize);
    proc.prepareToPlay(sr, kBlockSize);

    setParam(proc.parameters, "globalMix", 100.0f);
    setParam(proc.parameters, "subGuardFreq", (args.str("subguard", "") == "off") ? 0.0f : sgFreq);
    setParam(proc.parameters, "distortionAmount", drive);
    setParam(proc.parameters, "distMix", mix);

    // Input filter: --mode hp|lp|bp, --filterfreq <Hz>
    const auto mode = args.str("mode", "hp");
    const float filterModeIdx = (mode == "lp") ? 1.0f : (mode == "bp") ? 2.0f : 0.0f;
    setParam(proc.parameters, "filterMode", filterModeIdx);
    if (args.has("filterfreq"))
        setParam(proc.parameters, "highPassFreq", args.num("filterfreq", 20.0f));

    runPlugin(proc, input);

    const auto outFile = juce::File::getCurrentWorkingDirectory().getChildFile(outPath);
    if (! writeWav(outFile, input, sr))
    {
        std::cerr << "Failed to write output WAV: " << outPath << "\n";
        return 1;
    }

    std::cout << "Rendered " << input.getNumSamples() << " samples @ "
              << static_cast<int>(sr) << " Hz -> " << outFile.getFullPathName() << "\n";
    std::cout << "  signal=" << (inPath.isNotEmpty() ? inPath : signal)
              << "  subguard=" << (sgFreq > 0.0f ? juce::String((int) sgFreq) + "Hz" : juce::String("off"))
              << "  drive=" << drive << "  mix=" << mix << "\n";
    return 0;
}
} // namespace

//==============================================================================
int main(int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    const Args args = parseArgs(argc, argv);

    if (args.has("help"))
    {
        std::cout <<
            "Offline render harness for Monolit Distortion\n\n"
            "  --verify-subguard [--subguard <Hz>]   crossover flatness check (default 60 Hz)\n"
            "  --verify-filter                       input filter HP/LP/BP shaping check\n"
            "  --signal noise|sweep|kickbass         synthetic input (default kickbass)\n"
            "  --mode hp|lp|bp  --filterfreq <Hz>    input filter mode / cutoff\n"
            "  --in <file.wav>                       use a WAV file as input\n"
            "  --subguard <Hz|off>                   Sub Guard crossover frequency\n"
            "  --drive <0-100>  --mix <0-100>        distortion amount / wet mix\n"
            "  --seconds <n>                         length of synthetic input (default 3)\n"
            "  --out <file.wav>                      output path (default render.wav)\n";
        return 0;
    }

    if (args.has("verify-subguard"))
        return verifySubGuard(args.num("subguard", 60.0f));

    if (args.has("verify-filter"))
        return verifyFilter();

    return renderToFile(args);
}
