#include "Theme.h"

#include "BinaryData.h"

namespace illuminize
{

namespace
{
    juce::Colour rgb (int r, int g, int b, float a = 1.0f) { return juce::Colour ((juce::uint8) r, (juce::uint8) g, (juce::uint8) b, a); }

    Palette makeLiveLight()
    {
        // Sampled from Live's Light theme as it renders the Max for Live version.
        Palette p;
        p.surface = rgb (165, 168, 176);
        p.text = rgb (2, 3, 8);
        p.textDim = rgb (40, 42, 50);
        p.buttonFill = rgb (212, 214, 220);
        p.buttonOutline = rgb (2, 3, 8);
        p.buttonText = rgb (2, 3, 8);
        p.buttonOnFill = rgb (246, 187, 61);
        p.buttonOnText = rgb (2, 3, 8);
        p.menuFill = rgb (228, 229, 233);
        p.menuOutline = rgb (50, 53, 64);
        p.menuText = rgb (2, 3, 8);
        p.menuHighlight = rgb (103, 234, 252);
        p.menuHighlightText = rgb (2, 3, 8);
        p.display = rgb (9, 12, 21);
        p.grid = rgb (34, 37, 47);
        p.gridMajor = rgb (64, 67, 78);
        p.reference = rgb (110, 116, 132, 0.6f);
        p.curve = rgb (246, 187, 61);
        p.glow = rgb (246, 187, 61, 0.0f);
        p.handle = rgb (232, 120, 128);
        p.point = rgb (246, 187, 61);
        p.pointSelected = rgb (255, 255, 255);
        p.displayText = rgb (205, 210, 220);
        p.displayTextDim = rgb (110, 116, 130);
        p.knobTrack = rgb (9, 12, 21);
        p.knobArc = rgb (103, 234, 252);
        p.knobNeedle = rgb (2, 3, 8);
        p.toggleOn = rgb (246, 187, 61);
        p.toggleOff = rgb (212, 214, 220);
        p.toggleOutline = rgb (50, 53, 64);
        p.divider = rgb (88, 91, 102);
        p.meterBg = rgb (9, 12, 21);
        p.meterFill = rgb (246, 187, 61);
        p.formLine = rgb (103, 234, 252);
        p.pillButtons = true;
        return p;
    }

    Palette makeLiveDark()
    {
        Palette p;
        p.surface = rgb (49, 49, 49);
        p.text = rgb (212, 212, 212);
        p.textDim = rgb (160, 160, 160);
        p.buttonFill = rgb (66, 66, 66);
        p.buttonOutline = rgb (24, 24, 24);
        p.buttonText = rgb (212, 212, 212);
        p.buttonOnFill = rgb (255, 164, 43);
        p.buttonOnText = rgb (18, 18, 18);
        p.menuFill = rgb (32, 32, 32);
        p.menuOutline = rgb (80, 80, 80);
        p.menuText = rgb (212, 212, 212);
        p.menuHighlight = rgb (92, 158, 196);
        p.menuHighlightText = rgb (255, 255, 255);
        p.display = rgb (24, 24, 24);
        p.grid = rgb (44, 44, 44);
        p.gridMajor = rgb (72, 72, 72);
        p.reference = rgb (120, 120, 120, 0.55f);
        p.curve = rgb (255, 164, 43);
        p.glow = rgb (255, 164, 43, 0.0f);
        p.handle = rgb (140, 190, 235);
        p.point = rgb (255, 164, 43);
        p.pointSelected = rgb (255, 255, 255);
        p.displayText = rgb (210, 210, 210);
        p.displayTextDim = rgb (120, 120, 120);
        p.knobTrack = rgb (28, 28, 28);
        p.knobArc = rgb (103, 214, 236);
        p.knobNeedle = rgb (225, 225, 225);
        p.toggleOn = rgb (255, 164, 43);
        p.toggleOff = rgb (66, 66, 66);
        p.toggleOutline = rgb (24, 24, 24);
        p.divider = rgb (28, 28, 28);
        p.meterBg = rgb (24, 24, 24);
        p.meterFill = rgb (255, 164, 43);
        p.formLine = rgb (103, 214, 236);
        p.pillButtons = false;
        return p;
    }

