// Standalone check of the iBuy engine. Build: g++ -O2 -std=c++17 -I../Source engine_test.cpp -o engine_test
#include "Engine.h"
#include <functional>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>

static bool readWav (const char* path, std::vector<float>& L, std::vector<float>& R, int& sr)
{
    std::ifstream f (path, std::ios::binary);
    if (! f) return false;
    std::vector<char> d ((std::istreambuf_iterator<char> (f)), std::istreambuf_iterator<char>());
    size_t p = 12; int ch = 2; sr = 44100;
    while (p + 8 <= d.size())
    {
        uint32_t sz; std::memcpy (&sz, &d[p + 4], 4);
        if (! std::memcmp (&d[p], "fmt ", 4)) { uint16_t c; std::memcpy (&c, &d[p + 10], 2); ch = c; uint32_t s; std::memcpy (&s, &d[p + 12], 4); sr = (int) s; }
        if (! std::memcmp (&d[p], "data", 4))
        {
            const int16_t* s = (const int16_t*) &d[p + 8]; size_t n = sz / 2 / (size_t) ch;
            L.resize (n); R.resize (n);
            for (size_t i = 0; i < n; ++i) { L[i] = s[i * (size_t) ch] / 32768.f; R[i] = s[i * (size_t) ch + (ch > 1 ? 1 : 0)] / 32768.f; }
            return true;
        }
        p += 8 + sz + (sz & 1);
    }
    return false;
}

static void writeWav (const char* path, const std::vector<float>& L, const std::vector<float>& R, int sr)
{
    std::ofstream f (path, std::ios::binary);
    uint32_t n = (uint32_t) L.size(), dataSz = n * 4, riff = 36 + dataSz, fmtSz = 16, br = (uint32_t) sr * 4, srr = (uint32_t) sr;
    uint16_t pcm = 1, ch = 2, ba = 4, bits = 16;
    f.write ("RIFF", 4); f.write ((char*) &riff, 4); f.write ("WAVEfmt ", 8); f.write ((char*) &fmtSz, 4);
    f.write ((char*) &pcm, 2); f.write ((char*) &ch, 2); f.write ((char*) &srr, 4); f.write ((char*) &br, 4);
    f.write ((char*) &ba, 2); f.write ((char*) &bits, 2); f.write ("data", 4); f.write ((char*) &dataSz, 4);
    for (uint32_t i = 0; i < n; ++i)
    {
        int16_t a = (int16_t) std::clamp (L[i] * 32767.f, -32768.f, 32767.f), b = (int16_t) std::clamp (R[i] * 32767.f, -32768.f, 32767.f);
        f.write ((char*) &a, 2); f.write ((char*) &b, 2);
    }
}

struct Stats { double rms = 0, peak = 0; bool finite = true; };
static Stats stats (const std::vector<float>& x, size_t a, size_t b)
{
    Stats s; double acc = 0;
    for (size_t i = a; i < b && i < x.size(); ++i) { if (! std::isfinite (x[i])) s.finite = false; acc += (double) x[i] * x[i]; s.peak = std::max (s.peak, (double) std::fabs (x[i])); }
    s.rms = std::sqrt (acc / std::max<size_t> (1, b - a)); return s;
}

// rough pitch proxy: zero-crossings per second of a lowpassed signal
static double zcr (const std::vector<float>& x, size_t a, size_t b, int sr)
{
    int c = 0; for (size_t i = a + 1; i < b && i < x.size(); ++i) if ((x[i - 1] < 0) != (x[i] < 0)) ++c;
    return c / ((double) (b - a) / sr);
}

static void run (const char* name, std::vector<float> L, std::vector<float> R, int sr, const std::function<void (size_t, ibuy::Params&)>& setup, bool save = false)
{
    ibuy::Engine e; e.prepare (sr);
    ibuy::Params p; p.bpm = 120;
    const size_t blk = 512;
    for (size_t i = 0; i < L.size(); i += blk)
    {
        size_t n = std::min (blk, L.size() - i);
        setup (i, p);
        float* io[2] = { &L[i], &R[i] };
        e.process (io, 2, (int) n, p);
    }
    Stats s = stats (L, 0, L.size());
    std::printf ("%-26s finite=%d rms=%.3f peak=%.3f\n", name, s.finite, s.rms, s.peak);
    if (save) writeWav ((std::string ("/tmp/out_") + name + ".wav").c_str(), L, R, sr);
}

