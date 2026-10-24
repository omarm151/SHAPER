// Renders the real plugin UI to PNG files (one per theme) without needing a DAW.
// Usage: UiSnapshot <output-folder>
#include <juce_audio_processors/juce_audio_processors.h>

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
        editor->setSize (644 * 2, 170 * 2);
        auto img = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
        save (img, outDir.getChildFile ("panel_" + Themes::names()[theme].replaceCharacter (' ', '_') + ".png"));
    }

    // Big curve editor (EDIT CURVE window) in the default theme.
    proc.setTheme (Themes::liveLight);
    ShaperLookAndFeel lnf;
    CurveEditor big (proc);
    big.setLookAndFeel (&lnf);
    big.setSize (770, 410);
    save (big.createComponentSnapshot (big.getLocalBounds(), true, 1.0f), outDir.getChildFile ("curve_editor_large.png"));
    big.setLookAndFeel (nullptr);
    return 0;
}
