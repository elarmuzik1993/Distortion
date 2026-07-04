# First-Run Consent Notice — In-Canvas (fix headless pluginval crash) — Design

**Date:** 2026-07-04
**Status:** Approved design; ready for implementation plan
**Component:** `PluginEditor` (UI) — first-run consent notice
**Obsidian:** [[03 Projects/Monolit Distortion]]

---

## Context

The USE-53 bug-report pipeline added a one-time first-run consent notice. It is shown from the
editor constructor via a **native** `juce::AlertWindow::showMessageBoxAsync(...)`
(`Source/PluginEditor.cpp:605-610`), guarded by `!settingsFileExisted`.

On a fresh machine the settings file never exists, so the notice fires on **every** first
editor open. Under CI, `pluginval --strictness-level 10` opens the editor headlessly via
`xvfb-run` (no window manager). Creating a native window in that environment triggers an X11
error and aborts the process:

```
Starting tests in: pluginval / Editor...
X Error of failed request:  BadAtom (invalid Atom parameter)
  Major opcode of failed request:  18 (X_ChangeProperty)   Atom id: 0x0
##[error]Process completed with exit code 1.
```

This is the Linux CI blocker exposed once PR #21 fixed the `JUCE_USE_CURL` build failure. The
last green Linux run (2026-06-17) predated this notice and passed the Editor test (2462
assertions OK).

Beyond CI, popping a **native modal dialog from an editor constructor** is fragile across
hosts and undesirable while a host is merely scanning/validating the plugin.

## Locked decisions

| Decision | Choice |
|---|---|
| **Mechanism** | In-canvas child `juce::Component` overlay — **no native window** |
| **Pattern** | Mirror the existing `SettingsOverlay` (`PluginEditor.h:1194`) |
| **Trigger** | Unchanged: first run (`!settingsFileExisted`), still `saveSettings()` once |
| **Text** | Unchanged: "Monolit sends anonymous bug reports to help fix issues. You can turn them off anytime in Settings." |
| **Title** | None (keeps the recent "drop redundant title" decision) |
| **Dismissal** | "Got it" button **+** click-outside-panel **+** Esc (via `Button::addShortcut`) |

## Component: `FirstRunNotice`

A `juce::Component` declared alongside `SettingsOverlay` in `PluginEditor.h`.

- `paint()` — dark backdrop `0xD9000000`; centered rounded panel (~300×150) filled
  `0xFF111111`, 1px border `0xFFFF2244`, radius 6; message text `0xFFAAAAAA` ~13px, no title.
- `gotItButton` (`juce::TextButton "Got it"`) bottom-right → invokes `onDismiss`.
  `gotItButton.addShortcut(juce::KeyPress(juce::KeyPress::escapeKey))` for Esc — routes
  through the button's shortcut path, so no manual `grabKeyboardFocus()` during construction.
- `mouseDown` — click outside the panel bounds → `onDismiss`.
- `setInterceptsMouseClicks(true, true)` so controls behind it don't receive the click.
- Public `std::function<void()> onDismiss;`

### Interface
- **Does:** displays the one-time consent notice inside the editor and reports dismissal.
- **Use:** editor constructs it on first run, adds as child, sets `onDismiss` to tear it down.
- **Depends on:** nothing beyond JUCE + `Fonts` (already used by `SettingsOverlay`).

## Editor integration (`PluginEditor.h` / `.cpp`)

1. Add member `std::unique_ptr<FirstRunNotice> firstRunNotice;` and private methods
   `showFirstRunNotice()` / `hideFirstRunNotice()`, mirroring the settings-overlay methods.
2. Replace the `AlertWindow::showMessageBoxAsync(...)` block at `PluginEditor.cpp:605-610`
   with `showFirstRunNotice();` (keep the surrounding `saveSettings()`).
3. In `resized()`, if `firstRunNotice` is present set its bounds to `getLocalBounds()`
   (as `settingsOverlay` does at `PluginEditor.cpp:947-949`).
4. In `~PluginEditor()`, `firstRunNotice.reset();` before LookAndFeel teardown
   (as `settingsOverlay.reset()` at `PluginEditor.cpp:620-621`).
5. `showFirstRunNotice()` creates it, `addAndMakeVisible`, `setBounds(getLocalBounds())`,
   wires `onDismiss = [this]{ hideFirstRunNotice(); }`. `hideFirstRunNotice()` resets it.

## Out of scope (deliberately)

- The Report-a-Bug `DialogWindow` and preset-save `AlertWindow` remain native — they only
  open on user interaction, so pluginval never triggers them and they do not affect CI.
  A future pass could migrate them for cross-host robustness, but not here.

## Verification

- Local: plugin builds; existing unit-test suite passes (no regression).
- CI (definitive): `build-linux` reaches and passes `pluginval --strictness-level 10`,
  including the **Editor** test, under `xvfb-run` (local `xvfb-run` is unavailable, so the
  headless crash can only be reproduced on CI). This requires PR #21's `libcurl` build fix
  to also be present so the Linux build compiles far enough to run pluginval.
