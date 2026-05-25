# Scrollable Settings Panel — Design

**Date:** 2026-05-25
**Status:** Approved for planning

## Problem

The settings overlay panel lays out its content (PROCESSING + INTERFACE sections,
9 rows total) in hardcoded pixel offsets summing to ~412px, drawn in
`SettingsOverlay::paint()` / `resized()` (Source/PluginEditor.h:782–973). This
content height is **never scaled by `windowScalePercent`**.

The panel itself is height-clamped to the window:

```cpp
// PluginEditor.h:1035
const float panelH = juce::jmin(430.0f, static_cast<float>(getHeight()) - 10.0f);
```

So whenever the window is short — at 70–80% window scale, or in compact mode
(oscilloscope folded, base height 180px) — the panel shrinks but the content does
not. The bottom rows (Stereo, Scope Length) are clipped and become unreachable.

A current workaround expands the compact window to a fixed 390px when settings
opens (PluginEditor.cpp:1361–1366), but 390px × 70% scale ≈ 273px still clips the
~412px content. The fix is to decouple content height from panel height by making
the content scrollable.

## Decision

Use `juce::Viewport` (idiomatic JUCE) rather than a manual scroll-offset + custom
slider. The viewport provides a styled scrollbar, mouse-wheel scrolling, and
clipping for free. **The window stays compact** when settings opens — scrolling is
the sole mechanism, and the existing expand hack is removed.

## Architecture

Split the monolithic `SettingsOverlay` into two components.

### `SettingsContent : juce::Component` (new)

- Owns every control: `antiAliasToggle`, `oversamplingCombo`, `autoGainToggle`,
  `linearPhaseToggle`, `windowScaleCombo`, `tooltipsToggle`, `oscilloscopeToggle`,
  `scopeStereoToggle`, `scopeLengthSlider`, plus the parameter attachments and the
  `PillToggleLookAndFeel` / `OverlayComboLnf`.
- Draws all row labels, the PROCESSING / INTERFACE section headers, and dividers in
  its own `paint()`. This is the current overlay `paint()` minus the backdrop,
  panel frame, "SETTINGS" header, and close button.
- Positions controls in its own `resized()` — the current overlay `resized()` body,
  operating on `getLocalBounds()` instead of `getPanelBounds().reduced(16)`.
- Exposes the same callbacks the overlay currently does (`onClose`,
  `onOscilloscopeToggled`, `onScopeChannelModeChanged`, `onWindowScaleChanged`,
  `onScopeLengthChanged`) plus holds the `SettingsState&` and `PluginProcessor&`
  references. The control-setup / lambda wiring moves here from the overlay ctor.
- Defines `static constexpr int kContentHeight` = summed row heights of the
  scrollable region (the rows below the header — roughly the current 412px minus
  the ~32px header strip, ≈ 360px). Final value derived from the row-height sum
  during implementation.

### `SettingsOverlay : juce::Component` (modified)

- Keeps: dark backdrop fill, rounded panel frame + border, the fixed "SETTINGS"
  header text, and the close (×) button — all drawn in `paint()` at panel-relative
  positions, unchanged.
- Keeps `getPanelBounds()` (320px wide, height clamped to window) and
  `getCloseBtnBounds()` unchanged.
- Owns a `juce::Viewport` whose viewed component is a `SettingsContent` (not
  deleted on removal — `setViewedComponent(content, false)` with the overlay owning
  the `std::unique_ptr`).
- `resized()`: carve a fixed header strip (~32px, matching header + top padding)
  off the top of the panel inner area; the viewport fills the remaining inner area.
  Size the content to `(viewport.getMaximumVisibleWidth(), kContentHeight)`.
- Keeps its existing public `std::function` members so `PluginEditor.cpp` wiring is
  untouched; forwards each into the matching `SettingsContent` callback in its
  constructor.
- `mouseDown` (outside-click close + close-button hit test) stays on the overlay.

### Scrollbar styling

```cpp
viewport.setScrollBarsShown(true, false);          // vertical only
viewport.setScrollBarThickness(8);
auto& sb = viewport.getVerticalScrollBar();
sb.setColour(juce::ScrollBar::thumbColourId, juce::Colour(0xFFFF2244));
sb.setColour(juce::ScrollBar::trackColourId, juce::Colour(0xFF333333));
```

Mouse-wheel scrolling is automatic via `Viewport`.

## Removing the expand hack

Delete the compact-mode expand block in `showSettingsOverlay()`
(PluginEditor.cpp:1361–1366) so the window stays compact and the panel scrolls
within it. `hideSettingsOverlay()` already folds back via
`applyWindowScale(settingsState.windowScalePercent)`, so it needs no change.

## Data flow

`PluginEditor` → sets `overlay->onX` callbacks (unchanged) → overlay forwards to
`content->onX` → controls fire them on user interaction → `PluginEditor` lambdas
update `SettingsState` / processor / oscilloscope as today.

## Out of scope

- Scaling content pixel sizes by `windowScalePercent` (scroll makes this
  unnecessary).
- Any change to non-compact (full) window behavior beyond the shared refactor.
- Horizontal scrolling.

## Testing

Layout-only UI; not covered by the processor test suite. Manual verification:

1. Build the plugin.
2. Set Window Scale to 70%.
3. Disable the oscilloscope (compact mode, base height 180px).
4. Open settings: confirm the panel fits the compact window, the neon scrollbar
   renders on the right, and every row down to Scope Length is reachable via scroll
   and mouse-wheel.
5. Confirm header + close button stay pinned while scrolling, outside-click and ×
   both close, and closing folds the window back to compact.
6. Re-check full (oscilloscope-on) mode at 100% to confirm no regression.
