# Click-to-Cycle Dropdowns — Design

**Date:** 2026-05-26
**Branch:** `feature/click-cycle-dropdowns`
**Status:** Implemented

## Goal

Make the plugin's value dropdowns behave like nudgeable controls:

- **Single left-click** advances to the next item (cycles forward, wrapping at the end) — it does **not** open the popup list. The action fires after a short settle window (see timer note below), so it can be distinguished from a double-click.
- **Double left-click** opens the real dropdown popup list (`showPopup()`).
- **Right-click** is **unchanged**: on the three lockable combos (`clipType`, `compRatio`, `lfoDestination`) it still toggles the randomize-lock via the editor's existing `mouseUp` handler; on the others it stays a no-op.
- **Scroll wheel** over the box cycles items (up = previous, down = next), wrapping. Instant — use it for fast multi-step changes.

> **Gesture-model notes (decided during implementation):**
> 1. The original idea was *right-click opens the list*, but right-click is already bound to the randomize-lock on three combos. Decision: keep right-click = lock, and use **double-click** as the open-the-list gesture.
> 2. To avoid a stray cycle when the user actually means to double-click, click handling is **timer-debounced**: a click only acts once the burst settles. A single click cycles once; a double-click opens the list. There is no "cycle then revert" — nothing happens until the burst resolves. The cost is a small (~200ms) delay before a single click registers; fast multi-step changes use the scroll wheel instead.

### Combos affected

| Combo | Owner class | Converted to `CyclingComboBox`? |
|-------|-------------|---------------------------------|
| `clipTypeComboBox` | `PluginEditor` | Yes |
| `lfoWaveformComboBox` | `PluginEditor` | Yes |
| `lfoDestinationComboBox` | `PluginEditor` | Yes |
| `compRatioComboBox` | `PluginEditor` | Yes |
| `oversamplingCombo` | `SettingsContent` | Yes |
| `windowScaleCombo` | `SettingsContent` | Yes |
| `presetSelector` | `PluginEditor` | **No — stays a plain `juce::ComboBox`** |

### Preset selector: left as a normal dropdown

