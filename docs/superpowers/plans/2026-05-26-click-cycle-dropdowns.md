# Click-to-Cycle Dropdowns Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make every `juce::ComboBox` in the plugin cycle to the next value on single left-click (wrapping), open the full dropdown list on double-click, cycle on scroll-wheel, and keep right-click bound to the existing randomize-lock.

**Architecture:** Introduce one reusable subclass `CyclingComboBox : juce::ComboBox` (header-only, `Source/CyclingComboBox.h`) whose mouse/scroll overrides delegate to a single testable method `cycleSelection(int)`. Swap the declared type of all 7 combos to `CyclingComboBox`. The preset combo gets an "excluded IDs" set so cycling skips its Save/Delete action items. No changes to attachments, LookAndFeels, item population, or the existing lock handler.

**Tech Stack:** C++17, JUCE 7 (`juce::ComboBox`, `juce::UnitTest`), CMake.

**Spec:** `docs/superpowers/specs/2026-05-26-click-cycle-dropdowns-design.md`

---

## File Structure

- **Create** `Source/CyclingComboBox.h` — the `CyclingComboBox` subclass (header-only). Single responsibility: gesture handling + the `cycleSelection` core.
- **Modify** `CMakeLists.txt` — add `Source/CyclingComboBox.h` to the `PLUGIN_SOURCES` list so it ships in plugin and test builds.
- **Modify** `Source/PluginEditor.h` — `#include "CyclingComboBox.h"`; change 5 member types in `PluginEditor` and 2 in `SettingsContent` from `juce::ComboBox` to `CyclingComboBox`.
- **Modify** `Source/PluginEditor.cpp` — one line to set the preset combo's excluded IDs.
- **Modify** `Source/Tests/DistortionTests.h` — declare `CyclingComboBoxTests` and register it.
- **Modify** `Source/Tests/DistortionTests.cpp` — implement `CyclingComboBoxTests::runTest()`; add `#include "../CyclingComboBox.h"`.
- **Modify** `Distortion.jucer` — add `CyclingComboBox.h` to the Source file group (only if the file enumerates sources; verify in Task 7).

---

## Task 1: Create CyclingComboBox with the cycleSelection core

**Files:**
- Create: `Source/CyclingComboBox.h`

- [ ] **Step 1: Write the header with the full class**

Create `Source/CyclingComboBox.h`:

```cpp
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
```

- [ ] **Step 2: Add the header to CMake sources**

In `CMakeLists.txt`, find the `PLUGIN_SOURCES` list (contains `Source/CustomKnob.h`, `Source/LinearRamp.h`, etc.) and add the new header next to `Source/CustomKnob.h`:

```cmake
    Source/CustomKnob.cpp
    Source/CustomKnob.h
    Source/CyclingComboBox.h
    Source/LinearRamp.h
```

- [ ] **Step 3: Verify it compiles (header is included transitively by the test in later tasks)**

For now just confirm the file parses by configuring the build (no target build yet):

Run: `cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug`
Expected: configures without error (CMake re-globs the new source).

- [ ] **Step 4: Commit**

```bash
git add Source/CyclingComboBox.h CMakeLists.txt
git commit -m "feat(ui): add CyclingComboBox subclass for click-to-cycle dropdowns

Ref USE-XX"
```

---

## Task 2: Unit-test the cycleSelection core (TDD)

**Files:**
- Modify: `Source/Tests/DistortionTests.h` (declare + register the test class)
- Modify: `Source/Tests/DistortionTests.cpp` (implement; add include)

- [ ] **Step 1: Write the failing test class declaration**

In `Source/Tests/DistortionTests.h`, add this class right before the `SanityTests` class (around line 381):

```cpp
/** Tests for CyclingComboBox::cycleSelection — wrap + excluded-ID skipping */
class CyclingComboBoxTests : public juce::UnitTest
{
public:
    CyclingComboBoxTests() : juce::UnitTest("CyclingComboBox", "UI") {}
    void runTest() override;
};
```

- [ ] **Step 2: Register the test**

In `registerAllTests()` in `DistortionTests.h`, add after `static MinimalProcessorTest minimalProcessorTest;`:

```cpp
    static CyclingComboBoxTests cyclingComboBoxTests;
```

- [ ] **Step 3: Implement the test body**

At the top of `Source/Tests/DistortionTests.cpp`, add the include next to the other `#include` lines (after `#include "DistortionTests.h"`):

```cpp
#include "../CyclingComboBox.h"
```

At the end of `Source/Tests/DistortionTests.cpp` (before the closing `#endif // JUCE_DEBUG` if present, otherwise at end of file), add:

