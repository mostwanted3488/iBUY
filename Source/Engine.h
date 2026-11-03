// iBuy audio engine. Plain C++17, no JUCE dependency, so it can be unit-tested on its own.
#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <vector>

namespace ibuy
{
constexpr double kPi = 3.14159265358979323846;

// Snapshot of every control, read once per audio block.
struct Params
{
    int eq = 0;              // 0 Flat, 1 Bass Booster, 2 Treble Booster, 3 Lounge, 4 Small Speakers, 5 Spoken Word
    bool lowBattery = false;
    bool nextRoom = false;
    bool earbuds = false;
    bool reverb = false;
    bool delay = false;
    bool chorus = false;
    bool filter = false;
    bool bitcrush = false;
    bool power = true;       // false = clean pass-through
    int loopDiv = 2;         // 0..5 = 1/2, 1/4, 1/8, 1/16, 1/32, 1/64
    bool loopHold = false;   // looper is engaged while true
    double bpm = 120.0;
};

// ---------------------------------------------------------------- filters
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;

    void reset() { z1 = z2 = 0; }

    inline float process (float x)
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }

    void copyCoefs (const Biquad& o) { b0 = o.b0; b1 = o.b1; b2 = o.b2; a1 = o.a1; a2 = o.a2; }

    void lowpass (double sr, double f, double q)
    {
        f = std::clamp (f, 20.0, sr * 0.45);
        const double w = 2.0 * kPi * f / sr, c = std::cos (w), al = std::sin (w) / (2.0 * q);
        set ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al);
    }

    void highpass (double sr, double f, double q)
    {
        f = std::clamp (f, 20.0, sr * 0.45);
        const double w = 2.0 * kPi * f / sr, c = std::cos (w), al = std::sin (w) / (2.0 * q);
        set ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al);
    }

    void peak (double sr, double f, double q, double dB)
    {
        f = std::clamp (f, 20.0, sr * 0.45);
        const double A = std::pow (10.0, dB / 40.0), w = 2.0 * kPi * f / sr, c = std::cos (w), al = std::sin (w) / (2.0 * q);
        set (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
    }

    void lowShelf (double sr, double f, double dB)
    {
        f = std::clamp (f, 20.0, sr * 0.45);
        const double A = std::pow (10.0, dB / 40.0), w = 2.0 * kPi * f / sr, c = std::cos (w);
        const double al = std::sin (w) / 2.0 * std::sqrt (2.0), t = 2.0 * std::sqrt (A) * al;
        set (A * ((A + 1) - (A - 1) * c + t), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - t),
             (A + 1) + (A - 1) * c + t, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - t);
    }

    void highShelf (double sr, double f, double dB)
    {
        f = std::clamp (f, 20.0, sr * 0.45);
        const double A = std::pow (10.0, dB / 40.0), w = 2.0 * kPi * f / sr, c = std::cos (w);
        const double al = std::sin (w) / 2.0 * std::sqrt (2.0), t = 2.0 * std::sqrt (A) * al;
        set (A * ((A + 1) + (A - 1) * c + t), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - t),
             (A + 1) - (A - 1) * c + t, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - t);
    }

private:
    void set (double nb0, double nb1, double nb2, double a0, double na1, double na2)
    {
        b0 = (float) (nb0 / a0); b1 = (float) (nb1 / a0); b2 = (float) (nb2 / a0);
        a1 = (float) (na1 / a0); a2 = (float) (na2 / a0);
    }
};

// ---------------------------------------------------------------- reverb (Freeverb layout)
struct Comb
{
    std::vector<float> buf;
    int idx = 0;
    float store = 0;

    void init (int n) { buf.assign ((size_t) std::max (1, n), 0.f); idx = 0; store = 0; }

    inline float process (float in, float fb, float damp)
    {
        const float out = buf[(size_t) idx];
        store = out * (1.f - damp) + store * damp;
        buf[(size_t) idx] = in + store * fb;
        if (++idx >= (int) buf.size()) idx = 0;
        return out;
    }
};

struct Allpass
{
    std::vector<float> buf;
    int idx = 0;