    Palette makeNeonBlue()
    {
        Palette p;
        p.surface = rgb (12, 15, 22);
        p.text = rgb (224, 240, 255);
        p.textDim = rgb (158, 184, 217);
        p.buttonFill = rgb (23, 31, 43);
        p.buttonOutline = rgb (51, 92, 128);
        p.buttonText = rgb (207, 222, 255);
        p.buttonOnFill = rgb (9, 51, 87);
        p.buttonOnText = rgb (71, 199, 255);
        p.menuFill = rgb (19, 27, 38);
        p.menuOutline = rgb (51, 92, 128);
        p.menuText = rgb (224, 240, 255);
        p.menuHighlight = rgb (10, 59, 97);
        p.menuHighlightText = rgb (224, 240, 255);
        p.display = rgb (9, 12, 17);
        p.grid = rgb (24, 37, 51);
        p.gridMajor = rgb (54, 87, 117);
        p.reference = rgb (89, 112, 138, 0.45f);
        p.curve = rgb (13, 166, 255);
        p.glow = rgb (13, 166, 255, 0.12f);
        p.handle = rgb (87, 191, 255);
        p.point = rgb (184, 224, 255);
        p.pointSelected = rgb (255, 255, 255);
        p.displayText = rgb (171, 194, 224);
        p.displayTextDim = rgb (128, 166, 204);
        p.knobTrack = rgb (43, 59, 79);
        p.knobArc = rgb (8, 166, 255);
        p.knobNeedle = rgb (224, 240, 255);
        p.toggleOn = rgb (8, 166, 255);
        p.toggleOff = rgb (23, 31, 43);
        p.toggleOutline = rgb (77, 128, 178);
        p.divider = rgb (26, 38, 54);
        p.meterBg = rgb (31, 48, 66);
        p.meterFill = rgb (8, 166, 255);
        p.formLine = rgb (13, 166, 255);
        p.pillButtons = false;
        return p;
    }

    // Same hue rotation the Max for Live device uses for Neon Green / Neon Red.
    juce::Colour rotate (juce::Colour c, int which)
    {
        const float r = c.getFloatRed(), g = c.getFloatGreen(), b = c.getFloatBlue();
        if (std::max ({ r, g, b }) - std::min ({ r, g, b }) < 0.025f)
            return c;
        const float nr = which == 1 ? r : b;
        const float ng = which == 1 ? b : r + (g - r) * 0.20f;
        const float nb = which == 1 ? r + (g - r) * 0.65f : r + (g - r) * 0.38f;
        return juce::Colour::fromFloatRGBA (nr, ng, nb, c.getFloatAlpha());
    }

    Palette rotated (const Palette& src, int which)
    {
        Palette p = src;
        for (auto* c : { &p.surface, &p.text, &p.textDim, &p.buttonFill, &p.buttonOutline, &p.buttonText, &p.buttonOnFill,
                         &p.buttonOnText, &p.menuFill, &p.menuOutline, &p.menuText, &p.menuHighlight, &p.menuHighlightText,
                         &p.display, &p.grid, &p.gridMajor, &p.reference, &p.curve, &p.glow, &p.handle, &p.point,
                         &p.pointSelected, &p.displayText, &p.displayTextDim, &p.knobTrack, &p.knobArc, &p.knobNeedle,
                         &p.toggleOn, &p.toggleOff, &p.toggleOutline, &p.divider, &p.meterBg, &p.meterFill, &p.formLine })
            *c = rotate (*c, which);
        return p;
    }

