#include "PluginEditor.h"

namespace illuminize
{

namespace Layout
{
    // Base coordinates (scaled as a whole when the window is resized).
    constexpr int margin = 24, right = MainPanel::baseWidth - margin;  // 24 .. 736
    constexpr int headerCentre = 30;                                   // header row centre line
    constexpr int mainTop = 64, mainBottom = 432;                       // curve + AUDIO FORM
    constexpr int curveRight = 504, formLeft = 520;
    constexpr int ruleY = 446;                                          // horizontal divider
    constexpr int labelY = 452, knobY = 476, knobW = 96, knobH = 104;   // section labels, knobs
    constexpr int rowY = 594, rowH = 26;                                // toggles / menus / meter row
    constexpr int splitX = 516;                                         // vertical divider
    constexpr int limLeft = 532;
} // namespace Layout

MainPanel::MainPanel (ShaperProcessor& p) : proc (p), curve (p), form (p)
{
    auto& params = proc.getApvts();
    for (auto* c : std::initializer_list<juce::Component*> { &randomButton, &resetButton, &themeBox, &oversamplingBox, &curve, &form,
                                                            &pre, &bias, &mix, &post, &ceiling, &release, &dc, &autoGain,
                                                            &limiter, &grMeter })
        addAndMakeVisible (c);

    auto attach = [&] (Knob& k, const char* id, double resetValue)
    {
        sliderAttachments.push_back (std::make_unique<SliderAttachment> (params, id, k));
        k.setDoubleClickReturnValue (true, resetValue);
    };
    attach (pre, ParamIDs::pre, 0.0);
    attach (bias, ParamIDs::bias, 0.0);
    attach (mix, ParamIDs::mix, 100.0);
    attach (post, ParamIDs::post, 0.0);
    attach (ceiling, ParamIDs::ceiling, -0.3);
    attach (release, ParamIDs::release, 80.0);
    buttonAttachments.push_back (std::make_unique<ButtonAttachment> (params, ParamIDs::dc, dc));
    buttonAttachments.push_back (std::make_unique<ButtonAttachment> (params, ParamIDs::autoGain, autoGain));
    buttonAttachments.push_back (std::make_unique<ButtonAttachment> (params, ParamIDs::limiter, limiter));

    oversamplingBox.addItemList ({ "1x", "2x", "4x" }, 1);
    oversamplingAttachment = std::make_unique<ComboAttachment> (params, ParamIDs::oversampling, oversamplingBox);

    themeBox.addItemList (Themes::names(), 1);
    themeBox.setSelectedId (proc.getTheme() + 1, juce::dontSendNotification);
    themeBox.onChange = [this] { proc.setTheme (themeBox.getSelectedId() - 1); };

    randomButton.onClick = [this] { proc.randomCurve(); };
    resetButton.onClick = [this] { proc.resetAll(); };

    pre.setTooltip ("Pre: gain into the curve. More Pre pushes the signal further along the curve for more distortion.");
    bias.setTooltip ("Bias: shifts the signal off-centre before the curve so the top and bottom distort differently, adding "
                     "warmer even harmonics. The DC filter turns on automatically while Bias is not 0.");
    mix.setTooltip ("Mix: balance between the original (dry) and shaped (wet) signal. Both stay perfectly time-aligned.");
    post.setTooltip ("Post: output level after the curve and Mix.");
    dc.setTooltip ("DC filter: optional 10 Hz high-pass that removes slow offset. Off by default (like FL's Center), so the "
                   "default Linear curve clips exactly at 0 dB. Use it with asymmetric curves and Rectify.");
    autoGain.setTooltip ("Auto gain: keeps the shaped signal at about the same loudness as the input while you change Pre, "
                         "Bias or the curve, so you can compare sounds fairly. Off by default.");
    oversamplingBox.setTooltip ("Oversampling: runs the curve at 2x or 4x the sample rate to reduce harsh aliasing. Higher "
                                "settings use more CPU. Latency is the same in every mode.");
    limiter.setTooltip ("Limiter: output peak limiter after Post that keeps peaks below the Ceiling.");
    ceiling.setTooltip ("Ceiling: highest peak level the limiter lets through.");
    release.setTooltip ("Release: how quickly the limiter recovers. Shorter is louder but can distort low end; longer is smoother.");
    grMeter.setTooltip ("Gain reduction: how much the limiter is turning the signal down.");
    randomButton.setTooltip ("Creates a new random curve that follows the SYM setting. Use Undo to go back.");
    resetButton.setTooltip ("Returns the curve and all controls to their defaults. The theme is kept.");
    themeBox.setTooltip ("Theme: Live Light and Live Dark match Ableton's themes; Neon themes recolour the whole panel. "
                         "Drag the window edge to make the plugin bigger.");
}

MainPanel::~MainPanel() = default;

void MainPanel::applyTheme (int theme)
{
    themeBox.setSelectedId (theme + 1, juce::dontSendNotification);
    form.setAccent (Themes::get (theme).formLine);
    repaint();
}

void MainPanel::resized()
{
    using namespace Layout;
    const int buttonH = 28, buttonY = headerCentre - buttonH / 2;
    themeBox.setBounds (right - 116, buttonY, 116, buttonH);
    resetButton.setBounds (themeBox.getX() - 12 - 108, buttonY, 108, buttonH);
    randomButton.setBounds (resetButton.getX() - 12 - 96, buttonY, 96, buttonH);

    curve.setBounds (margin, mainTop, curveRight - margin, mainBottom - mainTop);
    form.setBounds (formLeft, mainTop, right - formLeft, mainBottom - mainTop);

    // Gain section: four knobs spread across the curve's width.
    const int gap = (curveRight - margin - 4 * knobW) / 3;
    Knob* gainKnobs[] = { &pre, &bias, &mix, &post };
    for (int i = 0; i < 4; ++i)
        gainKnobs[i]->setBounds (margin + i * (knobW + gap), knobY, knobW, knobH);

    // Bottom row of the gain section: toggles under Pre / Bias, oversampling under Mix / Post.
    dc.setBounds (pre.getX(), rowY, knobW + gap, rowH);
    autoGain.setBounds (bias.getX(), rowY, knobW + gap, rowH);
    oversamplingBox.setBounds (post.getX(), rowY, knobW, rowH);

    // Limiter section.
    limiter.setBounds (limLeft, labelY, 140, 22);  // 22 high: same small switch as the device
    const int limGap = (right - limLeft - 2 * knobW);
    ceiling.setBounds (limLeft, knobY, knobW, knobH);
    release.setBounds (limLeft + knobW + limGap, knobY, knobW, knobH);
    grMeter.setBounds (limLeft, rowY - 14, right - limLeft, 46);
}

void MainPanel::paint (juce::Graphics& g)
{
    using namespace Layout;
    const auto& pal = paletteFor (*this);
    g.fillAll (pal.surface);

    g.setColour (pal.text);
    g.setFont (Fonts::bold (30.0f));
    g.drawText ("ILLUMINIZE  /  SHAPER", juce::Rectangle<float> ((float) margin, (float) headerCentre - 18.0f, 380.0f, 36.0f),
                juce::Justification::centredLeft, false);

    g.setFont (Fonts::medium (14.0f));
    g.drawText ("GAIN / MIX", juce::Rectangle<float> ((float) margin, (float) labelY, 200.0f, 22.0f), juce::Justification::centredLeft, false);
    g.setFont (Fonts::regular (14.5f));
    g.drawText ("Oversampling", juce::Rectangle<float> ((float) mix.getX(), (float) rowY, (float) (post.getX() - mix.getX() - 8), (float) rowH),
                juce::Justification::centredLeft, false);

    g.setColour (pal.divider);
    g.fillRect ((float) margin, (float) ruleY, (float) (right - margin), 1.0f);
    g.fillRect ((float) splitX, (float) ruleY + 10.0f, 1.0f, (float) (baseHeight - ruleY - 22));
}

// ---------------------------------------------------------------- editor

ShaperEditor::ShaperEditor (ShaperProcessor& p) : AudioProcessorEditor (&p), proc (p), panel (p)
{
    setLookAndFeel (&lookAndFeel);
    addAndMakeVisible (panel);
    proc.addChangeListener (this);
    changeListenerCallback (nullptr);

    setResizable (true, false); // the host window resizes; keeps the aspect ratio
    setResizeLimits ((int) (MainPanel::baseWidth * 0.7), (int) (MainPanel::baseHeight * 0.7),
                     MainPanel::baseWidth * 2, MainPanel::baseHeight * 2);
    getConstrainer()->setFixedAspectRatio ((double) MainPanel::baseWidth / MainPanel::baseHeight);
    setSize (MainPanel::baseWidth, MainPanel::baseHeight);
    startTimerHz (20);
}

ShaperEditor::~ShaperEditor()
{
    proc.removeChangeListener (this);
    setLookAndFeel (nullptr);
}

void ShaperEditor::paint (juce::Graphics& g) { g.fillAll (lookAndFeel.palette().surface); }

void ShaperEditor::resized()
{
    const float scale = (float) getWidth() / (float) MainPanel::baseWidth;
    panel.setBounds (0, 0, MainPanel::baseWidth, MainPanel::baseHeight);
    panel.setTransform (juce::AffineTransform::scale (scale));
}

void ShaperEditor::timerCallback()
{
    panel.grMeter.setReduction (proc.getEngine().gainReductionDb.load());
}

void ShaperEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    const int theme = proc.getTheme();
    if (theme == currentTheme)
        return;
    currentTheme = theme;
    lookAndFeel.setTheme (theme);
    panel.applyTheme (theme);
    sendLookAndFeelChange();
    repaint();
}

} // namespace illuminize
