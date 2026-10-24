// Loads the built plugin the way a DAW does and checks the basics:
// name, parameters, reported latency, audio output, and state save/restore.
// Usage: HostCheck <path to "Illuminize Shaper.vst3" or .component>
#include <juce_audio_processors/juce_audio_processors.h>

static int failures = 0;
static void check (bool ok, const juce::String& what)
{
    std::printf ("%s  %s\n", ok ? "PASS" : "FAIL", what.toRawUTF8());
    if (! ok)
        ++failures;
}

static juce::AudioProcessorParameter* findParam (juce::AudioPluginInstance& p, const juce::String& name)
{
    for (auto* param : p.getParameters())
        if (param->getName (64) == name)
            return param;
    return nullptr;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    if (argc < 2)
    {
        std::printf ("usage: HostCheck <plugin path>\n");
        return 2;
    }
    const juce::String path (argv[1]);

    juce::AudioPluginFormatManager formats;
    juce::addDefaultFormatsToManager (formats);
    juce::OwnedArray<juce::PluginDescription> types;
    for (auto* format : formats.getFormats())
        if (format->fileMightContainThisPluginType (path))
            format->findAllTypesForFile (types, path);
    check (types.size() == 1, "plugin found (" + juce::String (types.size()) + " type)");
    if (types.isEmpty())
        return 1;
    std::printf ("      %s / %s / %s\n", types[0]->name.toRawUTF8(), types[0]->pluginFormatName.toRawUTF8(),
                 types[0]->manufacturerName.toRawUTF8());

    juce::String error;
    auto plugin = formats.createPluginInstance (*types[0], 44100.0, 512, error);
    check (plugin != nullptr, "instantiates" + (error.isEmpty() ? juce::String() : ": " + error));
    if (plugin == nullptr)
        return 1;

    check (plugin->getParameters().size() >= 10, "exposes " + juce::String (plugin->getParameters().size()) + " parameters");
    for (auto* name : { "Pre", "Bias", "Mix", "Post", "DC Filter", "Oversampling", "Auto Gain", "Limiter", "Limiter Ceiling", "Limiter Release" })
        check (findParam (*plugin, name) != nullptr, juce::String ("parameter: ") + name);

    plugin->setPlayConfigDetails (2, 2, 44100.0, 512);
    plugin->prepareToPlay (44100.0, 512);
    check (plugin->getLatencySamples() == 104, "reports latency " + juce::String (plugin->getLatencySamples()) + " samples");

    // Impulse with Mix at 0 % must come out exactly 104 samples later.
    if (auto* mix = findParam (*plugin, "Mix"))
        mix->setValueNotifyingHost (0.0f);
    juce::AudioBuffer<float> buffer (2, 512);
    juce::MidiBuffer midi;
    buffer.clear();
    plugin->processBlock (buffer, midi); // let parameter smoothing settle
    for (int i = 0; i < 4; ++i)
    {
        buffer.clear();
        plugin->processBlock (buffer, midi);
    }
    buffer.clear();
    buffer.setSample (0, 10, 0.5f);
    buffer.setSample (1, 10, 0.5f);
    plugin->processBlock (buffer, midi);
    int at = -1;
    for (int i = 0; i < 512; ++i)
        if (std::abs (buffer.getSample (0, i)) > 0.25f)
            at = i;
    check (at == 114, "impulse delayed by " + juce::String (at - 10) + " samples (Mix 0 %)");

    // Driven signal stays finite and bounded with the limiter on.
    if (auto* mix = findParam (*plugin, "Mix"))
        mix->setValueNotifyingHost (1.0f);
    if (auto* lim = findParam (*plugin, "Limiter"))
        lim->setValueNotifyingHost (1.0f);
    if (auto* pre = findParam (*plugin, "Pre"))
        pre->setValueNotifyingHost (0.8f);
    float peak = 0.0f;
    bool finite = true;
    for (int block = 0; block < 100; ++block)
    {
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 512; ++i)
                buffer.setSample (ch, i, 0.8f * std::sin (0.07f * (float) (block * 512 + i)));
        plugin->processBlock (buffer, midi);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 512; ++i)
            {
                const float v = buffer.getSample (ch, i);
                finite = finite && std::isfinite (v);
                if (block > 10)
                    peak = std::max (peak, std::abs (v));
            }
    }
    check (finite, "output is finite");
    check (peak <= 0.97f && peak > 0.3f, "limiter keeps peaks under the -0.3 dB ceiling (peak " + juce::String (juce::Decibels::gainToDecibels (peak), 2) + " dB)");

    // State round-trip.
    juce::MemoryBlock state;
    plugin->getStateInformation (state);
    auto* post = findParam (*plugin, "Post");
    const float saved = post != nullptr ? post->getValue() : 0.0f;
    if (post != nullptr)
        post->setValueNotifyingHost (0.1f);
    plugin->setStateInformation (state.getData(), (int) state.getSize());
    check (post != nullptr && std::abs (post->getValue() - saved) < 1e-4f, "state save/restore round-trips");
    check (state.toString().isNotEmpty() || state.getSize() > 50, "state contains data (" + juce::String ((int) state.getSize()) + " bytes)");

    plugin->releaseResources();
    plugin.reset();
    std::printf ("\n%s (%d failure%s)\n", failures == 0 ? "HOST CHECK PASSED" : "HOST CHECK FAILED", failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
