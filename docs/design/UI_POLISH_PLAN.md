# UI Polish Plan — Sledge Distortion

Pre-launch visual polish, captured from the design review of the v2.1 UI. Goal:
keep the (strong) shattered-glass / crimson identity, but fix the things that
make it read "samey / unfinished" in static shots and hard to parse in use.

**Guiding principle:** this is *polish, not a redesign*. The concept is right —
we're raising execution, not changing direction.

---

## What's already good (do NOT touch)

- **Shattered-glass + scratched texture concept** — thematically perfect
  (distortion = a broken signal; "Sledge" = the hammer that broke it). Keep it.
- **Crimson-on-dark identity** — correct emotional register for aggressive
  distortion. Keep the palette's *soul*; we're adding hierarchy, not repainting.
- **EXTREME whole-UI "ignition"** — flipping `extremeEnabled` heats the entire
  panel from cool→hot red. Loudest possible state feedback, on-brand, demo-worthy.
  This is the *reference bar* the rest of the plan aims to match.

---

## Priorities (ranked by impact)

### P0 — State clarity: propagate the EXTREME-level legibility to every control
**Problem:** EXTREME shouts its state (whole-UI ignition); the smaller toggles
(`BOOST`/`cleanBoost`, `LFO`/`lfoEnableToggle`, `COMP`/`compEnableToggle`,
`SYNC`/`lfoBpmSyncButton`, `INV`/`lfoInvertButton`, `TR`/`Tone` targets) whisper
theirs via a subtle filled-vs-outline pill. Once the eye learns EXTREME's loud
feedback, the quiet toggles feel inconsistent and their on/off state is easy to
misread.

**Fix:** one consistent active/inactive language for *all* toggles —
- **ON:** bright fill + subtle outer glow, white text.
- **OFF:** desaturated/dim fill, muted text (clearly "asleep"), not just a
  thinner border.
- The gap between on and off must be readable at a glance, in both the cool and
  ignited background states.

**Where:** today each button sets its own colours inline (e.g.
`PluginEditor.cpp:319–322` for `cleanBoostButton`). **Centralize this** — a shared
`LookAndFeel` (or a `styleToggle()` helper) that every toggle routes through, so
"on/off" is defined once and applied uniformly. This kills the scattered
per-button colour code and guarantees consistency.

*Do not* make everything ignite like EXTREME — that's chaos. EXTREME stays the
one "loud" control; everything else shares one clear "normal" tier.

### P1 — Colour hierarchy: stop everything glowing red at once
**Problem:** preset name, LFO, Compression, knob arcs, and every toggle all glow
the same red → no hierarchy, nothing leads the eye, reads "busy/samey" in stills.

**Fix:** a small, intentional colour language:
- **Red** = drive / distortion / output (the core identity).
- **Cyan** = modulation (already used for the LFO pulsing-glow on modulated
  knobs — extend it consistently to LFO controls / SYNC / INV so "modulation" is
  visually its own thing).
- **Neutral gray tier** for inactive/idle controls so active elements pop.

Result: the eye can find the important control; the UI gains depth without losing
the crimson soul.

### P1 — Label legibility + truncation
**Problem:**
- Knob labels ("Sub Guard", "Input Gain", "Wave Mix") are dim gray on the busy
  texture — low contrast.
- Truncated text: **"Peak Re…" / "Makeup…" / "Comp…"** reads unfinished. The
  strings are full ("Peak Reduction", "Makeup Gain", "COMP" — see
  `PluginEditor.cpp:202–224`); they're being **clipped by component width**, not
  abbreviated on purpose.

**Fix:**
- Raise label contrast (brighter text and/or a subtle dark scrim behind label
  rows — see P2 texture zoning).
- Fix the label bounds in `resized()` so full text fits, or drop the font size a
  hair / use two lines. No "…" in the shipping UI.

### P2 — Texture zoning: keep the grit, protect the words
**Problem:** the shattered texture sits at one intensity everywhere — great as
atmosphere, but it competes with small labels where it sits *behind text*.

**Fix:** give the texture a **loud/quiet rhythm** —
- Full grit in decorative/hero zones (scope surround, big panels, header
  backdrop).
- Dial opacity back ~15–20% (or add a subtle dark scrim) directly under label
  rows and the bottom knob strip, where the eye needs to read.

