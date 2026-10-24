#include "CurveEditor.h"
#include "Widgets.h"

namespace illuminize
{

namespace
{
    double interpolate (const CurvePoint& p, const CurvePoint& q, double x)
    {
        const double width = q.x - p.x;
        if (width <= 0.0)
            return q.y;
        double u = juce::jlimit (0.0, 1.0, (x - p.x) / width);
        const double k = std::pow (2.0, std::abs (p.t));
        if (p.t > 0.0)
            u = std::pow (u, k);
        else if (p.t < 0.0)
            u = 1.0 - std::pow (1.0 - u, k);
        return p.y + (q.y - p.y) * u;
    }
} // namespace

CurveEditor::CurveEditor (ShaperProcessor& p) : proc (p), model (p.getCurve())
{
    proc.addChangeListener (this);
    setTooltip ("Transfer curve: input level (left to right) mapped to output level (bottom to top). Drag points; "
                "double-click to add or delete; drag the squares to bend. Shift-drag keeps the level, Ctrl/Cmd-drag "
                "keeps the position. The menu has starter curves and Random.");
}

CurveEditor::~CurveEditor() { proc.removeChangeListener (this); }

void CurveEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    if (dragging >= 0 || segment >= 0)
        return; // don't interrupt an active drag with our own echo
    model = proc.getCurve();
    if (selected >= (int) model.points().size())
        selected = -1;
    repaint();
}

CurveEditor::Geometry CurveEditor::geometry() const
{
    const float w = (float) getWidth(), h = (float) getHeight();
    return { w, h, 30.0f, w - 14.0f, 32.0f, h - 23.0f };
}

CurveEditor::Toolbar CurveEditor::toolbar() const
{
    const float w = (float) getWidth();
    const bool compact = w < 320.0f, small = w < 400.0f;
    return { compact ? 86.0f : (small ? 106.0f : 154.0f), compact ? 100.0f : (small ? 122.0f : 174.0f),
             compact ? 138.0f : (small ? 166.0f : 232.0f), compact ? 32.0f : (small ? 38.0f : 52.0f),
             compact ? 178.0f : (small ? 218.0f : w - 88.0f), 80.0f };
}

int CurveEditor::pointAt (float x, float y, const Geometry& g) const
{
    int best = -1;
    float d = 81.0f;
    const auto& pts = model.points();
    for (int i = 0; i < (int) pts.size(); ++i)
    {
        const float dx = x - px (pts[(size_t) i].x, g), dy = y - py (pts[(size_t) i].y, g), v = dx * dx + dy * dy;
        if (v < d)
            d = v, best = i;
    }
    return best;
}

int CurveEditor::handleAt (float x, float y, const Geometry& g) const
{
    const auto snap = model.makeSnapshot();
    const auto& pts = model.points();
    for (int i = 0; i < (int) pts.size() - 1; ++i)
    {
        if (snap.pts[(size_t) i + 1].x == snap.pts[(size_t) i].x)
            continue;
        const double xx = (pts[(size_t) i].x + pts[(size_t) i + 1].x) * 0.5;
        if (std::abs (x - px (xx, g)) < 7.0f && std::abs (y - py (snap.eval (xx), g)) < 7.0f)
            return i;
    }
    return -1;
}

void CurveEditor::mirrorIfNeeded (float& x, float& y, const Geometry& g) const
{
    x = px (-ux (x, g), g);
    y = py (-uy (y, g), g);
}

void CurveEditor::commit()
{
    // Keep selection/drag targets attached to the same points after mirroring.
    const auto& before = model.points();
    const auto xOf = [&] (int i) { return (i >= 0 && i < (int) before.size()) ? before[(size_t) i].x : 99.0; };
    const double sx = xOf (selected), dx = xOf (dragging), gx = xOf (segment);
    model.enforceSymmetry();
    const auto& after = model.points();
    const auto find = [&] (double x)
    {
        if (x > 2.0)
            return -1;
        for (int k = 0; k < (int) after.size(); ++k)
            if (after[(size_t) k].x == x)
                return k;
        return -1;
    };
    selected = find (sx);
    dragging = find (dx);
    segment = find (gx);
    proc.setCurve (model, false);
    repaint();
}

