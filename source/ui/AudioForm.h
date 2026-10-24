#pragma once

#include "../PluginProcessor.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <atomic>
#include <vector>

namespace illuminize
{

// AUDIO FORM: audio-reactive wireframe of the current curve, Pre and Bias.
// Port of the Max for Live device's isl_sculpture.js. Click to pause.
class AudioForm : public juce::Component,
                  public juce::SettableTooltipClient,
                  private juce::Timer,
                  private juce::ChangeListener,
                  private juce::AudioProcessorValueTreeState::Listener
{
public:
    explicit AudioForm (ShaperProcessor& p);
    ~AudioForm() override;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void setAccent (juce::Colour c) { accent = c; repaint(); }

private:
    using Vec3 = std::array<double, 3>;
    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void parameterChanged (const juce::String&, float) override;
    void rebuild();
    void setMotion (bool on);
    double shaped (double x) const;
    Vec3 animatedVertex (int i) const;

    ShaperProcessor& proc;
    CurveSnapshot curve;
    double gain = 1.0, offset = 0.0;
    bool moving = true;
    std::atomic<double> pendingGain { 1.0 }, pendingOffset { 0.0 };
    std::atomic<bool> rebuildPending { true };
    double phase = 0.45, pulse = 0, body = 0, tone = 0, slowBody = 0, hit = 0, flow = 0;
    static constexpr int rows = 10, cols = 18;
    std::vector<Vec3> vertices, target;
    std::vector<std::pair<int, int>> edges;
    juce::Colour accent;
};

} // namespace illuminize
