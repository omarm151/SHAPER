#include "ShaperEngine.h"

#include <algorithm>

namespace illuminize
{

namespace
{
    constexpr double pi = 3.141592653589793;

    double clampd (double v, double lo, double hi) { return std::max (lo, std::min (hi, v)); }
    double dbToGain (double db) { return std::pow (10.0, db / 20.0); }

    double bessel0 (double v)
    {
        double s = 1.0, term = 1.0;
        for (int k = 1; k < 40; ++k)
        {
            const double q = v / (2.0 * k);
            term *= q * q;
            s += term;
        }
        return s;
    }

    // Kaiser-windowed sinc, cutoff at the original Nyquist, DC gain = factor (same as the device).
    void design (std::array<double, 260>& coef, int offset, int len, int factor)
    {
        const double beta = 7.86, norm = 1.0 / bessel0 (beta), half = (len - 1) * 0.5;
        double total = 0.0;
        for (int k = 0; k < len; ++k)
        {
            const double n = k - half, a = n / factor;
            const double sc = std::abs (a) > 0.0 ? std::sin (pi * a) / (pi * a) : 1.0;
            const double r = n / half;
            const double w = bessel0 (beta * std::sqrt (std::max (1.0 - r * r, 0.0))) * norm;
            coef[(size_t) (offset + k)] = sc * w;
            total += sc * w;
        }
        for (int k = 0; k < len; ++k)
            coef[(size_t) (offset + k)] *= factor / total;
    }
} // namespace

ShaperEngine::ShaperEngine()
{
    coef.fill (0.0);
    coef[0] = 1.0;
    design (coef, 8, 81, 2);
    design (coef, 96, 161, 4);
    CurveModel m;
    banks[0] = banks[1] = m.makeSnapshot();
    prepare (44100.0);
}

void ShaperEngine::prepare (double sampleRate)
{
    sr = sampleRate > 0 ? sampleRate : 44100.0;
    for (auto* r : { &preRamp, &mixRamp, &postRamp, &dcRamp, &biasRamp, &ceilingRamp })
        r->setup (sr, 20.0);
    autoGainRamp.setup (sr, 50.0);
    bankRamp.setup (sr, 10.0);
    reset();
}

void ShaperEngine::reset()
{
    for (auto& a : xh) a = { 0.0, 0.0 };
    for (auto& a : wh) a = { 0.0, 0.0 };
    for (auto& a : limAudio) a = { 0.0, 0.0 };
    limPeaks.fill (0.0);
    xpos = wpos = limPos = 0;
    mode = requestedMode; // start directly in the requested mode, then fade in
    hold = 0;
    fade = 0.0;
    dcxL = dcxR = dchL = dchR = 0.0;
    attenuation = 0.0;
    blend = limiterOn ? 1.0 : 0.0;
    visPower = visDiffPower = visPrevL = visPrevR = 0.0;
    preRamp.snap (preRamp.value());
    mixRamp.snap (mixRamp.value());
    postRamp.snap (postRamp.value());
    dcRamp.snap (dcRamp.value());
    biasRamp.snap (biasRamp.value());
    ceilingRamp.snap (ceilingRamp.value());
    autoGainRamp.snap (autoGainRamp.value());
}

void ShaperEngine::setParams (const EngineParams& p)
{
    preRamp.setTarget (dbToGain (clampd (p.preDb, -24.0, 36.0)));
    mixRamp.setTarget (clampd (p.mixPercent, 0.0, 100.0) * 0.01);
    postRamp.setTarget (dbToGain (clampd (p.postDb, -36.0, 6.0)));
    dcRamp.setTarget (p.dcFilter ? 1.0 : 0.0);
    biasRamp.setTarget (clampd (p.biasPercent, -100.0, 100.0) * 0.005);
    ceilingRamp.setTarget (clampd (p.ceilingDb, -12.0, 0.0));
    autoGainRamp.setTarget (clampd (p.autoGain, 0.0, 16.0));
    releaseMs = clampd (p.releaseMs, 10.0, 500.0);
    requestedMode = std::max (0, std::min (2, p.oversampling));
    limiterOn = p.limiter;
}

void ShaperEngine::snapToTargets()
{
    for (auto* r : { &preRamp, &mixRamp, &postRamp, &dcRamp, &biasRamp, &ceilingRamp, &autoGainRamp })
        r->snap (r->target());
    mode = requestedMode;
    blend = limiterOn ? 1.0 : 0.0;
}

void ShaperEngine::setCurve (const CurveSnapshot& c)
{
    // Write the inactive bank, then crossfade to it over 10 ms.
    const int target = 1 - activeBank;
    banks[target] = c;
    activeBank = target;
    bankRamp.setTarget ((double) target);
}

void ShaperEngine::process (float* left, float* right, int numSamples) noexcept
{
    double grMax = 0.0, peak = 0.0;
    const double a = std::exp (-1.0 / (0.02 * sr));
    for (int i = 0; i < numSamples; ++i)
    {
        const double inL = left[i], inR = right[i];

        // Input analysis for the AUDIO FORM display (does not affect the sound).
        peak = std::max (peak, std::max (std::abs (inL), std::abs (inR)));
        const double dL = inL - visPrevL, dR = inR - visPrevR;
        visPower = a * visPower + (1 - a) * (inL * inL + inR * inR) * 0.5;
        visDiffPower = a * visDiffPower + (1 - a) * (dL * dL + dR * dR) * 0.5;
        visPrevL = inL;
        visPrevR = inR;

        double shR = 0.0;
        const double shL = processSample (inL, inR, shR);
        double outR = 0.0;
        const double outL = limitSample (shL, shR, outR);
        left[i] = (float) outL;
        if (right != left)
            right[i] = (float) outR;
        grMax = std::max (grMax, -20.0 * std::log10 (std::max (lastGain, 1e-12)));
    }
    gainReductionDb.store ((float) grMax);
    inputPeak.store ((float) std::max ((double) inputPeak.load(), peak));
    inputEnergy.store ((float) std::sqrt (std::max (visPower, 0.0)));
    inputBrightness.store ((float) clampd (std::sqrt (std::max (visDiffPower, 0.0) / std::max (4.0 * visPower, 1e-20)), 0.0, 1.0));
}

double ShaperEngine::processSample (double in1, double in2, double& out2) noexcept
{
    // Click-free oversampling change: fade wet out, switch, flush filters, fade in.
    const double fstep = 1.0 / (0.003 * sr);
    if (requestedMode != mode)
    {
        fade -= fstep;
        if (fade <= 0.0)
        {
            fade = 0.0;
            mode = requestedMode;
            hold = 64;
        }
    }
    else if (hold > 0)
        --hold;
    else
        fade = std::min (1.0, fade + fstep);

    int factor = 1, len = 1, offset = 0, extra = 40;
    if (mode == 2)
        factor = 4, len = 161, offset = 96, extra = 0;
    else if (mode == 1)
        factor = 2, len = 81, offset = 8, extra = 0;

    const int xp = xpos;
    xh[(size_t) xp] = { in1, in2 };

    const double pre = std::max (preRamp.next(), 0.0);
    const double wet = clampd (mixRamp.next(), 0.0, 1.0);
    const double trim = std::max (postRamp.next(), 0.0);
    const double bank = clampd (bankRamp.next(), 0.0, 1.0);
    const double bias = clampd (biasRamp.next(), -0.5, 0.5);
    const double autoGain = autoGainRamp.next();
    // Bias adds an offset, so the DC filter fades in automatically while Bias is not 0.
    const double dc = std::max (clampd (dcRamp.next(), 0.0, 1.0), clampd (std::abs (bias) * 50.0, 0.0, 1.0));
    const double r = std::exp (-6.283185307179586 * 10.0 / (sr * factor));

    const CurveSnapshot& A = banks[0];
    const CurveSnapshot& B = banks[1];
    // Constant offset at silence is always removed, so SYM OFF curve changes never thump.
    const double zeroOut = A.zeroOutput + (B.zeroOutput - A.zeroOutput) * bank;

    double sxL = dcxL, sxR = dcxR, shL = dchL, shR = dchR;
    const int wp = wpos;

    for (int p = 0; p < factor; ++p)
    {
        // Polyphase interpolation: phase p of the upsampled signal.
        double uL = 0.0, uR = 0.0;
        for (int j = 0; p + factor * j < len; ++j)
        {
            const double h = coef[(size_t) (offset + p + factor * j)];
            const auto& x = xh[(size_t) ((xp - j + 64) % 64)];
            uL += h * x[0];
            uR += h * x[1];
        }
        const double xL = clampd (uL * pre + bias, -1.0, 1.0);
        const double xR = clampd (uR * pre + bias, -1.0, 1.0);

        double aL = 0, aR = 0, bL = 0, bR = 0;
        if (bank < 1.0)
        {
            aL = A.eval (xL);
            aR = A.eval (xR);
        }
        if (bank > 0.0)
        {
            bL = B.eval (xL);
            bR = B.eval (xR);
        }
        double vL = aL + (bL - aL) * bank - zeroOut;
        double vR = aR + (bR - aR) * bank - zeroOut;

        // 10 Hz DC blocker, crossfaded in by the DC switch.
        const double dL = vL - sxL + r * shL;
        const double dR = vR - sxR + r * shR;
        sxL = vL;
        sxR = vR;
        shL = dL;
        shR = dR;
        vL += (dL - vL) * dc;
        vR += (dR - vR) * dc;
        wh[(size_t) ((wp + p) % 256)] = { vL, vR };
    }
    dcxL = sxL;
    dcxR = sxR;
    dchL = shL;
    dchR = shR;

    // Decimation: anti-alias filter evaluated once per output sample.
    double yL = 0.0, yR = 0.0;
    for (int k = 0; k < len; ++k)
    {
        const double h = coef[(size_t) (offset + k)];
        const auto& w = wh[(size_t) ((wp - k - extra + 512) % 256)];
        yL += h * w[0];
        yR += h * w[1];
    }
    yL = yL / factor * fade * autoGain;
    yR = yR / factor * fade * autoGain;

    wpos = (wp + factor) % 256;
    xpos = (xp + 1) % 64;

    // Dry is delayed by the same 40 samples, so Mix stays phase-aligned.
    const auto& dry = xh[(size_t) ((xp - 40 + 64) % 64)];
    out2 = (dry[1] + (yR - dry[1]) * wet) * trim;
    return (dry[0] + (yL - dry[0]) * wet) * trim;
}

double ShaperEngine::limitSample (double in1, double in2, double& out2) noexcept
{
    // Stereo-linked sample-peak limiter, 64 samples fixed lookahead (always in the path).
    const double ceiling = dbToGain (clampd (ceilingRamp.next(), -12.0, 0.0));
    const double release = releaseMs;
    const double nextBlend = clampd (blend + (limiterOn ? 1.0 : -1.0) / (sr * 0.005), 0.0, 1.0);
    const int oldest = (limPos + 1) % 65;
    const double delayedL = limAudio[(size_t) oldest][0];
    const double delayedR = limAudio[(size_t) oldest][1];

    double required = 0.0;
    if (limiterOn || nextBlend > 0.0)
    {
        // Anticipate each future peak with a linear attack ramp.
        for (int j = 0; j < 64; ++j)
        {
            const double pk = std::max (limPeaks[(size_t) ((oldest + j) % 65)], 1e-12);
            const double reduction = std::max (0.0, 1.0 - ceiling / pk);
            required = std::max (required, reduction * (1.0 - j / 64.0));
        }
    }
    const double nextAttenuation = std::max (required, attenuation * std::exp (-1.0 / (sr * release * 0.001)));
    double limitedGain = clampd (1.0 - nextAttenuation, 0.0, 1.0);
    const double outputPeak = std::max (std::abs (delayedL), std::abs (delayedR));
    limitedGain = std::min (limitedGain, ceiling / std::max (outputPeak, 1e-12));
    const double actualGain = 1.0 - nextBlend * (1.0 - limitedGain);
    lastGain = actualGain;

    limAudio[(size_t) limPos] = { in1, in2 };
    limPeaks[(size_t) limPos] = std::max (std::abs (in1), std::abs (in2));
    limPos = oldest;
    attenuation = nextAttenuation;
    blend = nextBlend;

    out2 = delayedR * actualGain;
    return delayedL * actualGain;
}

double computeAutoGain (const CurveSnapshot& curve, double preDb, double biasPercent)
{
    const double pre = dbToGain (clampd (preDb, -24.0, 36.0));
    const double bias = clampd (biasPercent, -100.0, 100.0) * 0.005;
    const double amp = 0.25; // -12 dBFS reference sine
    constexpr int n = 512;
    double sumIn = 0, sumOut = 0, meanOut = 0;
    std::array<double, n> out {};
    for (int i = 0; i < n; ++i)
    {
        const double x = amp * std::sin (2.0 * pi * (i + 0.5) / n);
        out[(size_t) i] = curve.eval (clampd (x * pre + bias, -1.0, 1.0)) - curve.zeroOutput;
        meanOut += out[(size_t) i];
        sumIn += x * x;
    }
    meanOut /= n; // DC is not loudness
    for (double v : out)
        sumOut += (v - meanOut) * (v - meanOut);
    if (sumOut < 1e-12)
        return 1.0; // silent curve: leave the level alone
    return clampd (std::sqrt (sumIn / sumOut), dbToGain (-36.0), dbToGain (24.0));
}

} // namespace illuminize
