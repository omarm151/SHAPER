#include "Widgets.h"

namespace illuminize
{

const Palette& paletteFor (const juce::Component& c)
{
    if (auto* lnf = dynamic_cast<ShaperLookAndFeel*> (&c.getLookAndFeel()))
        return lnf->palette();
    return Themes::get (Themes::liveLight);
}

Knob::Knob (const juce::String& name) : title (name)
{
    setSliderStyle (juce::Slider::RotaryVerticalDrag);
    setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    setMouseDragSensitivity (200);
    // Hold Shift for fine adjustment.
    setVelocityModeParameters (0.25, 1, 0.0, true, juce::ModifierKeys::shiftModifier);
    setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
}

void Knob::paint (juce::Graphics& g)
{
    const auto& p = paletteFor (*this);
    const float w = (float) getWidth(), h = (float) getHeight();
    // Text and strokes grow with the knob (13 px text at the device's 66 px height).
    const float k = juce::jlimit (1.0f, 1.6f, h / 66.0f);
    const float fontH = 13.0f * k, stroke = 3.0f * k;

    g.setColour (p.text);
    g.setFont (Fonts::regular (fontH));
    g.drawText (title, juce::Rectangle<float> (0.0f, 0.0f, w, fontH + 2.0f), juce::Justification::centredTop, false);

    const float cy = (fontH + 2.0f + h - fontH - 2.0f) * 0.5f;
    const float cx = w * 0.5f, r = std::min (w * 0.31f, (h - 2.0f * (fontH + 6.0f)) * 0.5f);
    const auto range = getRange();
    const float frac = (float) valueToProportionOfLength (getValue());
    const float start = juce::MathConstants<float>::pi * 1.25f, end = juce::MathConstants<float>::pi * 2.75f;
    const float angle = start + frac * (end - start);
    juce::ignoreUnused (range);

    juce::Path track;
    track.addCentredArc (cx, cy, r, r, 0.0f, start, end, true);
    g.setColour (p.knobTrack);
    g.strokePath (track, juce::PathStrokeType (stroke, juce::PathStrokeType::curved, juce::PathStrokeType::butt));

    if (frac > 0.0005f)
    {
        juce::Path arc;
        arc.addCentredArc (cx, cy, r, r, 0.0f, start, angle, true);
        g.setColour (p.knobArc);
        g.strokePath (arc, juce::PathStrokeType (stroke, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
    }

    const float nx = cx + std::sin (angle) * (r - 1.0f), ny = cy - std::cos (angle) * (r - 1.0f);
    g.setColour (p.knobNeedle);
    g.drawLine (cx, cy, nx, ny, 2.2f * k);

    g.setColour (p.text);
    g.setFont (Fonts::regular (fontH));
    g.drawText (getTextFromValue (getValue()), juce::Rectangle<float> (-6.0f, h - fontH - 2.0f, w + 12.0f, fontH + 2.0f),
                juce::Justification::centredBottom, false);
}

void GainReductionMeter::setReduction (float db)
{
    db = juce::jlimit (0.0f, 120.0f, db);
    if (std::abs (db - reduction) > 0.05f)
    {
        reduction = db;
        repaint();
    }
}

void GainReductionMeter::paint (juce::Graphics& g)
{
    const auto& p = paletteFor (*this);
    const float w = (float) getWidth(), h = (float) getHeight();
    g.setColour (p.text);
    g.setFont (Fonts::regular (13.0f));
    const juce::String value = reduction < 0.05f ? "0.0" : juce::String::fromUTF8 ("\xe2\x88\x92") + juce::String (reduction, 1);
    g.drawText ("GR  " + value + " dB", juce::Rectangle<float> (4.0f, 0.0f, w - 8.0f, 17.0f), juce::Justification::bottomLeft, false);

    g.setColour (p.meterBg);
    g.fillRect (4.0f, 20.0f, w - 8.0f, 7.0f);
    g.setColour (p.meterFill);
    g.fillRect (4.0f, 20.0f, (w - 8.0f) * std::min (reduction / 18.0f, 1.0f), 7.0f);

    g.setColour (p.textDim);
    g.setFont (Fonts::regular (10.5f));
    const juce::Rectangle<float> scale (4.0f, h - 14.0f, w - 8.0f, 13.0f);
    g.drawText ("0", scale, juce::Justification::bottomLeft, false);
    g.drawText (juce::String::fromUTF8 ("\xe2\x88\x92" "9"), scale, juce::Justification::centredBottom, false);
    g.drawText (juce::String::fromUTF8 ("\xe2\x88\x92" "18"), scale, juce::Justification::bottomRight, false);
}

} // namespace illuminize
