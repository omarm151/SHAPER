// Illuminize Shaper — audio engine (plain C++, no JUCE).
// A line-by-line port of the Max for Live device's gen~ code (DSP_Source.genexpr v0.18.3
// and Limiter_Source.genexpr), plus auto-gain. All processing is double precision.
#pragma once

#include "Curve.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>

namespace illuminize
{

// line~-style linear ramp: reaches a new target in a fixed time.
class Ramp
{
public:
    void setup (double sampleRate, double ms) { length = std::max (1, (int) std::lround (sampleRate * ms * 0.001)); }
    void snap (double v) { current = goal = v; remaining = 0; }
    void setTarget (double v)
    {
        if (! (v < goal) && ! (v > goal)) // unchanged target
            return;
        goal = v;
        remaining = length;
        increment = (goal - current) / remaining;
    }
    double next() noexcept
    {
        if (remaining > 0)
        {
            current += increment;
            if (--remaining == 0)
                current = goal;
        }
        return current;
    }
    double value() const noexcept { return current; }
    double target() const noexcept { return goal; }
    bool settled() const noexcept { return remaining == 0; }

private:
    double current = 0, goal = 0, increment = 0;
    int remaining = 0, length = 1;
};

struct EngineParams
{
    double preDb = 0.0;        // -24..36
    double biasPercent = 0.0;  // -100..100
    double mixPercent = 100.0; // 0..100
    double postDb = 0.0;       // -36..6
    bool dcFilter = false;
    int oversampling = 1;      // 0 = 1x, 1 = 2x, 2 = 4x
    bool limiter = false;
    double ceilingDb = -0.3;   // -12..0
    double releaseMs = 80.0;   // 10..500
    double autoGain = 1.0;     // linear gain applied to the wet signal (1 = off)
};

class ShaperEngine
{
public:
    static constexpr int oversamplingDelay = 40;
    static constexpr int limiterLookahead = 64;
    static constexpr int latencySamples = oversamplingDelay + limiterLookahead; // 104, constant

    ShaperEngine();

    void prepare (double sampleRate);
    void reset();

    // Call at the start of each block (any thread that owns processing).
    void setParams (const EngineParams& p);
    // Jump all smoothed controls to their targets (use after the first setParams on load).
    void snapToTargets();

    // Starts a 10 ms crossfade to a new curve. Only call when curveFadeDone() is true.
    void setCurve (const CurveSnapshot& c);
    bool curveFadeDone() const noexcept { return bankRamp.settled(); }

    // In-place stereo processing. right may equal left for mono.
    void process (float* left, float* right, int numSamples) noexcept;

    // Meters (written by the audio thread, read by the UI).
    std::atomic<float> gainReductionDb { 0.0f };
    std::atomic<float> inputPeak { 0.0f };
    std::atomic<float> inputEnergy { 0.0f };
    std::atomic<float> inputBrightness { 0.0f };

private:
    double processSample (double inL, double inR, double& outR) noexcept;
    double limitSample (double inL, double inR, double& outR) noexcept;

    double sr = 44100.0;

    // Curve banks (crossfaded like the device's two buffers).
    CurveSnapshot banks[2];
    int activeBank = 0;
    Ramp bankRamp; // 0 = bank A, 1 = bank B

    // Smoothed controls (20 ms, like line~ in the device).
    Ramp preRamp, mixRamp, postRamp, dcRamp, biasRamp, ceilingRamp, autoGainRamp;
    double releaseMs = 80.0;
    int requestedMode = 1;
    bool limiterOn = false;

    // Oversampling FIR coefficients: [0] = 1x, [8..88] = 2x, [96..256] = 4x.
    std::array<double, 260> coef {};
    std::array<std::array<double, 2>, 64> xh {};   // base-rate input history
    std::array<std::array<double, 2>, 256> wh {};  // oversampled shaped history
    int xpos = 0, wpos = 0;
    int mode = 0, hold = 0;
    double fade = 0.0;
    double dcxL = 0, dcxR = 0, dchL = 0, dchR = 0;

    // Limiter state (port of Limiter_Source.genexpr).
    std::array<std::array<double, 2>, 65> limAudio {};
    std::array<double, 65> limPeaks {};
    int limPos = 0;
    double attenuation = 0.0, blend = 0.0, lastGain = 1.0;

    // Visual analysis state.
    double visPower = 0, visDiffPower = 0, visPrevL = 0, visPrevR = 0;
};

// Auto-gain: level compensation that keeps a -12 dBFS sine at the same RMS level through
// Pre, Bias and the curve. Computed off the audio thread whenever those change.
double computeAutoGain (const CurveSnapshot& curve, double preDb, double biasPercent);

} // namespace illuminize
