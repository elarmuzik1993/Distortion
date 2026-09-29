#include "NamProfile.h"

#include "NAM/activations.h"
#include "NAM/get_dsp.h"

#include <cmath>
#include <filesystem>
#include <mutex>

namespace
{
    std::filesystem::path toFsPath(const juce::File& file)
    {
       #if JUCE_WINDOWS
        return std::filesystem::path(file.getFullPathName().toWideCharPointer());
       #else
        return std::filesystem::path(file.getFullPathName().toStdString());
       #endif
    }

    // NAM's fast tanh approximation, which the NAM plugin ships with: about 3.5x
    // cheaper for WaveNet profiles (measured 28% -> 9% of a core per channel).
    // It swaps an entry in a global table that models read when they are built,
    // so it is switched on once, before the first model is constructed.
    void enableFastTanhOnce()
    {
        static std::once_flag once;
        std::call_once(once, [] { nam::activations::Activation::enable_fast_tanh(); });
    }
}

std::unique_ptr<NamProfile> NamProfile::load(const juce::File& file, int numChannels,
                                             double hostRate, int maxHostBlock,
                                             juce::String& errorOut)
{
    if (! file.existsAsFile())
    {
        errorOut = "Profile file not found: " + file.getFullPathName();
        return {};
    }
    if (! file.hasFileExtension("nam"))
    {
        errorOut = "Not a .nam profile: " + file.getFileName();
        return {};
    }

    try
    {
        enableFastTanhOnce();
        std::unique_ptr<NamProfile> profile(new NamProfile());
        profile->file = file;

        // Parse the file once; get_dsp(dspData&) consumes the weights it is
        // given, so every further channel gets its own copy of the config.
        nam::dspData config;
        profile->models.push_back(nam::get_dsp(toFsPath(file), config));
        for (int ch = 1; ch < juce::jmax(1, numChannels); ++ch)
        {
            nam::dspData copy = config;
            profile->models.push_back(nam::get_dsp(copy));
        }

        const auto& first = *profile->models.front();
        if (first.NumInputChannels() != 1 || first.NumOutputChannels() != 1)
        {
            errorOut = "Only mono (one-in, one-out) profiles are supported: " + file.getFileName();
            return {};
        }

        const double trainedRate = profile->models.front()->GetExpectedSampleRate();
        profile->expectedSampleRate = trainedRate > 0.0 ? trainedRate : 48000.0;

        if (first.HasLoudness())
            profile->outputGain = static_cast<float>(
                std::pow(10.0, (targetLoudnessDb - first.GetLoudness()) / 20.0));

        profile->islands.resize(profile->models.size());
        if (! profile->prepare(hostRate, maxHostBlock))
        {
            errorOut = unsupportedRateMessage(hostRate);
            return {};
        }
        return profile;
    }
    catch (const std::exception& e)
    {
        errorOut = "Could not load " + file.getFileName() + ": " + juce::String(e.what());
        return {};
    }
}

NamProfile::NamProfile() = default;
NamProfile::~NamProfile() = default;

juce::String NamProfile::unsupportedRateMessage(double hostRate)
{
    auto kHz = juce::String(hostRate / 1000.0, 3);
    while (kHz.endsWithChar('0'))
        kHz = kHz.dropLastCharacters(1);
    if (kHz.endsWithChar('.'))
        kHz = kHz.dropLastCharacters(1);
    return "This profile can't run at " + kHz + " kHz";
}

bool NamProfile::prepare(double hostRate, int maxHostBlock)
{
    preparedSampleRate = hostRate;
    preparedBlockSize = juce::jmax(1, maxHostBlock);
    ready = false;
    for (auto& island : islands)
        if (! island.prepare(hostRate, expectedSampleRate, preparedBlockSize))
            return false;

    // Each model runs at its trained rate, on blocks as large as its island hands
    // it. Prewarming settles its internal state, so the first block starts from rest.
    const int modelBlock = islands.empty() ? preparedBlockSize : islands.front().getMaxModelBlock();
    for (auto& model : models)
        model->ResetAndPrewarm(expectedSampleRate, modelBlock);
    settleSeconds = models.empty() ? 0.0 : models.front()->GetPrewarmSamples() / expectedSampleRate;
    ready = true;
    return true;
}

void NamProfile::reset() noexcept
{
    for (auto& island : islands)
        island.reset();
}

void NamProfile::process(int channel, const float* input, float* output, int numSamples) noexcept
{
    auto& model = *models[static_cast<size_t>(channel)];
    islands[static_cast<size_t>(channel)].process(input, output, numSamples,
        [&model](const float* in, float* out, int count)
        {
            // NAM takes non-const channel-pointer arrays; it does not write the input.
            NAM_SAMPLE* inPtr = const_cast<float*>(in);
            NAM_SAMPLE* outPtr = out;
            model.process(&inPtr, &outPtr, count);
        });
}
