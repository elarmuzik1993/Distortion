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
        startTimerHz(30);  // 30 FPS is enough for smooth display
        displayBuffer.setSize(2, 512);  // Smaller display buffer
        displayBuffer.clear();
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colours::black);

        // Draw grid
        g.setColour(juce::Colours::grey.withAlpha(0.3f));
        auto bounds = getLocalBounds().toFloat();

        // Horizontal center line
        g.drawLine(0, bounds.getHeight() / 2, bounds.getWidth(), bounds.getHeight() / 2);

        // Additional grid lines
        for (int i = 1; i < 4; ++i)
        {
            float y = bounds.getHeight() * i / 4.0f;
            g.drawLine(0, y, bounds.getWidth(), y, 0.5f);
        }

        // Get latest audio data
        processor.fillScopeBuffer(displayBuffer);

        // Draw waveforms
        g.setColour(juce::Colours::cyan.withAlpha(0.8f));
        drawChannel(g, 0);  // Left channel

        if (displayBuffer.getNumChannels() > 1)
        {
            g.setColour(juce::Colours::yellow.withAlpha(0.8f));
            drawChannel(g, 1);  // Right channel
        }
    }

    void timerCallback() override
    {
        repaint();
    }

private:
    PluginProcessor& processor;
    juce::AudioBuffer<float> displayBuffer;

    void drawChannel(juce::Graphics& g, int channel)
    {
        auto bounds = getLocalBounds().toFloat();
        const float height = bounds.getHeight();
        const float width = bounds.getWidth();
        const int numSamples = displayBuffer.getNumSamples();

        if (numSamples < 2) return;  // Need at least 2 samples to draw

        juce::Path waveformPath;
        bool pathStarted = false;

        for (int i = 0; i < numSamples; ++i)
        {
            const float sample = juce::jlimit(-1.0f, 1.0f,
                displayBuffer.getSample(channel, i));
            const float x = bounds.getX() + (float)i / (float)(numSamples - 1) * width;
            const float y = bounds.getY() + (0.5f - sample * 0.4f) * height;  // 0.4f for headroom

            if (!pathStarted)
            {
                waveformPath.startNewSubPath(x, y);
                pathStarted = true;
            }
            else
            {
                waveformPath.lineTo(x, y);
            }
        }

        g.strokePath(waveformPath, juce::PathStrokeType(2.0f));
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