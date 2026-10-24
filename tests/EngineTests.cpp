// Illuminize Shaper — engine tests (no JUCE needed).
// 1. Bit-level comparison against reference audio rendered by a Python port of the
//    Max for Live device's gen~ code (tests/reference, made by tools/makeref.py).
// 2. Behaviour checks: latency, null, clipping, limiter, curve model, thumps, auto-gain.
#include "../source/dsp/Curve.h"
#include "../source/dsp/ShaperEngine.h"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

using namespace illuminize;

static int failures = 0;
static void check (bool ok, const std::string& what)
{
    std::printf ("%s  %s\n", ok ? "PASS" : "FAIL", what.c_str());
    if (! ok)
        ++failures;
}

static std::vector<double> readF64 (const std::string& path)
{
    std::ifstream f (path, std::ios::binary);
    std::vector<double> v;
    double d;
    while (f.read (reinterpret_cast<char*> (&d), sizeof d))
        v.push_back (d);
    return v;
}

static void render (ShaperEngine& e, std::vector<float>& L, std::vector<float>& R, int block = 256)
{
    for (size_t i = 0; i < L.size(); i += (size_t) block)
    {
        const int n = (int) std::min ((size_t) block, L.size() - i);
        e.process (L.data() + i, R.data() + i, n);
    }
}

static ShaperEngine& makeEngine (ShaperEngine& e, const EngineParams& p, const CurveModel& m, double sr = 44100.0)
{
    e.prepare (sr);
    e.setParams (p);
    e.snapToTargets();
    e.setCurve (m.makeSnapshot());
    // Skip the 10 ms curve crossfade so the test starts on the curve itself.
    std::vector<float> s (2048, 0.0f), t (2048, 0.0f);
    render (e, s, t);
    e.reset();
    e.setParams (p);
    e.snapToTargets();
    return e;
}

static void referenceTests (const std::string& dir)
{
    std::ifstream idx (dir + "/cases.txt");
    std::string line;
    int count = 0;
    while (std::getline (idx, line))
    {
        std::vector<std::string> f;
        std::stringstream ss (line);
        std::string item;
        while (std::getline (ss, item, '|'))
            f.push_back (item);
        if (f.size() != 9)
            continue;
        ++count;
        const std::string name = f[0];
        const int frames = std::stoi (f[1]);
        EngineParams p;
        p.preDb = std::stod (f[2]);
        p.mixPercent = std::stod (f[3]);
        p.postDb = std::stod (f[4]);
        p.dcFilter = std::stod (f[5]) > 0.5;
        p.oversampling = std::stoi (f[6]);
        p.biasPercent = std::stod (f[7]);
        CurveModel m;
        check (m.fromString (f[8]), name + ": curve parses");
        ShaperEngine e;
        makeEngine (e, p, m);

        const auto in = readF64 (dir + "/" + name + "_in.f64");
        const auto ref = readF64 (dir + "/" + name + "_out.f64");
        std::vector<float> L ((size_t) frames + 64), R ((size_t) frames + 64);
        for (int i = 0; i < frames; ++i)
            L[(size_t) i] = (float) in[(size_t) 2 * i], R[(size_t) i] = (float) in[(size_t) 2 * i + 1];
        render (e, L, R);
        // The plugin includes the limiter's 64-sample lookahead (bypassed = pure delay).
        double maxErr = 0.0, peak = 0.0;
        for (int i = 200; i < frames; ++i)
        {
            maxErr = std::max (maxErr, std::abs (L[(size_t) i + 64] - ref[(size_t) 2 * i]));
            maxErr = std::max (maxErr, std::abs (R[(size_t) i + 64] - ref[(size_t) 2 * i + 1]));
            peak = std::max (peak, std::abs (ref[(size_t) 2 * i]));
        }
        char msg[200];
        std::snprintf (msg, sizeof msg, "%s: matches M4L reference (max error %.2e, peak %.2f)", name.c_str(), maxErr, peak);
        check (maxErr < 2e-4, msg);
    }
    check (count >= 5, "reference cases found");
}

