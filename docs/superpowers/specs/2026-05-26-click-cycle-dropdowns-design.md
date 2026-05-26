# Click-to-Cycle Dropdowns — Design

**Date:** 2026-05-26
**Branch:** `feature/click-cycle-dropdowns`
**Status:** Approved

## Goal

Change every dropdown (`juce::ComboBox`) in the plugin so that:

- **Single left-click** advances to the next item (cycles forward, wrapping at the end) — it does **not** open the popup list. Instant, no delay.
- **Double left-click** opens the real dropdown popup list (`showPopup()`).
- **Right-click** is **unchanged**: on the three lockable combos (`clipType`, `compRatio`, `lfoDestination`) it still toggles the randomize-lock via the editor's existing `mouseUp` handler; on the others it stays a no-op.
- **Scroll wheel** over the box cycles items (up = previous, down = next), also wrapping. Instant.

> **Gesture-model note (revised during planning):** The original idea was *right-click opens the list*, but right-click is already bound to the randomize-lock on three combos. Decision: keep right-click = lock, and use **double-click** as the open-the-list gesture, uniformly across all combos.

This applies to **all** dropdowns in the plugin:

| Combo | Owner class | Items |
|-------|-------------|-------|
| `clipTypeComboBox` | `PluginEditor` | 7 clip types |
| `lfoWaveformComboBox` | `PluginEditor` | 5 waveforms |
| `lfoDestinationComboBox` | `PluginEditor` | LFO destinations |
| `compRatioComboBox` | `PluginEditor` | compressor ratios |
| `presetSelector` | `PluginEditor` | presets (+ Save/Delete actions) |
| `oversamplingCombo` | Settings component | Off / 2x / 4x |
| `windowScaleCombo` | Settings component | 70–100% |

### Preset selector specifics

The preset selector is not a plain value list: real presets use IDs `1..N`, and there are two action items — **Save Preset…** (`9990`) and **Delete Preset…** (`9991`) — plus section headings and separators (see `refreshPresetList`, PluginEditor.cpp:1174). Decision: **left-click/scroll cycle real presets only**, skipping the Save/Delete action items. Section headings and separators are already excluded by JUCE's index-based item access. Double-click still opens the full list (including Save/Delete). This is handled by a per-combo "excluded IDs" set on `CyclingComboBox` (set to `{9990, 9991}` for the preset combo).

## Non-Goals

- No visual hint, tooltip, or arrow change. The dropdown arrow is already hidden by `ComboBoxLookAndFeel`; the box stays visually identical.
- No change to item population, parameter attachments, the neon `ComboBoxLookAndFeel`, the `OverlayComboLnf`, or the initials-drawing logic.
- No change to the existing randomize-lock behavior or keyboard behavior.

## Architecture

### New unit: `Source/CyclingComboBox.h` (header-only)

A small reusable subclass of `juce::ComboBox`, mirroring how `CustomKnob` is its own unit. It overrides only mouse handling and delegates the actual selection change to one testable method.

```cpp
class CyclingComboBox : public juce::ComboBox
{
public:
    using juce::ComboBox::ComboBox;

    // Advance selection by `direction` (+1 next, -1 previous), wrapping,
    // skipping any item whose ID is in excludedIds. Fires onChange via
    // sendNotification so APVTS attachments / onChange handlers update.
    void cycleSelection (int direction);

    // Items whose ID is listed here are skipped by cycleSelection (but
    // still appear in the popup opened by double-click). Used for the
    // preset combo's Save/Delete action items.
    void setExcludedFromCycle (juce::Array<int> ids) { excludedIds = std::move (ids); }

    void mouseDown (const juce::MouseEvent& e) override;
    void mouseWheelMove (const juce::MouseEvent& e,
                         const juce::MouseWheelDetails& wheel) override;

private:
    juce::Array<int> excludedIds;
    int preCycleIndex = -1;   // selected index captured before a single-click cycle
};
```

**`cycleSelection(int direction)`** — the single testable core:
- `n = getNumItems()`; if `n == 0`, return.
- Start from `i = getSelectedItemIndex()` (may be -1 → forward step lands on 0).
- Step up to `n` times: `i = ((i + direction) % n + n) % n`; if `getItemId(i)` is **not** in `excludedIds`, call `setSelectedItemIndex(i, juce::sendNotification)` and return.
- If every candidate is excluded, do nothing.

`getNumItems()` / `getItemId(index)` exclude separators and headings automatically, so cycling only ever visits real items; `excludedIds` removes the Save/Delete actions on top of that. `sendNotification` triggers `onChange`, which is what both the `ComboBoxAttachment`s and the settings combos' `onChange` lambdas listen to — so the bound parameter/setting updates identically to a manual pick.