    juce::Typeface::Ptr loadTypeface (const char* data, int size)
    {
        return juce::Typeface::createSystemTypefaceFor (data, (size_t) size);
    }
} // namespace

const juce::StringArray& Themes::names()
{
    static const juce::StringArray n { "Live Light", "Live Dark", "Neon Blue", "Neon Green", "Neon Red" };
    return n;
}

const Palette& Themes::get (int theme)
{
    static const Palette palettes[] = { makeLiveLight(), makeLiveDark(), makeNeonBlue(),
                                        rotated (makeNeonBlue(), 1), rotated (makeNeonBlue(), 2) };
    return palettes[juce::jlimit (0, (int) count - 1, theme)];
}

juce::Font Fonts::regular (float h)
{
    static auto tf = loadTypeface (BinaryData::SpaceGroteskRegular_ttf, BinaryData::SpaceGroteskRegular_ttfSize);
    return juce::Font (juce::FontOptions (tf).withHeight (h));
}

juce::Font Fonts::medium (float h)
{
    static auto tf = loadTypeface (BinaryData::SpaceGroteskMedium_ttf, BinaryData::SpaceGroteskMedium_ttfSize);
    return juce::Font (juce::FontOptions (tf).withHeight (h));
}

juce::Font Fonts::bold (float h)
{
    static auto tf = loadTypeface (BinaryData::SpaceGroteskBold_ttf, BinaryData::SpaceGroteskBold_ttfSize);
    return juce::Font (juce::FontOptions (tf).withHeight (h));
}

// ---------------------------------------------------------------- look and feel

ShaperLookAndFeel::ShaperLookAndFeel() { setTheme (Themes::liveLight); }

void ShaperLookAndFeel::setTheme (int t)
{
    theme = juce::jlimit (0, (int) Themes::count - 1, t);
    const auto& p = palette();
    setColour (juce::PopupMenu::backgroundColourId, p.menuFill);
    setColour (juce::PopupMenu::textColourId, p.menuText);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, p.menuHighlight);
    setColour (juce::PopupMenu::highlightedTextColourId, p.menuHighlightText);
    setColour (juce::ComboBox::textColourId, p.menuText);
    setColour (juce::ComboBox::backgroundColourId, p.menuFill);
    setColour (juce::ComboBox::outlineColourId, p.menuOutline);
    setColour (juce::ComboBox::arrowColourId, p.menuText);
    setColour (juce::TextButton::textColourOffId, p.buttonText);
    setColour (juce::TextButton::textColourOnId, p.buttonOnText);
    setColour (juce::ToggleButton::textColourId, p.text);
    setColour (juce::TooltipWindow::backgroundColourId, p.menuFill);
    setColour (juce::TooltipWindow::textColourId, p.menuText);
    setColour (juce::TooltipWindow::outlineColourId, p.menuOutline);
}

void ShaperLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    const auto& p = palette();
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState() || down;
    auto fill = on ? p.buttonOnFill : p.buttonFill;
    if (over && ! on)
        fill = fill.brighter (0.06f);
    if (p.pillButtons)
    {
        const float radius = r.getHeight() * 0.5f;
        g.setColour (fill);
        g.fillRoundedRectangle (r, radius);
        g.setColour (p.buttonOutline);
        g.drawRoundedRectangle (r, radius, 1.0f);
    }
    else
    {
        g.setColour (fill);
        g.fillRect (r);
    }
}

juce::Font ShaperLookAndFeel::getTextButtonFont (juce::TextButton&, int) { return Fonts::bold (11.5f); }

void ShaperLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool down)
{
    const auto& p = palette();
    g.setFont (getTextButtonFont (b, b.getHeight()));
    g.setColour ((b.getToggleState() || down) ? p.buttonOnText : p.buttonText);
    g.drawText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred, false);
}

void ShaperLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool)
{
    // Small square switch at the left, label to its right (like Live's toggles).
    const auto& p = palette();
    const float box = 12.0f, y = (float) b.getHeight() * 0.5f - box * 0.5f;
    juce::Rectangle<float> r (0.5f, y, box, box);
    g.setColour (b.getToggleState() ? p.toggleOn : (over ? p.toggleOff.brighter (0.05f) : p.toggleOff));
    g.fillRect (r);
    g.setColour (p.toggleOutline);
    g.drawRect (r, 1.0f);
    g.setColour (p.text);
    g.setFont (Fonts::regular (13.0f));
    g.drawText (b.getButtonText(), juce::Rectangle<float> (box + 8.0f, 0.0f, (float) b.getWidth() - box - 8.0f, (float) b.getHeight()),
                juce::Justification::centredLeft, false);
}

void ShaperLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox&)
{
    const auto& p = palette();
    juce::Rectangle<float> r (0.5f, 0.5f, (float) width - 1.0f, (float) height - 1.0f);
    g.setColour (p.menuFill);
    g.fillRect (r);
    g.setColour (p.menuOutline);
    g.drawRect (r, 1.0f);
    juce::Path tri;
    const float cx = (float) width - 9.0f, cy = (float) height * 0.5f;
    tri.addTriangle (cx - 4.0f, cy - 2.0f, cx + 4.0f, cy - 2.0f, cx, cy + 3.0f);
    g.setColour (p.menuText);
    g.fillPath (tri);
}

juce::Font ShaperLookAndFeel::getComboBoxFont (juce::ComboBox&) { return Fonts::regular (13.0f); }

void ShaperLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (2, 0, box.getWidth() - 18, box.getHeight());
    label.setFont (getComboBoxFont (box));
    label.setJustificationType (juce::Justification::centredLeft);
}

void ShaperLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
{
    const auto& p = palette();
    g.fillAll (p.menuFill);
    g.setColour (p.menuOutline);
    g.drawRect (0, 0, width, height, 1);
}

void ShaperLookAndFeel::drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area, bool isSeparator, bool isActive,
                                           bool isHighlighted, bool isTicked, bool, const juce::String& text,
                                           const juce::String&, const juce::Drawable*, const juce::Colour*)
{
    const auto& p = palette();
    if (isSeparator)
    {
        g.setColour (p.menuOutline.withAlpha (0.5f));
        g.fillRect (area.reduced (6, 0).withHeight (1).withY (area.getCentreY()));
        return;
    }
    if (isHighlighted && isActive)
    {
        g.setColour (p.menuHighlight);
        g.fillRect (area);
    }
    g.setColour (isHighlighted ? p.menuHighlightText : p.menuText.withAlpha (isActive ? 1.0f : 0.4f));
    g.setFont (getPopupMenuFont());
    auto r = area.reduced (8, 0);
    if (isTicked)
        g.drawText (juce::String::fromUTF8 ("\xe2\x80\xa2"), r.removeFromLeft (12), juce::Justification::centred, false);
    else
        r.removeFromLeft (12);
    g.drawText (text, r, juce::Justification::centredLeft, true);
}

juce::Font ShaperLookAndFeel::getPopupMenuFont() { return Fonts::regular (14.0f); }

void ShaperLookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& text, int width, int height)
{
    const auto& p = palette();
    g.fillAll (p.menuFill);
    g.setColour (p.menuOutline);
    g.drawRect (0, 0, width, height, 1);
    juce::AttributedString s;
    s.append (text, Fonts::regular (13.0f), p.menuText);
    s.setJustification (juce::Justification::centredLeft);
    juce::TextLayout layout;
    layout.createLayout (s, (float) width - 16.0f);
    layout.draw (g, juce::Rectangle<float> (8.0f, 6.0f, (float) width - 16.0f, (float) height - 12.0f));
}

juce::Rectangle<int> ShaperLookAndFeel::getTooltipBounds (const juce::String& tipText, juce::Point<int> screenPos, juce::Rectangle<int> parentArea)
{
    juce::AttributedString s;
    s.append (tipText, Fonts::regular (13.0f));
    juce::TextLayout layout;
    layout.createLayout (s, 260.0f);
    const int w = (int) std::ceil (layout.getWidth()) + 18, h = (int) std::ceil (layout.getHeight()) + 14;
    return juce::Rectangle<int> (screenPos.x > parentArea.getCentreX() ? screenPos.x - (w + 12) : screenPos.x + 24,
                                 screenPos.y > parentArea.getCentreY() ? screenPos.y - (h + 6) : screenPos.y + 6, w, h)
        .constrainedWithin (parentArea);
}

} // namespace illuminize
