# First-Run Consent Notice (In-Canvas) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the native first-run `AlertWindow` (opened from the editor constructor) with an in-canvas `FirstRunNotice` overlay so the plugin no longer creates a native window during a headless host scan, fixing the `pluginval` Editor-test crash on Linux CI.

**Architecture:** Add a `FirstRunNotice` child `juce::Component` that paints a dim backdrop + centered panel with the consent text and a "Got it" button, mirroring the existing `SettingsOverlay`. Wire it into `PluginEditor` via `show/hideFirstRunNotice()`; the first-run branch calls `showFirstRunNotice()` instead of `AlertWindow::showMessageBoxAsync`.

**Tech Stack:** C++17, JUCE, CMake.

## Global Constraints

- No native windows created during editor construction (native modals only on explicit user action).
- Consent copy unchanged, verbatim: `"Monolit sends anonymous bug reports to help fix issues. You can turn them off anytime in Settings."`
- No title on the notice (keeps the recent "drop redundant title" decision).
- First-run trigger unchanged: `!settingsFileExisted`, followed by `saveSettings()` so it shows once.
- Dismissal: "Got it" button + click-outside-panel + Esc.
- Match existing style tokens: backdrop `0xD9000000`, panel `0xFF111111`, accent border `0xFFFF2244`, radius 6.

---

### Task 1: Add the `FirstRunNotice` component + wire it into `PluginEditor`

**Files:**
- Modify: `Source/PluginEditor.h` (add `FirstRunNotice` class after `SettingsOverlay` ~line 1296; add member + method decls ~lines 1730/1736)
- Modify: `Source/PluginEditor.cpp` (destructor ~620; first-run block 603-612; `resized()` ~947-949; new methods near `showSettingsOverlay` ~1417)

**Interfaces:**
- Produces: `class FirstRunNotice : public juce::Component` with public `std::function<void()> onDismiss;`
- Produces: `void PluginEditor::showFirstRunNotice();` and `void PluginEditor::hideFirstRunNotice();`
- Consumes: existing `PluginEditor::saveSettings()`, member ordering conventions from `settingsOverlay`.

- [ ] **Step 1: Add the `FirstRunNotice` class** in `Source/PluginEditor.h`, immediately after the closing `};` of `SettingsOverlay` (after ~line 1296):

```cpp
// One-time first-run consent notice, drawn in-canvas (no native window) so it is safe
// under headless hosts / pluginval. Mirrors the SettingsOverlay pattern.
class FirstRunNotice : public juce::Component
{
public:
    FirstRunNotice()
    {
        setInterceptsMouseClicks(true, true);

        addAndMakeVisible(gotItButton);
        gotItButton.setButtonText("Got it");
        gotItButton.onClick = [this]() { if (onDismiss) onDismiss(); };
        gotItButton.addShortcut(juce::KeyPress(juce::KeyPress::escapeKey)); // Esc dismisses
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xD9000000)); // dim backdrop

        auto panel = getPanelBounds();
        g.setColour(juce::Colour(0xFF111111));
        g.fillRoundedRectangle(panel, 6.0f);
        g.setColour(juce::Colour(0xFFFF2244));
        g.drawRoundedRectangle(panel, 6.0f, 1.0f);

        auto inner = panel.reduced(16.0f);
        inner.removeFromBottom(34.0f); // reserve the button row
        g.setColour(juce::Colour(0xFFCCCCCC));
        g.setFont(juce::Font(13.0f));
        g.drawFittedText(
            "Monolit sends anonymous bug reports to help fix issues. "
            "You can turn them off anytime in Settings.",
            inner.toNearestInt(), juce::Justification::topLeft, 4);
    }

    void resized() override
    {
        auto row = getPanelBounds().reduced(16.0f).removeFromBottom(26.0f);
        gotItButton.setBounds(row.removeFromRight(84.0f).toNearestInt());
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (!getPanelBounds().contains(e.getPosition().toFloat()))
            if (onDismiss) onDismiss();
    }

    std::function<void()> onDismiss;

private:
    juce::Rectangle<float> getPanelBounds() const
    {
        const float w = juce::jmin(300.0f, static_cast<float>(getWidth())  - 20.0f);
        const float h = juce::jmin(150.0f, static_cast<float>(getHeight()) - 20.0f);
        return juce::Rectangle<float>(w, h).withCentre(getLocalBounds().getCentre().toFloat());
    }

    juce::TextButton gotItButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FirstRunNotice)
};
```

