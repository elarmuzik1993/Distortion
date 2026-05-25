# Scrollable Settings Panel Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the settings overlay's content scroll inside a fixed panel so rows are never clipped in compact mode or at reduced window scales.

**Architecture:** Split the monolithic `SettingsOverlay` into a new `SettingsContent` component (owns all controls + draws all row labels) wrapped in a `juce::Viewport`, while `SettingsOverlay` keeps the backdrop, panel frame, "SETTINGS" header, and close button. Remove the compact-mode window-expand hack so the window stays compact and scrolling is the sole mechanism.

**Tech Stack:** C++17, JUCE 7 (`juce::Viewport`, `juce::Component`), CMake build.

**Design reference:** `docs/superpowers/specs/2026-05-25-scrollable-settings-panel-design.md`

**Note on testing:** This is layout-only UI with no unit-test harness in the repo (tests cover `PluginProcessor` DSP only). The verification gate for each code task is **the plugin compiles** and **the existing test suite still passes** (regression guard). Final correctness is confirmed by the manual verification task at the end.

---

## File Structure

- **Modify:** `Source/PluginEditor.h`
  - Add new class `SettingsContent` (inserted immediately before the existing `SettingsOverlay`, i.e. before the `// Settings overlay modal panel` comment at line 655).
  - Rewrite the existing `SettingsOverlay` class (lines 655–1046) to host a `Viewport` + `SettingsContent` instead of owning controls directly.
- **Modify:** `Source/PluginEditor.cpp`
  - Remove the compact-mode expand block in `showSettingsOverlay()` (lines 1361–1366).

No other files change. `PluginEditor.cpp`'s callback wiring (`settingsOverlay->onClose = …` etc.) is untouched because `SettingsOverlay` keeps its public `std::function` members.

---

## Task 1: Add `SettingsContent` component

This new class is self-contained and compiles even while the old `SettingsOverlay` still owns its own copies of the controls (different classes, so duplicate member names do not collide). It is wired in Task 2.

**Files:**
- Modify: `Source/PluginEditor.h` (insert new class before line 655, the `// Settings overlay modal panel` comment)

- [ ] **Step 1: Insert the `SettingsContent` class**

Insert the following complete class immediately before the line `// Settings overlay modal panel`:

