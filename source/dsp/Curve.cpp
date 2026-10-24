#include "Curve.h"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace illuminize
{

namespace
{
    double clampd (double v, double lo, double hi) { return std::max (lo, std::min (hi, v)); }

    double interpolate (const CurvePoint& p, const CurvePoint& q, double x)
    {
        const double width = q.x - p.x;
        if (width <= 0.0)
            return q.y;
        double u = clampd ((x - p.x) / width, 0.0, 1.0);
        const double k = std::pow (2.0, std::abs (p.t));
        if (p.t > 0.0)
            u = std::pow (u, k);
        else if (p.t < 0.0)
            u = 1.0 - std::pow (1.0 - u, k);
        return p.y + (q.y - p.y) * u;
    }
} // namespace

// ---------------------------------------------------------------- snapshot

double CurveSnapshot::eval (double x) const noexcept
{
    x = clampd (x, -1.0, 1.0);
    if (symmetric && x == 0.0)
        return 0.0;

    // Binary search for the last point with pts[i].x <= x (right-continuous at steps).
    int lo = 0, hi = count;
    while (lo < hi)
    {
        const int mid = (lo + hi) / 2;
        if (pts[(size_t) mid].x <= x)
            lo = mid + 1;
        else
            hi = mid;
    }
    const int left = std::max (0, std::min (lo - 1, count - 1));
    if (left >= count - 1)
        return pts[(size_t) (count - 1)].y;
    return interpolate (pts[(size_t) left], pts[(size_t) left + 1], x);
}

// ---------------------------------------------------------------- model

const char* CurveModel::presetName (int index)
{
    static const char* names[] = { "Linear", "Soft", "Tape", "Tube", "Clip", "Fold",
                                   "Hard Fold", "Steps", "Rectify", "Half Rectify" };
    return (index >= 0 && index < numPresets) ? names[index] : "Custom";
}

CurveModel::CurveModel() { reset(); }

void CurveModel::reset()
{
    symmetric = true;
    pts = { { -1, -1, 0 }, { 0, 0, 0 }, { 1, 1, 0 } };
}

std::vector<CurvePoint> CurveModel::mirrored (const std::vector<CurvePoint>& p)
{
    std::vector<CurvePoint> pos { { 0, 0, 0 } };
    for (const auto& q : p)
    {
        if (q.x == 0.0)
            pos[0].t = q.t;
        if (q.x > 0.0)
            pos.push_back (q);
    }
    std::vector<CurvePoint> all;
    // Reversed power segments need the opposite tension sign.
    for (size_t j = pos.size() - 1; j > 0; --j)
        all.push_back ({ -pos[j].x, -pos[j].y, -pos[j - 1].t });
    all.insert (all.end(), pos.begin(), pos.end());
    return all;
}

void CurveModel::enforceSymmetry()
{
    if (symmetric)
        pts = mirrored (pts);
}

void CurveModel::setPoints (std::vector<CurvePoint> p, bool sym)
{
    if (p.size() < 2)
        return;
    std::sort (p.begin(), p.end(), [] (const CurvePoint& a, const CurvePoint& b) { return a.x < b.x; });
    for (auto& q : p)
    {
        q.x = clampd (q.x, -1.0, 1.0);
        q.y = clampd (q.y, -1.0, 1.0);
        q.t = clampd (q.t, -4.0, 4.0);
    }
    p.front().x = -1.0;
    p.back().x = 1.0;
    // Keep x strictly increasing (steps are represented by a tiny gap, like the device).
    for (size_t i = 1; i < p.size(); ++i)
        if (p[i].x <= p[i - 1].x)
            p[i].x = std::min (1.0, p[i - 1].x + stepGap);
    symmetric = sym;
    pts = std::move (p);
    enforceSymmetry();
    if ((int) pts.size() > CurveSnapshot::maxPoints)
        pts.resize ((size_t) CurveSnapshot::maxPoints), pts.back().x = 1.0;
}

void CurveModel::setSymmetric (bool sym)
{
    if (sym == symmetric)
        return;
    symmetric = sym;
    enforceSymmetry(); // turning ON keeps the positive half; OFF keeps the current shape
}

std::vector<CurvePoint> CurveModel::presetPoints (int preset, bool& sym)
{
    const double g = stepGap;
    sym = true;
    switch (preset)
    {
        case soft:      return { { -1, -1, 1.6 }, { 0, 0, -1.6 }, { 1, 1, 0 } };
        case tape:      return { { -1, -0.92, 0.6 }, { 0, 0, -0.6 }, { 1, 0.92, 0 } };
        case tube:      sym = false;
                        return { { -1, -0.62, 0 }, { -0.45, -0.6, 0.8 }, { 0, 0, -1.3 }, { 1, 0.85, 0 } };
        case clip:      return { { -1, -0.8, 0 }, { -0.35, -0.8, 0 }, { 0.35, 0.8, 0 }, { 1, 0.8, 0 } };
        case fold:      return { { -1, 0, 0 }, { -0.5, -1, 0 }, { 0, 0, 0 }, { 0.5, 1, 0 }, { 1, 0, 0 } };
        case hardFold:  return mirrored ({ { 0, 0, 0 }, { 0.33, 1, 0 }, { 0.66, -0.4, 0 }, { 1, 0.6, 0 } });
        case steps:     return mirrored ({ { 0, 0, 0 }, { 0.1, 0, 0 }, { 0.1 + g, 0.33, 0 }, { 0.4, 0.33, 0 },
                                           { 0.4 + g, 0.66, 0 }, { 0.7, 0.66, 0 }, { 0.7 + g, 1, 0 }, { 1, 1, 0 } });
        case rectify:   sym = false; return { { -1, 1, 0 }, { 0, 0, 0 }, { 1, 1, 0 } };
        case halfRectify: sym = false; return { { -1, 0, 0 }, { 0, 0, 0 }, { 1, 1, 0 } };
        case linear:
        default:        return { { -1, -1, 0 }, { 0, 0, 0 }, { 1, 1, 0 } };
    }
}

void CurveModel::applyPreset (int preset)
{
    bool sym = true;
    auto p = presetPoints (preset, sym);
    symmetric = sym;
    pts = std::move (p);
    enforceSymmetry();
}

int CurveModel::matchingPreset() const
{
    for (int n = 0; n < numPresets; ++n)
    {
        bool sym = true;
        auto p = presetPoints (n, sym);
        if (sym != symmetric)
            continue;
        if (sym)
            p = mirrored (p);
        if (p.size() != pts.size())
            continue;
        bool equal = true;
        for (size_t i = 0; i < p.size() && equal; ++i)
            equal = std::abs (p[i].x - pts[i].x) < 1e-9 && std::abs (p[i].y - pts[i].y) < 1e-9
                    && std::abs (p[i].t - pts[i].t) < 1e-9;
        if (equal)
            return n;
    }
    return -1;
}

void CurveModel::randomize (std::mt19937& rng)
{
    std::uniform_real_distribution<double> uni (0.0, 1.0);
    auto rnd = [&] (double a, double b) { return a + uni (rng) * (b - a); };
    auto tension = [&] { return uni (rng) < 0.35 ? 0.0 : std::round (rnd (-2.5, 2.5) * 10.0) / 10.0; };
    auto spaced = [&] (int n, double lo, double hi, double gap)
    {
        for (int tries = 0; tries < 40; ++tries)
        {
            std::vector<double> xs;
            for (int i = 0; i < n; ++i)
                xs.push_back (rnd (lo, hi));
            std::sort (xs.begin(), xs.end());
            bool ok = xs.front() - lo >= gap * 0.5 && hi - xs.back() >= gap * 0.5;
            for (int i = 1; i < n && ok; ++i)
                ok = xs[(size_t) i] - xs[(size_t) i - 1] >= gap;
            if (ok)
                return xs;
        }
        std::vector<double> even;
        for (int i = 0; i < n; ++i)
            even.push_back (lo + (hi - lo) * (i + 1) / (n + 1));
        return even;
    };

    const double variation = rnd (0.3, 1.0), drive = rnd (0.5, 3.0);
    std::vector<CurvePoint> p;
    if (symmetric)
    {
        const auto xs = spaced (1 + (int) (uni (rng) * 4.0), 0.06, 0.94, 0.08);
        p.push_back ({ 0, 0, tension() });
        for (double x : xs)
            p.push_back ({ x, clampd (std::min (1.0, x * drive) + rnd (-0.6, 0.6) * variation, -1, 1), tension() });
        p.push_back ({ 1, uni (rng) < 0.7 ? rnd (0.5, 1.0) : rnd (-1.0, 0.5), 0 });
    }
    else
    {
        const auto xs = spaced (2 + (int) (uni (rng) * 4.0), -0.94, 0.94, 0.08);
        const double off = rnd (-0.3, 0.3);
        p.push_back ({ -1, clampd (-1 + off + rnd (-0.5, 0.5) * variation, -1, 1), tension() });
        for (double x : xs)
            p.push_back ({ x, clampd (clampd (x * drive, -1, 1) + off + rnd (-0.6, 0.6) * variation, -1, 1), tension() });
        p.push_back ({ 1, clampd (1 + off + rnd (-0.5, 0.5) * variation, -1, 1), 0 });
    }
    for (auto& q : p)
    {
        q.x = std::round (q.x * 10000.0) / 10000.0;
        q.y = std::round (q.y * 10000.0) / 10000.0;
    }
    pts = std::move (p);
    enforceSymmetry();
}

std::string CurveModel::toString() const
{
    std::ostringstream os;
    os.precision (17);
    os << 2 << ' ' << (symmetric ? 1 : 0) << ' ' << pts.size();
    for (const auto& q : pts)
        os << ' ' << q.x << ' ' << q.y << ' ' << q.t;
    return os.str();
}

bool CurveModel::fromString (const std::string& s)
{
    std::istringstream is (s);
    std::vector<double> a;
    double v;
    while (is >> v)
        a.push_back (v);
    if (a.size() < 3)
        return false;
    const int ver = (int) a[0];
    const size_t offset = ver == 2 ? 3 : 2;
    const double countD = a[offset - 1];
    if ((ver != 1 && ver != 2) || countD != std::floor (countD) || countD < 2 || countD > CurveSnapshot::maxPoints
        || a.size() != offset + 3 * (size_t) countD)
        return false;
    const size_t count = (size_t) countD;
    std::vector<CurvePoint> p;
    for (size_t i = 0; i < count; ++i)
    {
        const double x = a[offset + 3 * i], y = a[offset + 3 * i + 1], t = a[offset + 3 * i + 2];
        if (! std::isfinite (x) || ! std::isfinite (y) || ! std::isfinite (t) || x < -1 || x > 1 || (i > 0 && x <= p.back().x))
            return false;
        p.push_back ({ x, clampd (y, -1, 1), clampd (t, -4, 4) });
    }
    if (p.front().x != -1.0 || p.back().x != 1.0)
        return false;
    symmetric = ver == 2 && a[1] == 1.0;
    pts = std::move (p);
    enforceSymmetry();
    return true;
}

CurveSnapshot CurveModel::makeSnapshot() const
{
    CurveSnapshot s;
    s.symmetric = symmetric;
    s.count = std::min ((int) pts.size(), CurveSnapshot::maxPoints);
    for (int i = 0; i < s.count; ++i)
        s.pts[(size_t) i] = pts[(size_t) i];

    // Merge coincident points into exact vertical steps (same rule as the device).
    for (int i = 0; i < s.count;)
    {
        int j = i;
        while (j + 1 < s.count && pts[(size_t) j + 1].x - pts[(size_t) j].x <= stepGap * 1.01)
            ++j;
        if (j > i)
        {
            double x = pts[(size_t) i].x;
            for (int k = i; k <= j; ++k)
                if (pts[(size_t) k].x == 0.0)
                    x = 0.0;
            for (int k = i; k <= j; ++k)
                s.pts[(size_t) k].x = x;
        }
        i = j + 1;
    }

    const double d = 1.0 / 2048.0; // table neighbours of x = 0 in the device
    s.zeroOutput = 0.5 * (s.eval (-d) + s.eval (d));
    return s;
}

} // namespace illuminize