    void init (int n) { buf.assign ((size_t) std::max (1, n), 0.f); idx = 0; }

    inline float process (float in)
    {
        const float b = buf[(size_t) idx];
        const float out = -in + b;
        buf[(size_t) idx] = in + b * 0.5f;
        if (++idx >= (int) buf.size()) idx = 0;
        return out;
    }
};

struct Freeverb
{
    Comb comb[2][8];
    Allpass ap[2][4];
    float fb = 0.84f, damp = 0.2f;

    void prepare (double sr)
    {
        static const int ct[8] = { 1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617 };
        static const int at[4] = { 556, 441, 341, 225 };
        for (int ch = 0; ch < 2; ++ch)
        {
            const int spread = ch * 23;
            for (int i = 0; i < 8; ++i) comb[ch][i].init ((int) ((ct[i] + spread) * sr / 44100.0));
            for (int i = 0; i < 4; ++i) ap[ch][i].init ((int) ((at[i] + spread) * sr / 44100.0));
        }
    }

    void setRoom (float room, float d) { fb = room * 0.28f + 0.7f; damp = d * 0.4f; }

    inline void process (float inL, float inR, float& outL, float& outR)
    {
        const float in = (inL + inR) * 0.015f;
        float l = 0, r = 0;
        for (int i = 0; i < 8; ++i) { l += comb[0][i].process (in, fb, damp); r += comb[1][i].process (in, fb, damp); }
        for (int i = 0; i < 4; ++i) { l = ap[0][i].process (l); r = ap[1][i].process (r); }
        outL = l; outR = r;
    }
};

// ---------------------------------------------------------------- delay line
struct DelayLine
{
    std::vector<float> buf;
    int w = 0;

    void init (int n) { buf.assign ((size_t) std::max (4, n), 0.f); w = 0; }

    inline void write (float x) { buf[(size_t) w] = x; if (++w >= (int) buf.size()) w = 0; }

    // d = delay in samples (>= 1)
    inline float read (float d) const
    {
        const int size = (int) buf.size();
        float pos = (float) w - d;
        while (pos < 0) pos += (float) size;
        int i0 = (int) pos;
        const float fr = pos - (float) i0;
        if (i0 >= size) i0 -= size;
        int i1 = i0 + 1; if (i1 >= size) i1 = 0;
        return buf[(size_t) i0] * (1.f - fr) + buf[(size_t) i1] * fr;
    }
};