```cpp
void CyclingComboBoxTests::runTest()
{
    beginTest("Forward cycle wraps");
    {
        CyclingComboBox box;
        box.addItem("A", 1);
        box.addItem("B", 2);
        box.addItem("C", 3);
        box.setSelectedItemIndex(0, juce::dontSendNotification);

        box.cycleSelection(+1);
        expect(box.getSelectedItemIndex() == 1, "should advance to index 1");
        box.cycleSelection(+1);
        expect(box.getSelectedItemIndex() == 2, "should advance to index 2");
        box.cycleSelection(+1);
        expect(box.getSelectedItemIndex() == 0, "should wrap to index 0");
    }

    beginTest("Backward cycle wraps");
    {
        CyclingComboBox box;
        box.addItem("A", 1);
        box.addItem("B", 2);
        box.addItem("C", 3);
        box.setSelectedItemIndex(0, juce::dontSendNotification);

        box.cycleSelection(-1);
        expect(box.getSelectedItemIndex() == 2, "should wrap back to index 2");
    }

    beginTest("Single item stays put");
    {
        CyclingComboBox box;
        box.addItem("Only", 1);
        box.setSelectedItemIndex(0, juce::dontSendNotification);

        box.cycleSelection(+1);
        expect(box.getSelectedItemIndex() == 0, "single item should not move");
    }

    beginTest("Empty box is a no-op");
    {
        CyclingComboBox box;
        box.cycleSelection(+1);
        expect(box.getSelectedItemIndex() == -1, "empty box should stay unselected");
    }

    beginTest("Excluded IDs are skipped");
    {
        CyclingComboBox box;
        box.addItem("Preset1", 1);
        box.addItem("Preset2", 2);
        box.addItem("Save", 9990);
        box.addItem("Delete", 9991);
        box.setExcludedFromCycle({ 9990, 9991 });
        box.setSelectedItemIndex(0, juce::dontSendNotification);

        // Repeated forward cycling must only ever land on index 0 or 1.
        for (int k = 0; k < 6; ++k)
        {
            box.cycleSelection(+1);
            const int idx = box.getSelectedItemIndex();
            expect(idx == 0 || idx == 1, "must skip excluded action items");
        }
    }
}
```

- [ ] **Step 4: Build the test runner**

Run:
```bash
cmake --build build --target DistortionTests -j$(nproc)
```
Expected: compiles and links successfully.

- [ ] **Step 5: Run the test and verify it passes**

Run:
```bash
./build/DistortionTests_artefacts/Debug/DistortionTests 2>&1 | grep -A6 "CyclingComboBox"
```
Expected: `PASSED: CyclingComboBox (N assertions)` with no failures.

- [ ] **Step 6: Commit**

```bash
git add Source/Tests/DistortionTests.h Source/Tests/DistortionTests.cpp
git commit -m "test(ui): cover CyclingComboBox cycle wrap and excluded IDs

Ref USE-XX"
```

---

## Task 3: Switch the four main combos + preset to CyclingComboBox

**Files:**
- Modify: `Source/PluginEditor.h:1` (add include) and member declarations at lines ~1391, 1393, 1402, 1418, 1450
- Modify: `Source/PluginEditor.cpp` (set preset excluded IDs)

- [ ] **Step 1: Add the include**

At the top of `Source/PluginEditor.h`, with the other project includes (e.g. near `#include "CustomKnob.h"`), add:

```cpp
#include "CyclingComboBox.h"
```

- [ ] **Step 2: Change the five member declarations in `PluginEditor`**

In `Source/PluginEditor.h`, change each of these lines from `juce::ComboBox` to `CyclingComboBox`:

- Line ~1391: `juce::ComboBox lfoWaveformComboBox;` → `CyclingComboBox lfoWaveformComboBox;`
- Line ~1393: `juce::ComboBox lfoDestinationComboBox;` → `CyclingComboBox lfoDestinationComboBox;`
- Line ~1402: `juce::ComboBox compRatioComboBox;` → `CyclingComboBox compRatioComboBox;`
- Line ~1418: `juce::ComboBox clipTypeComboBox;` → `CyclingComboBox clipTypeComboBox;`
- Line ~1450: `juce::ComboBox presetSelector;` → `CyclingComboBox presetSelector;`

- [ ] **Step 3: Set the preset combo's excluded IDs**

In `Source/PluginEditor.cpp`, the preset combo is populated in `refreshPresetList()` (around line 1175). Locate the constructor block where `presetSelector` is first configured (around line 245, after `presetSelector.setTextWhenNothingSelected("Select Preset...");`). Add one line there:

```cpp
    presetSelector.setExcludedFromCycle({ 9990, 9991 }); // skip Save/Delete when cycling
```

(Placing it in the constructor is sufficient — `refreshPresetList()` calls `presetSelector.clear()` which does not touch the excluded-IDs set.)

- [ ] **Step 4: Build the plugin**

Run:
```bash
cmake --build build --target Distortion_Standalone -j$(nproc)
```
Expected: compiles and links. The `ComboBoxAttachment` constructors and `setupKnobRightClick(...)` calls take `juce::ComboBox&`/`juce::Component&`, which `CyclingComboBox` satisfies by inheritance — no other code changes needed.

- [ ] **Step 5: Commit**

```bash
git add Source/PluginEditor.h Source/PluginEditor.cpp
git commit -m "feat(ui): use CyclingComboBox for clip/LFO/comp/preset dropdowns

Ref USE-XX"
```

---

## Task 4: Switch the two Settings combos to CyclingComboBox

**Files:**
- Modify: `Source/PluginEditor.h` member declarations at lines ~976-977 (inside `SettingsContent`)

