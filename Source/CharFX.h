#pragma once
#include <JuceHeader.h>
#include <array>
#include <vector>

namespace ic
{
// Character effects that change the timbre itself (not the room):
// 1 Comb/Flanger, 2 Phaser, 3 Frequency Shifter, 4 Tempo Stutter.
class CharFX
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        comb.assign ((size_t) (0.06 * sr) + 4, 0.0f);
        combR.assign (comb.size(), 0.0f);
        stut[0].assign ((size_t) (4.0 * sr), 0.0f);
        stut[1].assign ((size_t) (4.0 * sr), 0.0f);
        reset();
    }

    void reset()
    {
        std::fill (comb.begin(), comb.end(), 0.0f);
        std::fill (combR.begin(), combR.end(), 0.0f);
        for (auto& b : stut) std::fill (b.begin(), b.end(), 0.0f);
        combPos = stutPos = 0; lfo = 0; phase = 0;
        for (auto& c : apState) c.fill (0.0f);
        for (auto& h : hil) h = {};
        fbL = fbR = 0; lastSlice = -1; repeating = false;
    }

    // p1, p2 in 0..1 (meaning depends on type), mix 0..1
    void process (int type, float* L, float* R, int n, float p1, float p2, float mix,
                  double beat0, double beatsPerSample)
    {
        if (type <= 0 || mix <= 0.0f) return;
        switch (type)
        {
            case 1: processComb (L, R, n, p1, p2, mix); break;
            case 2: processPhaser (L, R, n, p1, p2, mix); break;
            case 3: processShift (L, R, n, p1, p2, mix); break;
            default: processStutter (L, R, n, p1, p2, mix, beat0, beatsPerSample); break;
        }
    }

