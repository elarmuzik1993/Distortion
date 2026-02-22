/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "CustomKnob.h"
#include "FontHelper.h"
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

    void setStereoMode(bool isStereo)
    {
        stereoMode = isStereo;
        repaint();
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
        if (stereoMode)
        {
            drawChannelWithGlow(g, 0, juce::Colour(0xffFF0044)); // Neon red with glow

            if (cachedBuffer.getNumChannels() > 1)
            {
                drawChannelWithGlow(g, 1, juce::Colour(0xffFF3366)); // Light neon red with glow
            }
        }
        else
        {
            drawMonoWithGlow(g, juce::Colour(0xffFF0044)); // Summed mono with channel-0 color
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
    bool stereoMode = true;

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

    void drawMonoWithGlow(juce::Graphics& g, juce::Colour colour)
    {
        auto bounds = getLocalBounds().toFloat();
        const float height = bounds.getHeight();
        const float width = bounds.getWidth();
        const int numSamples = cachedBuffer.getNumSamples();
        const int numChannels = cachedBuffer.getNumChannels();

        if (numSamples < 2 || width < 2 || height < 2 || numChannels < 1) return;

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
            if (numChannels >= 2)
            {
                float s0, s1;
                if (bufferIndex < numSamples - 1 && fraction > 0.0f)
                {
                    s0 = cachedBuffer.getSample(0, bufferIndex) + fraction * (cachedBuffer.getSample(0, bufferIndex + 1) - cachedBuffer.getSample(0, bufferIndex));
                    s1 = cachedBuffer.getSample(1, bufferIndex) + fraction * (cachedBuffer.getSample(1, bufferIndex + 1) - cachedBuffer.getSample(1, bufferIndex));
                }
                else
                {
                    s0 = cachedBuffer.getSample(0, bufferIndex);
                    s1 = cachedBuffer.getSample(1, bufferIndex);
                }
                sample = (s0 + s1) * 0.5f;
            }
            else
            {
                if (bufferIndex < numSamples - 1 && fraction > 0.0f)
                {
                    const float sample1 = cachedBuffer.getSample(0, bufferIndex);
                    const float sample2 = cachedBuffer.getSample(0, bufferIndex + 1);
                    sample = sample1 + fraction * (sample2 - sample1);
                }
                else
                {
                    sample = cachedBuffer.getSample(0, bufferIndex);
                }
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
            g.setColour(colour.withAlpha(0.15f));
            g.strokePath(waveformPath, juce::PathStrokeType(6.0f));

            g.setColour(colour.withAlpha(0.3f));
            g.strokePath(waveformPath, juce::PathStrokeType(4.0f));

            g.setColour(colour.withAlpha(0.9f));
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

// Phase Correlation Meter - horizontal bar with needle (-1 to +1)
class PhaseCorrelationMeter : public juce::Component, public juce::Timer
{
public:
    PhaseCorrelationMeter(PluginProcessor& p) : processor(p)
    {
        startTimerHz(30);
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        // Background
        g.setColour(juce::Colour(0xFF080808));
        g.fillRect(bounds);

        // Border
        g.setColour(juce::Colours::grey.withAlpha(0.3f));
        g.drawRect(bounds, 1.0f);

        // Labels
        g.setFont(juce::Font(8.0f));
        g.setColour(juce::Colour(0xFFFF4466));
        g.drawText("-1", bounds.withTrimmedLeft(6.0f).withWidth(16.0f), juce::Justification::centredLeft, false);
        g.setColour(juce::Colour(0xFF44FF88));
        g.drawText("+1", bounds.withTrimmedRight(6.0f).removeFromRight(16.0f), juce::Justification::centredRight, false);

        // Track
        float trackHeight = 5.0f;
        float trackY = bounds.getCentreY() - trackHeight / 2.0f;
        float trackLeft = bounds.getX() + 26.0f;
        float trackRight = bounds.getRight() - 26.0f;
        float trackWidth = trackRight - trackLeft;
        auto trackBounds = juce::Rectangle<float>(trackLeft, trackY, trackWidth, trackHeight);

        g.setColour(juce::Colour(0xFF111111));
        g.fillRoundedRectangle(trackBounds, 3.0f);

        // Tick marks at 0/25/50/75/100%
        g.setColour(juce::Colour(0xFF333333));
        for (int i = 0; i <= 4; ++i)
        {
            float tickX = trackLeft + trackWidth * (float)i / 4.0f;
            float tickH = (i == 2) ? 5.0f : 3.0f;
            g.drawLine(tickX, trackY - 1.0f, tickX, trackY + tickH + 1.0f, 1.0f);
        }

        // Needle position: -1.0 maps to left, +1.0 maps to right
        float normalizedPos = (smoothedCorrelation + 1.0f) / 2.0f;
        float needleX = trackLeft + normalizedPos * trackWidth;

        // Glow trail
        juce::ColourGradient trail(
            juce::Colour(0xFF44FF88).withAlpha(0.3f), needleX, trackY,
            juce::Colours::transparentBlack, needleX - 24.0f, trackY,
            false);
        g.setGradientFill(trail);
        g.fillRect(needleX - 24.0f, trackY, 24.0f, trackHeight);

        // Needle
        g.setColour(juce::Colour(0xFF44FF88));
        g.fillRoundedRectangle(needleX - 1.5f, trackY - 1.0f, 3.0f, trackHeight + 2.0f, 1.0f);
    }

    void timerCallback() override
    {
        float raw = processor.phaseCorrelation.load(std::memory_order_relaxed);
        smoothedCorrelation += (raw - smoothedCorrelation) * 0.3f;
        repaint();
    }

private:
    PluginProcessor& processor;
    float smoothedCorrelation = 1.0f;
};

// Logo Title Component - Displays branded logo image
class LogoTitle : public juce::Component
{
public:
    LogoTitle()
    {
        // Load logo image from binary data
        logoImage = juce::ImageCache::getFromMemory(
            BinaryData::Logo_Title_png,
            BinaryData::Logo_Title_pngSize
        );

        // Verify image loaded successfully
        if (!logoImage.isValid())
        {
            DBG("WARNING: Logo Title image failed to load!");
        }
    }

    void paint(juce::Graphics& g) override
    {
        if (!logoImage.isValid())
        {
            // Fallback: draw error text for debugging
            g.setColour(juce::Colours::red.withAlpha(0.5f));
            g.setFont(12.0f);
            g.drawText("Logo Missing", getLocalBounds(), juce::Justification::centred);
            return;
        }

        auto bounds = getLocalBounds().toFloat();

        // Scale down by 3x - use only 1/3 of the available space
        const float scaleFactor = 3.0f;
        auto scaledBounds = bounds.withSizeKeepingCentre(
            bounds.getWidth() / scaleFactor,
            bounds.getHeight() / scaleFactor
        );

        // Calculate scaling to fit within scaled bounds while maintaining aspect ratio
        const float imageAspect = (float)logoImage.getWidth() / (float)logoImage.getHeight();
        const float boundsAspect = scaledBounds.getWidth() / scaledBounds.getHeight();

        juce::Rectangle<float> targetBounds;

        if (imageAspect > boundsAspect)
        {
            // Image is wider - fit to width
            float scaledHeight = scaledBounds.getWidth() / imageAspect;
            float yOffset = (scaledBounds.getHeight() - scaledHeight) * 0.5f;
            targetBounds = juce::Rectangle<float>(
                scaledBounds.getX(),
                scaledBounds.getY() + yOffset,
                scaledBounds.getWidth(),
                scaledHeight
            );
        }
        else
        {
            // Image is taller - fit to height
            float scaledWidth = scaledBounds.getHeight() * imageAspect;
            float xOffset = (scaledBounds.getWidth() - scaledWidth) * 0.5f;
            targetBounds = juce::Rectangle<float>(
                scaledBounds.getX() + xOffset,
                scaledBounds.getY(),
                scaledWidth,
                scaledBounds.getHeight()
            );
        }

        // Draw logo with high quality interpolation
        g.setOpacity(1.0f);
        g.drawImage(logoImage, targetBounds,
                    juce::RectanglePlacement::centred | juce::RectanglePlacement::onlyReduceInSize);
    }

private:
    juce::Image logoImage;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LogoTitle)
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
        g.drawRoundedRectangle(bounds, 2.0f, 0.7f);

        // Draw neon red tick if checked
        if (ticked)
        {
            g.setColour(juce::Colour(0xFF, 0x00, 0x44));
            auto tick = bounds.reduced(w * 0.2f);
            g.fillRoundedRectangle(tick, 1.0f);
        }
    }
};

// Custom gear button that draws a gear icon programmatically
class GearButton : public juce::Button
{
public:
    GearButton() : juce::Button("Settings") {}

    void paintButton(juce::Graphics& g, bool isMouseOver, bool isButtonDown) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(2.0f);
        auto centre = bounds.getCentre();
        float outerR = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.45f;
        float innerR = outerR * 0.55f;
        float holeR = outerR * 0.28f;

        juce::Colour col = isButtonDown ? juce::Colours::white
                         : isMouseOver  ? juce::Colour(0xFFFF4466)
                                        : juce::Colour(0xFFFF0044);

        // Draw gear teeth
        const int numTeeth = 6;
        juce::Path gear;
        for (int i = 0; i < numTeeth * 2; ++i)
        {
            float angle = juce::MathConstants<float>::twoPi * i / (numTeeth * 2);
            float r = (i % 2 == 0) ? outerR : innerR;
            float x = centre.x + std::cos(angle) * r;
            float y = centre.y + std::sin(angle) * r;
            if (i == 0)
                gear.startNewSubPath(x, y);
            else
                gear.lineTo(x, y);
        }
        gear.closeSubPath();

        // Cut out center hole
        gear.addEllipse(centre.x - holeR, centre.y - holeR, holeR * 2, holeR * 2);
        gear.setUsingNonZeroWinding(false);

        g.setColour(col);
        g.fillPath(gear);
    }
};

// Settings state for UI-only settings persisted via XML
struct SettingsState
{
    bool oscilloscopeEnabled = true;
    bool oscilloscopeStereo = true;
    bool tooltipsEnabled = false;
    int windowScalePercent = 70;
    int oversamplingMode = 2; // 0=Off, 1=2x, 2=4x

    void saveToFile(const juce::File& file) const
    {
        juce::XmlElement xml("Settings");
        xml.setAttribute("oscilloscope", oscilloscopeEnabled);
        xml.setAttribute("oscilloscopeStereo", oscilloscopeStereo);
        xml.setAttribute("tooltips", tooltipsEnabled);
        xml.setAttribute("windowScale", windowScalePercent);
        xml.setAttribute("oversampling", oversamplingMode);
        xml.writeTo(file);
    }

    void loadFromFile(const juce::File& file)
    {
        if (!file.existsAsFile()) return;
        auto xml = juce::parseXML(file);
        if (xml == nullptr) return;
        oscilloscopeEnabled = xml->getBoolAttribute("oscilloscope", true);
        oscilloscopeStereo = xml->getBoolAttribute("oscilloscopeStereo", true);
        tooltipsEnabled = xml->getBoolAttribute("tooltips", false);
        windowScalePercent = xml->getIntAttribute("windowScale", 100);
        oversamplingMode = xml->getIntAttribute("oversampling", 2);
    }
};

// Pill-shaped toggle LookAndFeel for settings overlay
class PillToggleLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawTickBox(juce::Graphics& g, juce::Component&,
        float x, float y, float w, float h,
        bool ticked, bool, bool, bool) override
    {
        juce::ignoreUnused(w, h);
        const float pillW = 36.0f;
        const float pillH = 18.0f;
        const float pillX = x;
        const float pillY = y + (h - pillH) / 2.0f;
        const float knobDiameter = pillH - 4.0f;

        auto pillBounds = juce::Rectangle<float>(pillX, pillY, pillW, pillH);

        // Background
        g.setColour(ticked ? juce::Colour(0xFFFF2244).withAlpha(0.3f)
                           : juce::Colour(0xFF1A1A1A));
        g.fillRoundedRectangle(pillBounds, pillH / 2.0f);

        // Border
        g.setColour(ticked ? juce::Colour(0xFFFF2244) : juce::Colour(0xFF333333));
        g.drawRoundedRectangle(pillBounds, pillH / 2.0f, 1.0f);

        // Knob
        float knobX = ticked ? (pillX + pillW - knobDiameter - 3.0f)
                             : (pillX + 3.0f);
        float knobY = pillY + (pillH - knobDiameter) / 2.0f;
        g.setColour(ticked ? juce::Colour(0xFFFF2244) : juce::Colour(0xFF888888));
        g.fillEllipse(knobX, knobY, knobDiameter, knobDiameter);
    }
};

