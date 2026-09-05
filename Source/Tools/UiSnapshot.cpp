/*
  ==============================================================================

    Offline UI Snapshot Harness

    A headless console tool that builds the real PluginEditor and paints it into
    an image, with no DAW and no standalone window. Host chrome (the standalone's
    menu bar and "audio input is muted" banner) otherwise shifts and clips the
    editor, which makes screenshots an unreliable way to check layout.

    Because the editor is driven purely by setSize() here, this renders exactly
    the geometry a host would give it at each window-scale setting.

    Examples:
      DistortionUiSnapshot --out shots
      DistortionUiSnapshot --scale 70 --out shots
      DistortionUiSnapshot --signal --out shots   # scope shows a waveform

  ==============================================================================
*/

#include <JuceHeader.h>
#include "../PluginProcessor.h"
#include "../PluginEditor.h"

#include <iostream>

namespace
{
// The editor's design size; every scale setting is this times a percentage.
constexpr float kBaseWidth  = 960.0f;
constexpr float kBaseHeight = 564.0f;

// The editor reads its fold state, window scale and overlay mode from
// settings.xml. Left pointing at the user's AppData that makes a render depend
// on whatever this machine last saved: ask for --collapsed on a machine whose
// saved state is expanded and the editor lays out expanded, then gets sized to
// the collapsed height — every panel overlaps the knob row and it looks like a
// layout regression that does not exist. So the harness writes the state it
// wants into a scratch root and renders from that; the real settings are never
// read or touched.
juce::File writeScratchSettings(const juce::File& root, bool collapsed, int scalePercent)
{
    root.createDirectory();

    juce::XmlElement xml("Settings");
    xml.setAttribute("oscilloscope", collapsed ? 0 : 1);
    xml.setAttribute("windowScale", scalePercent);
    xml.setAttribute("oscilloscopeStereo", 1);
    xml.setAttribute("scopeOverlay", 0);   // no overlay intercepting the scope
    xml.setAttribute("tooltips", 0);
    xml.setAttribute("bugReports", 0);     // headless: never queue a report

    auto settings = root.getChildFile("settings.xml");
    xml.writeTo(settings);
    return settings;
}

// Runs a fixed tone through the processor so the oscilloscope has something to
// draw. Opt-in (--signal): with no signal the harness stays a pure layout gate
// whose renders do not depend on DSP behaviour, which is what makes it usable
// for spotting layout regressions.
//
// The tone is deliberately above the Sub Guard crossover (50-200 Hz). Below it
// the low band is routed around the distortion, so a bass tone would render as
// a clean sine and advertise a character the plugin did not apply.
void feedScope(PluginProcessor& proc)
{
    constexpr double sampleRate = 44100.0;
    constexpr int    blockSize  = 512;
    // The scope FIFO holds 2048 samples and is fed one per SCOPE_UPDATE_DECIMATION
    // (2) input samples, so 8 blocks fill it. 32 blocks is ~0.37 s: well past the
    // parameter smoothing ramps and the compressor's attack, so what lands in the
    // FIFO is steady state rather than the onset transient.
    constexpr int    numBlocks  = 32;
    constexpr double freqHz     = 220.0;
    constexpr float  amplitude  = 0.7f;
    // A small offset keeps the two stereo traces distinguishable in the render.
    constexpr double rightPhaseOffset = 0.25;

    // Drive the distortion. Without this the render is a picture of true bypass:
    // distortionAmount defaults to 0 and compEnabled to false, so processBlock
    // takes the bypass branch (PluginProcessor.cpp), which still feeds the scope
    // - producing a clean sine that shows the plugin doing nothing at all.
    // 65% on the default Brutal Fuzz clip is well past the 0.5% bypass threshold
    // and gives visible saturation rather than a barely-bent sine.
    constexpr float driveNormalised = 0.65f;   // range is 0-100, so this is 65%
    if (auto* p = proc.parameters.getParameter("distortionAmount"))
        p->setValueNotifyingHost(driveNormalised);

    juce::AudioBuffer<float> buffer(2, blockSize);
    juce::MidiBuffer midi;

    const double phaseStep = juce::MathConstants<double>::twoPi * freqHz / sampleRate;
    double phase = 0.0;

    for (int b = 0; b < numBlocks; ++b)
    {
        for (int i = 0; i < blockSize; ++i)
        {
            buffer.setSample(0, i, amplitude * (float) std::sin(phase));
            buffer.setSample(1, i, amplitude * (float) std::sin(phase + rightPhaseOffset));
            phase += phaseStep;
        }

        // processBlock writes its output over the input, which is fine: the next
        // block is regenerated from the running phase, not read back.
        proc.processBlock(buffer, midi);
    }
}

bool snapshot(PluginProcessor& proc, int width, int height, const juce::File& out,
              bool extreme = false, bool withSignal = false)
{
    PluginEditor editor(proc);
    editor.setSize(width, height);

    if (extreme)
    {
        if (auto* p = proc.parameters.getParameter("extremeEnabled"))
            p->setValueNotifyingHost(1.0f);

        // The crack crossfade is driven by the scope's timer, so let the message
        // loop run past the fade duration before capturing the settled frame.
        juce::MessageManager::getInstance()->runDispatchLoopUntil(600);
    }

    if (withSignal)
    {
        feedScope(proc);

        // The scope pulls from the FIFO on its own 30 Hz timer, so the message
        // loop has to run at least one tick for the samples to reach the
        // component. fillScopeBuffer early-returns on an empty FIFO without
        // clearing, so the later ticks in this window leave the trace intact.
        juce::MessageManager::getInstance()->runDispatchLoopUntil(150);
    }

    // Lay out and paint synchronously into an offscreen image.
    juce::Image image(juce::Image::ARGB, width, height, true);
    {
        juce::Graphics g(image);
        editor.paintEntireComponent(g, true);
    }

    out.deleteFile();
    juce::PNGImageFormat png;
    std::unique_ptr<juce::FileOutputStream> stream(out.createOutputStream());
    if (stream == nullptr)
        return false;

    const bool ok = png.writeImageToStream(image, *stream);
    std::cout << "  " << out.getFileName() << "  " << width << "x" << height
              << (ok ? "  ok" : "  FAILED") << "\n";
    return ok;
}
} // namespace