```cpp
// Scrollable content for the settings overlay (everything below the fixed header).
// Lives inside a juce::Viewport so rows are never clipped at small window sizes.
class SettingsContent : public juce::Component
{
public:
    // Summed height of all rows below the header (336px of content + 16px bottom slack).
    static constexpr int kContentHeight = 352;

    SettingsContent(juce::AudioProcessorValueTreeState& apvts, SettingsState& state, PluginProcessor& proc)
        : settingsState(state), processor(proc)
    {
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
            int mode = oversamplingCombo.getSelectedId() - 1; // 0=Off, 1=2x, 2=4x
            settingsState.oversamplingMode = mode;
            processor.requestOversamplingRebuild(mode);
        };

        // Auto Gain toggle
        addAndMakeVisible(autoGainToggle);
        autoGainToggle.setButtonText("");
        autoGainToggle.setLookAndFeel(&pillLnf);
        autoGainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            apvts, "autoGainEnabled", autoGainToggle);

        // Linear Phase Dry toggle
        addAndMakeVisible(linearPhaseToggle);
        linearPhaseToggle.setButtonText("");
        linearPhaseToggle.setLookAndFeel(&pillLnf);
        {
            bool lpOn = apvts.getRawParameterValue("linearPhaseDry")->load() > 0.5f;
            linearPhaseToggle.setToggleState(lpOn, juce::dontSendNotification);
        }
        linearPhaseToggle.onClick = [this, &apvts]()
        {
            bool on = linearPhaseToggle.getToggleState();
            if (auto* param = apvts.getParameter("linearPhaseDry"))
                param->setValueNotifyingHost(on ? 1.0f : 0.0f);
            processor.requestOversamplingRebuild(settingsState.oversamplingMode);
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

        // Scope Length slider
        addAndMakeVisible(scopeLengthSlider);
        scopeLengthSlider.setRange(64.0, 1024.0, 1.0);
        scopeLengthSlider.setValue(state.scopeLength, juce::dontSendNotification);
        scopeLengthSlider.setSliderStyle(juce::Slider::LinearHorizontal);
        scopeLengthSlider.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
        scopeLengthSlider.setColour(juce::Slider::trackColourId, juce::Colour(0xFFFF2244));
        scopeLengthSlider.setColour(juce::Slider::thumbColourId, juce::Colour(0xFFFF2244));
        scopeLengthSlider.setColour(juce::Slider::backgroundColourId, juce::Colour(0xFF333333));
        scopeLengthSlider.onValueChange = [this]() {
            int val = static_cast<int>(scopeLengthSlider.getValue());
            settingsState.scopeLength = val;
            if (onScopeLengthChanged)
                onScopeLengthChanged(val);
        };
    }

    ~SettingsContent() override
    {
        antiAliasToggle.setLookAndFeel(nullptr);
        autoGainToggle.setLookAndFeel(nullptr);
        linearPhaseToggle.setLookAndFeel(nullptr);
        tooltipsToggle.setLookAndFeel(nullptr);
        oscilloscopeToggle.setLookAndFeel(nullptr);
        scopeStereoToggle.setLookAndFeel(nullptr);
        oversamplingCombo.setLookAndFeel(nullptr);
        windowScaleCombo.setLookAndFeel(nullptr);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xFF111111)); // panel bg behind scrolled content (avoids smear)

        auto inner = getLocalBounds().toFloat();

        // PROCESSING section header
        g.setFont(Fonts::getOrbitron(10.0f, true));
        g.setColour(juce::Colour(0xFFFF2244).withAlpha(0.6f));
        g.drawText("PROCESSING", inner.removeFromTop(18.0f), juce::Justification::centredLeft);

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
        inner.removeFromTop(6.0f);

        // Auto Gain row label
        auto agRow = inner.removeFromTop(24.0f);
        g.drawText("Auto Gain", agRow.removeFromLeft(140.0f), juce::Justification::centredLeft);
        inner.removeFromTop(6.0f);

        // Linear Phase Dry row label
        auto lpRow = inner.removeFromTop(24.0f);
        g.drawText("Lin. Phase Dry", lpRow.removeFromLeft(140.0f), juce::Justification::centredLeft);
        inner.removeFromTop(16.0f);

        // INTERFACE section header
        g.setFont(Fonts::getOrbitron(10.0f, true));
        g.setColour(juce::Colour(0xFFFF2244).withAlpha(0.6f));
        g.drawText("INTERFACE", inner.removeFromTop(18.0f), juce::Justification::centredLeft);

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
        inner.removeFromTop(6.0f);

        // Scope Length row label
        auto slRow = inner.removeFromTop(24.0f);
        g.drawText("Scope Length", slRow.removeFromLeft(140.0f), juce::Justification::centredLeft);
    }

    void resized() override
    {
        auto inner = getLocalBounds();

        // PROCESSING header + divider
        inner.removeFromTop(18);
        inner.removeFromTop(4);
        inner.removeFromTop(1);
        inner.removeFromTop(8);

        // Anti-Alias row
        auto aaRow = inner.removeFromTop(24);
        aaRow.removeFromLeft(140);
        antiAliasToggle.setBounds(aaRow.removeFromLeft(50).reduced(0, 2));
        inner.removeFromTop(6);

        // Oversampling row
        auto osRow = inner.removeFromTop(24);
        osRow.removeFromLeft(140);
        oversamplingCombo.setBounds(osRow.removeFromLeft(70).reduced(0, 2));
        inner.removeFromTop(6);

        // Auto Gain row
        auto agRow = inner.removeFromTop(24);
        agRow.removeFromLeft(140);
        autoGainToggle.setBounds(agRow.removeFromLeft(50).reduced(0, 2));
        inner.removeFromTop(6);

        // Linear Phase Dry row
        auto lpRow = inner.removeFromTop(24);
        lpRow.removeFromLeft(140);
        linearPhaseToggle.setBounds(lpRow.removeFromLeft(50).reduced(0, 2));
        inner.removeFromTop(16);

        // INTERFACE header + divider
        inner.removeFromTop(18);
        inner.removeFromTop(4);
        inner.removeFromTop(1);
        inner.removeFromTop(8);

        // Window Scale row
        auto wsRow = inner.removeFromTop(24);
        wsRow.removeFromLeft(140);
        windowScaleCombo.setBounds(wsRow.removeFromLeft(70).reduced(0, 2));
        inner.removeFromTop(6);

        // Tooltips row
        auto ttRow = inner.removeFromTop(24);
        ttRow.removeFromLeft(140);
        tooltipsToggle.setBounds(ttRow.removeFromLeft(50).reduced(0, 2));
        inner.removeFromTop(6);

        // Oscilloscope row
        auto scRow = inner.removeFromTop(24);
        scRow.removeFromLeft(140);
        oscilloscopeToggle.setBounds(scRow.removeFromLeft(50).reduced(0, 2));
        inner.removeFromTop(6);

        // Stereo row
        auto stRow = inner.removeFromTop(24);
        stRow.removeFromLeft(140);
        scopeStereoToggle.setBounds(stRow.removeFromLeft(50).reduced(0, 2));
        inner.removeFromTop(6);

        // Scope Length row
        auto slRow = inner.removeFromTop(24);
        slRow.removeFromLeft(140);
        scopeLengthSlider.setBounds(slRow.removeFromLeft(130).reduced(0, 4));
    }

    std::function<void(bool)> onOscilloscopeToggled;
    std::function<void(bool)> onScopeChannelModeChanged;
    std::function<void(int)>  onWindowScaleChanged;
    std::function<void(int)>  onScopeLengthChanged;

private:
    SettingsState& settingsState;
    PluginProcessor& processor;
    PillToggleLookAndFeel pillLnf;

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
    juce::ToggleButton autoGainToggle;
    juce::ToggleButton linearPhaseToggle;
    juce::ComboBox oversamplingCombo;
    juce::ComboBox windowScaleCombo;
    juce::ToggleButton tooltipsToggle;
    juce::ToggleButton oscilloscopeToggle;
    juce::ToggleButton scopeStereoToggle;
    juce::Slider scopeLengthSlider;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> cleanModeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> autoGainAttachment;
};
```

