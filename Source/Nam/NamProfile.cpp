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
                                             double sampleRate, int maxBlockSize,
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

        profile->prepare(sampleRate, maxBlockSize);
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

void NamProfile::prepare(double sampleRate, int maxBlockSize)
{
    preparedSampleRate = sampleRate;
    preparedBlockSize = juce::jmax(1, maxBlockSize);
    // Prewarming settles each model's internal state, so the first block
    // processed afterwards starts from rest instead of clicking.
    for (auto& model : models)
        model->ResetAndPrewarm(preparedSampleRate, preparedBlockSize);
}

void NamProfile::process(int channel, const float* input, float* output, int numSamples) noexcept
{
    auto& model = *models[static_cast<size_t>(channel)];
    for (int done = 0; done < numSamples;)
    {
        const int count = juce::jmin(preparedBlockSize, numSamples - done);
        // NAM takes non-const channel-pointer arrays; it does not write the input.
        NAM_SAMPLE* in = const_cast<float*>(input + done);
        NAM_SAMPLE* out = output + done;
        model.process(&in, &out, count);
        done += count;
    }
}
