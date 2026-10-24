#pragma once

#include "../PluginProcessor.h"
#include "Theme.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace illuminize
{

// Transfer-curve editor with its toolbar (preset menu, Undo, Redo, SYM).
// Same interactions as the Max for Live device.
class CurveEditor : public juce::Component,
                    public juce::SettableTooltipClient,
                    private juce::ChangeListener
{
public:
    explicit CurveEditor (ShaperProcessor& p);
    ~CurveEditor() override;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    struct Geometry
    {
        float w, h, l, r, t, b;
    };
    struct Toolbar
    {
        float pw, ux, rx, bw, sx, sw;
    };

    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    Geometry geometry() const;
    Toolbar toolbar() const;
    float px (double x, const Geometry& g) const { return g.l + (float) ((x + 1.0) * 0.5) * (g.r - g.l); }
    float py (double y, const Geometry& g) const { return g.b - (float) ((y + 1.0) * 0.5) * (g.b - g.t); }
    double ux (float x, const Geometry& g) const { return juce::jlimit (-1.0, 1.0, 2.0 * (x - g.l) / (g.r - g.l) - 1.0); }
    double uy (float y, const Geometry& g) const { return juce::jlimit (-1.0, 1.0, 2.0 * (g.b - y) / (g.b - g.t) - 1.0); }
    int pointAt (float x, float y, const Geometry& g) const;
    int handleAt (float x, float y, const Geometry& g) const;
    void mirrorIfNeeded (float& x, float& y, const Geometry& g) const;
    void commit();                       // enforce symmetry, keep selection, publish
    void showPresetMenu();

    ShaperProcessor& proc;
    CurveModel model;
    int selected = -1, dragging = -1, segment = -1;
    bool mirroredDrag = false, menuOpen = false;
    float startY = 0.0f;
    double startT = 0.0;
};

} // namespace illuminize
