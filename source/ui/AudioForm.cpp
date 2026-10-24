#include "AudioForm.h"
#include "Widgets.h"

#include <algorithm>

namespace illuminize
{

namespace
{
    double clampd (double v, double lo, double hi) { return std::max (lo, std::min (hi, v)); }
    constexpr double pi = 3.141592653589793;
} // namespace

AudioForm::AudioForm (ShaperProcessor& p) : proc (p)
{
    setTooltip ("Audio Form: an audio-reactive picture of the current curve, Pre and Bias. Click to pause or resume.");
    proc.addChangeListener (this);
    proc.getApvts().addParameterListener (ParamIDs::pre, this);
    proc.getApvts().addParameterListener (ParamIDs::bias, this);
    curve = proc.getCurve().makeSnapshot();
    pendingGain.store (std::pow (10.0, proc.getApvts().getRawParameterValue (ParamIDs::pre)->load() / 20.0));
    pendingOffset.store (proc.getApvts().getRawParameterValue (ParamIDs::bias)->load() * 0.005);
    moving = proc.getMotion();
    rebuild();
    startTimer (50);
}

AudioForm::~AudioForm()
{
    proc.getApvts().removeParameterListener (ParamIDs::pre, this);
    proc.getApvts().removeParameterListener (ParamIDs::bias, this);
    proc.removeChangeListener (this);
}

void AudioForm::changeListenerCallback (juce::ChangeBroadcaster*)
{
    curve = proc.getCurve().makeSnapshot();
    rebuild();
}

void AudioForm::parameterChanged (const juce::String& id, float v)
{
    // May arrive on the audio thread: just record and rebuild on the next timer tick.
    if (id == ParamIDs::pre)
        pendingGain.store (std::pow (10.0, clampd (v, -24.0, 36.0) / 20.0));
    else
        pendingOffset.store (clampd (v, -100.0, 100.0) * 0.005);
    rebuildPending.store (true);
}

double AudioForm::shaped (double x) const { return curve.eval (x * gain + offset) - curve.eval (offset); }

void AudioForm::rebuild()
{
    rebuildPending.store (false);
    gain = pendingGain.load();
    offset = pendingOffset.load();
    double energy = 0;
    for (int s = 0; s < 64; ++s)
    {
        const double x = std::sin (s * pi / 32.0), d = shaped (x) - x;
        energy += d * d;
    }
    const double amount = clampd (std::sqrt (energy / 64.0), 0.0, 1.5);
    const double asym = (shaped (0.65) + shaped (-0.65)) * 0.5;
    target.clear();
    edges.clear();
    for (int i = 0; i <= rows; ++i)
        for (int j = 0; j < cols; ++j)
        {
            const double lat = pi * i / rows, lon = 2.0 * pi * j / cols, base = std::sin (3 * lon + 2 * lat);
            const double d = shaped (base) - base;
            const double radius = clampd (1 + 0.65 * d + 0.28 * amount * std::cos (6 * lon) * std::sin (lat), 0.25, 1.65);
            const double twist = lon + 1.5 * amount * std::cos (lat);
            const double x = radius * std::sin (lat) * std::cos (twist) + 0.38 * asym * std::sin (lat) * std::sin (lat);
            const double y = 0.35 * std::cos (lat) + 0.65 * shaped (std::cos (lat));
            const double z = radius * std::sin (lat) * std::sin (twist);
            target.push_back ({ x, y, z });
            const int id = i * cols + j;
            if (i > 0 && i < rows)
                edges.push_back ({ id, i * cols + (j + 1) % cols });
            if (i < rows)
                edges.push_back ({ id, id + cols });
        }
    if (! moving || vertices.size() != target.size())
        vertices = target;
    repaint();
}

AudioForm::Vec3 AudioForm::animatedVertex (int i) const
{
    const auto& v = vertices[(size_t) i];
    const double lat = pi * (i / cols) / rows, lon = 2 * pi * (i % cols) / cols;
    const double weight = std::sin (lat), a = body * (0.10 + 0.18 * tone);
    const double wave = std::sin (3 * lon + 2 * lat + flow) * weight;
    const double radius = 1 + a * wave + 0.06 * hit * std::cos (5 * lon - flow) * weight;
    const double tw = a * 1.4 * std::sin (lat * 3 - flow * 0.7), c = std::cos (tw), s = std::sin (tw);
    return { (v[0] * c + v[2] * s) * radius, v[1] + a * 0.65 * std::sin (lon * 2 + flow * 1.3) * weight, (-v[0] * s + v[2] * c) * radius };
}

void AudioForm::setMotion (bool on)
{
    moving = on;
    pulse = body = slowBody = hit = tone = 0;
    if (! moving)
        vertices = target;
    proc.setMotion (on);
    repaint();
}

void AudioForm::mouseDown (const juce::MouseEvent&) { setMotion (! moving); }

void AudioForm::timerCallback()
{
    if (rebuildPending.load())
        rebuild();
    auto& engine = proc.getEngine();
    const double level = clampd (engine.inputPeak.exchange (0.0f), 0.0, 1.0);
    if (! moving)
        return;
    const double rms = clampd (engine.inputEnergy.load(), 0.0, 4.0);
    const double bright = clampd (engine.inputBrightness.load(), 0.0, 1.0);

    const double excitation = level < 0.001 ? 0.0 : std::pow (level, 0.55);
    pulse = std::max (excitation, pulse * std::exp (-50.0 / 180.0));
    if (pulse < 0.0001)
        pulse = 0;
    double e = clampd (std::pow (rms, 0.55), 0.0, 1.0);
    if (rms < 0.0001)
        e = 0;
    body += (e - body) * (e > body ? 0.48 : 0.22);
    slowBody += (body - slowBody) * 0.10;
    hit = std::max (clampd ((body - slowBody) * 2.5, 0.0, 1.0), hit * 0.68);
    tone += ((e > 0 ? bright : 0.0) - tone) * 0.25;
    flow = std::fmod (flow + 0.035 + body * (0.16 + 0.20 * tone), pi * 20);
    phase = std::fmod (phase + 0.014 + 0.045 * body + 0.018 * hit, 2 * pi);
    for (size_t i = 0; i < vertices.size(); ++i)
        for (int k = 0; k < 3; ++k)
            vertices[i][(size_t) k] += 0.22 * (target[i][(size_t) k] - vertices[i][(size_t) k]);
    repaint();
}

void AudioForm::paint (juce::Graphics& g)
{
    const auto& pal = paletteFor (*this);
    const float w = (float) getWidth(), h = (float) getHeight();
    const float cx = w * 0.5f, cy = h * 0.52f, scale = std::min (w * 0.26f, h * 0.25f);
    g.fillAll (pal.display);

    g.setColour (pal.displayTextDim);
    g.setFont (Fonts::regular (12.0f));
    g.drawText ("AUDIO FORM", juce::Rectangle<float> (0.0f, 5.0f, w, 13.0f), juce::Justification::centred, false);

    g.setColour (pal.gridMajor);
    g.drawEllipse (cx - scale * 0.95f, cy + scale * 1.15f, scale * 1.9f, 6.0f, 1.0f);

    // Project, fit and depth-sort the wireframe.
    std::vector<std::array<float, 3>> proj;
    proj.reserve (vertices.size());
    const double ca = std::cos (phase), sa = std::sin (phase), tilt = 0.3, ct = std::cos (tilt), st = std::sin (tilt);
    const double breath = 0.78 + 0.25 * body + 0.13 * pulse + 0.08 * hit;
    for (int i = 0; i < (int) vertices.size(); ++i)
    {
        const auto v = animatedVertex (i);
        const double x = v[0] * ca + v[2] * sa;
        double z = -v[0] * sa + v[2] * ca;
        const double y = v[1] * ct - z * st;
        z = v[1] * st + z * ct;
        const double persp = 3.8 / (3.8 - z);
        proj.push_back ({ (float) (cx + x * scale * persp * breath), (float) (cy - y * scale * persp * breath), (float) z });
    }
    float fit = 1.0f;
    for (const auto& p : proj)
    {
        const float dx = std::abs (p[0] - cx), dy = std::abs (p[1] - cy);
        if (dx > 0)
            fit = std::min (fit, (w / 2 - 5) / dx);
        if (dy > 0)
            fit = std::min (fit, std::min (cy - 24.0f, h - 22.0f - cy) / dy);
    }
    if (fit < 1.0f)
        for (auto& p : proj)
            p[0] = cx + (p[0] - cx) * fit, p[1] = cy + (p[1] - cy) * fit;

    auto order = edges;
    std::sort (order.begin(), order.end(), [&] (const auto& a, const auto& b)
               { return proj[(size_t) a.first][2] + proj[(size_t) a.second][2] < proj[(size_t) b.first][2] + proj[(size_t) b.second][2]; });
    const auto line = accent.isTransparent() ? pal.formLine : accent;
    for (const auto& e : order)
    {
        const auto& a = proj[(size_t) e.first];
        const auto& b = proj[(size_t) e.second];
        const float depth = (float) clampd ((a[2] + b[2] + 2.5) / 5.0, 0.0, 1.0);
        g.setColour (pal.gridMajor.interpolatedWith (line, 0.35f + 0.65f * depth)
                         .withAlpha ((float) clampd (0.18 + 0.6 * depth + 0.22 * pulse, 0.0, 1.0)));
        g.drawLine (a[0], a[1], b[0], b[1], (float) (0.55 + 0.45 * depth + 0.35 * pulse));
    }

    g.setColour (line);
    g.setFont (Fonts::regular (12.0f));
    g.drawText (moving ? "II" : juce::String::fromUTF8 ("\xe2\x96\xb6"), juce::Rectangle<float> (0.0f, h - 17.0f, w, 13.0f),
                juce::Justification::centred, false);
}

} // namespace illuminize
