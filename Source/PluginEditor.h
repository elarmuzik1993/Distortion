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

        g.fillAll(juce::Colours::black);

        g.setColour(juce::Colours::grey.withAlpha(0.3f));
        g.drawLine(0, bounds.getHeight() / 2, bounds.getWidth(), bounds.getHeight() / 2);

        for (int i = 1; i < 4; ++i)
        {
            float y = bounds.getHeight() * i / 4.0f;
            g.drawLine(0, y, bounds.getWidth(), y, 0.5f);
        }

        // Lock buffer while drawing
        const juce::ScopedLock sl(bufferLock);

        g.setColour(juce::Colours::cyan.withAlpha(0.85f));
        drawChannel(g, 0);

        if (cachedBuffer.getNumChannels() > 1)
        {
            g.setColour(juce::Colours::yellow.withAlpha(0.4f));
            drawChannel(g, 1);
        }
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

    void drawChannel(juce::Graphics& g, int channel)
    {
        auto bounds = getLocalBounds().toFloat();
        const float height = bounds.getHeight();
        const float width = bounds.getWidth();
        const int numSamples = cachedBuffer.getNumSamples();  // CHANGED: use cachedBuffer

        if (numSamples < 2 || width < 2 || height < 2) return;
        if (channel >= cachedBuffer.getNumChannels()) return;

        juce::Path waveformPath;

        // Use fixed number of points to prevent resize artifacts
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
            const float y = bounds.getY() + (0.5f - sample * 0.4f) * height;

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
    CustomKnob inputGainSlider, distortionAmountSlider, outputGainSlider;
    juce::Label inputGainLabel, distortionAmountLabel, outputGainLabel;

    // Parameter attachments
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> inputGainAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> distortionAmountAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outputGainAttachment;

    // Helper methods
    void setupSlider(CustomKnob& slider,
        juce::Label& label,
        const juce::String& text,
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>& attachment,
        const juce::String& paramID);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditor)
};