The preset selector is a chooser, not a value list: it has Save/Delete action items, section headings, separators, and an `onChange` with side effects. Cycling it is awkward and was explicitly **dropped** — it remains a plain `juce::ComboBox` where a single click opens the list as usual. (An earlier iteration cycled presets while skipping the Save/Delete action items, but that behavior was removed at the user's request.)

## Non-Goals

- No visual hint, tooltip, or arrow change. The dropdown arrow is already hidden by `ComboBoxLookAndFeel`; the boxes stay visually identical.
- No change to item population, parameter attachments, the neon `ComboBoxLookAndFeel`, the `OverlayComboLnf`, or the initials-drawing logic.
- No change to the existing randomize-lock behavior or keyboard behavior.

## Architecture

### New unit: `Source/CyclingComboBox.h` (header-only)

A small reusable subclass of `juce::ComboBox` (also a private `juce::Timer`), mirroring how `CustomKnob` is its own unit. It overrides only mouse handling and delegates the actual selection change to one testable method.

```cpp
class CyclingComboBox : public juce::ComboBox,
                        private juce::Timer
{
public:
    using juce::ComboBox::ComboBox;
    ~CyclingComboBox() override;                 // stopTimer()

    // Advance selection by `direction` (+1 next, -1 previous), wrapping.
    // Fires onChange via sendNotification so APVTS attachments / onChange
    // handlers update exactly as a manual pick would.
    void cycleSelection (int direction);

    void mouseDown (const juce::MouseEvent& e) override;
    void mouseWheelMove (const juce::MouseEvent& e,
                         const juce::MouseWheelDetails& wheel) override;

private:
    void timerCallback() override;               // resolves a settled click burst
    int pendingClicks = 0;
    static constexpr int clickBurstMs = 200;     // grouping window for click bursts
};
```

**`cycleSelection(int direction)`** — the single testable core:
- `n = getNumItems()`; if `n == 0`, return.
- `i = getSelectedItemIndex()` (may be -1 → forward step lands on 0).
- `setSelectedItemIndex(((i + direction) % n + n) % n, juce::sendNotification)`.

`getNumItems()` / index-based access exclude separators and headings automatically. `sendNotification` triggers `onChange`, which is what both the `ComboBoxAttachment`s and the settings combos' `onChange` lambdas listen to — so the bound parameter/setting updates identically to a manual pick.

**`mouseDown(e)`** — defers to the timer so a single click and a double-click can be told apart:
- If **not** `e.mods.isLeftButtonDown()` → `return;`. Right-click falls through to the editor's existing `mouseUp` lock handler; we never call `ComboBox::mouseDown`, so the default left-click popup is suppressed and right-click never opens a popup.
- Otherwise: `pendingClicks = e.getNumberOfClicks();` then `startTimer(clickBurstMs);`. The timer restarts on every click in the burst, so it only fires once clicking stops.

**`timerCallback()`**:
- `stopTimer();`
- If `pendingClicks >= 2` → `showPopup();` (double-click or more → open the list).
- Else → `cycleSelection(+1);` (single click → advance one step).
- `pendingClicks = 0;`

**`mouseWheelMove(e, wheel)`** — instant, no debounce:
- If `getNumItems() > 0` → `cycleSelection(wheel.deltaY > 0.0f ? -1 : +1);` (scroll up = previous).
- Else → call `juce::ComboBox::mouseWheelMove(e, wheel)` so an empty box lets the event propagate (e.g. to a parent `Viewport`'s scrollbar).

### Wiring changes

In `Source/PluginEditor.h`, add `#include "CyclingComboBox.h"`, then change the declared type from `juce::ComboBox` to `CyclingComboBox` for: `clipTypeComboBox`, `lfoWaveformComboBox`, `lfoDestinationComboBox`, `compRatioComboBox` (in `PluginEditor`), and `oversamplingCombo`, `windowScaleCombo` (in `SettingsContent`). `presetSelector` is left as `juce::ComboBox`.

The existing `setupKnobRightClick(clipTypeComboBox, ...)` / `compRatio` / `lfoDestination` calls and the `mouseUp` lock handler stay exactly as-is — right-click continues to toggle the lock.

Because `CyclingComboBox` *is-a* `juce::ComboBox`, attachments (`ComboBoxAttachment` constructors take a `juce::ComboBox&`), `setLookAndFeel(&comboBoxLookAndFeel)`, `addItem`, `onChange`, and all existing setup code compile and behave unchanged.

## Data Flow

```
single left-click ─► mouseDown ─► startTimer ─┐
                                              │ (burst settles, pendingClicks == 1)
                                              ▼
                              timerCallback ─► cycleSelection(+1) ─► setSelectedItemIndex(next, sendNotification)
                                                                          │
                                                                          ▼
                                                  ComboBox::onChange ─► ComboBoxAttachment → APVTS param
                                                                          (or settings onChange lambda)
double left-click ─► mouseDown ─► startTimer ─► timerCallback (pendingClicks >= 2) ─► showPopup() ─► onChange
right-click ───────► (CyclingComboBox ignores) ─► editor mouseUp ─► toggleParameterLock (3 combos only)
scroll ────────────► mouseWheelMove ─► cycleSelection(±1)  (instant, no timer)
```

## Error Handling / Edge Cases

- **Empty box (`getNumItems() == 0`):** `cycleSelection` returns early — single-click resolves to a no-op cycle; scroll propagates to the parent; double-click still calls `showPopup()` on the (empty) box.
- **Single item:** stepping wraps back to the same index and re-selects it (harmless no-op selection).
- **Nothing currently selected (`getSelectedItemIndex() == -1`):** forward step lands deterministically on index 0.
- **Rapid repeated single-clicks:** because clicks within `clickBurstMs` group into one burst, three fast clicks read as `pendingClicks == 3` and open the list rather than cycling three times. Multi-step cycling is done with the scroll wheel.
- **Component destroyed mid-burst:** the destructor calls `stopTimer()`, so no callback fires on a dead object.
- **Disabled items:** none of the current combos disable items, so no skip logic is added (YAGNI).

## Testing

Unit test class `CyclingComboBoxTests` registered in `registerAllTests()`, following the existing `juce::UnitTest` pattern. The message thread is already initialized by `ScopedJuceInitialiser_GUI` in the test runner, so constructing a `juce::Component`-derived `CyclingComboBox` is safe. Tests exercise the `cycleSelection` core directly (no mouse events or message-thread pumping needed):

1. **Forward wrap:** `addItem` ×3, select index 0; `cycleSelection(+1)` → 1, 2, then wraps to 0.
2. **Backward wrap:** from index 0, `cycleSelection(-1)` → wraps to index 2.
3. **Single item:** one item, `cycleSelection(+1)` stays at index 0.
4. **Empty box:** no items, `cycleSelection(+1)` is a no-op (`getSelectedItemIndex()` stays -1).

The timer-gated `mouseDown`/`timerCallback` gesture routing is verified manually in the Standalone app (it depends on real click timing).

## Build Notes

- `CMakeLists.txt` enumerates headers explicitly; `Source/CyclingComboBox.h` is added to the `PLUGIN_SOURCES` list. If `Distortion.jucer` also lists source files, add it there too so the Windows/Projucer build stays in sync.
- Build/verify: `Distortion_Standalone` + `Distortion_VST3`, then `DistortionTests`.