Same texture, same vibe — just quieter where it carries information. Concept
stays 100% intact.

**Idle vs. ignited — protect the contrast (load-bearing rule).** The cool→hot
shift when `extremeEnabled` fires is the UI's single best moment, and it works
because it's a *delta*: restrained idle → red bloom on ignition. So the variable
that swings between states must be **heat/glow (cool → hot red), never texture
presence**. Red is EXTREME's currency — don't spend it in the idle state.
- If the idle (non-EXTREME) state reads "empty," make it **richer in cool/dark
  tones only** — deepen the shattered-glass detail, a subtle vignette, faint cool
  edge-lighting — so idle reads "moody premium dark," not "unfinished."
- **Never** add red / brightness / glow to idle to make the texture "more
  obvious" — that pre-spends the ignition contrast and flattens the EXTREME payoff.
- Idle = rich, cool, dark. EXTREME = the same surface catching fire.
- Sanity test: side by side, idle should read as "the calm before" and EXTREME as
  "the impact." Added richness stays in the decorative zones — still quiet under
  the label rows.
- Reminder: "idle looks empty" is mostly fixed by hierarchy + the live scope + a
  drawn EQ curve, not by louder texture.

### P2 — BF / EXTREME / BOOST grouping
**Problem:** "BF" is a *state readout* (current clip type / Brutal Fuzz) but it's
styled like a button and stacked with two *toggles* (EXTREME, BOOST). Three items
look like one button group; newcomers can't tell readout from switch.

**Fix:** visually separate the readout from the toggles — e.g. give the clip-type
readout a different shape/treatment (a label/badge, not a pill), or add spacing /
a divider so EXTREME + BOOST clearly read as the switch pair. Keep the words
"EXTREME" / "BOOST" — they're evocative and on-brand; it's the grouping that
misleads.

### P3 — At-rest discoverability of the hook
**Context:** the scope works live (this was a muted static shot). But the
signature features — draw-EQ on the scope + the XY morph pad — are *invisible
until activated*, so a new user who hasn't engaged an overlay has no cue they can
draw/morph.

**Fix (low-effort, high-payoff):** a subtle at-rest affordance when an overlay is
active — a faint "draw here" ghost hint, a dim default curve, or a brief
first-run pulse on the overlay toggles — so the thing we market as the star is
*discoverable* without reading docs. Directly supports the locked hook
("sculpt your distortion").

### P3 — Ignited-state legibility check
When EXTREME is on and the whole panel washes brighter red, **verify knob labels
and values stay readable** against the hotter background. If contrast drops, the
label-contrast work in P1 should account for *both* background states (cool + hot),
not just the default.

---

## Cross-cutting refactor (enables P0/P1 cleanly)

**Centralize the theme.** Colours are currently set per-component inline. Introduce
a single source of truth — a `LookAndFeel` subclass and/or a `DesignTokens`
header (`Source/`) holding: the red/cyan/neutral roles, on/off toggle states, and
label colours. Every control routes through it. This turns P0 and P1 from
"edit 20 call sites" into "edit one table," and keeps future UI consistent.

---

## Explicitly out of scope (don't gold-plate)

- No new layout / no moving the signal-flow arrangement.
- No new features. This is visual polish on existing controls.
- Don't touch the DSP, the scope engine, or the texture *art* itself.

---

## Marketing tie-in — the "screenshot recipe"

Once P0–P2 land, capture every marketing still the same way so the hook and the
polish both show (feeds `docs/marketing/DEMO_SCRIPT.md`):

1. **EXTREME ON** — the ignited hot-red state (far more photogenic than the dark
   idle state, which reads empty).
2. **Scope live** — audio playing, waveform visible.
3. **A drawn EQ curve** on the overlay — proves the hook at a glance, even as a
   still.
4. Optionally the **XY pad** engaged.

Never publish a dark/idle/empty-scope frame — that's the worst-case shot.

---

## Suggested sequence

1. Centralize theme (`LookAndFeel` / `DesignTokens`).
2. P0 toggle state system → P1 colour hierarchy (they share the token work).
3. P1 label contrast + truncation fix.
4. P2 texture zoning + BF/toggle grouping.
5. P3 at-rest hook affordance + ignited-state legibility pass.
6. Re-shoot marketing stills with the recipe above.
