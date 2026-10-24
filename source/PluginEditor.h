#pragma once

#include "PluginProcessor.h"
#include "ui/AudioForm.h"
#include "ui/CurveEditor.h"
#include "ui/Theme.h"
#include "ui/Widgets.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace illuminize
{

// The plugin panel: a large, near-square layout (not limited to Live's device strip).
class MainPanel : public juce::Component
{
public:
    explicit MainPanel (ShaperProcessor& p);
    ~MainPanel() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void applyTheme (int theme);

    static constexpr int baseWidth = 760, baseHeight = 648;

    GainReductionMeter grMeter;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    ShaperProcessor& proc;
    juce::TextButton randomButton { "RANDOM" }, resetButton { "RESET ALL" };
    juce::ComboBox themeBox, oversamplingBox;
    CurveEditor curve;
    AudioForm form;
    Knob pre { "Pre" }, bias { "Bias" }, mix { "Mix" }, post { "Post" }, ceiling { "Ceiling" }, release { "Release" };
    juce::ToggleButton dc { "DC filter" }, autoGain { "Auto gain" }, limiter { "LIMITER" };
    std::vector<std::unique_ptr<SliderAttachment>> sliderAttachments;
    std::vector<std::unique_ptr<ButtonAttachment>> buttonAttachments;
    std::unique_ptr<ComboAttachment> oversamplingAttachment;
};

class ShaperEditor : public juce::AudioProcessorEditor,
                     private juce::Timer,
                     private juce::ChangeListener
{
public:
    explicit ShaperEditor (ShaperProcessor&);
    ~ShaperEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    ShaperProcessor& proc;
    ShaperLookAndFeel lookAndFeel;
    MainPanel panel;
    juce::TooltipWindow tooltips { this, 700 };
    int currentTheme = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ShaperEditor)
};

} // namespace illuminize
