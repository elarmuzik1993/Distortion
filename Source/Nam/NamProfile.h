#pragma once

#include <JuceHeader.h>
#include "ResamplingIsland.h"
#include <memory>
#include <vector>

namespace nam { class DSP; }

// A loaded Neural Amp Modeler (.nam) profile: one mono model per channel, each
// running at the rate it was trained at inside a ResamplingIsland, prepared for
// one host rate and maximum block size. NAM's own headers (and Eigen's) stay in
// NamProfile.cpp so the rest of the plugin compiles without them.
//
// Threading: load() and prepare() allocate, so they run off the audio thread.
// process() is the only audio-thread entry point and does not allocate.
class NamProfile
{
public:
    // Loudness every profile is normalised to when the file reports its own,
    // matching the NAM plugin's convention.
    static constexpr double targetLoudnessDb = -18.0;

    // Loads a .nam file and prepares numChannels models for the host rate. Returns
    // nullptr and fills errorOut if the file can't be read, isn't a single-input,
    // single-output model, or can't run at this host rate.
    static std::unique_ptr<NamProfile> load(const juce::File& file, int numChannels,
                                            double hostRate, int maxHostBlock,
                                            juce::String& errorOut);
    // "This profile can't run at 44.056 kHz"
    static juce::String unsupportedRateMessage(double hostRate);
    ~NamProfile();

    // Prepares every island and model for a new host rate or block size. Allocates.
    // Returns false when the island can't convert between this host rate and the
    // trained rate; the profile must not run until it is prepared again.
    bool prepare(double hostRate, int maxHostBlock);

    // Runs one channel's model over numSamples at the host rate, through its island.
    // input and output may not alias. The output is getLatencySamples() behind.
    void process(int channel, const float* input, float* output, int numSamples) noexcept;

    bool isPrepared() const noexcept { return ready; }

    int getNumChannels() const noexcept { return static_cast<int>(models.size()); }
    double getPreparedSampleRate() const noexcept { return preparedSampleRate; }
    int getPreparedBlockSize() const noexcept { return preparedBlockSize; }
    // The rate the profile was trained at; 48 kHz when the file doesn't say.
    double getExpectedSampleRate() const noexcept { return expectedSampleRate; }
    // The islands' round-trip delay in host samples: 0 at the trained rate.
    int getLatencySamples() const noexcept { return islands.empty() ? 0 : islands.front().getLatencySamples(); }
    // How long a model takes to settle on new input (its own prewarm length).
    double getSettleSeconds() const noexcept { return settleSeconds; }
    // Linear gain that brings the profile to targetLoudnessDb (1 if unknown).
    float getOutputGain() const noexcept { return outputGain; }
    const juce::File& getFile() const noexcept { return file; }
    juce::String getName() const { return file.getFileNameWithoutExtension(); }

private:
    NamProfile();   // defined where nam::DSP is complete

    juce::File file;
    std::vector<std::unique_ptr<nam::DSP>> models;
    std::vector<ResamplingIsland> islands;   // one per model
    double preparedSampleRate = 0.0;
    int preparedBlockSize = 0;
    double expectedSampleRate = 48000.0;
    double settleSeconds = 0.0;
    float outputGain = 1.0f;
    bool ready = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NamProfile)
};