- [ ] **Step 2: Build to verify it compiles**

Run:
```bash
cmake --build build --target Distortion_Standalone -j$(nproc)
```
Expected: build succeeds (no errors). `SettingsContent` exists but is not yet referenced.

- [ ] **Step 3: Commit**

```bash
git add Source/PluginEditor.h
git commit -m "$(cat <<'EOF'
refactor(ui): add SettingsContent component for scrollable settings

Extracts settings rows/controls into a standalone component; not yet wired.
Prep for Viewport-based scrolling. Ref USE-48

Co-Authored-By: Claude Sonnet 4.6 <noreply@anthropic.com>
EOF
)"
```

---

## Task 2: Rewrite `SettingsOverlay` to host a `Viewport`

Replace the entire existing `SettingsOverlay` class body with a version that owns a `juce::Viewport` + `SettingsContent`, keeps the backdrop/frame/header/close, and forwards callbacks. Its public `std::function` members keep the same names/signatures so `PluginEditor.cpp` needs no changes.

**Files:**
- Modify: `Source/PluginEditor.h` — replace the whole `SettingsOverlay` class (from `// Settings overlay modal panel` through its closing `};`, originally lines 655–1046, now shifted down by Task 1's insertion)

- [ ] **Step 1: Replace the entire `SettingsOverlay` class**

Delete the old `SettingsOverlay` class in full and replace it (including the leading `// Settings overlay modal panel` comment) with:

```cpp
// Settings overlay modal panel — fixed frame + header, with scrollable content.
class SettingsOverlay : public juce::Component
{
public:
    SettingsOverlay(juce::AudioProcessorValueTreeState& apvts, SettingsState& state, PluginProcessor& proc)
    {
        setInterceptsMouseClicks(true, true);

        content = std::make_unique<SettingsContent>(apvts, state, proc);
        content->onOscilloscopeToggled    = [this](bool b) { if (onOscilloscopeToggled)    onOscilloscopeToggled(b); };
        content->onScopeChannelModeChanged = [this](bool b) { if (onScopeChannelModeChanged) onScopeChannelModeChanged(b); };
        content->onWindowScaleChanged      = [this](int p)  { if (onWindowScaleChanged)      onWindowScaleChanged(p); };
        content->onScopeLengthChanged      = [this](int v)  { if (onScopeLengthChanged)      onScopeLengthChanged(v); };

        addAndMakeVisible(viewport);
        viewport.setViewedComponent(content.get(), false); // overlay owns content
        viewport.setScrollBarsShown(true, false);          // vertical only
        viewport.setScrollBarThickness(8);
        viewport.getVerticalScrollBar().setColour(juce::ScrollBar::thumbColourId, juce::Colour(0xFFFF2244));
        viewport.getVerticalScrollBar().setColour(juce::ScrollBar::trackColourId, juce::Colour(0xFF333333));
    }

    void paint(juce::Graphics& g) override
    {
        // Dark backdrop
        g.fillAll(juce::Colour(0xD9000000));

        auto panelBounds = getPanelBounds();

        // Panel background + border
        g.setColour(juce::Colour(0xFF111111));
        g.fillRoundedRectangle(panelBounds, 6.0f);
        g.setColour(juce::Colour(0xFFFF2244));
        g.drawRoundedRectangle(panelBounds, 6.0f, 1.0f);

        // Header: "SETTINGS"
        auto inner = panelBounds.reduced(16.0f);
        g.setFont(Fonts::getOrbitron(12.0f, true));
        g.setColour(juce::Colour(0xFFFF2244));
        g.drawText("SETTINGS", inner.removeFromTop(24.0f), juce::Justification::centredLeft);

        // Close button (X) - top-right of panel
        auto closeBtn = getCloseBtnBounds();
        g.setColour(juce::Colour(0xFFFF2244));
        g.setFont(16.0f);
        g.drawText(juce::CharPointer_UTF8("\xc3\x97"), closeBtn, juce::Justification::centred);
    }

    void resized() override
    {
        auto panelBounds = getPanelBounds();
        auto inner = panelBounds.reduced(16.0f);
        inner.removeFromTop(24.0f); // header
        inner.removeFromTop(8.0f);  // gap

        viewport.setBounds(inner.toNearestInt());
        // Width excludes the vertical scrollbar when it is shown; reflows when not.
        content->setSize(viewport.getMaximumVisibleWidth(), SettingsContent::kContentHeight);
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        // Click outside panel closes overlay
        if (!getPanelBounds().contains(e.getPosition().toFloat()))
        {
            if (onClose) onClose();
            return;
        }

        // Close button (X)
        if (getCloseBtnBounds().contains(e.getPosition().toFloat()))
        {
            if (onClose) onClose();
            return;
        }
    }

    std::function<void()>     onClose;
    std::function<void(bool)> onOscilloscopeToggled;
    std::function<void(bool)> onScopeChannelModeChanged;
    std::function<void(int)>  onWindowScaleChanged;
    std::function<void(int)>  onScopeLengthChanged;

private:
    juce::Viewport viewport;
    std::unique_ptr<SettingsContent> content;

    juce::Rectangle<float> getPanelBounds() const
    {
        const float panelW = 320.0f;
        const float panelH = juce::jmin(430.0f, static_cast<float>(getHeight()) - 10.0f);
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
```

- [ ] **Step 2: Build the plugin to verify it compiles**

Run:
```bash
cmake --build build --target Distortion_Standalone -j$(nproc)
```
Expected: build succeeds. The settings overlay now uses the viewport; `PluginEditor.cpp` is unchanged because the overlay's public callbacks kept their names.

- [ ] **Step 3: Run the existing test suite (regression guard)**

Run:
```bash
cmake --build build --target DistortionTests -j$(nproc) && ./build/DistortionTests_artefacts/Debug/DistortionTests
```
Expected: all tests pass (these cover DSP, which is untouched — confirms no accidental breakage).

- [ ] **Step 4: Commit**

```bash
git add Source/PluginEditor.h
git commit -m "$(cat <<'EOF'
feat(ui): scrollable settings panel via Viewport

SettingsOverlay now hosts a Viewport wrapping SettingsContent so rows scroll
instead of clipping. Fixed header + close button stay pinned. Ref USE-48

Co-Authored-By: Claude Sonnet 4.6 <noreply@anthropic.com>
EOF
)"
```

---

## Task 3: Remove the compact-mode window-expand hack

With scrolling in place, the window no longer needs to grow when settings opens.

**Files:**
- Modify: `Source/PluginEditor.cpp:1361-1366` (inside `showSettingsOverlay()`)

- [ ] **Step 1: Delete the expand block**

Remove these lines from `showSettingsOverlay()` (the block immediately after `if (settingsOverlay) return;`):

```cpp
    // In compact mode, expand just enough to show the settings panel (~390px base covers all rows)
    if (!settingsState.oscilloscopeEnabled)
    {
        const int w = juce::roundToInt(960.0f * settingsState.windowScalePercent / 100.0f);
        const int h = juce::roundToInt(390.0f * settingsState.windowScalePercent / 100.0f);
        setSize(w, h);
    }
```

After removal, the function should go straight from `if (settingsOverlay) return;` to `settingsOverlay = std::make_unique<SettingsOverlay>(...)`. Do not change `hideSettingsOverlay()` — it already folds back via `applyWindowScale(settingsState.windowScalePercent)`.

- [ ] **Step 2: Build to verify it compiles**

Run:
```bash
cmake --build build --target Distortion_Standalone -j$(nproc)
```
Expected: build succeeds.

- [ ] **Step 3: Commit**

```bash
git add Source/PluginEditor.cpp
git commit -m "$(cat <<'EOF'
fix(ui): keep window compact when opening settings

Drop the 390px expand hack now that settings content scrolls; this removes
the residual clipping at reduced window scales. Fixes USE-48

Co-Authored-By: Claude Sonnet 4.6 <noreply@anthropic.com>
EOF
)"
```

---

## Task 4: Manual verification

No automated coverage exists for layout; verify by running the standalone app.

**Files:** none (verification only)

- [ ] **Step 1: Launch the standalone build**

Run:
```bash
"./build/Distortion_artefacts/Debug/Standalone/Monolit Distortion"
```

- [ ] **Step 2: Verify compact-mode scrolling**

1. Open Settings (gear button). In the INTERFACE section set **Window Scale → 70%**.
2. Toggle **Oscilloscope off** to enter compact mode (window folds to ~126px tall at 70%).
3. Re-open Settings. Confirm:
   - [ ] The panel fits inside the compact window (no rows drawn outside the frame).
   - [ ] A neon-red scrollbar (~8px) renders on the right edge of the content area.
   - [ ] Scrolling (drag the bar **and** mouse-wheel over the rows) reaches every row down to **Scope Length**, and the slider is fully usable.
   - [ ] The "SETTINGS" header and the × close button stay pinned (do not scroll).
   - [ ] Clicking outside the panel and clicking × both close the overlay.
   - [ ] After closing, the window folds back to compact height.

- [ ] **Step 3: Verify no regression in full mode**

1. Re-open Settings, toggle **Oscilloscope on**, set **Window Scale → 100%**.
2. Confirm:
   - [ ] All rows are visible without needing to scroll (content shorter than the panel).
   - [ ] No scrollbar artifacts; controls behave exactly as before (toggles, combos, slider all functional).

- [ ] **Step 4: Update changelog if appropriate**

If `CHANGELOG.md` tracks UI fixes, add an entry under the current unreleased section:
```
- Fixed settings panel rows being clipped in compact mode / at small window scales (now scrolls).
```
Then commit:
```bash
git add CHANGELOG.md
git commit -m "docs(changelog): note scrollable settings panel fix. Ref USE-48"
```

---

## Self-Review Notes

- **Spec coverage:** SettingsContent (spec §Architecture) → Task 1. Viewport host + scrollbar styling + callback forwarding (spec §Architecture, §Scrollbar styling) → Task 2. Expand-hack removal (spec §Removing the expand hack) → Task 3. Manual testing (spec §Testing) → Task 4. All spec sections covered.
- **Type consistency:** `SettingsContent::kContentHeight` (defined Task 1) is referenced in Task 2 `resized()`. Callback names (`onOscilloscopeToggled`, `onScopeChannelModeChanged`, `onWindowScaleChanged`, `onScopeLengthChanged`, `onClose`) match between `SettingsContent`, `SettingsOverlay`, and the existing `PluginEditor.cpp` wiring.
- **kContentHeight math:** rows consume 31 (PROCESSING hdr/divider) + 90 (3×30 aa/os/ag) + 40 (lp 24+16) + 31 (INTERFACE hdr/divider) + 120 (4×30 ws/tt/osc/stereo) + 24 (scope length) = 336; `kContentHeight = 352` adds 16px bottom slack.