**`mouseDown(e)`** — instant single-click cycle, double-click opens list, right-click passes through:
- If **not** `e.mods.isLeftButtonDown()` → `return;` (right-click falls through to the editor's existing `mouseUp` lock handler; we never call `ComboBox::mouseDown`, so the default left-click popup is suppressed and right-click never opens a popup — matching today's behavior).
- If `e.getNumberOfClicks() >= 2` (double-click): revert the stray cycle from the first click with `setSelectedItemIndex(preCycleIndex, juce::sendNotification)`, then `showPopup();`.
- Else (single left-click): `preCycleIndex = getSelectedItemIndex();` then `cycleSelection(+1);`.

This keeps the primary action (single-click cycle) instant with no timer/debounce. A double-click cycles once on the first press, then the second press reverts to `preCycleIndex` and opens the list — so if the user dismisses the popup without choosing, the value is unchanged.

**`mouseWheelMove(e, wheel)`**
- `direction = (wheel.deltaY > 0.0f) ? -1 : +1;` (scroll up = previous).
- `cycleSelection(direction);`

### Wiring changes

In `Source/PluginEditor.h`, add `#include "CyclingComboBox.h"`, then change the declared type from `juce::ComboBox` to `CyclingComboBox` for: `clipTypeComboBox`, `lfoWaveformComboBox`, `lfoDestinationComboBox`, `compRatioComboBox`, `presetSelector`, plus `oversamplingCombo` and `windowScaleCombo` in the `SettingsContent` class.

In `PluginEditor.cpp`, after the preset combo is populated, call `presetSelector.setExcludedFromCycle({ 9990, 9991 });` so cycling skips the Save/Delete actions.

The existing `setupKnobRightClick(clipTypeComboBox, ...)` / `compRatio` / `lfoDestination` calls and the `mouseUp` lock handler stay exactly as-is — right-click continues to toggle the lock.

Because `CyclingComboBox` *is-a* `juce::ComboBox`, attachments (`ComboBoxAttachment` constructors take a `juce::ComboBox&`), `setLookAndFeel(&comboBoxLookAndFeel)`, `addItem`, `onChange`, and all existing setup code compile and behave unchanged.

## Data Flow

```
single left-click ─► mouseDown ─► cycleSelection(+1) ─► setSelectedItemIndex(next, sendNotification)
                                                              │
                                                              ▼
                                                      ComboBox::onChange
                                                              │
                                                              ▼
                                    ComboBoxAttachment → APVTS param  (or settings onChange lambda)
double left-click ─► mouseDown ─► revert to preCycleIndex ─► showPopup() ─► (normal selection) ─► onChange
right-click ───────► (CyclingComboBox ignores) ─► editor mouseUp ─► toggleParameterLock (3 combos only)
scroll ────────────► mouseWheelMove ─► cycleSelection(±1) ─► (same path as single-click)
```

## Error Handling / Edge Cases

- **Empty box (`getNumItems() == 0`):** `cycleSelection` returns early — left-click and scroll are no-ops; double-click still calls `showPopup()` on the (empty) box.
- **Single cyclable item:** stepping wraps back to the same index and re-selects it (harmless no-op selection).
- **Nothing currently selected (`getSelectedItemIndex() == -1`):** forward step lands deterministically on index 0; the double-click revert to `preCycleIndex == -1` clears the selection, which is the correct "nothing was chosen" state.
- **All items excluded (preset combo with zero real presets is impossible — factory presets always exist):** loop exits without selecting; no-op.
- **Disabled items:** none of the current combos disable items, so no skip logic beyond `excludedIds` is added (YAGNI). `cycleSelection` is the single place to extend if that changes.

## Testing

Add a unit test class (`CyclingComboBoxTests`) registered in `registerAllTests()`, following the existing `juce::UnitTest` pattern. The message thread is already initialized by `ScopedJuceInitialiser_GUI` in the test runner, so constructing a `juce::Component`-derived `CyclingComboBox` is safe.

1. **Forward wrap:** `addItem` ×3, select index 0; `cycleSelection(+1)` → 1, 2, then wraps to 0.
2. **Backward wrap:** from index 0, `cycleSelection(-1)` → wraps to index 2.
3. **Single item:** one item, `cycleSelection(+1)` stays at index 0.
4. **Empty box:** no items, `cycleSelection(+1)` is a no-op (`getSelectedItemIndex()` stays -1).
5. **Excluded IDs:** items with IDs `1,2,9990,9991`; `setExcludedFromCycle({9990,9991})`; from index 0, repeated `cycleSelection(+1)` only ever visits indices 0 and 1 (the IDs `1` and `2`), never the excluded ones.

The logic lives entirely in `cycleSelection`, so no mouse events or message-thread pumping are required.

## Build Notes

- `CMakeLists.txt` enumerates headers explicitly (e.g. `Source/CustomKnob.h`, `Source/LinearRamp.h` in the `PLUGIN_SOURCES` list). Add `Source/CyclingComboBox.h` there. If `Distortion.jucer` likewise lists source files, add it there too so the Windows/Projucer build stays in sync.
- Build/verify: `Distortion_Standalone` + `Distortion_VST3`, then `DistortionTests`.
