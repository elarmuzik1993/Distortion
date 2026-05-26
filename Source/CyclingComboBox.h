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
class CyclingComboBox : public juce::ComboBox
{
public:
    using juce::ComboBox::ComboBox;

    /** Advance the selection by `direction` (+1 = next, -1 = previous),
        wrapping at the ends, skipping any item whose ID is in excludedIds.
        No-op if there are no items or every candidate is excluded. */
    void cycleSelection (int direction)
    {
        const int n = getNumItems();
        if (n == 0)
            return;

        int i = getSelectedItemIndex(); // -1 if nothing selected
        for (int step = 0; step < n; ++step)
        {
            i = ((i + direction) % n + n) % n;
            if (! excludedIds.contains (getItemId (i)))
            {
                setSelectedItemIndex (i, juce::sendNotification);
                return;
            }
        }
    }

    /** Item IDs listed here are skipped by cycleSelection() but still appear in
        the popup opened by double-click (used for the preset Save/Delete items). */
    void setExcludedFromCycle (juce::Array<int> ids) { excludedIds = std::move (ids); }

    void mouseDown (const juce::MouseEvent& e) override
    {
        // Right-click (or any non-left button): do nothing here so the default
        // popup never opens on right-click and the editor's mouseUp lock handler
        // still receives the event. We never call ComboBox::mouseDown, so the
        // default left-click popup is suppressed too.
        if (! e.mods.isLeftButtonDown())
            return;

        if (e.getNumberOfClicks() >= 2)
        {
            // Double-click: undo the stray cycle from the first click, then open
            // the real list. If the user dismisses it, the value is unchanged.
            setSelectedItemIndex (preCycleIndex, juce::sendNotification);
            showPopup();
            return;
        }

        preCycleIndex = getSelectedItemIndex();
        cycleSelection (+1);
    }

    void mouseWheelMove (const juce::MouseEvent&,
                         const juce::MouseWheelDetails& wheel) override
    {
        cycleSelection (wheel.deltaY > 0.0f ? -1 : +1);
    }

private:
    juce::Array<int> excludedIds;
    int preCycleIndex = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CyclingComboBox)
};