// ---------------------------------------------------------------- engine
class Engine
{
public:
    static constexpr int kBars = 14;
    static constexpr int kScope = 256;

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        N = (int) (8.0 * sr) + 8;
        for (int c = 0; c < 2; ++c)
        {
            loopRing[c].assign ((size_t) N, 0.f);
            loopBuf[c].assign ((size_t) N, 0.f);
            tapeRing[c].assign ((size_t) N, 0.f);
            chDL[c].init ((int) (0.1 * sr) + 4);
            dlDL[c].init ((int) (1.5 * sr) + 4);
        }
        mainRev.prepare (sr); mainRev.setRoom (0.85f, 0.35f);
        roomRev.prepare (sr); roomRev.setRoom (0.45f, 0.7f);
        smoothK = (float) (1.0 - std::exp (-1.0 / (0.02 * sr)));
        for (int i = 0; i < 1024; ++i) hann[(size_t) i] = 0.5f * (1.f - std::cos (2.f * (float) kPi * (float) i / 1023.f));
        initFft();
        reset();
        lastEq = -1;
        roomLP[0].lowpass (sr, 620, 0.6); roomLP[1].copyCoefs (roomLP[0]);
        roomLS[0].lowShelf (sr, 110, 6.0); roomLS[1].copyCoefs (roomLS[0]);
        ebHP.highpass (sr, 240, 0.7); ebLP.lowpass (sr, 6500, 0.7); ebPk.peak (sr, 2600, 1.0, 5.0);
        crLP[0].lowpass (sr, 5200, 0.5); crLP[1].copyCoefs (crLP[0]);
    }

    void reset()
    {
        w = 0; absW = 0;
        loopFade = 0; loopLen = 0; loopPos = 0; prevHold = false;
        tapeOn = false; tapeFade = 0; tapeTarget = 0; tapeT = 0; readAbs = 0; prevLB = false;
        lbFreq = 20000; ctr = 0;
        rmT = ebT = fltT = crT = chT = dT = rT = 0; pgT = 1;
        fltPhase = chPhase = 0;
        for (int c = 0; c < 2; ++c)
        {
            std::fill (loopRing[c].begin(), loopRing[c].end(), 0.f);
            std::fill (tapeRing[c].begin(), tapeRing[c].end(), 0.f);
            eqL[c].reset(); eqM[c].reset(); eqH[c].reset(); roomLP[c].reset(); roomLS[c].reset();
            lbLP[c].reset(); fltBq[c].reset(); crLP[c].reset();
            chDL[c].init ((int) chDL[c].buf.size()); dlDL[c].init ((int) dlDL[c].buf.size());
        }
        ebHP.reset(); ebLP.reset(); ebPk.reset();
        mainRev.prepare (sr); roomRev.prepare (sr);
        visN = 0; scopeW = 0;
    }

    void process (float* const* io, int nCh, int n, const Params& p)
    {
        if (nCh < 1 || n < 1 || N == 0) return;
        updateEq (p.eq);

        const float stepFast = 1.f / (0.005f * (float) sr);
        const float stepSlow = 1.f / (0.025f * (float) sr);
        double sumL = 0, sumR = 0;

        for (int i = 0; i < n; ++i)
        {
            const float inL = io[0][i];
            const float inR = nCh > 1 ? io[1][i] : inL;

            // 1) record the raw input, then the looper stage
            loopRing[0][(size_t) w] = inL; loopRing[1][(size_t) w] = inR;

            if (p.loopHold && ! prevHold)
            {
                const double bpm = p.bpm > 20.0 ? p.bpm : 120.0;
                static const double den[6] = { 2, 4, 8, 16, 32, 64 };
                int L = (int) std::lround ((240.0 / bpm) / den[std::clamp (p.loopDiv, 0, 5)] * sr);
                L = std::clamp (L, 32, N - 8);
                loopLen = L; loopPos = 0;
                int start = w - L + 1; if (start < 0) start += N;
                for (int c = 0; c < 2; ++c)
                    for (int k = 0; k < L; ++k) { int s = start + k; if (s >= N) s -= N; loopBuf[c][(size_t) k] = loopRing[c][(size_t) s]; }
            }
            prevHold = p.loopHold;
            loopFade = p.loopHold ? std::min (1.f, loopFade + stepFast) : std::max (0.f, loopFade - stepFast);

            float sL = inL, sR = inR;
            if (loopFade > 0.f && loopLen > 0)
            {
                const float fadeN = (float) std::max (1, std::min (64, loopLen / 8));
                const float gs = std::min (1.f, std::min ((float) loopPos, (float) (loopLen - 1 - loopPos)) / fadeN);
                const float lL = loopBuf[0][(size_t) loopPos] * gs, lR = loopBuf[1][(size_t) loopPos] * gs;
                sL = inL * (1.f - loopFade) + lL * loopFade;
                sR = inR * (1.f - loopFade) + lR * loopFade;
                if (++loopPos >= loopLen) loopPos = 0;
            }

            // 2) Low Battery: a tape that slows down
            tapeRing[0][(size_t) w] = sL; tapeRing[1][(size_t) w] = sR;
            if (p.lowBattery && ! prevLB) { tapeOn = true; readAbs = (double) absW; tapeT = 0; tapeTarget = 1.f; }
            if (! p.lowBattery && prevLB) tapeTarget = 0.f;
            prevLB = p.lowBattery;

            if (tapeOn)
            {
                const double rate = 1.0 - 0.75 * std::min (1.0, tapeT / 3.5);
                if (p.lowBattery) tapeT += 1.0 / sr;
                const double maxDelay = (double) (N - 8);
                if ((double) absW - readAbs > maxDelay) readAbs = (double) absW - maxDelay;
                const std::int64_t i0 = (std::int64_t) std::floor (readAbs);
                const float fr = (float) (readAbs - (double) i0);
                int a = (int) (i0 % N); if (a < 0) a += N;
                int b = a + 1; if (b >= N) b = 0;
                const float tL = tapeRing[0][(size_t) a] * (1.f - fr) + tapeRing[0][(size_t) b] * fr;
                const float tR = tapeRing[1][(size_t) a] * (1.f - fr) + tapeRing[1][(size_t) b] * fr;
                readAbs += rate;
                if (tapeTarget > tapeFade) tapeFade = std::min (tapeTarget, tapeFade + stepFast);
                else tapeFade = std::max (tapeTarget, tapeFade - stepSlow);
                sL = sL * (1.f - tapeFade) + tL * tapeFade;
                sR = sR * (1.f - tapeFade) + tR * tapeFade;
                if (tapeFade <= 0.f && tapeTarget <= 0.f) tapeOn = false;
            }

            // slow-changing coefficients, every 16 samples
            if ((ctr & 15) == 0)
            {
                const double target = (p.lowBattery && tapeOn) ? 20000.0 * std::pow (450.0 / 20000.0, std::min (1.0, tapeT / 3.5)) : 20000.0;
                lbFreq = std::exp (std::log (lbFreq) + (std::log (target) - std::log (lbFreq)) * 0.15);
                lbLP[0].lowpass (sr, lbFreq, 0.5); lbLP[1].copyCoefs (lbLP[0]);
                if (fltT > 0.001f)
                {
                    fltPhase += 2.0 * kPi * 0.22 * 16.0 / sr;
                    fltBq[0].lowpass (sr, 1500.0 + 1100.0 * std::sin (fltPhase), 3.0); fltBq[1].copyCoefs (fltBq[0]);
                }
            }
            ++ctr;

            // 3) EQ preset
            float yL = eqH[0].process (eqM[0].process (eqL[0].process (sL))) * eqGain;
            float yR = eqH[1].process (eqM[1].process (eqL[1].process (sR))) * eqGain;

            // smoothed effect amounts
            rmT += ((p.nextRoom ? 1.f : 0.f) - rmT) * smoothK;
            ebT += ((p.earbuds ? 1.f : 0.f) - ebT) * smoothK;
            fltT += ((p.filter ? 1.f : 0.f) - fltT) * smoothK;
            crT += ((p.bitcrush ? 1.f : 0.f) - crT) * smoothK;
            chT += ((p.chorus ? 1.f : 0.f) - chT) * smoothK;
            dT += ((p.delay ? 1.f : 0.f) - dT) * smoothK;
            rT += ((p.reverb ? 1.f : 0.f) - rT) * smoothK;
            pgT += ((p.power ? 1.f : 0.f) - pgT) * smoothK;

            // 4) Next Room
            if (rmT > 0.001f)
            {
                const float rl = roomLS[0].process (roomLP[0].process (yL));
                const float rr = roomLS[1].process (roomLP[1].process (yR));
                float vL, vR; roomRev.process (rl, rr, vL, vR);
                yL = yL * (1.f - rmT) + (0.55f * rl + 0.5f * vL) * rmT;
                yR = yR * (1.f - rmT) + (0.55f * rr + 0.5f * vR) * rmT;
            }

            // 5) Earbuds
            if (ebT > 0.001f)
            {
                float m = 0.5f * (yL + yR);
                m = ebPk.process (ebLP.process (ebHP.process (m)));
                m = std::tanh (1.8f * m) / 1.8f * 1.3f;
                yL = yL * (1.f - ebT) + m * ebT;
                yR = yR * (1.f - ebT) + m * ebT;
            }

            // 6) Low Battery low-pass
            if (lbFreq < 17000.0) { yL = lbLP[0].process (yL); yR = lbLP[1].process (yR); }

            // 7) Filter (slow sweep)
            if (fltT > 0.001f)
            {
                yL = yL * (1.f - fltT) + fltBq[0].process (yL) * fltT;
                yR = yR * (1.f - fltT) + fltBq[1].process (yR) * fltT;
            }

            // 8) Bitcrush (7 bit, smoothed)
            if (crT > 0.001f)
            {
                const float cL = crLP[0].process (std::round (yL * 64.f) / 64.f);
                const float cR = crLP[1].process (std::round (yR * 64.f) / 64.f);
                yL = yL * (1.f - crT) + cL * crT;
                yR = yR * (1.f - crT) + cR * crT;
            }

            // 9) Chorus
            chDL[0].write (yL); chDL[1].write (yR);
            chPhase += 2.0 * kPi * 0.9 / sr;
            const float cd = (float) ((0.022 + 0.0045 * std::sin (chPhase)) * sr);
            yL += chT * 0.6f * chDL[0].read (cd);
            yR += chT * 0.6f * chDL[1].read (cd);

            // 10) Delay
            const float dd = (float) (0.34 * sr);
            const float oL = dlDL[0].read (dd), oR = dlDL[1].read (dd);
            dlDL[0].write (yL + oL * 0.38f); dlDL[1].write (yR + oR * 0.38f);
            yL += dT * 0.42f * oL; yR += dT * 0.42f * oR;

            // 11) Reverb
            float vL, vR; mainRev.process (yL, yR, vL, vR);
            yL += rT * 0.45f * vL; yR += rT * 0.45f * vR;

            // master, safety, power
            yL = softLimit (yL); yR = softLimit (yR);
            const float oL2 = inL * (1.f - pgT) + yL * pgT;
            const float oR2 = inR * (1.f - pgT) + yR * pgT;
            io[0][i] = oL2;
            if (nCh > 1) io[1][i] = oR2;

            // visuals
            sumL += (double) oL2 * oL2; sumR += (double) oR2 * oR2;
            visBuf[(size_t) visN] = 0.5f * (oL2 + oR2);
            scope[(size_t) scopeW] = 0.5f * (oL2 + oR2); scopeW = (scopeW + 1) & 511;
            if (++visN >= 1024) { computeSpectrum(); visN = 0; }

            if (++w >= N) w = 0;
            ++absW;
        }

        levelL.store ((float) std::sqrt (sumL / n)); levelR.store ((float) std::sqrt (sumR / n));
        battery.store (tapeOn && prevLB ? (float) (1.0 - 0.92 * std::min (1.0, tapeT / 3.5)) : 1.f);
    }

    float getLevelL() const { return levelL.load(); }
    float getLevelR() const { return levelR.load(); }
    float getBar (int i) const { return bars[(size_t) i].load(); }
    float getBattery() const { return battery.load(); }

    void copyScope (float* dst, int count) const
    {
        const int start = (scopeW - count + 512 * 4) & 511;
        for (int i = 0; i < count; ++i) dst[i] = scope[(size_t) ((start + i) & 511)];
    }