- [ ] **Step 1: Change the two member declarations in `SettingsContent`**

In `Source/PluginEditor.h`, inside the `SettingsContent` class:

- Line ~976: `juce::ComboBox oversamplingCombo;` → `CyclingComboBox oversamplingCombo;`
- Line ~977: `juce::ComboBox windowScaleCombo;` → `CyclingComboBox windowScaleCombo;`

No other change needed: both combos already drive their behavior through `onChange` lambdas (PluginEditor.h:697 and :734), which `cycleSelection`'s `sendNotification` triggers.

- [ ] **Step 2: Build the plugin**

Run:
```bash
cmake --build build --target Distortion_Standalone -j$(nproc)
```
Expected: compiles and links.

- [ ] **Step 3: Commit**

```bash
git add Source/PluginEditor.h
git commit -m "feat(ui): use CyclingComboBox for settings dropdowns

Ref USE-XX"
```

---

## Task 5: Full build + test gate

**Files:** none (verification only)

- [ ] **Step 1: Build plugin + VST3 + tests**

Run:
```bash
cmake --build build --target Distortion_Standalone --target Distortion_VST3 --target DistortionTests -j$(nproc)
```
Expected: all three targets build successfully.

- [ ] **Step 2: Run the full test suite**

Run:
```bash
./build/DistortionTests_artefacts/Debug/DistortionTests
```
Expected: `ALL TESTS PASSED` (including `CyclingComboBox`), exit code 0. No previously-passing test regresses.

- [ ] **Step 3: Commit (only if any fix was needed; otherwise skip)**

```bash
git add -A
git commit -m "fix(ui): address build/test issues from CyclingComboBox rollout

Ref USE-XX"
```

---

## Task 6: Manual verification in the Standalone app

**Files:** none (manual verification)

- [ ] **Step 1: Launch the Standalone**

Run:
```bash
./build/Distortion_artefacts/Debug/Standalone/Distortion &
```
(Path may be `.../Standalone/Monolit Distortion` — adjust to the produced artefact name.)

- [ ] **Step 2: Verify each gesture on the Clip Type box**
  - Single left-click → value advances one step; at the last clip type it wraps to the first.
  - Scroll up/down over the box → cycles backward/forward.
  - Double-click → the full dropdown list opens; pick an item → it applies.
  - Right-click → toggles the lock (lock icon state changes), does **not** open the list.

- [ ] **Step 3: Repeat the click/scroll/double-click checks on:** LFO Waveform, LFO Destination, Comp Ratio, and (in the gear ⚙ Settings overlay) Oversampling and Window Scale. Confirm Oversampling/Window Scale actually change behavior (oversampling mode, window size) when cycled.

- [ ] **Step 4: Verify the Preset selector**
  - Single-click / scroll → cycles through real presets only; never triggers Save/Delete and never gets stuck.
  - Double-click → full list opens including "Save Preset…" and "Delete Preset…", which still work.

- [ ] **Step 5: Note the manual result.** If anything misbehaves, stop and fix before finishing. If all good, record "manual verification passed" in the finishing step.

---

## Task 7: Sync Projucer build + finish

**Files:**
- Modify: `Distortion.jucer` (only if it enumerates source files)

- [ ] **Step 1: Check whether the .jucer lists source files**

Run:
```bash
grep -n "CustomKnob.h" Distortion.jucer
```
Expected: either a `<FILE ... file="Source/CustomKnob.h"/>` entry (then headers ARE listed) or no match (then they aren't).

- [ ] **Step 2: If headers are listed, add CyclingComboBox.h**

Edit `Distortion.jucer` and add an entry mirroring the `CustomKnob.h` one, e.g.:

```xml
<FILE id="cyccmb" name="CyclingComboBox.h" compile="0" resource="0" file="Source/CyclingComboBox.h"/>
```
(Use a unique `id`. If Step 1 found no match, skip this step — the CMake build is authoritative on Linux/Mac and Projucer pulls headers via the include path.)

- [ ] **Step 3: Commit any .jucer change**

```bash
git add Distortion.jucer
git commit -m "build: add CyclingComboBox.h to Projucer project

Ref USE-XX"
```

- [ ] **Step 4: Finish the branch**

Use the `superpowers:finishing-a-development-branch` skill to choose how to integrate (the branch is `feature/click-cycle-dropdowns`). Confirm the spec's requirements are all met before merging.

---

## Notes for the implementer

- Replace `USE-XX` in commit messages with the real Linear issue ID if one exists for this work; otherwise drop the `Ref USE-XX` line. (A global post-commit hook closes Linear issues from `Fixes USE-XX` — do not use `Fixes` unless you intend to close the issue.)
- Do **not** pass `--no-verify`; let the git hooks run.
- `CyclingComboBox` is-a `juce::ComboBox`, so `juce::AudioProcessorValueTreeState::ComboBoxAttachment`, `setLookAndFeel`, `addItem`, `onChange`, and `setupKnobRightClick` all continue to work unchanged.
- The line numbers in this plan are from the current `main`/branch snapshot; if they've drifted, locate by the surrounding code shown.
