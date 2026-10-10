#pragma once
#include <JuceHeader.h>
#include <array>
#include <vector>

namespace ic
{
// 8-line feedback delay network reverb tuned for techno:
// low-cut on the input keeps the kick/bass dry, damping darkens the tail,
// slow modulation on the delay lines avoids metallic ringing.
class IronVerb
{
public:
    static constexpr int numLines = 8;

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        const int maxLen = (int) std::ceil (0.075 * 2.0 * sr) + 64;
        for (auto& l : lines) { l.buf.assign ((size_t) maxLen, 0.0f); l.pos = 0; l.lp = 0.0f; }
        preBuf.assign ((size_t) (0.25 * sr) + 2, 0.0f);
        prePos = 0;
        hpState = 0.0f;
        lfoPhase = 0.0;
    }

    void reset()
    {
        for (auto& l : lines) { std::fill (l.buf.begin(), l.buf.end(), 0.0f); l.lp = 0.0f; }
        std::fill (preBuf.begin(), preBuf.end(), 0.0f);
        hpState = 0.0f;
    }

    // size 0..1, damp 0..1 (dark), preDelayMs 0..200
    void setParameters (float size, float damp, float preDelayMs)
    {
        static constexpr float baseMs[numLines] { 29.7f, 37.1f, 41.1f, 43.7f, 53.3f, 59.9f, 67.3f, 73.1f };
        const float scale = 0.5f + size * 1.5f;
        const float rt60 = 0.3f + size * size * 8.0f;
        for (int i = 0; i < numLines; ++i)
        {
            auto& l = lines[(size_t) i];
            l.delay = juce::jlimit (8.0f, (float) l.buf.size() - 20.0f, baseMs[i] * 0.001f * scale * (float) sr);
            l.gain = std::pow (10.0f, -3.0f * l.delay / (rt60 * (float) sr));
        }
        const float cutoff = 12000.0f * std::pow (0.125f, damp); // 12 kHz .. 1.5 kHz
        dampCoef = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * cutoff / (float) sr);
        hpCoef = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * 180.0f / (float) sr);
        preSamples = juce::jlimit (0, (int) preBuf.size() - 1, (int) (preDelayMs * 0.001f * (float) sr));
    }

    void process (const float* inL, const float* inR, float* outL, float* outR, int n)
    {
        const double lfoInc = 0.37 / sr;
        for (int s = 0; s < n; ++s)
        {
            // input: mono, high-passed, pre-delayed
            float x = 0.5f * (inL[s] + inR[s]);
            hpState += (x - hpState) * hpCoef;
            x -= hpState;
            preBuf[(size_t) prePos] = x;
            int rd = prePos - preSamples;
            if (rd < 0) rd += (int) preBuf.size();
            x = preBuf[(size_t) rd];
            if (++prePos >= (int) preBuf.size()) prePos = 0;

            lfoPhase += lfoInc;
            if (lfoPhase >= 1.0) lfoPhase -= 1.0;

            std::array<float, numLines> o {};
            float sum = 0.0f;
            for (int i = 0; i < numLines; ++i)
            {
                auto& l = lines[(size_t) i];
                const float mod = (i % 3 == 0) ? 6.0f * (float) std::sin (juce::MathConstants<double>::twoPi * (lfoPhase + i * 0.125)) : 0.0f;
                float rp = (float) l.pos - (l.delay + mod);
                const float size = (float) l.buf.size();
                while (rp < 0.0f) rp += size;
                const int i0 = juce::jlimit (0, (int) l.buf.size() - 1, (int) rp); // rounding can give exactly size -> would read past the buffer
                const int i1 = (i0 + 1) % (int) l.buf.size();
                const float fr = rp - (float) i0;
                float v = l.buf[(size_t) i0] + (l.buf[(size_t) i1] - l.buf[(size_t) i0]) * fr;
                l.lp += (v - l.lp) * dampCoef;
                o[(size_t) i] = l.lp * l.gain;
                sum += o[(size_t) i];
            }

            // Householder feedback matrix: y = o - 2/N * sum(o)
            const float h = sum * (2.0f / (float) numLines);
            float l = 0.0f, r = 0.0f;
            for (int i = 0; i < numLines; ++i)
            {
                auto& ln = lines[(size_t) i];
                ln.buf[(size_t) ln.pos] = o[(size_t) i] - h + x;
                if (++ln.pos >= (int) ln.buf.size()) ln.pos = 0;
                if (i % 2 == 0) l += o[(size_t) i] * ((i & 2) ? -1.0f : 1.0f);
                else            r += o[(size_t) i] * ((i & 2) ? -1.0f : 1.0f);
            }
            outL[s] = l * 0.35f;
            outR[s] = r * 0.35f;
        }
    }

private:
    struct Line
    {
        std::vector<float> buf;
        int pos = 0;
        float delay = 1000.0f, gain = 0.5f, lp = 0.0f;
    };
    std::array<Line, numLines> lines;
    std::vector<float> preBuf;
    int prePos = 0, preSamples = 0;
    double sr = 44100.0, lfoPhase = 0.0;
    float dampCoef = 0.5f, hpCoef = 0.01f, hpState = 0.0f;
};
} // namespace ic