private:
    static constexpr int FN = 1024;

    // Keeps stacked effects from clipping the host: transparent below 0.95, smoothly capped at 1.0.
    static inline float softLimit (float x)
    {
        if (! std::isfinite (x)) return 0.f;
        const float a = std::fabs (x);
        if (a <= 0.95f) return x;
        const float y = 0.95f + 0.05f * std::tanh ((a - 0.95f) / 0.05f);
        return x < 0.f ? -y : y;
    }

    void updateEq (int e)
    {
        if (e == lastEq) return;
        lastEq = e;
        static const float g[6][3] = { { 0, 0, 0 }, { 9, 0, 0 }, { 0, 0, 8 }, { -3, 4, -2 }, { -10, 2, 4 }, { -4, 5, 2 } };
        static const float trim[6] = { 0.f, -6.f, -4.f, -1.f, 0.f, -1.f };   // headroom for the boosting presets
        eqGain = std::pow (10.f, trim[std::clamp (e, 0, 5)] / 20.f);
        const float* v = g[std::clamp (e, 0, 5)];
        for (int c = 0; c < 2; ++c)
        {
            eqL[c].lowShelf (sr, 120, v[0]);
            eqM[c].peak (sr, 1400, 0.9, v[1]);
            eqH[c].highShelf (sr, 7000, v[2]);
        }
    }

    void initFft()
    {
        for (int i = 0; i < FN / 2; ++i) { fcos[(size_t) i] = (float) std::cos (2.0 * kPi * i / FN); fsin[(size_t) i] = (float) std::sin (2.0 * kPi * i / FN); }
        for (int i = 0; i < FN; ++i) { int r = 0, x = i; for (int b = 0; b < 10; ++b) { r = (r << 1) | (x & 1); x >>= 1; } frev[(size_t) i] = r; }
    }

    void fft (float* re, float* im)
    {
        for (int i = 0; i < FN; ++i) { const int j = frev[(size_t) i]; if (j > i) { std::swap (re[i], re[j]); std::swap (im[i], im[j]); } }
        for (int size = 2; size <= FN; size <<= 1)
        {
            const int half = size >> 1, step = FN / size;
            for (int i = 0; i < FN; i += size)
                for (int j = 0, k = 0; j < half; ++j, k += step)
                {
                    const float wr = fcos[(size_t) k], wi = -fsin[(size_t) k];
                    const int a = i + j, b = a + half;
                    const float tr = re[b] * wr - im[b] * wi, ti = re[b] * wi + im[b] * wr;
                    re[b] = re[a] - tr; im[b] = im[a] - ti; re[a] += tr; im[a] += ti;
                }
        }
    }

    void computeSpectrum()
    {
        std::array<float, FN> re, im;
        for (int i = 0; i < FN; ++i) { re[(size_t) i] = visBuf[(size_t) i] * hann[(size_t) i]; im[(size_t) i] = 0.f; }
        fft (re.data(), im.data());
        std::array<float, FN / 2> mag;
        for (int b = 0; b < FN / 2; ++b) mag[(size_t) b] = std::sqrt (re[(size_t) b] * re[(size_t) b] + im[(size_t) b] * im[(size_t) b]) / 256.f;
        const double binHz = sr / FN;
        for (int i = 0; i < kBars; ++i)
        {
            // log-spaced bands between 40 Hz and 16 kHz
            const double f0 = 40.0 * std::pow (16000.0 / 40.0, (double) i / kBars), f1 = 40.0 * std::pow (16000.0 / 40.0, (double) (i + 1) / kBars);
            int a = std::max (1, (int) (f0 / binHz)), b = std::min (FN / 2 - 1, std::max (a, (int) (f1 / binHz)));
            float m = 0.f; for (int k = a; k <= b; ++k) m = std::max (m, mag[(size_t) k]);
            const float db = 20.f * std::log10 (m + 1e-6f);
            const float v = std::clamp ((db + 70.f) / 55.f, 0.f, 1.f);
            bars[(size_t) i].store (std::max (v, bars[(size_t) i].load() * 0.85f));
        }
    }

    double sr = 44100.0;
    int N = 0, w = 0;
    std::int64_t absW = 0;
    float smoothK = 0.002f;

    // looper
    std::vector<float> loopRing[2], loopBuf[2];
    int loopLen = 0, loopPos = 0;
    float loopFade = 0.f;
    bool prevHold = false;

    // low battery
    std::vector<float> tapeRing[2];
    bool tapeOn = false, prevLB = false;
    float tapeFade = 0.f, tapeTarget = 0.f;
    double tapeT = 0.0, readAbs = 0.0, lbFreq = 20000.0;
    int ctr = 0;

    // eq + effects
    int lastEq = -1;
    float eqGain = 1.f;
    Biquad eqL[2], eqM[2], eqH[2], roomLP[2], roomLS[2], lbLP[2], fltBq[2], crLP[2];
    Biquad ebHP, ebLP, ebPk;
    Freeverb mainRev, roomRev;
    DelayLine chDL[2], dlDL[2];
    float rmT = 0, ebT = 0, fltT = 0, crT = 0, chT = 0, dT = 0, rT = 0, pgT = 1;
    double fltPhase = 0, chPhase = 0;

    // analysis
    std::array<float, 1024> visBuf {}, hann {};
    std::array<float, 512> scope {};
    int visN = 0, scopeW = 0;
    std::array<float, FN / 2> fcos {}, fsin {};
    std::array<int, FN> frev {};
    std::array<std::atomic<float>, kBars> bars {};
    std::atomic<float> levelL { 0.f }, levelR { 0.f }, battery { 1.f };
};

} // namespace ibuy