void CurveEditor::showPresetMenu()
{
    juce::PopupMenu m;
    const int current = model.matchingPreset();
    for (int i = 0; i < CurveModel::numPresets; ++i)
        m.addItem (i + 1, CurveModel::presetName (i), true, i == current);
    m.addSeparator();
    m.addItem (1000, "Random");
    menuOpen = true;
    repaint();
    const auto tb = toolbar();
    juce::Component::SafePointer<CurveEditor> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withTargetScreenArea (
                         localAreaToGlobal (juce::Rectangle<int> (8, 1, (int) tb.pw, 20))),
                     [safe] (int result)
                     {
                         if (safe == nullptr)
                             return;
                         safe->menuOpen = false;
                         if (result == 1000)
                             safe->proc.randomCurve();
                         else if (result > 0)
                             safe->proc.applyPreset (result - 1);
                         safe->selected = -1;
                         safe->repaint();
                     });
}

void CurveEditor::mouseDown (const juce::MouseEvent& e)
{
    const auto g = geometry();
    const auto tb = toolbar();
    float x = (float) e.position.x, y = (float) e.position.y;
    dragging = segment = -1;
    mirroredDrag = false;
    model = proc.getCurve();

    if (y < 24.0f)
    {
        if (x >= 8.0f && x <= 8.0f + tb.pw)
            showPresetMenu();
        else if (x >= tb.ux && x < tb.ux + tb.bw)
            proc.undo();
        else if (x >= tb.rx && x < tb.rx + tb.bw)
            proc.redo();
        else if (x >= tb.sx && x <= tb.sx + tb.sw)
            proc.setSymmetric (! model.isSymmetric());
        return;
    }
    if (x < g.l || x > g.r || y < g.t || y > g.b)
        return;

    const bool sym = model.isSymmetric();
    if (sym && x < px (0.0, g))
    {
        mirroredDrag = true;
        mirrorIfNeeded (x, y, g);
    }
    const auto& pts = model.points();
    const int p = pointAt (x, y, g);
    const int n = (int) pts.size();
    if (e.mods.isAltDown() && p > 0 && p < n - 1 && (! sym || pts[(size_t) p].x != 0.0))
    {
        proc.beginCurveEdit();
        model.mutablePoints().erase (model.mutablePoints().begin() + p);
        selected = -1;
        commit();
        return;
    }
    if (p >= 0 && sym && pts[(size_t) p].x == 0.0)
    {
        selected = p;
        repaint();
        return;
    }
    if (p >= 0)
    {
        proc.beginCurveEdit();
        selected = dragging = p;
        repaint();
        return;
    }
    const int s = handleAt (x, y, g);
    if (s >= 0)
    {
        proc.beginCurveEdit();
        segment = s;
        startY = y;
        startT = pts[(size_t) s].t;
        return;
    }
    selected = -1;
    repaint();
}

void CurveEditor::mouseDrag (const juce::MouseEvent& e)
{
    const auto g = geometry();
    float x = (float) e.position.x, y = (float) e.position.y;
    if (mirroredDrag)
        mirrorIfNeeded (x, y, g);
    auto& pts = model.mutablePoints();
    const bool lockX = e.mods.isCtrlDown() || e.mods.isCommandDown();
    if (dragging >= 0 && dragging < (int) pts.size())
    {
        auto& p = pts[(size_t) dragging];
        if (! lockX && dragging > 0 && dragging < (int) pts.size() - 1)
            p.x = juce::jlimit (pts[(size_t) dragging - 1].x + CurveModel::stepGap,
                                pts[(size_t) dragging + 1].x - CurveModel::stepGap, ux (x, g));
        if (! e.mods.isShiftDown())
            p.y = uy (y, g);
        commit();
    }
    else if (segment >= 0 && segment < (int) pts.size())
    {
        pts[(size_t) segment].t = juce::jlimit (-4.0, 4.0, startT + (y - startY) * (lockX ? 0.005 : 0.03));
        commit();
    }
}

