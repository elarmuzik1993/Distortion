#pragma once

#if JUCE_DEBUG

#include <JuceHeader.h>
#include <cmath>

namespace TestUtilities
{
    //==============================================================================
    // Parameter Helper Functions
    //==============================================================================

    /** Set parameter value for tests (bypasses host notification) */
    inline void setParameter(juce::AudioProcessorValueTreeState& apvts,
                            const juce::String& paramID,
                            float value)
    {
        // CRITICAL: Use getParameter() to directly access the RangedAudioParameter,
        // then setValueNotifyingHost() to update both the parameter AND the atomic pointer.
        // The old method using getParameterAsValue() had a race condition where the
        // atomic pointer wouldn't be immediately updated, causing NaN in tests.
        auto* param = apvts.getParameter(paramID);
        if (param != nullptr)
        {
            // Convert to normalized value (0-1) for setValueNotifyingHost
            const auto normalizedValue = param->convertTo0to1(value);
            param->setValueNotifyingHost(normalizedValue);
        }
    }

    //==============================================================================
    // Audio Buffer Generation
    //==============================================================================

    /** Generate a sine wave test signal */
    inline juce::AudioBuffer<float> generateSineWave(double frequency, double sampleRate,
                                                      int numSamples, float amplitude = 1.0f,
                                                      int numChannels = 2)
    {
        juce::AudioBuffer<float> buffer(numChannels, numSamples);
        const double phaseIncrement = juce::MathConstants<double>::twoPi * frequency / sampleRate;

        for (int channel = 0; channel < numChannels; ++channel)
        {
            auto* data = buffer.getWritePointer(channel);
            double phase = 0.0;

            for (int sample = 0; sample < numSamples; ++sample)
            {
                data[sample] = amplitude * static_cast<float>(std::sin(phase));
                phase += phaseIncrement;
                if (phase >= juce::MathConstants<double>::twoPi)
                    phase -= juce::MathConstants<double>::twoPi;
            }
        }

        return buffer;
    }

    /** Generate a silent buffer */
    inline juce::AudioBuffer<float> generateSilence(int numSamples, int numChannels = 2)
    {
        juce::AudioBuffer<float> buffer(numChannels, numSamples);
        buffer.clear();
        return buffer;
    }

    /** Generate an impulse (single sample spike) */
    inline juce::AudioBuffer<float> generateImpulse(int numSamples, int numChannels = 2,
                                                     int impulsePosition = 0, float amplitude = 1.0f)
    {
        juce::AudioBuffer<float> buffer(numChannels, numSamples);
        buffer.clear();

        if (impulsePosition >= 0 && impulsePosition < numSamples)
        {
            for (int channel = 0; channel < numChannels; ++channel)
                buffer.setSample(channel, impulsePosition, amplitude);
        }

        return buffer;
    }

    /** Generate a buffer with DC offset */
    inline juce::AudioBuffer<float> generateDCOffset(int numSamples, float offset, int numChannels = 2)
    {
        juce::AudioBuffer<float> buffer(numChannels, numSamples);

        for (int channel = 0; channel < numChannels; ++channel)
        {
            auto* data = buffer.getWritePointer(channel);
            for (int sample = 0; sample < numSamples; ++sample)
                data[sample] = offset;
        }

        return buffer;
    }

    /** Generate white noise */
    inline juce::AudioBuffer<float> generateWhiteNoise(int numSamples, float amplitude = 1.0f,
                                                        int numChannels = 2)
    {
        juce::AudioBuffer<float> buffer(numChannels, numSamples);
        juce::Random random;

        for (int channel = 0; channel < numChannels; ++channel)
        {
            auto* data = buffer.getWritePointer(channel);
            for (int sample = 0; sample < numSamples; ++sample)
                data[sample] = (random.nextFloat() * 2.0f - 1.0f) * amplitude;
        }

        return buffer;
    }

    //==============================================================================
    // Buffer Analysis
    //==============================================================================

    /** Calculate RMS level of a buffer */
    inline float calculateRMS(const juce::AudioBuffer<float>& buffer)
    {
        float sumSquares = 0.0f;
        int totalSamples = 0;

        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            const auto* data = buffer.getReadPointer(channel);
            for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            {
                sumSquares += data[sample] * data[sample];
                ++totalSamples;
            }
        }