// Settings overlay modal panel
class SettingsOverlay : public juce::Component
{
public:
    SettingsOverlay(juce::AudioProcessorValueTreeState& apvts, SettingsState& state)
        : settingsState(state)
    {
        setInterceptsMouseClicks(true, true);

        // Anti-Alias toggle (attached to waveshaperClean parameter)
        addAndMakeVisible(antiAliasToggle);
        antiAliasToggle.setButtonText("");
        antiAliasToggle.setLookAndFeel(&pillLnf);
        cleanModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            apvts, "waveshaperClean", antiAliasToggle);

        // Oversampling combo
        addAndMakeVisible(oversamplingCombo);
        oversamplingCombo.addItem("Off", 1);
        oversamplingCombo.addItem("2x", 2);
        oversamplingCombo.addItem("4x", 3);
        oversamplingCombo.setSelectedId(state.oversamplingMode + 1, juce::dontSendNotification);
        oversamplingCombo.setLookAndFeel(&comboLnf);
        oversamplingCombo.onChange = [this]() {
            settingsState.oversamplingMode = oversamplingCombo.getSelectedId() - 1;
        };

        // Window Scale combo
        addAndMakeVisible(windowScaleCombo);
        windowScaleCombo.addItem("70%", 70);
        windowScaleCombo.addItem("80%", 80);
        windowScaleCombo.addItem("90%", 90);
        windowScaleCombo.addItem("100%", 100);
        windowScaleCombo.setSelectedId(state.windowScalePercent, juce::dontSendNotification);
        windowScaleCombo.setLookAndFeel(&comboLnf);
        windowScaleCombo.onChange = [this]() {
            int percent = windowScaleCombo.getSelectedId();
            settingsState.windowScalePercent = percent;
            if (onWindowScaleChanged)
                onWindowScaleChanged(percent);
        };