void CurveEditor::mouseUp (const juce::MouseEvent&)
{
    dragging = segment = -1;
    model = proc.getCurve();
    repaint();
}

void CurveEditor::mouseDoubleClick (const juce::MouseEvent& e)
{
    const auto g = geometry();
    float x = (float) e.position.x, y = (float) e.position.y;
    if (menuOpen || y < g.t || y > g.b || x < g.l || x > g.r)
        return;
    model = proc.getCurve();
    const bool sym = model.isSymmetric();
    if (sym && x < px (0.0, g))
        mirrorIfNeeded (x, y, g);
    dragging = segment = -1;
    mirroredDrag = false;
    auto& pts = model.mutablePoints();
    const int p = pointAt (x, y, g);
    if (p >= 0)
    {
        if (p > 0 && p < (int) pts.size() - 1 && (! sym || pts[(size_t) p].x != 0.0))
        {
            proc.beginCurveEdit();
            pts.erase (pts.begin() + p);
            selected = -1;
            commit();
        }
        return;
    }
    const int s = handleAt (x, y, g);
    if (s >= 0)
    {
        proc.beginCurveEdit();
        pts[(size_t) s].t = 0.0;
        commit();
        return;
    }
    if ((int) pts.size() >= (sym ? CurveModel::maxPointsSym : CurveModel::maxPointsAsym))
        return;
    const double xx = ux (x, g);
    for (int i = 0; i < (int) pts.size() - 1; ++i)
        if (xx > pts[(size_t) i].x + 0.002 && xx < pts[(size_t) i + 1].x - 0.002)
        {
            proc.beginCurveEdit();
            pts.insert (pts.begin() + i + 1, CurvePoint { xx, uy (y, g), 0.0 });
            selected = i + 1;
            commit();
            return;
        }
}

