// Renders the real plugin UI to PNG files (one per theme) without needing a DAW.
// Usage: UiSnapshot <output-folder>
#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "ui/CurveEditor.h"
#include "ui/Theme.h"

using namespace illuminize;

static void setParam (ShaperProcessor& p, const char* id, float value)
{
    auto* param = p.getApvts().getParameter (id);
    param->setValueNotifyingHost (param->convertTo0to1 (value));
}

static void save (const juce::Image& img, const juce::File& file)
{
    file.deleteFile();
    juce::FileOutputStream out (file);
    juce::PNGImageFormat png;
    png.writeImageToStream (img, out);
    std::printf ("wrote %s (%d x %d)\n", file.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::File outDir (argc > 1 ? juce::String (argv[1]) : juce::File::getCurrentWorkingDirectory().getFullPathName());
    outDir.createDirectory();

    std::unique_ptr<juce::AudioProcessor> base (createPluginFilter());
    auto& proc = *dynamic_cast<ShaperProcessor*> (base.get());
    proc.prepareToPlay (44100.0, 512);
    proc.applyPreset (CurveModel::soft);
    setParam (proc, ParamIDs::pre, 6.0f);
    setParam (proc, ParamIDs::bias, 12.0f);
    setParam (proc, ParamIDs::limiter, 1.0f);

    // Feed some audio so the meters have something to show.
    juce::AudioBuffer<float> buffer (2, 512);
    juce::MidiBuffer midi;
    for (int block = 0; block < 40; ++block)
    {
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 512; ++i)
                buffer.setSample (ch, i, 0.9f * std::sin (0.05f * (float) (block * 512 + i)));
        proc.processBlock (buffer, midi);
    }

    for (int theme = 0; theme < Themes::count; ++theme)
    {
        proc.setTheme (theme);
        std::unique_ptr<juce::AudioProcessorEditor> editor (proc.createEditor());
        editor->setSize (MainPanel::baseWidth, MainPanel::baseHeight);
        auto img = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
        save (img, outDir.getChildFile ("panel_" + Themes::names()[theme].replaceCharacter (' ', '_') + ".png"));
    }

    // Largest window size (2x) in the default theme.
    proc.setTheme (Themes::liveLight);
    {
        std::unique_ptr<juce::AudioProcessorEditor> editor (proc.createEditor());
        editor->setSize (MainPanel::baseWidth * 2, MainPanel::baseHeight * 2);
        save (editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f), outDir.getChildFile ("panel_2x.png"));
    }
    return 0;
}
