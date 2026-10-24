// Colours, fonts and the Ableton-style look for the plugin UI.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace illuminize
{

struct Palette
{
    juce::Colour surface, text, textDim;
    juce::Colour buttonFill, buttonOutline, buttonText, buttonOnFill, buttonOnText;
    juce::Colour menuFill, menuOutline, menuText, menuHighlight, menuHighlightText;
    juce::Colour display, grid, gridMajor, reference, curve, glow, handle, point, pointSelected;
    juce::Colour displayText, displayTextDim;
    juce::Colour knobTrack, knobArc, knobNeedle;
    juce::Colour toggleOn, toggleOff, toggleOutline;
    juce::Colour divider, meterBg, meterFill, formLine;
    bool pillButtons = true; // Live Light draws outlined "pill" buttons
};

namespace Themes
{
    enum { liveLight = 0, liveDark, neonBlue, neonGreen, neonRed, count };
    const juce::StringArray& names();
    const Palette& get (int theme);
} // namespace Themes

namespace Fonts
{
    juce::Font regular (float height);
    juce::Font medium (float height);
    juce::Font bold (float height);
} // namespace Fonts

class ShaperLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ShaperLookAndFeel();
    void setTheme (int t);
    const Palette& palette() const { return Themes::get (theme); }

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool, bool) override;
    juce::Font getTextButtonFont (juce::TextButton&, int) override;

    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool, bool) override;

    void drawComboBox (juce::Graphics&, int width, int height, bool, int, int, int, int, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;

    void drawPopupMenuBackground (juce::Graphics&, int width, int height) override;
    void drawPopupMenuItem (juce::Graphics&, const juce::Rectangle<int>& area, bool isSeparator, bool isActive,
                            bool isHighlighted, bool isTicked, bool hasSubMenu, const juce::String& text,
                            const juce::String& shortcutKeyText, const juce::Drawable* icon,
                            const juce::Colour* textColour) override;
    juce::Font getPopupMenuFont() override;

    void drawTooltip (juce::Graphics&, const juce::String& text, int width, int height) override;
    juce::Rectangle<int> getTooltipBounds (const juce::String& tipText, juce::Point<int> screenPos,
                                           juce::Rectangle<int> parentArea) override;

private:
    int theme = Themes::liveLight;
};

} // namespace illuminize
