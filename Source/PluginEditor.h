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
        startTimerHz(60);  // 60 FPS
        displayBuffer.setSize(2, 512);  // Buffer for display
    }

    void paint(juce::Graphics& g) override
    {
        // Fill background
        g.fillAll(juce::Colours::black);

        // Draw grid
        g.setColour(juce::Colours::grey.withAlpha(0.3f));
        auto bounds = getLocalBounds();

        // Vertical center line
        g.drawLine(0, bounds.getHeight() / 2, bounds.getWidth(), bounds.getHeight() / 2);

        // Horizontal lines
        for (int i = 1; i < 4; ++i) {
            float y = bounds.getHeight() * i / 4.0f;
            g.drawLine(0, y, bounds.getWidth(), y);
        }

        // Get latest audio data
        processor.fillScopeBuffer(displayBuffer);

        // Draw waveform
        g.setColour(juce::Colours::cyan);
        drawChannel(g, 0);  // Left channel

        g.setColour(juce::Colours::yellow);
        drawChannel(g, 1);  // Right channel
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
        auto bounds = getLocalBounds();
        const float height = (float)bounds.getHeight();
        const float width = (float)bounds.getWidth();
        const int numSamples = displayBuffer.getNumSamples();

        juce::Path waveformPath;
        bool started = false;

        for (int i = 0; i < numSamples; ++i)
        {
            const float sample = displayBuffer.getSample(channel, i);
            const float x = (float)i / (float)numSamples * width;
            const float y = (1.0f - sample) * height * 0.5f;  // Convert to screen coordinates

            if (!started)
            {
                waveformPath.startNewSubPath(x, y);
                started = true;
            }
            else
            {
                waveformPath.lineTo(x, y);
            }
        }

        g.strokePath(waveformPath, juce::PathStrokeType(1.5f));
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