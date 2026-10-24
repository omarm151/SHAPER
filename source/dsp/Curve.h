// Illuminize Shaper — transfer curve model and real-time snapshot.
// Plain C++ (no JUCE) so it can be unit-tested anywhere.
// Behaviour mirrors the Max for Live device's isl_curve.js and DSP_Source.genexpr.
#pragma once

#include <array>
#include <cstdint>
#include <random>
#include <string>
#include <vector>

namespace illuminize
{

struct CurvePoint
{
    double x = 0.0, y = 0.0, t = 0.0; // position (-1..1), level (-1..1), segment tension (-4..4)
};

// Immutable, allocation-free copy of a curve used on the audio thread.
struct CurveSnapshot
{
    static constexpr int maxPoints = 65;

    std::array<CurvePoint, maxPoints> pts {}; // "evaluated" points: coincident points merged into exact steps
    int count = 3;
    bool symmetric = true;
    double zeroOutput = 0.0; // output for near-silent input (average just left/right of 0)

    // Exact evaluation, right-continuous at steps; odd symmetry holds exactly at 0.
    double eval (double x) const noexcept;
};

class CurveModel
{
public:
    static constexpr double stepGap = 1e-12;
    static constexpr int maxPointsSym = 63, maxPointsAsym = 32;

    enum Preset
    {
        linear = 0, soft, tape, tube, clip, fold, hardFold, steps, rectify, halfRectify,
        numPresets
    };
    static const char* presetName (int index);

    CurveModel();

    const std::vector<CurvePoint>& points() const noexcept { return pts; }
    std::vector<CurvePoint>& mutablePoints() noexcept { return pts; } // call enforceSymmetry() after editing
    bool isSymmetric() const noexcept { return symmetric; }

    // Editing (keeps the model valid; enforces symmetry when SYM is on)
    void setPoints (std::vector<CurvePoint> p, bool sym);
    void setSymmetric (bool sym);
    void applyPreset (int preset);
    void randomize (std::mt19937& rng);
    void reset();

    // Matches the device: preset index if the curve equals one, else -1 ("Custom")
    int matchingPreset() const;

    // Serialisation compatible with the device's stored format: "2 sym count x y t ..."
    std::string toString() const;
    bool fromString (const std::string& s);

    CurveSnapshot makeSnapshot() const;

    // Mirror a positive half (x >= 0 points, including the centre) into a full odd-symmetric curve.
    static std::vector<CurvePoint> mirrored (const std::vector<CurvePoint>& positiveHalf);

    // Helper used by the editor and tests: evaluate the current curve exactly.
    double eval (double x) const { return makeSnapshot().eval (x); }

    void enforceSymmetry();

private:
    std::vector<CurvePoint> pts;
    bool symmetric = true;

    static std::vector<CurvePoint> presetPoints (int preset, bool& symOut);
};

} // namespace illuminize
