# Project: Distortion (Market-Ready Release Note)

## **Headline: Grit without the Guilt.**
### *A Studio-Grade Saturation Processor built for the modern mix.*

Most digital distortion ruins your low-end and smears your stereo image. We built this plugin to be different. By combining state-of-the-art DSP with professional phase-alignment logic, this processor gives you the heat, grit, and character of analog hardware without the digital artifacts that kill a mix.

---

## **Key Selling Points**

### **1. The "Sub Guard" System**
Protect your foundation. Our Linkwitz-Riley crossover allows you to saturate the mids and highs while keeping your sub-bass 100% clean. Unlike other crossovers, our **Phase-Matched Architecture** ensures that your low-end and high-end sum perfectly, preventing the "hollow" or "weak" crossover point found in amateur plugins.

### **2. 7 Stateful Saturation Models**
We didn't just build a clipper; we simulated the physics of tone.
*   **Tube Overdrive:** Asymmetric saturation with dynamic cathode-bias simulation.
*   **Tape Saturation:** Magnetic hysteresis modeling that responds to the "stiffness" of the signal.
*   **Transformer Saturation:** Rich, heavy harmonic density for "iron" character.
*   **Brutal Fuzz:** Aggressive hard clipping with analog noise injection.
*   **Bit Crusher & Decimator:** Precisely quantized digital destruction.
*   **Diode Clipper:** Classic asymmetric pedal-style grit.

### **3. Master-Grade Audio Integrity**
*   **Fractional Phase Alignment:** A specialized delay-compensation engine ensures that your "Mix" knob is sample-accurate down to the microsecond. No comb-filtering, no phase-smearing—just a solid, reinforced sound.
*   **4x Polyphase Oversampling:** High-definition processing that eliminates aliasing, ensuring your distortion sounds "silky" even at 44.1kHz.
*   **Optical LA-2A Compression:** A dedicated T4-cell simulation to glue your saturation and control transients with a classic, "breathing" response.

### **4. Modern Control & Feedback**
*   **Clean/Gritty Modes:** Toggle your signal flow to place the Tone filter before or after the waveshaper for unique aliasing vs. character semantics.
*   **LFO Modulation Engine:** Modulate everything from Drive to Tone Filter sweeps with 5 synchronized waveforms.
*   **Pro Safety:** Built-in DC blocking, Auto-Gain compensation, and a stereo-linked Peak Limiter for DAC protection.

---

## **Technical Specs for Professionals**
*   **Architecture:** C++ / JUCE with high-performance SIMD-friendly loops.
*   **Stability:** Emergency NaN/Infinity guards and Flush-to-Zero (FTZ) denormal protection.
*   **Phase:** Linear-interpolated parameter smoothing for click-free automation.
*   **Latency:** Sample-accurate reporting to host DAW with internal fractional compensation.

---

## **Formats & Platforms**
*   **Windows / Linux:** VST3 + Standalone.
*   **macOS:** VST3 + **AU** + Standalone, universal binary (Apple Silicon + Intel) — fully at home in Logic Pro and GarageBand.
*   **Free-draw Graphic EQ:** Draw a correction curve straight onto the oscilloscope — 12 automatable bands at the output stage, bypassable in one click, and completely transparent when flat.

## **Privacy**
Sledge Distortion can send **anonymous** bug reports — version, OS, host, sample rate, and anomaly counts. **No audio, no presets, no file paths, no personal data.** It's on by default, explained by a one-time notice on first launch, and switched off any time in Settings. Full details: **[Privacy & Bug Reporting](../docs/PRIVACY.md)**.

---

### **Closing Note**
This is not just another "fuzz" box. It is a surgical tone-shaping tool designed for engineers who need their tracks to sound **heavy, expensive, and clear.**
