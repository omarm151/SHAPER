"""Line-by-line Python port of DSP_Source.genexpr v0.16 for verification."""
import math
import numpy as np

STORAGE = 4300


def build_table(points, sym, exact=None):
    """Mirror isl_curve.js flush(): table + exact data."""
    ev = [list(p) for p in points]
    # Merge coincident points into exact steps, as isl_curve.js prepareCurve() does.
    i = 0
    while i < len(ev):
        j = i
        while j + 1 < len(points) and points[j + 1][0] - points[j][0] <= 1e-12 * 1.01:
            j += 1
        if j > i:
            x = points[i][0]
            for k in range(i, j + 1):
                if points[k][0] == 0: x = 0
            for k in range(i, j + 1):
                ev[k][0] = x
        i = j + 1
    exact_mode = any(ev[i + 1][0] - ev[i][0] < 2 / 2048 for i in range(len(ev) - 1))

    def interp(p, q, x):
        w = q[0] - p[0]
        if w <= 0:
            return q[1]
        u = min(max((x - p[0]) / w, 0), 1)
        k = 2 ** abs(p[2])
        if p[2] > 0:
            u = u ** k
        elif p[2] < 0:
            u = 1 - (1 - u) ** k
        return p[1] + (q[1] - p[1]) * u

    def curve(x):
        x = min(max(x, -1), 1)
        if sym and x == 0:
            return 0
        i = 0
        while i + 1 < len(ev) and ev[i + 1][0] <= x:
            i += 1
        if i == len(ev) - 1:
            return ev[i][1]
        return interp(ev[i], ev[i + 1], x)

    a = [curve(2 * i / 4096 - 1) for i in range(4097)]
    a += [1 if (exact if exact is not None else exact_mode) else 0, len(ev), 1 if sym else 0]
    for p in ev:
        a += p
    a += [0] * (STORAGE - len(a))
    return np.array(a, float)


def peek(arr, i):
    i = int(i)
    return arr[i] if 0 <= i < len(arr) else 0.0


def shape(buf, x):
    u = (x + 1) * 2048
    i = math.floor(u)
    f = u - i
    y = peek(buf, i) * (1 - f) + peek(buf, min(i + 1, 4096)) * f
    if peek(buf, 4097) > 0.5:
        count = min(max(peek(buf, 4098), 2), 65)
        lo, hi, mid = 0, count, 0
        for _ in range(7):
            if lo < hi:
                mid = math.floor((lo + hi) * 0.5)
                if peek(buf, 4100 + 3 * mid) <= x:
                    lo = mid + 1
                else:
                    hi = mid
        left = min(max(lo - 1, 0), count - 1)
        right = min(left + 1, count - 1)
        px0, py0, tension = peek(buf, 4100 + 3 * left), peek(buf, 4101 + 3 * left), peek(buf, 4102 + 3 * left)
        px1, py1 = peek(buf, 4100 + 3 * right), peek(buf, 4101 + 3 * right)
        t = min(max((x - px0) / max(px1 - px0, 1e-30), 0), 1)
        ex = 2 ** abs(tension)
        if tension > 0:
            t = t ** ex
        elif tension < 0:
            t = 1 - (1 - t) ** ex
        y = py0 + (py1 - py0) * t
        if x == 0 and peek(buf, 4099) > 0.5:
            y = 0
    return y


def bessel0(v):
    s = term = 1.0
    for k in range(1, 40):
        q = v / (2 * k)
        term *= q * q
        s += term
    return s


def design(coef, offset, length, factor):
    beta = 7.86
    norm = 1 / bessel0(beta)
    half = (length - 1) * 0.5
    total = 0
    for k in range(length):
        n = k - half
        a = n / factor
        sc = 1.0
        if abs(a) > 0:
            sc = math.sin(math.pi * a) / (math.pi * a)
        r = n / half
        w = bessel0(beta * math.sqrt(max(1 - r * r, 0))) * norm
        h = sc * w
        coef[offset + k] = h
        total += h
    for k in range(length):
        coef[offset + k] = coef[offset + k] * factor / total


