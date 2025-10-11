/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "CustomKnob.h"
#include "PluginProcessor.h"
class PluginProcessor;

//==============================================================================
/**
*/
class Oscilloscope : public juce::Component, public juce::Timer
{
public:
    Oscilloscope(PluginProcessor& p) : processor(p)
    {
        setOpaque(true);
        setBufferedToImage(true);
        startTimerHz(30);
        displayBuffer.setSize(2, 512);
        displayBuffer.clear();
        cachedBuffer.setSize(2, 512);
        cachedBuffer.clear();
    }

    void resized() override
    {
        // Stop timer during resize to prevent concurrent buffer access
        stopTimer();

        // Restart after a short delay to let resize settle
        juce::Timer::callAfterDelay(50, [this]()
            {
                if (!isTimerRunning())
                    startTimerHz(30);
            });
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        if (bounds.getWidth() < 2 || bounds.getHeight() < 2)
        {
            g.fillAll(juce::Colours::black);
            return;
        }

        // Darker background with subtle gradient
        juce::ColourGradient bgGradient(
            juce::Colour(0xff0a0a0a), 0, 0,
            juce::Colour(0xff1a1a1a), 0, bounds.getHeight(),
            false);
        g.setGradientFill(bgGradient);
        g.fillAll();

        // Draw grid with better styling
        g.setColour(juce::Colours::grey.withAlpha(0.15f));

        // Vertical grid lines
        const int numVerticalLines = 8;
        for (int i = 1; i < numVerticalLines; ++i)
        {
            float x = bounds.getWidth() * i / (float)numVerticalLines;
            g.drawLine(x, 0, x, bounds.getHeight(), 1.0f);
        }

        // Horizontal grid lines
        for (int i = 1; i < 4; ++i)
        {
            float y = bounds.getHeight() * i / 4.0f;
            g.drawLine(0, y, bounds.getWidth(), y, 1.0f);
        }

        // Emphasized center line
        g.setColour(juce::Colours::grey.withAlpha(0.4f));
        g.drawLine(0, bounds.getHeight() / 2, bounds.getWidth(), bounds.getHeight() / 2, 1.5f);

        // Lock buffer while drawing
        const juce::ScopedLock sl(bufferLock);

        // Draw waveforms with glow effect
        drawChannelWithGlow(g, 0, juce::Colour(0xff00d4ff)); // Cyan with glow

        if (cachedBuffer.getNumChannels() > 1)
        {
            drawChannelWithGlow(g, 1, juce::Colour(0xffffbb00)); // Yellow/orange with glow
        }

        // Draw border
        g.setColour(juce::Colours::grey.withAlpha(0.3f));
        g.drawRect(bounds, 1.0f);
    }

    void timerCallback() override
    {
        if (getWidth() > 0 && getHeight() > 0)
        {
            const juce::ScopedLock sl(bufferLock);
            processor.fillScopeBuffer(cachedBuffer);
            repaint();
        }
    }

private:
    PluginProcessor& processor;
    juce::AudioBuffer<float> displayBuffer;  // Not used anymore
    juce::AudioBuffer<float> cachedBuffer;   // Use this for drawing
    juce::CriticalSection bufferLock;

    void drawChannelWithGlow(juce::Graphics& g, int channel, juce::Colour colour)
    {
        auto bounds = getLocalBounds().toFloat();
        const float height = bounds.getHeight();
        const float width = bounds.getWidth();
        const int numSamples = cachedBuffer.getNumSamples();

        if (numSamples < 2 || width < 2 || height < 2) return;
        if (channel >= cachedBuffer.getNumChannels()) return;

        juce::Path waveformPath;
        const int numPoints = juce::jmin(512, numSamples);
        if (numPoints < 2) return;

        bool pathStarted = false;

        for (int i = 0; i < numPoints; ++i)
        {
            const float bufferPos = (float)i * (float)(numSamples - 1) / (float)(numPoints - 1);
            const int bufferIndex = juce::jlimit(0, numSamples - 1, (int)bufferPos);
            const float fraction = bufferPos - bufferIndex;

            float sample;
            if (bufferIndex < numSamples - 1 && fraction > 0.0f)
            {
                const float sample1 = cachedBuffer.getSample(channel, bufferIndex);
                const float sample2 = cachedBuffer.getSample(channel, bufferIndex + 1);
                sample = sample1 + fraction * (sample2 - sample1);
            }
            else
            {
                sample = cachedBuffer.getSample(channel, bufferIndex);
            }

            if (!std::isfinite(sample))
                continue;

            sample = juce::jlimit(-1.0f, 1.0f, sample);

            const float xPos = (float)i / (float)(numPoints - 1);
            const float x = bounds.getX() + xPos * width;
            const float y = bounds.getY() + (0.5f - sample * 0.45f) * height;

            if (!std::isfinite(x) || !std::isfinite(y))
                continue;

            const float clampedX = juce::jlimit(bounds.getX(), bounds.getRight(), x);
            const float clampedY = juce::jlimit(bounds.getY(), bounds.getBottom(), y);

            if (!pathStarted)
            {
                waveformPath.startNewSubPath(clampedX, clampedY);
                pathStarted = true;
            }
            else
            {
                waveformPath.lineTo(clampedX, clampedY);
            }
        }

        if (pathStarted)
        {
            // Draw glow effect (outer shadow)
            g.setColour(colour.withAlpha(0.15f));
            g.strokePath(waveformPath, juce::PathStrokeType(6.0f));

            g.setColour(colour.withAlpha(0.3f));
            g.strokePath(waveformPath, juce::PathStrokeType(4.0f));

            // Draw main waveform
            g.setColour(colour.withAlpha(channel == 0 ? 0.9f : 0.6f));
            g.strokePath(waveformPath, juce::PathStrokeType(2.0f));
        }
    }
};

class PluginEditor : public juce::AudioProcessorEditor
{
public:
    explicit PluginEditor(PluginProcessor&);
    ~PluginEditor() override = default;

    //==============================================================================
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    PluginProcessor& audioProcessor;
    Oscilloscope oscilloscope;

    // UI Components
    CustomKnob inputGainSlider, distortionAmountSlider, outputGainSlider, highPassFreqSlider;
    juce::Label inputGainLabel, distortionAmountLabel, outputGainLabel, highPassFreqLabel;

    // Parameter attachments
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> inputGainAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> distortionAmountAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outputGainAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> highPassFreqAttachment;

    // Helper methods
    void setupSlider(CustomKnob& slider,
        juce::Label& label,
        const juce::String& text,
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>& attachment,
        const juce::String& paramID);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditor)
};