void CurveEditor::paint (juce::Graphics& g)
{
    const auto& pal = paletteFor (*this);
    const auto geo = geometry();
    const auto tb = toolbar();
    const auto snap = model.makeSnapshot();
    const auto& pts = model.points();

    g.fillAll (pal.surface);
    g.setColour (pal.display);
    g.fillRect (0.0f, 25.0f, geo.w, geo.h - 25.0f);

    // Toolbar: flat Live-style buttons.
    const float btnSize = geo.w < 400.0f ? 13.5f : 15.0f;
    auto button = [&] (float x, float w, const juce::String& label, bool lit, bool centred)
    {
        g.setColour (lit ? pal.buttonOnFill : pal.buttonFill);
        g.fillRect (x, 1.0f, w, 20.0f);
        g.setColour (lit ? pal.buttonOnText : pal.buttonText);
        g.setFont (Fonts::regular (btnSize));
        g.drawText (label, juce::Rectangle<float> (centred ? x : x + (geo.w < 400.0f ? 6.0f : 10.0f), 1.0f, w, 20.0f),
                    centred ? juce::Justification::centred : juce::Justification::centredLeft, false);
    };
    const int preset = model.matchingPreset();
    button (8.0f, tb.pw, preset >= 0 ? CurveModel::presetName (preset) : "Custom", menuOpen, false);
    {
        juce::Path tri;
        tri.addTriangle (tb.pw - 1.0f, 9.0f, tb.pw + 5.0f, 9.0f, tb.pw + 2.0f, 13.0f);
        g.setColour (menuOpen ? pal.buttonOnText : pal.buttonText);
        g.fillPath (tri);
    }
    button (tb.ux, tb.bw, "Undo", false, true);
    button (tb.rx, tb.bw, "Redo", false, true);
    button (tb.sx, tb.sw, model.isSymmetric() ? "SYM ON" : "SYM OFF", model.isSymmetric(), true);

    // Grid and identity reference.
    for (int j = 0; j <= 8; ++j)
    {
        const double v = j / 4.0 - 1.0;
        g.setColour (j == 4 ? pal.gridMajor : pal.grid);
        g.drawLine (px (v, geo), geo.t, px (v, geo), geo.b, 1.0f);
        g.drawLine (geo.l, py (v, geo), geo.r, py (v, geo), 1.0f);
    }
    g.setColour (pal.reference);
    g.drawLine (geo.l, geo.b, geo.r, geo.t, 1.0f);

    // Transfer curve (exact steps are drawn as true vertical lines).
    juce::Path path;
    path.startNewSubPath (px (snap.pts[0].x, geo), py (snap.pts[0].y, geo));
    for (int k = 0; k < snap.count - 1; ++k)
    {
        const auto& p0 = snap.pts[(size_t) k];
        const auto& p1 = snap.pts[(size_t) k + 1];
        if (p1.x == p0.x)
            path.lineTo (px (p1.x, geo), py (p1.y, geo));
        else
            for (int z = 1; z <= 32; ++z)
            {
                const double xx = p0.x + (p1.x - p0.x) * z / 32.0;
                path.lineTo (px (xx, geo), py (interpolate (p0, p1, xx), geo));
            }
    }
    if (pal.glow.getAlpha() > 0)
    {
        g.setColour (pal.glow);
        g.strokePath (path, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    g.setColour (pal.curve);
    g.strokePath (path, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Bend handles and points.
    g.setColour (pal.handle);
    for (int q = 0; q < (int) pts.size() - 1; ++q)
    {
        if (snap.pts[(size_t) q + 1].x == snap.pts[(size_t) q].x)
            continue;
        const double hx = (pts[(size_t) q].x + pts[(size_t) q + 1].x) * 0.5;
        g.fillRect (px (hx, geo) - 2.5f, py (snap.eval (hx), geo) - 2.5f, 5.0f, 5.0f);
    }
    for (int i = 0; i < (int) pts.size(); ++i)
    {
        g.setColour (i == selected ? pal.pointSelected : pal.point);
        g.fillEllipse (px (pts[(size_t) i].x, geo) - 3.5f, py (pts[(size_t) i].y, geo) - 3.5f, 7.0f, 7.0f);
    }

    // Labels.
    g.setFont (Fonts::regular (12.0f));
    g.setColour (pal.displayTextDim);
    g.drawText ("+1", juce::Rectangle<float> (4.0f, geo.t - 3.0f, 24.0f, 12.0f), juce::Justification::centredLeft, false);
    g.drawText ("-1", juce::Rectangle<float> (4.0f, geo.b - 10.0f, 24.0f, 12.0f), juce::Justification::centredLeft, false);
    const juce::Rectangle<float> bottom (geo.l, geo.h - 17.0f, geo.r - geo.l, 14.0f);
    g.drawText ("-1", bottom, juce::Justification::centredLeft, false);
    g.drawText ("+1", bottom, juce::Justification::centredRight, false);
    juce::String info;
    if (selected >= 0 && selected < (int) pts.size())
    {
        g.setColour (pal.displayText);
        info = "x " + juce::String (pts[(size_t) selected].x, 3) + "    y " + juce::String (pts[(size_t) selected].y, 3);
    }
    else
        info = geo.w < 400.0f ? "Double-click: add/delete" : "Double-click: add/delete  /  Squares: bend";
    g.drawText (info, bottom.withTrimmedLeft (24.0f).withTrimmedRight (24.0f), juce::Justification::centredLeft, false);
}

} // namespace illuminize