- [ ] **Step 2: Declare the member + methods** in `Source/PluginEditor.h`. Next to the settings-overlay member (`std::unique_ptr<SettingsOverlay> settingsOverlay;`, ~line 1730) add:

```cpp
    std::unique_ptr<FirstRunNotice> firstRunNotice;
```

Next to `void showSettingsOverlay();` / `void hideSettingsOverlay();` (~lines 1735-1736) add:

```cpp
    void showFirstRunNotice();
    void hideFirstRunNotice();
```

- [ ] **Step 3: Replace the native AlertWindow** in `Source/PluginEditor.cpp`. Change the first-run block (currently lines 603-612):

```cpp
    if (!settingsFileExisted)
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::InfoIcon,
            "",
            "Monolit sends anonymous bug reports to help fix issues. "
            "You can turn them off anytime in Settings.",
            "OK");
        saveSettings();
    }
```

to:

```cpp
    if (!settingsFileExisted)
    {
        showFirstRunNotice();
        saveSettings();
    }
```

- [ ] **Step 4: Reset before LookAndFeel teardown** in `Source/PluginEditor.cpp`. After `settingsOverlay.reset();` (~line 621) add:

```cpp
    firstRunNotice.reset();
```

- [ ] **Step 5: Keep bounds in sync** in `Source/PluginEditor.cpp` `resized()`. After the `settingsOverlay` bounds block (~lines 947-949) add:

```cpp
    if (firstRunNotice)
        firstRunNotice->setBounds(getLocalBounds());
```

- [ ] **Step 6: Add the methods** in `Source/PluginEditor.cpp`, next to `showSettingsOverlay`/`hideSettingsOverlay` (~line 1417):

```cpp
void PluginEditor::showFirstRunNotice()
{
    if (firstRunNotice) return;
    firstRunNotice = std::make_unique<FirstRunNotice>();
    addAndMakeVisible(*firstRunNotice);
    firstRunNotice->setBounds(getLocalBounds());
    firstRunNotice->onDismiss = [this]() { hideFirstRunNotice(); };
}

void PluginEditor::hideFirstRunNotice()
{
    firstRunNotice.reset();
}
```

- [ ] **Step 7: Build the plugin.**

Run: `cmake --build build --target Distortion_VST3 -j` (reuse the existing `build/` dir)
Expected: compiles with no errors; no remaining reference to `showMessageBoxAsync` in `PluginEditor.cpp` (`grep -n showMessageBoxAsync Source/PluginEditor.cpp` → only the preset-save one at ~line 372, not the first-run one).

- [ ] **Step 8: Build + run the existing test suite** (guard against regressions).

Run: `cmake --build build --target <test-target> -j && <run tests>` (match the CI "Build Tests"/"Run Tests" steps in `.github/workflows/build.yml`)
Expected: all assertions pass (parity with the 2462-assertion baseline).

- [ ] **Step 9: Commit.**

```bash
git add Source/PluginEditor.h Source/PluginEditor.cpp docs/superpowers/
git commit -m "Replace first-run AlertWindow with in-canvas notice (fixes headless pluginval Editor crash)"
```

---

## Verification (definitive gate — CI)

Local `xvfb-run` is unavailable, so the headless X11 crash cannot be reproduced locally. The
authoritative check is Linux CI, which must also carry PR #21's `libcurl4-openssl-dev` build
fix so the build reaches pluginval:

- `build-linux` job: `Build Plugin` + `Run Tests` pass, then
  `pluginval --strictness-level 10 ... --validate "…/Monolit Distortion.vst3"` completes the
  **Editor** test (previously `X Error … BadAtom … X_ChangeProperty`) and the job goes green.

## Self-Review

- **Spec coverage:** mechanism (in-canvas component) ✓ Step 1; pattern mirrors SettingsOverlay ✓; trigger unchanged ✓ Step 3; text/no-title ✓ Step 1; dismissal button+click-out+Esc ✓ Step 1; editor wiring (member/methods/resized/dtor) ✓ Steps 2,4,5,6. Out-of-scope native dialogs untouched ✓ (Step 7 grep confirms only preset-save `showMessageBoxAsync` remains).
- **Placeholder scan:** Steps 8's exact test target/command are intentionally deferred to the CI workflow's own commands (no editor test harness exists); all code steps contain full code.
- **Type consistency:** `showFirstRunNotice`/`hideFirstRunNotice`, `firstRunNotice`, `onDismiss` used consistently across Steps 1-6.
