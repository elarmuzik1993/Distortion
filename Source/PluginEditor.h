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
        startTimerHz(DSPConstants::SCOPE_REFRESH_RATE_HZ);
        cachedBuffer.setSize(2, DSPConstants::SCOPE_DISPLAY_POINTS);
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
                    startTimerHz(DSPConstants::SCOPE_REFRESH_RATE_HZ);
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

        // Draw waveforms with neon red glow effect
        drawChannelWithGlow(g, 0, juce::Colour(0xffFF0044)); // Neon red with glow

        if (cachedBuffer.getNumChannels() > 1)
        {
            drawChannelWithGlow(g, 1, juce::Colour(0xffFF3366)); // Light neon red with glow
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
        const int numPoints = juce::jmin(DSPConstants::SCOPE_DISPLAY_POINTS, numSamples);
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

// Gain Reduction Meter Component
class GainReductionMeter : public juce::Component, public juce::Timer
{
public:
    GainReductionMeter(PluginProcessor& p) : processor(p)
    {
        startTimerHz(DSPConstants::METER_REFRESH_RATE_HZ);
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        // Background
        g.setColour(juce::Colours::black);
        g.fillRoundedRectangle(bounds, 4.0f);

        // Border
        g.setColour(juce::Colours::grey.withAlpha(0.5f));
        g.drawRoundedRectangle(bounds, 4.0f, 1.5f);

        // Get current gain reduction value
        const float grDB = currentGainReduction;

        if (grDB > 0.01f)  // Only draw if there's meaningful gain reduction
        {
            // Map gain reduction to meter height
            const float maxDB = DSPConstants::METER_MAX_DB;
            const float clampedDB = juce::jlimit(0.0f, maxDB, grDB);
            const float normalizedLevel = clampedDB / maxDB;

            // Calculate meter bar height
            const float meterHeight = bounds.getHeight() * normalizedLevel;
            const float meterY = bounds.getBottom() - meterHeight;

            // Create gradient from green -> yellow -> red
            juce::ColourGradient gradient(
                juce::Colour(0xff00ff00),  // Green at bottom
                bounds.getCentreX(), bounds.getBottom(),
                juce::Colour(0xffff0000),  // Red at top
                bounds.getCentreX(), bounds.getY(),
                false);
            gradient.addColour(0.3, juce::Colour(0xffffff00));  // Yellow in middle

            g.setGradientFill(gradient);
            g.fillRoundedRectangle(bounds.getX() + 2, meterY, bounds.getWidth() - 4, meterHeight, 2.0f);

            // Draw gain reduction value as text
            g.setColour(juce::Colours::white);
            g.setFont(11.0f);
            juce::String text = juce::String(grDB, 1) + " dB";
            g.drawText(text, bounds.reduced(2), juce::Justification::centredTop, false);
        }
        else
        {
            // No compression - show "0 dB"
            g.setColour(juce::Colours::grey);
            g.setFont(11.0f);
            g.drawText("0 dB", bounds, juce::Justification::centred, false);
        }
    }

    void timerCallback() override
    {
        // Read gain reduction from processor
        const float newGR = processor.currentGainReductionDB.load(std::memory_order_relaxed);

        // Smooth the value for visual stability
        currentGainReduction = currentGainReduction * DSPConstants::METER_SMOOTHING +
                              newGR * (1.0f - DSPConstants::METER_SMOOTHING);

        repaint();
    }

private:
    PluginProcessor& processor;
    float currentGainReduction = 0.0f;
};

// Lock Icon Component
class LockIcon : public juce::Component
{
public:
    LockIcon() { setSize(16, 16); }

    void setLocked(bool shouldBeLocked)
    {
        isLocked = shouldBeLocked;
        repaint();
    }

    bool getLocked() const { return isLocked; }

    void paint(juce::Graphics& g) override
    {
        if (!isLocked) return;  // Don't draw if unlocked

        auto bounds = getLocalBounds().toFloat().reduced(2.0f);

        // Draw neon red lock icon
        g.setColour(juce::Colour(0xFF, 0x00, 0x44));  // Neon red

        // Lock body (rectangle)
        auto body = bounds.removeFromBottom(bounds.getHeight() * 0.6f);
        g.fillRoundedRectangle(body, 2.0f);

        // Lock shackle (arc)
        auto shackle = bounds;
        g.drawRoundedRectangle(shackle.reduced(2.0f, 0.0f), shackle.getWidth() * 0.3f, 2.0f);
    }

private:
    bool isLocked = false;
};

// Custom LookAndFeel for solid black checkbox with green tick
class CheckboxLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawTickBox(juce::Graphics& g, juce::Component& component,
        float x, float y, float w, float h,
        bool ticked, bool isEnabled, bool isMouseOverButton, bool isButtonDown) override
    {
        juce::ignoreUnused(component, isEnabled, isMouseOverButton, isButtonDown);

        auto bounds = juce::Rectangle<float>(x, y, w, h).reduced(2.0f);

        // Draw solid black box
        g.setColour(juce::Colours::black);
        g.fillRoundedRectangle(bounds, 2.0f);

        // Draw white border for visibility
        g.setColour(juce::Colours::white);
        g.drawRoundedRectangle(bounds, 2.0f, 1.5f);

        // Draw bright green tick if checked
        if (ticked)
        {
            g.setColour(juce::Colours::lime);
            auto tick = bounds.reduced(w * 0.2f);
            g.fillRoundedRectangle(tick, 1.0f);
        }
    }
};

