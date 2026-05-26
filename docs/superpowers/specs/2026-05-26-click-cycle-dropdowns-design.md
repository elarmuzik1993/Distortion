# Click-to-Cycle Dropdowns — Design

**Date:** 2026-05-26
**Branch:** `feature/click-cycle-dropdowns`
**Status:** Approved

## Goal

Change every dropdown (`juce::ComboBox`) in the plugin so that:

- **Left-click** advances to the next item (cycles forward, wrapping at the end) — it does **not** open the popup list.
- **Right-click** (or ctrl/cmd-click) opens the real dropdown popup list, exactly as a normal ComboBox does today.
- **Scroll wheel** over the box cycles items (up = previous, down = next), also wrapping.

This applies to **all** dropdowns in the plugin:

| Combo | Owner class | Items |
|-------|-------------|-------|
| `clipTypeComboBox` | `PluginEditor` | 7 clip types |
| `lfoWaveformComboBox` | `PluginEditor` | 5 waveforms |
| `lfoDestinationComboBox` | `PluginEditor` | LFO destinations |
| `compRatioComboBox` | `PluginEditor` | compressor ratios |
| `presetSelector` | `PluginEditor` | presets |
| `oversamplingCombo` | Settings component | Off / 2x / 4x |
| `windowScaleCombo` | Settings component | 70–100% |

## Non-Goals

- No visual hint, tooltip, or arrow change. The dropdown arrow is already hidden by `ComboBoxLookAndFeel`; the box stays visually identical.
- No change to item population, parameter attachments, the neon `ComboBoxLookAndFeel`, or the initials-drawing logic.
- No change to keyboard behavior.

## Architecture

### New unit: `Source/CyclingComboBox.h` (header-only)

A small reusable subclass of `juce::ComboBox`, mirroring how `CustomKnob` is its own unit. It overrides only mouse handling and delegates the actual selection change to one testable method.

```cpp
class CyclingComboBox : public juce::ComboBox
{
public:
    using juce::ComboBox::ComboBox;

    // Advance selection by `direction` (+1 next, -1 previous), wrapping.
    // Fires onChange via sendNotification so APVTS attachments update.
    void cycleSelection (int direction);

    void mouseDown (const juce::MouseEvent& e) override;
    void mouseWheelMove (const juce::MouseEvent& e,
                         const juce::MouseWheelDetails& wheel) override;
};
```

**`cycleSelection(int direction)`**
- `numItems = getNumItems()`; if `numItems <= 1`, return (nothing to cycle).
- `current = getSelectedItemIndex()` (may be -1 if nothing selected → treat as 0 start).
- `next = ((current + direction) % numItems + numItems) % numItems` (handles negative).
- `setSelectedItemIndex(next, juce::sendNotification)`.

`getNumItems()` and index-based access exclude separators and headers automatically, so cycling never lands on a non-item row. `sendNotification` triggers `onChange`, which is what the existing `juce::AudioProcessorValueTreeState::ComboBoxAttachment` listens to — so the bound parameter updates identically to a manual pick.

**`mouseDown(e)`**
- If `e.mods.isRightButtonDown() || e.mods.isCommandDown()` → `showPopup();`
- Else (plain left-click) → `cycleSelection(+1);`
- Do **not** call `juce::ComboBox::mouseDown`, so the default left-click popup is suppressed.

**`mouseWheelMove(e, wheel)`**
- `direction = (wheel.deltaY > 0.0f) ? -1 : +1;` (scroll up = previous).
- `cycleSelection(direction);`

### Wiring changes

In `Source/PluginEditor.h`, change the declared type from `juce::ComboBox` to `CyclingComboBox` for: `clipTypeComboBox`, `lfoWaveformComboBox`, `lfoDestinationComboBox`, `compRatioComboBox`, `presetSelector`, plus `oversamplingCombo` and `windowScaleCombo` in the Settings component class. Add `#include "CyclingComboBox.h"`.

Because `CyclingComboBox` *is-a* `juce::ComboBox`, attachments (`ComboBoxAttachment` constructors take a `juce::ComboBox&`), `setLookAndFeel(&comboBoxLookAndFeel)`, `addItem`, `onChange`, and all existing setup code compile and behave unchanged.

## Data Flow

```
left-click ──► mouseDown ──► cycleSelection(+1) ──► setSelectedItemIndex(next, sendNotification)
                                                          │
                                                          ▼
                                                  ComboBox::onChange
                                                          │
                                                          ▼
                                          ComboBoxAttachment → APVTS parameter
right-click ─► mouseDown ──► showPopup() ──► (normal selection) ──► onChange ──► attachment
scroll ──────► mouseWheelMove ──► cycleSelection(±1) ──► (same as left-click path)
```

## Error Handling / Edge Cases

- **Empty or single-item box:** `cycleSelection` returns early when `getNumItems() <= 1` — left-click and scroll become no-ops, right-click still opens the (possibly empty) popup.
- **Nothing currently selected (`getSelectedItemIndex() == -1`):** treated as index 0 anchor, so the first cycle lands deterministically on the first item (direction +1 → index 0) rather than skipping.
- **Disabled items:** none of the current combos disable items, so no skip logic is added (YAGNI). If that changes later, `cycleSelection` is the single place to extend.

## Testing

Add a unit test (in the existing `Source/Tests` suite) that:

1. Constructs a `CyclingComboBox`, `addItem` ×3, selects index 0.
2. `cycleSelection(+1)` ×3 → asserts indices 1, 2, then wraps to 0.
3. `cycleSelection(-1)` from index 0 → asserts wrap to index 2.
4. Single-item box → `cycleSelection(+1)` is a no-op (still index 0).

The logic lives in `cycleSelection`, so no mouse events or message-thread pumping are required.

## Build Notes

- `CMakeLists.txt` enumerates headers explicitly (e.g. `Source/CustomKnob.h`, `Source/LinearRamp.h` in the `PLUGIN_SOURCES` list). Add `Source/CyclingComboBox.h` there. If `Distortion.jucer` likewise lists source files, add it there too so the Windows/Projucer build stays in sync.
- Build/verify: `Distortion_Standalone` + `Distortion_VST3`, then `DistortionTests`.
