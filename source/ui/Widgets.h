#pragma once

#include "Theme.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace illuminize
{

const Palette& paletteFor (const juce::Component& c);

// Live-style dial: name above, arc + needle, value below. Drag up/down; Shift for fine;
// double-click resets.
class Knob : public juce::Slider
{
public:
    explicit Knob (const juce::String& name);
    void paint (juce::Graphics&) override;

private:
    juce::String title;
};

// Limiter gain-reduction meter (read-only).
class GainReductionMeter : public juce::Component,
                           public juce::SettableTooltipClient
{
public:
    void setReduction (float db);
    void paint (juce::Graphics&) override;

private:
    float reduction = 0.0f;
};

} // namespace illuminize