// Collapsible Tab Header for compression section
class CollapsibleTabHeader : public juce::Component
{
public:
    CollapsibleTabHeader(const juce::String& labelText)
    {
        label.setText(labelText, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centredRight);
        label.setColour(juce::Label::textColourId, juce::Colour(0xFF, 0x00, 0x44)); // Neon red
        label.setFont(juce::Font(14.0f, juce::Font::bold));
        label.setInterceptsMouseClicks(false, false); // Let parent handle clicks
        addAndMakeVisible(label);
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        // Dark background
        g.setColour(juce::Colour(0xff1a1a1a));
        g.fillRoundedRectangle(bounds, 4.0f);

        // Neon red border (subtle)
        g.setColour(juce::Colour(0xFF, 0x00, 0x44).withAlpha(0.5f));
        g.drawRoundedRectangle(bounds.reduced(0.5f), 4.0f, 1.5f);

        // Draw chevron indicator on right side
        drawChevron(g, bounds.removeFromRight(25).reduced(5));
    }

    void mouseDown(const juce::MouseEvent&) override
    {
        isExpanded = !isExpanded;
        repaint();

        if (onToggle)
            onToggle(isExpanded);
    }

    void setExpanded(bool expanded)
    {
        isExpanded = expanded;
        repaint();
    }

    bool getExpanded() const { return isExpanded; }

    std::function<void(bool)> onToggle; // Callback for state change

private:
    juce::Label label;
    bool isExpanded = true; // Default: expanded

    void drawChevron(juce::Graphics& g, juce::Rectangle<float> bounds)
    {
        juce::Path chevron;
        auto centre = bounds.getCentre();

        if (isExpanded)
        {
            // V shape (down chevron) - expanded state
            chevron.startNewSubPath(centre.x - 6, centre.y - 3);
            chevron.lineTo(centre.x, centre.y + 3);
            chevron.lineTo(centre.x + 6, centre.y - 3);
        }
        else
        {
            // > shape (right chevron) - collapsed state
            chevron.startNewSubPath(centre.x - 3, centre.y - 6);
            chevron.lineTo(centre.x + 3, centre.y);
            chevron.lineTo(centre.x - 3, centre.y + 6);
        }

        g.setColour(juce::Colour(0xFF, 0x00, 0x44)); // Neon red
        g.strokePath(chevron, juce::PathStrokeType(2.0f));
    }

    void resized() override
    {
        auto bounds = getLocalBounds();
        // Label takes all space except chevron area
        label.setBounds(bounds.removeFromLeft(bounds.getWidth() - 30));
    }
};

// Invisible XY Morph Pad overlay for oscilloscope
// Uses timer-based smoothing to prevent zipper noise during fast mouse movements
class XYMorphPad : public juce::Component, public juce::Timer
{
public:
    XYMorphPad()
    {
        setInterceptsMouseClicks(true, false);
        setOpaque(false);
        startTimerHz(30);  // 30Hz update rate for smooth parameter changes
    }

    ~XYMorphPad() override
    {
        stopTimer();
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (e.mods.isLeftButtonDown())
        {
            isDragging = true;
            updateTargetPosition(e);
            // Snap to target immediately on initial click (no lag)
            currentX = targetX;
            currentY = targetY;
            if (onPositionChanged)
                onPositionChanged(currentX, currentY);
        }
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (isDragging)
            updateTargetPosition(e);  // Only updates target, timer does smoothing
    }

    void mouseUp(const juce::MouseEvent&) override
    {
        isDragging = false;
    }

    void timerCallback() override
    {
        if (isDragging && onPositionChanged)
        {
            // Smooth interpolation toward target position
            constexpr float smoothing = 0.3f;  // 0-1, higher = faster response
            currentX += (targetX - currentX) * smoothing;
            currentY += (targetY - currentY) * smoothing;

            onPositionChanged(currentX, currentY);
        }
    }

    // Callback when position changes
    std::function<void(float x, float y)> onPositionChanged;

private:
    bool isDragging = false;
    float targetX = 0.0f, targetY = 0.0f;    // Where mouse is pointing
    float currentX = 0.0f, currentY = 0.0f;  // Smoothed output position