int main()
{
    std::vector<float> L, R; int sr;
    if (! readWav ("/tmp/in.wav", L, R, sr)) { std::puts ("no wav"); return 1; }
    std::printf ("input: %zu samples @ %d\n", L.size(), sr);
    run ("dry", L, R, sr, [] (size_t, ibuy::Params&) {});
    for (int eq = 1; eq < 6; ++eq)
    {
        char nm[32]; std::snprintf (nm, sizeof nm, "eq%d", eq);
        run (nm, L, R, sr, [eq] (size_t, ibuy::Params& p) { p.eq = eq; });
    }
    run ("nextRoom", L, R, sr, [] (size_t, ibuy::Params& p) { p.nextRoom = true; });
    run ("earbuds", L, R, sr, [] (size_t, ibuy::Params& p) { p.earbuds = true; });
    run ("reverb", L, R, sr, [] (size_t, ibuy::Params& p) { p.reverb = true; });
    run ("delay", L, R, sr, [] (size_t, ibuy::Params& p) { p.delay = true; });
    run ("chorus", L, R, sr, [] (size_t, ibuy::Params& p) { p.chorus = true; });
    run ("filter", L, R, sr, [] (size_t, ibuy::Params& p) { p.filter = true; });
    run ("bitcrush", L, R, sr, [] (size_t, ibuy::Params& p) { p.bitcrush = true; });
    run ("everything", L, R, sr, [] (size_t, ibuy::Params& p) { p.eq = 1; p.nextRoom = p.earbuds = p.reverb = p.delay = p.chorus = p.filter = p.bitcrush = true; });

    // Low Battery: on at 5 s, off at 15 s. Pitch proxy must fall while on and recover after.
    {
        std::vector<float> a = L, b = R; ibuy::Engine e; e.prepare (sr); ibuy::Params p;
        for (size_t i = 0; i < a.size(); i += 512)
        {
            size_t n = std::min<size_t> (512, a.size() - i);
            p.lowBattery = (i >= (size_t) sr * 5 && i < (size_t) sr * 15);
            float* io[2] = { &a[i], &b[i] }; e.process (io, 2, (int) n, p);
        }
        std::printf ("lowBattery: zcr before=%.0f  at 6s=%.0f  at 13s=%.0f  after(20s)=%.0f  finite=%d\n",
            zcr (a, sr * 3, sr * 4, sr), zcr (a, sr * 6, sr * 7, sr), zcr (a, sr * 13, sr * 14, sr), zcr (a, sr * 20, sr * 21, sr), stats (a, 0, a.size()).finite);
        std::printf ("            rms  before=%.3f  at 13s=%.3f  after=%.3f\n", stats (a, sr * 3, sr * 4).rms, stats (a, sr * 13, sr * 14).rms, stats (a, sr * 20, sr * 21).rms);
        writeWav ("/tmp/out_lowbattery.wav", a, b, sr);
    }

    // Looper: hold from 10 s to 12 s at 1/4 (=0.5 s at 120 bpm). Output must repeat with period 0.5 s while held, then follow the track again.
    for (int div = 1; div <= 5; div += 2)
    {
        std::vector<float> a = L, b = R; ibuy::Engine e; e.prepare (sr); ibuy::Params p; p.loopDiv = div;
        for (size_t i = 0; i < a.size(); i += 512)
        {
            size_t n = std::min<size_t> (512, a.size() - i);
            p.loopHold = (i >= (size_t) sr * 10 && i < (size_t) sr * 12);
            float* io[2] = { &a[i], &b[i] }; e.process (io, 2, (int) n, p);
        }
        static const double den[6] = { 2, 4, 8, 16, 32, 64 };
        const size_t period = (size_t) std::lround (240.0 / 120.0 / den[div] * sr);
        // compare sample k with k+period inside the held window, away from the fade edges
        double err = 0, ref = 0; size_t a0 = (size_t) sr * 10 + sr / 4, a1 = (size_t) sr * 12 - period - 10;
        for (size_t k = a0; k < a1; ++k) { err += std::fabs (a[k] - a[k + period]); ref += std::fabs (a[k]); }
        double errFree = 0, refFree = 0; size_t f0 = (size_t) sr * 13;
        for (size_t k = f0; k < f0 + sr; ++k) { errFree += std::fabs (a[k] - L[k]); refFree += std::fabs (L[k]); }
        std::printf ("looper 1/%-2d period=%zu: repeat error=%.4f  (free-run diff vs dry after release=%.4f)  finite=%d\n",
            (int) den[div], period, err / std::max (1e-9, ref), errFree / std::max (1e-9, refFree), stats (a, 0, a.size()).finite);
        if (div == 3) writeWav ("/tmp/out_looper.wav", a, b, sr);
    }

    // power off must be bit-exact
    {
        std::vector<float> a = L, b = R; ibuy::Engine e; e.prepare (sr); ibuy::Params p; p.power = false; p.reverb = true; p.eq = 1;
        for (size_t i = 0; i < a.size(); i += 512) { size_t n = std::min<size_t> (512, a.size() - i); float* io[2] = { &a[i], &b[i] }; e.process (io, 2, (int) n, p); }
        double d = 0; for (size_t i = (size_t) sr; i < a.size(); ++i) d = std::max (d, (double) std::fabs (a[i] - L[i]));
        std::printf ("power off: max diff vs dry = %.6f\n", d);
    }
    return 0;
}
