#pragma once

#include <JuceHeader.h>

/**
    A juce::ComboBox that cycles to the next item on single left-click and on
    scroll, opens the full popup list on double-click, and leaves right-click
    untouched (so an external mouse listener can keep handling it, e.g. the
    randomize-lock toggle in PluginEditor).

    All selection changes go through cycleSelection(), which fires onChange via
    juce::sendNotification so APVTS attachments and onChange lambdas update
    exactly as a manual pick would.
*/
class CyclingComboBox : public juce::ComboBox,
                        private juce::Timer
{
public:
    using juce::ComboBox::ComboBox;

    ~CyclingComboBox() override { stopTimer(); }

    /** Advance the selection by `direction` (+1 = next, -1 = previous),
        wrapping at the ends. No-op if there are no items. */
    void cycleSelection (int direction)
    {
        const int n = getNumItems();
        if (n == 0)
            return;

        const int i = getSelectedItemIndex(); // -1 if nothing selected
        setSelectedItemIndex (((i + direction) % n + n) % n, juce::sendNotification);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        // Right-click (or any non-left button): do nothing here so the default
        // popup never opens on right-click and the editor's mouseUp lock handler
        // still receives the event. We never call ComboBox::mouseDown, so the
        // default left-click popup is suppressed too.
        if (! e.mods.isLeftButtonDown())
            return;

        // Defer acting until the click burst settles (timer restarts on each
        // click). This lets us tell a single click (cycle once) from a
        // double-click (open the list) without ever firing a stray cycle.
        pendingClicks = e.getNumberOfClicks();
        startTimer (clickBurstMs);
    }

    void mouseWheelMove (const juce::MouseEvent& e,
                         const juce::MouseWheelDetails& wheel) override
    {
        // Only consume the wheel when we actually have items to cycle; otherwise
        // let it propagate (e.g. to a parent Viewport's scrollbar).
        if (getNumItems() > 0)
            cycleSelection (wheel.deltaY > 0.0f ? -1 : +1);
        else
            juce::ComboBox::mouseWheelMove (e, wheel);
    }

private:
    void timerCallback() override
    {
        stopTimer();
        if (pendingClicks >= 2)
            showPopup();            // double-click (or more): open the real list
        else
            cycleSelection (+1);    // single click: advance one step
        pendingClicks = 0;
    }

    int pendingClicks = 0;
    static constexpr int clickBurstMs = 200; // grouping window for click bursts

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CyclingComboBox)
};