static void behaviourTests()
{
    CurveModel linear;

    // Latency: impulse comes out exactly 104 samples later in every oversampling mode.
    for (int mode = 0; mode < 3; ++mode)
    {
        EngineParams p;
        p.oversampling = mode;
        p.mixPercent = 0.0;
        ShaperEngine e;
        makeEngine (e, p, linear);
        std::vector<float> L (400, 0.0f), R (400, 0.0f);
        L[10] = R[10] = 0.5f;
        render (e, L, R);
        int at = -1;
        for (int i = 0; i < 400; ++i)
            if (std::abs (L[(size_t) i]) > 0.25f)
                at = i;
        check (at == 10 + ShaperEngine::latencySamples, "latency " + std::to_string (at - 10) + " samples, mode " + std::to_string (mode));
    }

    // Null: Linear, Mix 100 %, band-limited content returns unchanged (delayed) in all modes.
    for (int mode = 0; mode < 3; ++mode)
    {
        EngineParams p;
        p.oversampling = mode;
        ShaperEngine e;
        makeEngine (e, p, linear);
        const int n = 8000;
        std::vector<float> L (n), R (n), src (n);
        for (int i = 0; i < n; ++i)
            src[(size_t) i] = L[(size_t) i] = R[(size_t) i] = (float) (0.3 * std::sin (i * 0.05) + 0.2 * std::sin (i * 0.31));
        render (e, L, R);
        double err = 0, ref = 0;
        for (int i = 1000; i < n; ++i)
        {
            const double d = L[(size_t) i] - src[(size_t) i - 104];
            err += d * d;
            ref += src[(size_t) i - 104] * src[(size_t) i - 104];
        }
        const double db = 10 * std::log10 (err / ref + 1e-30);
        check (db < -80.0, "null at mode " + std::to_string (mode) + ": " + std::to_string ((int) db) + " dB");
    }

    // Default clipper: Linear at 1x shaves everything above 0 dBFS.
    {
        EngineParams p;
        p.oversampling = 0;
        ShaperEngine e;
        makeEngine (e, p, linear);
        std::vector<float> L (6000), R (6000);
        for (int i = 0; i < 6000; ++i)
            L[(size_t) i] = R[(size_t) i] = (float) (2.0 * std::sin (2 * 3.14159265 * 100 * i / 44100.0));
        render (e, L, R);
        float mx = 0;
        for (int i = 500; i < 6000; ++i)
            mx = std::max (mx, std::abs (L[(size_t) i]));
        check (mx <= 1.0f + 1e-6f, "1x Linear clips at 0 dBFS (peak " + std::to_string (mx) + ")");
    }

    // Limiter holds the ceiling.
    {
        EngineParams p;
        p.postDb = 6.0;
        p.limiter = true;
        p.ceilingDb = -1.0;
        ShaperEngine e;
        CurveModel soft;
        soft.applyPreset (CurveModel::soft);
        makeEngine (e, p, soft);
        std::vector<float> L (20000), R (20000);
        for (int i = 0; i < 20000; ++i)
            L[(size_t) i] = R[(size_t) i] = (float) (0.9 * std::sin (2 * 3.14159265 * 220 * i / 44100.0));
        render (e, L, R);
        float mx = 0;
        for (int i = 2000; i < 20000; ++i)
            mx = std::max (mx, std::abs (L[(size_t) i]));
        check (mx <= (float) std::pow (10.0, -1.0 / 20.0) + 1e-5f, "limiter holds -1 dB ceiling (peak " + std::to_string (20 * std::log10 (mx)) + " dB)");
        check (e.gainReductionDb.load() > 1.0f, "limiter reports gain reduction");
    }

    // No thumps: switching asymmetric curves on silence stays silent (DC filter off and on).
    for (int dc = 0; dc < 2; ++dc)
    {
        EngineParams p;
        p.dcFilter = dc == 1;
        ShaperEngine e;
        CurveModel a, b;
        a.setPoints ({ { -1, -0.9, 0 }, { -0.4, -0.2, 1.1 }, { 0.1, 0.55, 0 }, { 0.6, 0.7, -1.3 }, { 1, 0.95, 0 } }, false);
        b.setPoints ({ { -1, -0.3, 0 }, { -0.5, -0.95, 0 }, { -0.05, -0.45, 0.8 }, { 0.5, 0.2, 0 }, { 1, 0.6, 0 } }, false);
        makeEngine (e, p, a);
        float mx = 0;
        for (int k = 0; k < 20; ++k)
        {
            if (e.curveFadeDone())
                e.setCurve ((k % 2 ? a : b).makeSnapshot());
            std::vector<float> L (2205, 0.0f), R (2205, 0.0f);
            render (e, L, R);
            for (float v : L)
                mx = std::max (mx, std::abs (v));
        }
        check (mx < 1e-6f, std::string ("curve changes on silence are silent, DC ") + (dc ? "on" : "off"));
    }

    // Curve model: presets, symmetry, serialisation, random curves.
    for (int i = 0; i < CurveModel::numPresets; ++i)
    {
        CurveModel m;
        m.applyPreset (i);
        check (m.matchingPreset() == i, std::string ("preset recognised: ") + CurveModel::presetName (i));
        CurveModel copy;
        check (copy.fromString (m.toString()) && copy.toString() == m.toString(), std::string ("preset round-trips: ") + CurveModel::presetName (i));
    }
    {
        CurveModel m;
        m.applyPreset (CurveModel::soft);
        bool odd = true;
        for (double x = -1; x <= 1; x += 0.01)
            odd = odd && std::abs (m.eval (x) + m.eval (-x)) < 1e-12;
        check (odd, "SYM ON curves are odd-symmetric");
        // Device's stored format loads (v0.18 curve state string).
        CurveModel d;
        check (d.fromString ("2 1 3 -1 -1 1.6 0 0 -1.6 1 1 0") && d.matchingPreset() == CurveModel::soft, "loads M4L curve state format");
    }
    {
        std::mt19937 rng (42);
        int bad = 0;
        for (int sym = 0; sym < 2; ++sym)
            for (int i = 0; i < 2000; ++i)
            {
                CurveModel m;
                m.setSymmetric (sym == 1);
                m.randomize (rng);
                CurveModel copy;
                if (! copy.fromString (m.toString()))
                    ++bad;
                const auto s = m.makeSnapshot();
                for (double x = -1; x <= 1; x += 0.05)
                    if (! std::isfinite (s.eval (x)) || std::abs (s.eval (x)) > 1.0 + 1e-9)
                        ++bad;
            }
        check (bad == 0, "4000 random curves valid");
    }
    {
        // Vertical step at the centre with SYM OFF keeps its full -1..+1 range (no offset shift).
        CurveModel m;
        m.setPoints ({ { -1, -1, 0 }, { -1e-12, -1, 0 }, { 0, 1, 0 }, { 1, 1, 0 } }, false);
        check (std::abs (m.makeSnapshot().zeroOutput) < 1e-9, "SYM OFF centre step has zero offset");
    }

    // Auto-gain: matches loudness for a driven Soft curve and is ~unity for Linear at 0 dB.
    {
        CurveModel soft;
        soft.applyPreset (CurveModel::soft);
        const double g = computeAutoGain (soft.makeSnapshot(), 12.0, 0.0);
        const double gl = computeAutoGain (linear.makeSnapshot(), 0.0, 0.0);
        check (gl > 0.999 && gl < 1.001, "auto-gain is unity for Linear at 0 dB");
        check (g < 0.5, "auto-gain turns Soft +12 dB down (" + std::to_string (20 * std::log10 (g)) + " dB)");

        EngineParams p;
        p.preDb = 12.0;
        p.autoGain = g;
        ShaperEngine e;
        makeEngine (e, p, soft);
        std::vector<float> L (20000), R (20000), src (20000);
        for (int i = 0; i < 20000; ++i)
            src[(size_t) i] = L[(size_t) i] = R[(size_t) i] = (float) (0.25 * std::sin (2 * 3.14159265 * 440 * i / 44100.0));
        render (e, L, R);
        double si = 0, so = 0;
        for (int i = 2000; i < 20000; ++i)
            si += src[(size_t) i] * src[(size_t) i], so += L[(size_t) i] * L[(size_t) i];
        const double diff = 10 * std::log10 (so / si);
        check (std::abs (diff) < 0.5, "auto-gain level match within 0.5 dB (" + std::to_string (diff) + " dB)");
    }

    // Oversampling switch during playback: no jump bigger than the signal's own movement.
    {
        EngineParams p;
        ShaperEngine e;
        CurveModel soft;
        soft.applyPreset (CurveModel::soft);
        makeEngine (e, p, soft);
        float biggest = 0, prev = 0;
        int n = 0;
        for (int k = 0; k < 40; ++k)
        {
            p.oversampling = k % 3;
            e.setParams (p);
            std::vector<float> L (512), R (512);
            for (int i = 0; i < 512; ++i, ++n)
                L[(size_t) i] = R[(size_t) i] = (float) (0.4 * std::sin (2 * 3.14159265 * 200 * n / 44100.0));
            render (e, L, R);
            for (float v : L)
            {
                if (n > 2000)
                    biggest = std::max (biggest, std::abs (v - prev));
                prev = v;
            }
        }
        check (biggest < 0.06f, "oversampling switches are click-free (max step " + std::to_string (biggest) + ")");
    }
}

int main (int argc, char** argv)
{
    const std::string refDir = argc > 1 ? argv[1] : "tests/reference";
    referenceTests (refDir);
    behaviourTests();
    std::printf ("\n%s (%d failure%s)\n", failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED", failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