    void updateTargetPosition(const juce::MouseEvent& e)
    {
        auto bounds = getLocalBounds().toFloat();
        if (bounds.getWidth() < 1 || bounds.getHeight() < 1)
            return;

        targetX = juce::jlimit(0.0f, 1.0f, e.position.x / bounds.getWidth());
        targetY = juce::jlimit(0.0f, 1.0f, 1.0f - e.position.y / bounds.getHeight()); // Invert Y
    }
};

// Custom LookAndFeel for neon red ComboBox styling
class ComboBoxLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ComboBoxLookAndFeel()
    {
        // Neon red color scheme
        setColour(juce::ComboBox::backgroundColourId, juce::Colours::black);
        setColour(juce::ComboBox::textColourId, juce::Colour(0xFF, 0x00, 0x44));  // Neon red
        setColour(juce::ComboBox::outlineColourId, juce::Colour(0xFF, 0x00, 0x44));  // Neon red border
        setColour(juce::ComboBox::buttonColourId, juce::Colours::black);
        setColour(juce::ComboBox::arrowColourId, juce::Colours::transparentBlack);  // Hide arrow

        // Popup menu colors
        setColour(juce::PopupMenu::backgroundColourId, juce::Colours::black);
        setColour(juce::PopupMenu::textColourId, juce::Colour(0xFF, 0x00, 0x44));
        setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0xFF, 0x00, 0x44).withAlpha(0.3f));
        setColour(juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    }

    void drawComboBox(juce::Graphics& g, int width, int height, bool,
                      int, int, int, int, juce::ComboBox& box) override
    {
        auto cornerSize = box.findColour(juce::ComboBox::outlineColourId)
                              .contrasting().withAlpha(0.0f) != juce::Colours::transparentBlack ? 3.0f : 0.0f;
        juce::Rectangle<int> boxBounds(0, 0, width, height);

        g.setColour(box.findColour(juce::ComboBox::backgroundColourId));
        g.fillRoundedRectangle(boxBounds.toFloat(), cornerSize);

        g.setColour(box.findColour(juce::ComboBox::outlineColourId));
        g.drawRoundedRectangle(boxBounds.toFloat().reduced(0.5f, 0.5f), cornerSize, 1.0f);

        // Get initials based on selected item
        juce::String displayText = getInitials(box);

        // Draw initials text
        g.setColour(box.findColour(juce::ComboBox::textColourId));
        g.setFont(juce::Font(11.0f, juce::Font::bold));

        auto textBounds = boxBounds.reduced(3, 0);
        g.drawText(displayText, textBounds, juce::Justification::centred, true);
    }

    juce::String getInitials(juce::ComboBox& box)
    {
        int selectedId = box.getSelectedId();
        juce::String selectedText = box.getText();

        // Check if this is the clip type combo box (has items like "Brutal Fuzz")
        if (selectedText.contains("Fuzz") || selectedText.contains("Overdrive") ||
            selectedText.contains("Crusher") || selectedText.contains("Saturation") ||
            selectedText.contains("Transformer") || selectedText.contains("Clipper") ||
            selectedText.contains("Decimator"))
        {
            // Clip type initials
            if (selectedId == 1) return "BF";  // Brutal Fuzz
            if (selectedId == 2) return "TO";  // Tube Overdrive
            if (selectedId == 3) return "BC";  // Bit Crusher
            if (selectedId == 4) return "TS";  // Tape Saturation
            if (selectedId == 5) return "TF";  // Transformer
            if (selectedId == 6) return "DC";  // Diode Clipper
            if (selectedId == 7) return "DM";  // Decimator
        }
        // Check if this is LFO waveform combo box
        else if (selectedText.contains("Sine") || selectedText.contains("Triangle") ||
                 selectedText.contains("Square") || selectedText.contains("Saw") ||
                 selectedText.contains("Random"))
        {
            // LFO waveform initials
            if (selectedId == 1) return "SI";  // Sine
            if (selectedId == 2) return "TR";  // Triangle
            if (selectedId == 3) return "SQ";  // Square
            if (selectedId == 4) return "SW";  // Saw
            if (selectedId == 5) return "RN";  // Random
        }
        // For preset selector and other combo boxes, return full text
        else
        {
            return selectedText;
        }

        // Default fallback
        return selectedText;
    }

    juce::Label* createComboBoxTextBox(juce::ComboBox&) override
    {
        auto* label = new juce::Label();
        label->setJustificationType(juce::Justification::centred);
        label->setInterceptsMouseClicks(false, false);
        label->setColour(juce::Label::textColourId, juce::Colours::transparentBlack);
        label->setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
        label->setColour(juce::Label::outlineColourId, juce::Colours::transparentBlack);
        label->setBounds(0, 0, 0, 0);  // Make it invisible
        return label;
    }

    void positionComboBoxText(juce::ComboBox&, juce::Label& label) override
    {
        label.setBounds(0, 0, 0, 0);  // Keep label hidden
    }
};