        // Tooltips toggle
        addAndMakeVisible(tooltipsToggle);
        tooltipsToggle.setButtonText("");
        tooltipsToggle.setLookAndFeel(&pillLnf);
        tooltipsToggle.setToggleState(state.tooltipsEnabled, juce::dontSendNotification);
        tooltipsToggle.onClick = [this]() {
            settingsState.tooltipsEnabled = tooltipsToggle.getToggleState();
        };

        // Oscilloscope toggle
        addAndMakeVisible(oscilloscopeToggle);
        oscilloscopeToggle.setButtonText("");
        oscilloscopeToggle.setLookAndFeel(&pillLnf);
        oscilloscopeToggle.setToggleState(state.oscilloscopeEnabled, juce::dontSendNotification);
        oscilloscopeToggle.onClick = [this]() {
            settingsState.oscilloscopeEnabled = oscilloscopeToggle.getToggleState();
            if (onOscilloscopeToggled)
                onOscilloscopeToggled(oscilloscopeToggle.getToggleState());
        };

        // Scope Stereo/Mono toggle
        addAndMakeVisible(scopeStereoToggle);
        scopeStereoToggle.setButtonText("");
        scopeStereoToggle.setLookAndFeel(&pillLnf);
        scopeStereoToggle.setToggleState(state.oscilloscopeStereo, juce::dontSendNotification);
        scopeStereoToggle.onClick = [this]() {
            settingsState.oscilloscopeStereo = scopeStereoToggle.getToggleState();
            if (onScopeChannelModeChanged)
                onScopeChannelModeChanged(scopeStereoToggle.getToggleState());
        };
    }

    ~SettingsOverlay() override
    {
        antiAliasToggle.setLookAndFeel(nullptr);
        tooltipsToggle.setLookAndFeel(nullptr);
        oscilloscopeToggle.setLookAndFeel(nullptr);
        scopeStereoToggle.setLookAndFeel(nullptr);
        oversamplingCombo.setLookAndFeel(nullptr);
        windowScaleCombo.setLookAndFeel(nullptr);
    }

    void paint(juce::Graphics& g) override
    {
        // Dark backdrop
        g.fillAll(juce::Colour(0xD9000000));

        auto panelBounds = getPanelBounds();

        // Panel background
        g.setColour(juce::Colour(0xFF111111));
        g.fillRoundedRectangle(panelBounds, 6.0f);

        // Panel border
        g.setColour(juce::Colour(0xFFFF2244));
        g.drawRoundedRectangle(panelBounds, 6.0f, 1.0f);

        auto inner = panelBounds.reduced(16.0f);

        // Header: "SETTINGS"
        g.setFont(Fonts::getOrbitron(12.0f, true));
        g.setColour(juce::Colour(0xFFFF2244));
        g.drawText("SETTINGS", inner.removeFromTop(24.0f), juce::Justification::centredLeft);

        // Close button (X) - drawn in top-right of panel
        auto closeBtn = getCloseBtnBounds();
        g.setColour(juce::Colour(0xFFFF2244));
        g.setFont(16.0f);
        g.drawText(juce::CharPointer_UTF8("\xc3\x97"), closeBtn, juce::Justification::centred);

        inner.removeFromTop(8.0f);

        // PROCESSING section header
        g.setFont(Fonts::getOrbitron(10.0f, true));
        g.setColour(juce::Colour(0xFFFF2244).withAlpha(0.6f));
        auto processingHeader = inner.removeFromTop(18.0f);
        g.drawText("PROCESSING", processingHeader, juce::Justification::centredLeft);

        // Divider
        g.setColour(juce::Colour(0xFF282828));
        inner.removeFromTop(4.0f);
        g.fillRect(inner.removeFromTop(1.0f));
        inner.removeFromTop(8.0f);

        // Anti-Alias row label
        g.setFont(12.0f);
        g.setColour(juce::Colours::white);
        auto aaRow = inner.removeFromTop(24.0f);
        g.drawText("Anti-Alias", aaRow.removeFromLeft(140.0f), juce::Justification::centredLeft);

        inner.removeFromTop(6.0f);

        // Oversampling row label
        auto osRow = inner.removeFromTop(24.0f);
        g.drawText("Oversampling", osRow.removeFromLeft(140.0f), juce::Justification::centredLeft);

        inner.removeFromTop(16.0f);

        // INTERFACE section header
        g.setFont(Fonts::getOrbitron(10.0f, true));
        g.setColour(juce::Colour(0xFFFF2244).withAlpha(0.6f));
        auto interfaceHeader = inner.removeFromTop(18.0f);
        g.drawText("INTERFACE", interfaceHeader, juce::Justification::centredLeft);

        // Divider
        g.setColour(juce::Colour(0xFF282828));
        inner.removeFromTop(4.0f);
        g.fillRect(inner.removeFromTop(1.0f));
        inner.removeFromTop(8.0f);

        // Window Scale row label
        g.setFont(12.0f);
        g.setColour(juce::Colours::white);
        auto wsRow = inner.removeFromTop(24.0f);
        g.drawText("Window Scale", wsRow.removeFromLeft(140.0f), juce::Justification::centredLeft);

        inner.removeFromTop(6.0f);

        // Tooltips row label
        auto ttRow = inner.removeFromTop(24.0f);
        g.drawText("Tooltips", ttRow.removeFromLeft(140.0f), juce::Justification::centredLeft);

        inner.removeFromTop(6.0f);

        // Oscilloscope row label
        auto scRow = inner.removeFromTop(24.0f);
        g.drawText("Oscilloscope", scRow.removeFromLeft(140.0f), juce::Justification::centredLeft);

        inner.removeFromTop(6.0f);

        // Stereo row label
        auto stRow = inner.removeFromTop(24.0f);
        g.drawText("Stereo", stRow.removeFromLeft(140.0f), juce::Justification::centredLeft);
    }

    void resized() override
    {
        auto panelBounds = getPanelBounds();
        auto inner = panelBounds.reduced(16.0f);

        inner.removeFromTop(24.0f); // header
        inner.removeFromTop(8.0f);

        // PROCESSING header + divider
        inner.removeFromTop(18.0f);
        inner.removeFromTop(4.0f);
        inner.removeFromTop(1.0f);
        inner.removeFromTop(8.0f);

        // Anti-Alias row
        auto aaRow = inner.removeFromTop(24.0f);
        aaRow.removeFromLeft(140.0f);
        antiAliasToggle.setBounds(aaRow.removeFromLeft(50).reduced(0, 2).toNearestInt());

        inner.removeFromTop(6.0f);

        // Oversampling row
        auto osRow = inner.removeFromTop(24.0f);
        osRow.removeFromLeft(140.0f);
        oversamplingCombo.setBounds(osRow.removeFromLeft(70).reduced(0, 2).toNearestInt());

        inner.removeFromTop(16.0f);

        // INTERFACE header + divider
        inner.removeFromTop(18.0f);
        inner.removeFromTop(4.0f);
        inner.removeFromTop(1.0f);
        inner.removeFromTop(8.0f);

        // Window Scale row
        auto wsRow = inner.removeFromTop(24.0f);
        wsRow.removeFromLeft(140.0f);
        windowScaleCombo.setBounds(wsRow.removeFromLeft(70).reduced(0, 2).toNearestInt());

        inner.removeFromTop(6.0f);

        // Tooltips row
        auto ttRow = inner.removeFromTop(24.0f);
        ttRow.removeFromLeft(140.0f);
        tooltipsToggle.setBounds(ttRow.removeFromLeft(50).reduced(0, 2).toNearestInt());

        inner.removeFromTop(6.0f);

        // Oscilloscope row
        auto scRow = inner.removeFromTop(24.0f);
        scRow.removeFromLeft(140.0f);
        oscilloscopeToggle.setBounds(scRow.removeFromLeft(50).reduced(0, 2).toNearestInt());

        inner.removeFromTop(6.0f);

        // Stereo row
        auto stRow = inner.removeFromTop(24.0f);
        stRow.removeFromLeft(140.0f);
        scopeStereoToggle.setBounds(stRow.removeFromLeft(50).reduced(0, 2).toNearestInt());
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        // Click outside panel closes overlay
        if (!getPanelBounds().contains(e.getPosition().toFloat()))
        {
            if (onClose) onClose();
            return;
        }

        // Check close button
        if (getCloseBtnBounds().contains(e.getPosition().toFloat()))
        {
            if (onClose) onClose();
            return;
        }
    }

    std::function<void()> onClose;
    std::function<void(bool)> onOscilloscopeToggled;
    std::function<void(bool)> onScopeChannelModeChanged;
    std::function<void(int)> onWindowScaleChanged;

