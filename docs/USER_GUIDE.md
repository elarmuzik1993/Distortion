# Sledge Distortion: User Guide

Sledge Distortion is a distortion plugin for bass, 808s, guitars, drums and vocals. It
keeps your low end clean while it distorts the rest, and it can also play your own NAM amp
and pedal captures. This guide explains every control in plain terms.

Not installed yet? See [INSTALL.md](../INSTALL.md).

---

## Quick start

1. **Insert it** on a track, like any other effect.
2. **Pick a preset** from the box at the top left. *808 Safe* and *Bass Driver* are good
   starting points for low end, *Warm Tube* and *Vintage Tape* for gentle colour, *Hard
   Clip* and *EXTREME* for heavy sounds.
3. **Turn Distortion Amount** (the big knob in the middle) until you like the grit. A fresh
   instance starts at 20%, so it already does something.
4. **Set Sub Guard** if the bass gets muddy: it keeps everything below its frequency clean.
5. **Use Dist Mix** to blend clean signal back in, and **Output Gain** to match the level.

Auto Gain is on by default, so turning the drive up does not make the track much louder.

---

## The main knobs (bottom row, left to right)

| Knob | What it does |
|---|---|
| **Sub Guard** | Keeps the lows clean. Everything below the set frequency (up to 200 Hz) bypasses the distortion and is added back afterwards. Turn it fully left (25 Hz or below) to switch it off. Start around 80–120 Hz for 808s and bass. |
| **Input Gain** | How hard the signal hits the distortion. The centre is unchanged; fully right adds 9 dB, fully left turns it down to silence. |
| **Shape** | One-knob voicing. Left (Bark) pushes the mids for a forward, aggressive sound; centre (Flat) does nothing; right (Scoop) cuts the mids and brightens the top. Double-click to reset to Flat. |
| **Dist Mix** | Blend of distorted and clean signal inside the distortion stage. 100% is fully distorted; lower it for parallel distortion that keeps the original punch. |
| **Distortion Amount** | How much distortion. Below 0.5% (with the compressor off) the distortion is skipped entirely and the sound passes through clean. |
| **Tone** | Smooths the top end of the distortion, from 2 kHz (dark) to 20 kHz (fully open). |
| **Wave Mix** | Blends in an extra waveshaper for more harmonic density. 0% leaves it out. |
| **Output Gain** | Final level, ±9 dB. The centre is unchanged. |

**MIX** (the slider under the preset box) blends the whole processed sound with your dry
signal. 100% is fully processed.

---

## The clip-type column (between Distortion Amount and Tone)

- **Clip type** (top button): the character of the distortion. Click to step to the next
  one, double-click for the full list:
  - *Brutal Fuzz*: thick, saturated fuzz
  - *Tube*: warm, smooth overdrive
  - *Bit Crusher*: lo-fi, reduced bit depth
  - *Tape*: soft, compressed saturation
  - *Transformer*: dense, gluey saturation
  - *Diode*: hard, bright clipping
  - *Decimator*: lo-fi, reduced sample rate
- **EXTREME**: pushes the drive another 9 dB for very heavy sounds (the glass artwork
  cracks while it is on).
- **BOOST**: a clean lift in front of the distortion that drives it harder and brighter,
  like a boost pedal before an amp.
- **PROFILE**: plays a NAM capture instead of the clip type (see below).
- **CLIP / NAM** switch (under the column): chooses whether the clip type or the loaded
  NAM profile plays.

---

## NAM profiles (prototype)

NAM (Neural Amp Modeler) profiles are `.nam` files that capture a real amp or pedal. Many
are free to download online.

- **Load profiles**: click **PROFILE** and choose **Load profile...** (you can pick several),
  or simply **drag `.nam` files or whole folders onto the plugin window**. They are copied
  into Sledge's own Profiles folder, so your projects keep working if you move or delete the
  originals.
- **Browse**: the small arrows on the ends of the PROFILE button step through your library.
  A card shows the name of each profile as it loads.
- **Switch engines**: the **CLIP / NAM** switch flips between the clip types and the loaded
  profile without reloading. With nothing loaded, NAM brings back your last profile.
- **Remove**: PROFILE → **Clear profile**. **Show library folder** opens the folder in your
  file browser.
- Distortion Amount sets how hard the capture is driven, and EXTREME pushes it further.
- New instances reopen your last profile; turn that off in Settings → **Recall Profile**.

---

## Top bar

- **Preset box**: factory presets and your own. Choose **Save Preset...** to save the
  current sound, **Delete Preset...** to remove one of yours.
- **Randomize**: a random sound to spark ideas. **Right-click** it to lock controls so
  Randomize leaves them alone (you can also right-click a knob to lock it).
