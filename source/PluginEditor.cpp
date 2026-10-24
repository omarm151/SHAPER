#include "PluginEditor.h"

namespace illuminize
{

// ---------------------------------------------------------------- big curve window

class CurveWindowContent : public juce::Component
{
public:
    explicit CurveWindowContent (ShaperProcessor& p) : proc (p), curve (p)
    {
        addAndMakeVisible (curve);
        addAndMakeVisible (reset);
        reset.setTooltip ("Returns the curve and all controls to their defaults. The theme is kept.");
        reset.onClick = [this] { proc.resetAll(); };
        setSize (800, 500);
    }
    void paint (juce::Graphics& g) override
    {
        const auto& pal = paletteFor (*this);
        g.fillAll (pal.surface);
        g.setColour (pal.text);
        g.setFont (Fonts::bold (21.0f));
        g.drawText ("ILLUMINIZE SHAPER  /  CURVE EDITOR", juce::Rectangle<float> (15.0f, 10.0f, 600.0f, 26.0f), juce::Justification::centredLeft, false);
        g.setColour (pal.textDim);
        g.setFont (Fonts::regular (13.0f));
        g.drawText ("Drag points  /  Shift: lock level  /  Ctrl or Cmd: lock position or fine bend  /  Double-click: add or delete  /  "
                    "Double-click a square: straighten",
                    juce::Rectangle<float> (15.0f, 464.0f, 770.0f, 22.0f), juce::Justification::centredLeft, true);
    }
    void resized() override
    {
        curve.setBounds (15, 45, 770, 410);
        reset.setBounds (685, 12, 100, 23);
    }

private:
    ShaperProcessor& proc;
    CurveEditor curve;
    juce::TextButton reset { "RESET ALL" };
};

class CurveWindow : public juce::DocumentWindow
{
public:
    CurveWindow (ShaperProcessor& p, juce::LookAndFeel& lnf)
        : DocumentWindow ("Illuminize Shaper - Curve Editor", paletteForTheme (p).surface, DocumentWindow::closeButton)
    {
        setLookAndFeel (&lnf);
        setUsingNativeTitleBar (true);
        setContentOwned (new CurveWindowContent (p), true);
        setResizable (false, false);
        centreWithSize (getWidth(), getHeight());
    }
    ~CurveWindow() override { setLookAndFeel (nullptr); }
    void closeButtonPressed() override { setVisible (false); }

private:
    static const Palette& paletteForTheme (ShaperProcessor& p) { return Themes::get (p.getTheme()); }
};

// ---------------------------------------------------------------- main panel

MainPanel::MainPanel (ShaperProcessor& p) : proc (p), curve (p), form (p)
{
    auto& params = proc.getApvts();
    for (auto* c : std::initializer_list<juce::Component*> { &editButton, &randomButton, &resetButton, &themeBox, &oversamplingBox,
                                                            &curve, &form, &pre, &bias, &mix, &post, &ceiling, &release, &dc,
                                                            &autoGain, &limiter, &grMeter })
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
    themeBox.onChange = [this]
    {
        proc.setTheme (themeBox.getSelectedId() - 1);
    };

    editButton.onClick = [this] { if (onEditCurve) onEditCurve(); };
    randomButton.onClick = [this] { proc.randomCurve(); };
    resetButton.onClick = [this] { proc.resetAll(); };

    // Info texts (shown as tooltips, like Live's Info View for the device).
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
    editButton.setTooltip ("Opens a large window for detailed curve editing.");
    randomButton.setTooltip ("Creates a new random curve that follows the SYM setting. Use Undo to go back.");
    resetButton.setTooltip ("Returns the curve and all controls to their defaults. The theme is kept.");
    themeBox.setTooltip ("Theme: Live Light and Live Dark match Ableton's themes; Neon themes recolour the whole panel.");
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
    // Coordinates from the aligned Max for Live layout (v0.18.4).
    editButton.setBounds (186, 5, 80, 20);
    randomButton.setBounds (282, 5, 66, 20);
    resetButton.setBounds (362, 5, 74, 20);
    themeBox.setBounds (550, 6, 86, 20);
    curve.setBounds (8, 29, 268, 138);
    form.setBounds (282, 29, 66, 138);
    pre.setBounds (362, 54, 42, 56);
    bias.setBounds (406, 54, 42, 56);
    mix.setBounds (450, 54, 42, 56);
    post.setBounds (494, 54, 42, 56);
    dc.setBounds (362, 120, 84, 20);
    autoGain.setBounds (450, 120, 88, 20);
    oversamplingBox.setBounds (443, 147, 45, 20);
    limiter.setBounds (550, 30, 90, 20);
    ceiling.setBounds (550, 54, 42, 56);
    release.setBounds (594, 54, 42, 56);
    grMeter.setBounds (546, 121, 94, 44);
}

void MainPanel::paint (juce::Graphics& g)
{
    const auto& pal = paletteFor (*this);
    g.fillAll (pal.surface);

    g.setColour (pal.text);
    g.setFont (Fonts::bold (19.5f));
    g.drawText ("ILLUMINIZE  /  SHAPER", juce::Rectangle<float> (14.0f, 3.0f, 172.0f, 24.0f), juce::Justification::centredLeft, false);

    g.setFont (Fonts::regular (13.0f));
    g.drawText ("Theme", juce::Rectangle<float> (486.0f, 6.0f, 56.0f, 20.0f), juce::Justification::centredRight, false);
    g.drawText ("GAIN / MIX", juce::Rectangle<float> (366.0f, 31.0f, 130.0f, 16.0f), juce::Justification::centredLeft, false);
    g.drawText ("Oversampling", juce::Rectangle<float> (366.0f, 147.0f, 76.0f, 20.0f), juce::Justification::centredLeft, false);

    g.setColour (pal.divider);
    g.fillRect (354.0f, 34.0f, 1.0f, 128.0f);
    g.fillRect (542.0f, 34.0f, 1.0f, 128.0f);
}

// ---------------------------------------------------------------- editor

ShaperEditor::ShaperEditor (ShaperProcessor& p) : AudioProcessorEditor (&p), proc (p), panel (p)
{
    setLookAndFeel (&lookAndFeel);
    addAndMakeVisible (panel);
    panel.onEditCurve = [this] { openCurveWindow(); };
    proc.addChangeListener (this);
    changeListenerCallback (nullptr);

    setResizable (true, false); // the host window resizes; no grip drawn over the panel
    setResizeLimits (MainPanel::baseWidth, MainPanel::baseHeight, (int) (MainPanel::baseWidth * 2.5), (int) (MainPanel::baseHeight * 2.5));
    getConstrainer()->setFixedAspectRatio ((double) MainPanel::baseWidth / MainPanel::baseHeight);
    setSize (MainPanel::baseWidth, MainPanel::baseHeight);
    startTimerHz (20);
}

ShaperEditor::~ShaperEditor()
{
    proc.removeChangeListener (this);
    curveWindow.reset();
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
    if (curveWindow != nullptr)
    {
        curveWindow->setBackgroundColour (lookAndFeel.palette().surface);
        curveWindow->sendLookAndFeelChange();
    }
    repaint();
}

void ShaperEditor::openCurveWindow()
{
    if (curveWindow == nullptr)
        curveWindow = std::make_unique<CurveWindow> (proc, lookAndFeel);
    curveWindow->setVisible (true);
    curveWindow->toFront (true);
}

} // namespace illuminize