private:
    SettingsState& settingsState;
    PillToggleLookAndFeel pillLnf;

    // Combo styling
    struct OverlayComboLnf : public juce::LookAndFeel_V4
    {
        OverlayComboLnf()
        {
            setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xFF1A1A1A));
            setColour(juce::ComboBox::textColourId, juce::Colours::white);
            setColour(juce::ComboBox::outlineColourId, juce::Colour(0xFF333333));
            setColour(juce::ComboBox::arrowColourId, juce::Colour(0xFFFF2244));
            setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xFF111111));
            setColour(juce::PopupMenu::textColourId, juce::Colours::white);
            setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0xFFFF2244).withAlpha(0.3f));
            setColour(juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
        }
    } comboLnf;

    juce::ToggleButton antiAliasToggle;
    juce::ComboBox oversamplingCombo;
    juce::ComboBox windowScaleCombo;
    juce::ToggleButton tooltipsToggle;
    juce::ToggleButton oscilloscopeToggle;
    juce::ToggleButton scopeStereoToggle;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> cleanModeAttachment;

    juce::Rectangle<float> getPanelBounds() const
    {
        const float panelW = 320.0f;
        const float panelH = 370.0f;
        return juce::Rectangle<float>(panelW, panelH)
            .withCentre(getLocalBounds().getCentre().toFloat());
    }

    juce::Rectangle<float> getCloseBtnBounds() const
    {
        auto panel = getPanelBounds();
        return juce::Rectangle<float>(22.0f, 22.0f)
            .withPosition(panel.getRight() - 30.0f, panel.getY() + 8.0f);
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

class PluginEditor : public juce::AudioProcessorEditor, public juce::Timer
{
public:
    explicit PluginEditor(PluginProcessor&);
    ~PluginEditor() override;

    //==============================================================================
    void paint(juce::Graphics&) override;
    void timerCallback() override;
    void resized() override;
    void mouseUp(const juce::MouseEvent& e) override;

private:
    
    PluginProcessor& audioProcessor;
    Oscilloscope oscilloscope;
    XYMorphPad xyMorphPad;  // Invisible XY pad overlay on oscilloscope
    GainReductionMeter gainReductionMeter;
    PhaseCorrelationMeter phaseCorrelationMeter;
    CheckboxLookAndFeel checkboxLookAndFeel;
    ComboBoxLookAndFeel comboBoxLookAndFeel;  // Neon red styling for dropdowns
    LogoTitle logoTitle;  // Logo image title
    juce::Label versionLabel;  // Build version display at bottom left

    // UI Components
    CustomKnob inputGainSlider, distortionAmountSlider, outputGainSlider, highPassFreqSlider, distMixSlider, toneSlider;
    juce::Label inputGainLabel, distortionAmountLabel, outputGainLabel, highPassFreqLabel, distMixLabel, toneLabel;
    
    CustomKnob lfoRateSlider, lfoDepthSlider;
    juce::Label lfoRateLabel, lfoDepthLabel;
    juce::ComboBox lfoWaveformComboBox;
    juce::Label lfoWaveformLabel;
    juce::ComboBox lfoDestinationComboBox;
    juce::Label lfoDestinationLabel;
    LockIcon lfoDestinationLock;

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

    // Collapsible LFO section
    bool isLFOExpanded = true;  // Default: expanded
    CollapsibleTabHeader lfoTabHeader{"LFO"};
    juce::ToggleButton lfoEnableToggle;

    CustomKnob subGuardSlider;
    juce::Label subGuardLabel;

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
    juce::Label presetLabel;

    // Lock icons for each parameter
    LockIcon inputGainLock, outputGainLock, distortionAmountLock, highPassFreqLock;
    LockIcon distMixLock, lfoRateLock, lfoDepthLock, lfoEnableLock;
    LockIcon compPeakReductionLock, compMakeupGainLock;
    LockIcon subGuardLock, clipTypeLock, compRatioLock, compEnableLock;

    // Lock toggles for randomization
    std::map<juce::String, bool> parameterLocks;

    // Parameter attachments
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> inputGainAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> distortionAmountAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outputGainAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> highPassFreqAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> distMixAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> toneAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> subGuardAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> clipTypeAttachment;
    juce::Image backgroundImage;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lfoRateAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lfoDepthAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> lfoWaveformAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> lfoDestinationAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> lfoEnableAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> waveshaperAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> compPeakReductionAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> compMakeupGainAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> compRatioAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> compEnableAttachment;

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
    void updateLFOVisibility();
    void updateModulationHighlight();
    void morphDistortionParameters(float x, float y);  // XY Morph Pad callback

    // LFO modulation visual feedback state
    int currentModulatedDestination = -1;
    float modulationPulsePhase = 0.0f;
    float uiLfoPhase = 0.0f;  // Smooth UI-side phase accumulator for LFO arc animation

    // Preset management methods
    void savePreset(const juce::String& presetName);
    void loadPreset(const juce::String& presetName);
    void loadFactoryPreset(const juce::String& presetName);
    void deletePreset(const juce::String& presetName);
    void refreshPresetList();
    juce::File getPresetDirectory();

    // Settings overlay
    std::unique_ptr<SettingsOverlay> settingsOverlay;
    GearButton settingsButton;
    SettingsState settingsState;

    void showSettingsOverlay();
    void hideSettingsOverlay();
    void applyOscilloscopeEnabled(bool enabled);
    void applyWindowScale(int scalePercent);
    void loadSettings();
    void saveSettings();
    juce::File getSettingsFile();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditor)
};