class Gen:
    def __init__(self, sr):
        self.sr = sr
        self.coef = np.zeros(260)
        self.xh = np.zeros((64, 2))
        self.wh = np.zeros((256, 2))
        self.ready = 0
        self.xpos = self.wpos = 0
        self.mode = 0
        self.fade = 0.0
        self.hold = 0
        self.dcx = [0.0, 0.0]
        self.dch = [0.0, 0.0]

    def tick(self, in1, in2, in3, in4, in5, in6, in7, in8, curveA, curveB, in9=0.0):
        sr = self.sr
        if self.ready < 0.5:
            self.coef[0] = 1
            design(self.coef, 8, 81, 2)
            design(self.coef, 96, 161, 4)
            self.ready = 1
        req = min(max(math.floor(in8 + 0.5), 0), 2)
        fstep = 1 / (0.003 * sr)
        curmode, curfade, curhold = self.mode, self.fade, self.hold
        if req != curmode:
            curfade -= fstep
            if curfade <= 0:
                curfade = 0
                curmode = req
                curhold = 64
        elif curhold > 0:
            curhold -= 1
        else:
            curfade = min(1, curfade + fstep)
        self.mode, self.fade, self.hold = curmode, curfade, curhold
        factor, length, offset, extra = 1, 1, 0, 40
        if curmode > 1.5:
            factor, length, offset, extra = 4, 161, 96, 0
        elif curmode > 0.5:
            factor, length, offset, extra = 2, 81, 8, 0
        xp = self.xpos
        self.xh[xp] = (in1, in2)
        pre = max(in3, 0)
        bank = min(max(in7, 0), 1)
        bias = min(max(in9, -0.5), 0.5)
        dc = max(min(max(in6, 0), 1), min(max(abs(bias) * 50, 0), 1))
        r = math.exp(-6.283185307179586 * 10 / (sr * factor))
        sx = list(self.dcx)
        sh = list(self.dch)
        wp = self.wpos
        za = 0.5 * (curveA[2047] + curveA[2049]); zb = 0.5 * (curveB[2047] + curveB[2049])
        zero_out = za + (zb - za) * bank
        coef = self.coef
        for p in range(factor):
            uL = uR = 0.0
            j = 0
            while p + factor * j < length:
                h = coef[offset + p + factor * j]
                idx = (xp - j + 64) % 64
                uL += h * self.xh[idx, 0]
                uR += h * self.xh[idx, 1]
                j += 1
            xs = [min(max(uL * pre + bias, -1), 1), min(max(uR * pre + bias, -1), 1)]
            for c in range(2):
                a = b = 0.0
                if bank < 1:
                    a = shape(curveA, xs[c])
                if bank > 0:
                    b = shape(curveB, xs[c])
                v = a + (b - a) * bank - zero_out
                d = v - sx[c] + r * sh[c]
                sx[c] = v
                sh[c] = d
                v = v + (d - v) * dc
                self.wh[(wp + p) % 256, c] = v
        self.dcx, self.dch = sx, sh
        yL = yR = 0.0
        for k in range(length):
            h = coef[offset + k]
            idx = (wp - k - extra + 512) % 256
            yL += h * self.wh[idx, 0]
            yR += h * self.wh[idx, 1]
        yL = yL / factor * curfade
        yR = yR / factor * curfade
        self.wpos = (wp + factor) % 256
        self.xpos = (xp + 1) % 64
        didx = (xp - 40 + 64) % 64
        dryL, dryR = self.xh[didx]
        wet = min(max(in4, 0), 1)
        trim = max(in5, 0)
        return (dryL + (yL - dryL) * wet) * trim, (dryR + (yR - dryR) * wet) * trim


def run(x, mode_seq, curve, pre=1.0, mix=1.0, post=1.0, dc=0.0, sr=44100, sym=True, bias=0.0):
    g = Gen(sr)
    tab = build_table(curve, sym)
    out = np.zeros((len(x), 2))
    for n in range(len(x)):
        m = mode_seq[n] if hasattr(mode_seq, '__len__') else mode_seq
        out[n] = g.tick(x[n, 0], x[n, 1], pre, mix, post, dc, 0, m, tab, tab, bias)
    return out
