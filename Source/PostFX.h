#pragma once
#include <JuceHeader.h>
#include <vector>

namespace ic
{
// RBJ biquad (transposed direct form II), stereo
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    float z1[2] {}, z2[2] {};

    float process (float x, int ch) noexcept
    {
        const float y = b0 * x + z1[ch];
        z1[ch] = b1 * x - a1 * y + z2[ch];
        z2[ch] = b2 * x - a2 * y;
        return y;
    }
    void reset() { z1[0] = z1[1] = z2[0] = z2[1] = 0; }

    void set (double sr, int type, float freq, float q, float gainDb)
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w = juce::MathConstants<double>::twoPi * juce::jlimit (10.0, sr * 0.45, (double) freq) / sr;
        const double cw = std::cos (w), sw = std::sin (w), alpha = sw / (2.0 * q);
        double B0, B1, B2, A0, A1, A2;
        switch (type)
        {
            case 0: // high-pass
                B0 = (1 + cw) / 2; B1 = -(1 + cw); B2 = (1 + cw) / 2; A0 = 1 + alpha; A1 = -2 * cw; A2 = 1 - alpha; break;
            case 1: // peaking
                B0 = 1 + alpha * A; B1 = -2 * cw; B2 = 1 - alpha * A; A0 = 1 + alpha / A; A1 = -2 * cw; A2 = 1 - alpha / A; break;
            case 2: // low shelf
            {
                const double s2 = 2 * std::sqrt (A) * alpha;
                B0 = A * ((A + 1) - (A - 1) * cw + s2); B1 = 2 * A * ((A - 1) - (A + 1) * cw); B2 = A * ((A + 1) - (A - 1) * cw - s2);
                A0 = (A + 1) + (A - 1) * cw + s2; A1 = -2 * ((A - 1) + (A + 1) * cw); A2 = (A + 1) + (A - 1) * cw - s2; break;
            }
            default: // high shelf
            {
                const double s2 = 2 * std::sqrt (A) * alpha;
                B0 = A * ((A + 1) + (A - 1) * cw + s2); B1 = -2 * A * ((A - 1) + (A + 1) * cw); B2 = A * ((A + 1) + (A - 1) * cw - s2);
                A0 = (A + 1) - (A - 1) * cw + s2; A1 = 2 * ((A - 1) - (A + 1) * cw); A2 = (A + 1) - (A - 1) * cw - s2; break;
            }
        }
        b0 = (float) (B0 / A0); b1 = (float) (B1 / A0); b2 = (float) (B2 / A0); a1 = (float) (A1 / A0); a2 = (float) (A2 / A0);
    }
};

// Post EQ (low cut, mid boost, tilt) + mono-safe stereo widener + tempo pump
class PostFX
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        widthBuf.assign ((size_t) (0.05 * sr) + 4, 0.0f);
        widthPos = 0; lfo = 0;
        for (auto* b : { &lowCut, &mid, &lowShelf, &highShelf, &sideHp }) b->reset();
    }

    void process (float* L, float* R, int n, float lowCutHz, float midDb, float tilt, float width,
                  float pumpAmt, double beat0, double beatsPerSample, float pumpBeats)
    {
        lowCut.set (sr, 0, lowCutHz, 0.707f, 0);
        mid.set (sr, 1, 1200.0f, 0.8f, midDb);
        lowShelf.set (sr, 2, 300.0f, 0.707f, -tilt * 6.0f);
        highShelf.set (sr, 3, 3000.0f, 0.707f, tilt * 6.0f);
        sideHp.set (sr, 0, 300.0f, 0.707f, 0);
        const bool doLowCut = lowCutHz > 21.0f, doMid = std::abs (midDb) > 0.05f, doTilt = std::abs (tilt) > 0.01f;
        const int wl = (int) widthBuf.size();

        for (int s = 0; s < n; ++s)
        {
            float l = L[s], r = R[s];
            for (int ch = 0; ch < 2; ++ch)
            {
                float& x = ch == 0 ? l : r;
                if (doLowCut) x = lowCut.process (x, ch);
                if (doMid)    x = mid.process (x, ch);
                if (doTilt)   x = highShelf.process (lowShelf.process (x, ch), ch);
            }

            if (width > 0.001f)
            {
                // synthesize extra side signal from a modulated, high-passed delay of the mid -> mono sum unchanged
                const float m = 0.5f * (l + r), sd = 0.5f * (l - r);
                widthBuf[(size_t) widthPos] = m;
                lfo += 0.31 / sr; if (lfo >= 1.0) lfo -= 1.0;
                const float dly = (float) ((0.012 + 0.003 * std::sin (juce::MathConstants<double>::twoPi * lfo)) * sr);
                float rp = (float) widthPos - dly;
                if (rp < 0) rp += (float) wl;
                const int i0 = juce::jlimit (0, wl - 1, (int) rp); const float fr = rp - (float) i0;
                const float d = widthBuf[(size_t) i0] + (widthBuf[(size_t) ((i0 + 1) % wl)] - widthBuf[(size_t) i0]) * fr;
                if (++widthPos >= wl) widthPos = 0;
                const float side = sd + width * 0.8f * sideHp.process (d, 0);
                l = m + side; r = m - side;
            }

            if (pumpAmt > 0.001f)
            {
                const double ph = std::fmod ((beat0 + s * beatsPerSample) / pumpBeats, 1.0);
                const float rise = juce::jmin (1.0f, (float) (ph < 0 ? ph + 1.0 : ph) / 0.55f);
                const float g = 1.0f - pumpAmt * (1.0f - rise) * (1.0f - rise);
                l *= g; r *= g;
            }
            L[s] = l; R[s] = r;
        }
    }

private:
    double sr = 44100.0, lfo = 0.0;
    Biquad lowCut, mid, lowShelf, highShelf, sideHp;
    std::vector<float> widthBuf;
    int widthPos = 0;
};
} // namespace ic