private:
    // ---------- Comb / flanger: P1 = delay (tuning), P2 = feedback (-/+) ----------
    void processComb (float* L, float* R, int n, float p1, float p2, float mix)
    {
        const int len = (int) comb.size();
        const float baseMs = 0.3f + p1 * p1 * 20.0f;
        const float fb = (p2 * 2.0f - 1.0f) * 0.93f;
        for (int s = 0; s < n; ++s)
        {
            lfo += 0.15 / sr; if (lfo >= 1.0) lfo -= 1.0;
            const float mod = 1.0f + 0.12f * (float) std::sin (juce::MathConstants<double>::twoPi * lfo);
            const float d = juce::jlimit (1.0f, (float) len - 3.0f, baseMs * 0.001f * (float) sr * mod);
            const float dR = juce::jlimit (1.0f, (float) len - 3.0f, d * 1.07f); // slight stereo offset
            const float yL = read (comb, combPos, d), yR = read (combR, combPos, dR);
            comb[(size_t) combPos] = softClip (L[s] + yL * fb);
            combR[(size_t) combPos] = softClip (R[s] + yR * fb);
            if (++combPos >= len) combPos = 0;
            L[s] += (0.7f * (L[s] + yL) - L[s]) * mix;
            R[s] += (0.7f * (R[s] + yR) - R[s]) * mix;
        }
    }

    // ---------- Phaser: P1 = rate (0.05..6 Hz), P2 = feedback/resonance ----------
    void processPhaser (float* L, float* R, int n, float p1, float p2, float mix)
    {
        const double rate = 0.05 + p1 * p1 * 6.0;
        const float fb = p2 * 0.85f;
        for (int s = 0; s < n; ++s)
        {
            if ((s & 15) == 0)
            {
                lfo += rate * 16.0 / sr; if (lfo >= 1.0) lfo -= 1.0;
                for (int ch = 0; ch < 2; ++ch)
                {
                    const double ph = lfo + (ch ? 0.25 : 0.0);
                    const double sweep = 0.5 + 0.5 * std::sin (juce::MathConstants<double>::twoPi * ph);
                    const double f = 200.0 * std::pow (20.0, sweep);           // 200 Hz .. 4 kHz
                    const double t = std::tan (juce::MathConstants<double>::pi * f / sr);
                    apCoef[ch] = (float) ((t - 1.0) / (t + 1.0));
                }
            }
            float* io[2] { L, R };
            for (int ch = 0; ch < 2; ++ch)
            {
                float x = io[ch][s] + (ch ? fbR : fbL) * fb;
                for (int k = 0; k < 6; ++k)
                {
                    const float y = apCoef[ch] * x + apState[(size_t) ch][(size_t) k];
                    apState[(size_t) ch][(size_t) k] = x - apCoef[ch] * y;
                    x = y;
                }
                (ch ? fbR : fbL) = softClip (x);
                io[ch][s] += (0.5f * (io[ch][s] + x) * 1.4f - io[ch][s]) * mix;
            }
        }
    }

    // ---------- Frequency shifter: P1 = shift (-1500..+1500 Hz, centre = 0), P2 = feedback ----------
    struct Hilbert
    {
        // Olli Niemitalo's 90-degree phase-difference allpass network
        std::array<float, 4> xa1 {}, xa2 {}, ya1 {}, ya2 {}, xb1 {}, xb2 {}, yb1 {}, yb2 {};
        float bDelay = 0;
        void process (float x, float& i, float& q)
        {
            static constexpr float ca[4] { 0.6923878f, 0.9360654322959f, 0.9882295226860f, 0.9987488452737f };
            static constexpr float cb[4] { 0.4021921162426f, 0.8561710882420f, 0.9722909545651f, 0.9952884791278f };
            float a = x, b = x;
            for (int k = 0; k < 4; ++k)
            {
                const float c2 = ca[k] * ca[k];
                const float y = c2 * (a + ya2[(size_t) k]) - xa2[(size_t) k];
                xa2[(size_t) k] = xa1[(size_t) k]; xa1[(size_t) k] = a;
                ya2[(size_t) k] = ya1[(size_t) k]; ya1[(size_t) k] = y;
                a = y;
            }
            for (int k = 0; k < 4; ++k)
            {
                const float c2 = cb[k] * cb[k];
                const float y = c2 * (b + yb2[(size_t) k]) - xb2[(size_t) k];
                xb2[(size_t) k] = xb1[(size_t) k]; xb1[(size_t) k] = b;
                yb2[(size_t) k] = yb1[(size_t) k]; yb1[(size_t) k] = y;
                b = y;
            }
            i = a;
            q = bDelay; bDelay = b;
        }
    };

    void processShift (float* L, float* R, int n, float p1, float p2, float mix)
    {
        const float c = p1 * 2.0f - 1.0f;
        const double shiftHz = (double) (c * c * c) * 1500.0;
        const float fb = p2 * 0.8f;
        const double inc = shiftHz / sr;
        float* io[2] { L, R };
        for (int s = 0; s < n; ++s)
        {
            phase += inc; phase -= std::floor (phase);
            const float cs = (float) std::cos (juce::MathConstants<double>::twoPi * phase);
            const float sn = (float) std::sin (juce::MathConstants<double>::twoPi * phase);
            for (int ch = 0; ch < 2; ++ch)
            {
                float i, q;
                hil[ch].process (io[ch][s] + (ch ? fbR : fbL) * fb, i, q);
                const float y = i * cs - q * sn;
                (ch ? fbR : fbL) = softClip (y);
                io[ch][s] += (y - io[ch][s]) * mix;
            }
        }
    }

    // ---------- Stutter: P1 = slice length (1/32 .. 1/4), P2 = probability ----------
    void processStutter (float* L, float* R, int n, float p1, float p2, float mix, double beat0, double bps)
    {
        static constexpr double lens[4] { 0.125, 0.25, 0.5, 1.0 };
        const double sliceBeats = lens[juce::jlimit (0, 3, (int) (p1 * 3.999f))];
        const int len = (int) stut[0].size();
        const int sliceSamples = juce::jlimit (16, len / 3, (int) (sliceBeats / juce::jmax (1.0e-9, bps)));
        const int fade = juce::jmin (96, sliceSamples / 4);
        for (int s = 0; s < n; ++s)
        {
            stut[0][(size_t) stutPos] = L[s];
            stut[1][(size_t) stutPos] = R[s];

            const long long slice = (long long) std::floor ((beat0 + s * bps) / sliceBeats);
            if (slice != lastSlice)
            {
                lastSlice = slice;
                repeating = rng.nextFloat() < p2;
                reverse = repeating && rng.nextFloat() < 0.3f;
                srcStart = stutPos - sliceSamples; // the slice that just played
                if (srcStart < 0) srcStart += len;
                sliceIdx = 0;
            }
            if (repeating)
            {
                const int k = reverse ? sliceSamples - 1 - (sliceIdx % sliceSamples) : (sliceIdx % sliceSamples);
                int rp = srcStart + k; if (rp >= len) rp -= len;
                const int pos = sliceIdx % sliceSamples;
                const float env = juce::jmin (1.0f, juce::jmin ((float) pos / (float) fade, (float) (sliceSamples - pos) / (float) fade));
                L[s] += (stut[0][(size_t) rp] * env - L[s]) * mix;
                R[s] += (stut[1][(size_t) rp] * env - R[s]) * mix;
            }
            ++sliceIdx;
            if (++stutPos >= len) stutPos = 0;
        }
    }

    static float softClip (float x) { return x / (1.0f + std::abs (x) * 0.3f); }
    static float read (const std::vector<float>& b, int pos, float d)
    {
        const int len = (int) b.size();
        float rp = (float) pos - d; if (rp < 0) rp += (float) len;
        const int i0 = juce::jlimit (0, len - 1, (int) rp); const float fr = rp - (float) i0;
        return b[(size_t) i0] + (b[(size_t) ((i0 + 1) % len)] - b[(size_t) i0]) * fr;
    }

    double sr = 44100.0, lfo = 0.0, phase = 0.0;
    std::vector<float> comb, combR;
    int combPos = 0;
    std::array<std::array<float, 6>, 2> apState {};
    float apCoef[2] {}, fbL = 0, fbR = 0;
    Hilbert hil[2];
    std::vector<float> stut[2];
    int stutPos = 0, srcStart = 0, sliceIdx = 0;
    long long lastSlice = -1;
    bool repeating = false, reverse = false;
    juce::Random rng;
};
} // namespace ic
