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

bool snapshot(PluginProcessor& proc, int width, int height, const juce::File& out,
              bool extreme = false)
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
    for (int i = 1; i < argc; ++i)
    {
        const juce::String t(argv[i]);
        if (t == "--out" && i + 1 < argc)        outDir    = argv[++i];
        else if (t == "--scale" && i + 1 < argc) onlyScale = juce::String(argv[++i]).getIntValue();
        else if (t == "--collapsed")             collapsed = true;
        else if (t == "--extreme")               extreme   = true;
    }

    // Collapsed (scope folded away) is the other fold state; the editor reads the
    // fold flag itself from settings.xml, so set that too when rendering these.
    const float baseHeight = collapsed ? 180.0f : kBaseHeight;

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
        juce::String name = juce::String(collapsed ? "collapsed-" : "scale-") + juce::String(pct);
        if (extreme)
            name += "-extreme";
        allOk &= snapshot(proc, w, h, dir.getChildFile(name + ".png"), extreme);
    }

    std::cout << (allOk ? "OK" : "FAILED") << " -> " << dir.getFullPathName() << "\n";
    return allOk ? 0 : 1;
}