- **Gear**: opens Settings.
- **Scope, XY and EQ buttons**: show or hide the oscilloscope, and turn the XY Morph pad or
  the graphic EQ on or off (only one of the two at a time).
- **LFO** and **COMPRESSION**: tick the box to switch each on, click the name to open its
  panel.

---

## The screen: oscilloscope, XY Morph and graphic EQ

The large display shows your waveform. It can also host one of two tools:

**XY Morph**: drag anywhere on the display to morph several distortion settings at once.
Left to right adds drive; bottom to top goes from Bark (mid push) to Scoop (mid cut). A puck and a short trail
show where you are, with a readout of the drive and Shape.

**Graphic EQ**: draw an EQ curve straight onto the display. Each of the 31 dots is one
third of an octave, from 20 Hz to 20 kHz, and can cut down to −24 dB or boost up to +12 dB.

- Drag across the dots to draw. Hover a dot to see its frequency and level.
- **Double-click** to flatten the whole curve.
- The **first and last dots are filters**: drag the first one down for a high-pass (low
  cut), the last one down for a low-pass (high cut). At 0 dB or above they are off. A ring
  on the curve marks where each filter starts.
- The **power button** in the top-left corner bypasses the EQ for an A/B comparison; your
  curve is kept.
- The EQ is part of each preset and session, and works even with the distortion off, so
  you can use Sledge as an EQ alone.

---

## LFO (movement)

The LFO moves a control up and down automatically, for wobbles, pulses and tremolo.

- **Rate**: speed, in Hz. Press **SYNC** to lock it to your song's tempo instead; Rate then
  picks a note length (1/1 to 1/32, including triplets).
- **LFO Depth**: how far it moves the target.
- **Wave**: the movement's shape: sine, triangle, square, saw or random.
- **Target**: what it moves: Distortion amount, Tone, Shape, Dist Mix or Output Gain.
- **INV**: reverses the direction of the movement.

Knobs being moved by the LFO pulse so you can see what is happening.

---

## Compression

An LA-2A-style compressor that evens out the level after the distortion.

- **Peak Reduction**: how much it compresses. The meter shows the gain reduction.
- **Makeup Gain**: brings the level back up after compressing (±12 dB).
- **Mode**: *Compress* (gentle, 3:1) or *Limit* (firm, 12:1).

---

## Settings (gear button)

**Processing**
- **Anti-Alias**: a smoother, less harsh distortion order. Off is grittier.
- **Oversampling**: Off, 2x or 4x (default). Higher sounds cleaner on bright, heavy
  distortion and uses more CPU.
- **Auto Gain**: keeps the level steady as you change the drive. On by default.
- **Lin. Phase Dry**: keeps the clean signal perfectly lined up with the processed one when
  you blend them with MIX. Leave it off unless you hear phasing at partial mix.
- **Legacy Filter**: turns the Shape knob back into the filter knob from version 2.3, for
  old projects. Sessions that used it switch it on by themselves.
- **Recall Profile**: new instances reopen your last NAM profile.
- **Mono Input**: if a mono source (a bass or guitar on one input) plays from only one
  speaker, switch this on to centre it. A light in the title bar tells you when Sledge
  spots a one-sided signal.

**Interface**
- **Window Scale**: plugin size, 70% to 100%.
- **Tooltips**: hover help on the controls.
- **Oscilloscope**: switch it off to fold the window down to a compact strip of controls.
- **Overlay**: Off, XY Morph or Graphic EQ on the display.
- **Stereo** and **Scope Length**: how the oscilloscope draws.

**Diagnostics**
- **Bug Reports**: sends anonymous crash and error reports so problems can be fixed. On by
  default; switch it off any time. **Report a Bug** lets you describe a problem yourself.
  What is sent is described in [PRIVACY.md](PRIVACY.md).

---

## Tips

- **808s and bass**: Sub Guard at 80–120 Hz, Shape slightly toward Bark, Tube or Tape.
  The sub stays clean and solid; the distortion adds harmonics you hear on small speakers.
- **Drums**: Transformer or Tape with Dist Mix around 50% keeps the transient punch.
- **Vocals**: low Distortion Amount, Tube, Dist Mix 30–50%, Tone around 8–10 kHz.
- **Too harsh?** Lower Tone, try Shape toward Bark, or switch on Anti-Alias.
- **Too quiet or loud after tweaking?** Check that Auto Gain is on, then trim Output Gain.

---

## Help

Found a bug or have a question? Use **Report a Bug** in Settings, or open an issue at
<https://github.com/elarmuzik1993/Distortion/issues>. Telling us which DAW you use and
whether Sledge worked in it helps a lot:
[DAW compatibility results](https://github.com/elarmuzik1993/Distortion/issues/39).
