#pragma once

#include <JuceHeader.h>
#include <memory>
#include <vector>

namespace nam { class DSP; }

// A loaded Neural Amp Modeler (.nam) profile: one mono model instance per
// channel, prepared for one sample rate and maximum block size. NAM's own
// headers (and Eigen's) stay in NamProfile.cpp so the rest of the plugin
// compiles without them.
//
// Threading: load() and prepare() allocate, so they run off the audio thread.
// process() is the only audio-thread entry point and does not allocate.
class NamProfile
{
public:
    // Loudness every profile is normalised to when the file reports its own,
    // matching the NAM plugin's convention.
    static constexpr double targetLoudnessDb = -18.0;

    // Loads a .nam file and prepares numChannels model instances. Returns
    // nullptr and fills errorOut if the file can't be read or isn't a
    // single-input, single-output model.
    static std::unique_ptr<NamProfile> load(const juce::File& file, int numChannels,
                                            double sampleRate, int maxBlockSize,
                                            juce::String& errorOut);
    // A profile with no models: handing it to the audio thread clears the
    // active profile through the same path a new one arrives by.
    static std::unique_ptr<NamProfile> makeEmpty() { return std::unique_ptr<NamProfile>(new NamProfile()); }
    ~NamProfile();

    // Prepares every model for a new rate or block size. Allocates.
    void prepare(double sampleRate, int maxBlockSize);

    // Runs one channel's model over numSamples, in chunks of the prepared block
    // size. input and output may not alias.
    void process(int channel, const float* input, float* output, int numSamples) noexcept;

    int getNumChannels() const noexcept { return static_cast<int>(models.size()); }
    double getPreparedSampleRate() const noexcept { return preparedSampleRate; }
    int getPreparedBlockSize() const noexcept { return preparedBlockSize; }
    // The rate the profile was trained at; 48 kHz when the file doesn't say.
    double getExpectedSampleRate() const noexcept { return expectedSampleRate; }
    // Linear gain that brings the profile to targetLoudnessDb (1 if unknown).
    float getOutputGain() const noexcept { return outputGain; }
    const juce::File& getFile() const noexcept { return file; }
    juce::String getName() const { return file.getFileNameWithoutExtension(); }

private:
    NamProfile();   // defined where nam::DSP is complete

    juce::File file;
    std::vector<std::unique_ptr<nam::DSP>> models;
    double preparedSampleRate = 0.0;
    int preparedBlockSize = 0;
    double expectedSampleRate = 48000.0;
    float outputGain = 1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NamProfile)
};