        return totalSamples > 0 ? std::sqrt(sumSquares / static_cast<float>(totalSamples)) : 0.0f;
    }

    /** Calculate peak level of a buffer */
    inline float calculatePeak(const juce::AudioBuffer<float>& buffer)
    {
        float peak = 0.0f;

        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            const auto* data = buffer.getReadPointer(channel);
            for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
                peak = std::max(peak, std::abs(data[sample]));
        }

        return peak;
    }

    /** Check if buffer contains any NaN or Inf values */
    inline bool containsInvalidSamples(const juce::AudioBuffer<float>& buffer)
    {
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            const auto* data = buffer.getReadPointer(channel);
            for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            {
                if (std::isnan(data[sample]) || std::isinf(data[sample]))
                    return true;
            }
        }
        return false;
    }

    /** Check if buffer is effectively silent (below threshold) */
    inline bool isSilent(const juce::AudioBuffer<float>& buffer, float thresholdDB = -90.0f)
    {
        float threshold = juce::Decibels::decibelsToGain(thresholdDB);
        return calculatePeak(buffer) < threshold;
    }

    //==============================================================================
    // Buffer Comparison
    //==============================================================================

    /** Calculate maximum absolute difference between two buffers */
    inline float calculateMaxDifference(const juce::AudioBuffer<float>& a,
                                         const juce::AudioBuffer<float>& b)
    {
        jassert(a.getNumChannels() == b.getNumChannels());
        jassert(a.getNumSamples() == b.getNumSamples());

        float maxDiff = 0.0f;

        for (int channel = 0; channel < a.getNumChannels(); ++channel)
        {
            const auto* dataA = a.getReadPointer(channel);
            const auto* dataB = b.getReadPointer(channel);

            for (int sample = 0; sample < a.getNumSamples(); ++sample)
                maxDiff = std::max(maxDiff, std::abs(dataA[sample] - dataB[sample]));
        }

        return maxDiff;
    }

    /** Calculate correlation coefficient between two buffers */
    inline float calculateCorrelation(const juce::AudioBuffer<float>& a,
                                       const juce::AudioBuffer<float>& b)
    {
        jassert(a.getNumChannels() == b.getNumChannels());
        jassert(a.getNumSamples() == b.getNumSamples());

        float sumA = 0.0f, sumB = 0.0f, sumAB = 0.0f;
        float sumA2 = 0.0f, sumB2 = 0.0f;
        int n = 0;

        for (int channel = 0; channel < a.getNumChannels(); ++channel)
        {
            const auto* dataA = a.getReadPointer(channel);
            const auto* dataB = b.getReadPointer(channel);

            for (int sample = 0; sample < a.getNumSamples(); ++sample)
            {
                float va = dataA[sample];
                float vb = dataB[sample];
                sumA += va;
                sumB += vb;
                sumAB += va * vb;
                sumA2 += va * va;
                sumB2 += vb * vb;
                ++n;
            }
        }

        if (n == 0) return 0.0f;

        float meanA = sumA / n;
        float meanB = sumB / n;
        float varA = sumA2 / n - meanA * meanA;
        float varB = sumB2 / n - meanB * meanB;
        float covar = sumAB / n - meanA * meanB;

        if (varA <= 0.0f || varB <= 0.0f) return 0.0f;

        return covar / (std::sqrt(varA) * std::sqrt(varB));
    }

    /** Compare buffers with tolerance in dB */
    inline bool compareWithTolerance(const juce::AudioBuffer<float>& actual,
                                      const juce::AudioBuffer<float>& expected,
                                      float toleranceDB)
    {
        if (actual.getNumChannels() != expected.getNumChannels() ||
            actual.getNumSamples() != expected.getNumSamples())
            return false;

        float tolerance = juce::Decibels::decibelsToGain(toleranceDB);
        return calculateMaxDifference(actual, expected) < tolerance;
    }

    //==============================================================================
    // WAV File I/O for Golden Tests
    //==============================================================================

    /** Save buffer to WAV file */
    inline bool saveWavFile(const juce::AudioBuffer<float>& buffer,
                            const juce::File& file,
                            double sampleRate)
    {
        juce::WavAudioFormat wavFormat;
        std::unique_ptr<juce::AudioFormatWriter> writer;

        file.deleteFile();
        auto stream = file.createOutputStream();
        if (stream == nullptr)
            return false;

        writer.reset(wavFormat.createWriterFor(stream.release(),
                                                sampleRate,
                                                static_cast<unsigned int>(buffer.getNumChannels()),
                                                24,  // bits per sample
                                                {},
                                                0));
        if (writer == nullptr)
            return false;

        return writer->writeFromAudioSampleBuffer(buffer, 0, buffer.getNumSamples());
    }

    /** Load buffer from WAV file */
    inline juce::AudioBuffer<float> loadWavFile(const juce::File& file, double& sampleRate)
    {
        juce::AudioBuffer<float> buffer;
        juce::AudioFormatManager formatManager;
        formatManager.registerBasicFormats();

        std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));

        if (reader != nullptr)
        {
            sampleRate = reader->sampleRate;
            buffer.setSize(static_cast<int>(reader->numChannels),
                          static_cast<int>(reader->lengthInSamples));
            reader->read(&buffer, 0, static_cast<int>(reader->lengthInSamples), 0, true, true);
        }

        return buffer;
    }

    //==============================================================================
    // Test Tolerances
    //==============================================================================

    namespace Tolerances
    {
        constexpr float SILENCE_DB = -90.0f;      // Very strict for silence tests
        constexpr float BYPASS_DB = -80.0f;       // Strict for bypass mode
        constexpr float COMPRESSION_DB = -50.0f;  // Moderate for compression
        constexpr float DISTORTION_DB = -40.0f;   // Allow variation in distortion
        constexpr float FLOAT_EPSILON = 1e-6f;    // For exact float comparisons
    }

    //==============================================================================
    // Test Buffer Sizes
    //==============================================================================

    namespace BufferSizes
    {
        constexpr int TINY = 64;       // Quick sanity checks
        constexpr int SMALL = 256;     // Fast tests
        constexpr int MEDIUM = 512;    // Standard tests
        constexpr int LARGE = 2048;    // Thorough tests
        // Named STRESS, not HUGE: macOS's <math.h> defines HUGE as a macro
        // (#define HUGE MAXFLOAT), so `constexpr int HUGE` is rewritten by the
        // preprocessor into a syntax error before the namespace can protect it.
        // Windows and Linux do not define it, which is why this only ever broke
        // the macOS build - and only once build-macos actually ran.
        constexpr int STRESS = 8192;   // Stress tests
    }
}

#endif // JUCE_DEBUG