class PluginEditor : public juce::AudioProcessorEditor
{
public:
    explicit PluginEditor(PluginProcessor&);
    ~PluginEditor() override;

    //==============================================================================
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseUp(const juce::MouseEvent& e) override;

private:
    
    PluginProcessor& audioProcessor;
    Oscilloscope oscilloscope;
    XYMorphPad xyMorphPad;  // Invisible XY pad overlay on oscilloscope
    GainReductionMeter gainReductionMeter;
    CheckboxLookAndFeel checkboxLookAndFeel;
    ComboBoxLookAndFeel comboBoxLookAndFeel;  // Neon red styling for dropdowns
    juce::Label titleLabel;  // Title text "MONOLIT BEATZ"
    juce::Label versionLabel;  // Build version display at bottom left

    // UI Components
    CustomKnob inputGainSlider, distortionAmountSlider, outputGainSlider, highPassFreqSlider, distMixSlider, toneSlider;
    juce::Label inputGainLabel, distortionAmountLabel, outputGainLabel, highPassFreqLabel, distMixLabel, toneLabel;
    
    CustomKnob lfoRateSlider, lfoDepthSlider;
    juce::Label lfoRateLabel, lfoDepthLabel;
    juce::ComboBox lfoWaveformComboBox;
    juce::Label lfoWaveformLabel;

    CustomKnob waveshaperSlider;
    juce::Label waveshaperLabel;

    CustomKnob compPeakReductionSlider, compMakeupGainSlider;
    juce::Label compPeakReductionLabel, compMakeupGainLabel;
    juce::ComboBox compRatioComboBox;
    juce::Label compRatioLabel;
    juce::ToggleButton compEnableToggle;

    // Collapsible compression section
    bool isCompressionExpanded = true;  // Default: expanded
    CollapsibleTabHeader compressionTabHeader{"COMPRESSION"};

    CustomKnob compWetDrySlider, compCrossoverSlider;
    juce::Label compWetDryLabel, compCrossoverLabel;
    juce::ToggleButton bandSplitToggle;
    juce::Label bandSplitLabel;
    juce::ToggleButton cleanModeToggle;
    juce::Label cleanModeLabel;

    juce::ComboBox clipTypeComboBox;
    juce::Label clipTypeLabel;

    // Custom button class for randomize with right-click menu
    class RandomizeButton : public juce::TextButton
    {
    public:
        std::function<void()> onRightClick;

        void mouseUp(const juce::MouseEvent& e) override
        {
            if (e.mods.isPopupMenu() && onRightClick)
                onRightClick();
            else
                juce::TextButton::mouseUp(e);
        }
    };

    RandomizeButton randomizeButton;

    // Preset selector components
    juce::ComboBox presetSelector;
    juce::TextButton savePresetButton;
    juce::TextButton deletePresetButton;
    juce::Label presetLabel;

    // Lock icons for each parameter
    LockIcon inputGainLock, outputGainLock, distortionAmountLock, highPassFreqLock;
    LockIcon distMixLock, lfoRateLock, lfoDepthLock;
    LockIcon compPeakReductionLock, compMakeupGainLock, compWetDryLock, compCrossoverLock;
    LockIcon bandSplitLock, clipTypeLock, compRatioLock, compEnableLock, cleanModeLock;

    // Lock toggles for randomization
    std::map<juce::String, bool> parameterLocks;

    // Parameter attachments
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> inputGainAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> distortionAmountAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outputGainAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> highPassFreqAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> distMixAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> toneAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bandSplitAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> cleanModeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> clipTypeAttachment;
    juce::Image backgroundImage;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lfoRateAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lfoDepthAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> lfoWaveformAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> waveshaperAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> compPeakReductionAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> compMakeupGainAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> compRatioAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> compEnableAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> compWetDryAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> compCrossoverAttachment;
    // Helper methods
    void setupSlider(CustomKnob& slider,
        juce::Label& label,
        const juce::String& text,
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>& attachment,
        const juce::String& paramID);

    void randomizeAllParameters();
    bool isParameterLocked(const juce::String& paramID) const;
    void toggleParameterLock(const juce::String& paramID);
    void updateLockIcons();
    void setupKnobRightClick(juce::Component& component, const juce::String& paramID);
    void updateCompressionVisibility();
    void morphDistortionParameters(float x, float y);  // XY Morph Pad callback

    // Preset management methods
    void savePreset(const juce::String& presetName);
    void loadPreset(const juce::String& presetName);
    void loadFactoryPreset(const juce::String& presetName);
    void deletePreset(const juce::String& presetName);
    void refreshPresetList();
    juce::File getPresetDirectory();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditor)
};