//==============================================================================
int main(int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    juce::String outDir = "ui-shots";
    int onlyScale = 0;
    bool collapsed = false;
    bool extreme = false;
    bool signal = false;
    for (int i = 1; i < argc; ++i)
    {
        const juce::String t(argv[i]);
        if (t == "--out" && i + 1 < argc)        outDir    = argv[++i];
        else if (t == "--scale" && i + 1 < argc) onlyScale = juce::String(argv[++i]).getIntValue();
        else if (t == "--collapsed")             collapsed = true;
        else if (t == "--extreme")               extreme   = true;
        else if (t == "--signal")                signal    = true;
    }

    // Collapsed (scope folded away) is the other fold state. The harness drives it
    // through a scratch settings root rather than the machine's saved state, so
    // --collapsed alone is enough and nothing here depends on the real AppData.
    const float baseHeight = collapsed ? 180.0f : kBaseHeight;

    auto scratchRoot = juce::File::getSpecialLocation(juce::File::tempDirectory)
                           .getChildFile("DistortionUiSnapshot");
    scratchRoot.deleteRecursively();
    PluginEditor::setDataRootOverride(scratchRoot);

    auto dir = juce::File::getCurrentWorkingDirectory().getChildFile(outDir);
    dir.createDirectory();

    PluginProcessor proc;
    proc.setRateAndBufferSizeDetails(44100.0, 512);
    proc.prepareToPlay(44100.0, 512);

    const int scales[] = { 70, 80, 90, 100 };
    bool allOk = true;

    for (int pct : scales)
    {
        if (onlyScale != 0 && pct != onlyScale)
            continue;

        const int w = juce::roundToInt(kBaseWidth * pct / 100.0f);
        const int h = juce::roundToInt(baseHeight * pct / 100.0f);

        // Rewritten per scale: the editor applies windowScale from settings in its
        // constructor, so matching it to the size requested below keeps the two
        // from disagreeing.
        writeScratchSettings(scratchRoot, collapsed, pct);

        juce::String name = juce::String(collapsed ? "collapsed-" : "scale-") + juce::String(pct);
        if (extreme)
            name += "-extreme";
        if (signal)
            name += "-signal";
        allOk &= snapshot(proc, w, h, dir.getChildFile(name + ".png"), extreme, signal);
    }

    scratchRoot.deleteRecursively();

    std::cout << (allOk ? "OK" : "FAILED") << " -> " << dir.getFullPathName() << "\n";
    return allOk ? 0 : 1;
}
