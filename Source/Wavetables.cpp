#include "Wavetables.h"

namespace ic
{
namespace
{
float gauss (float x, float centre, float width) { const float d = (x - centre) / width; return std::exp (-d * d); }

float hashRand (int seed, int h)
{
    uint32_t v = (uint32_t) seed * 2654435761u ^ (uint32_t) h * 2246822519u;
    v = (v ^ (v >> 15)) * 2246822519u;
    v ^= v >> 13;
    return (float) (v & 0xffff) / 65535.0f;
}

// Harmonic amplitude for (set, frame, harmonic h >= 1)
float harmonicAmp (int set, int frame, int h)
{
    const float hf = (float) h;
    switch (set)
    {
        case 0: // IRON: saw -> odd-boosted saw -> square -> narrow pulse
            switch (frame)
            {
                case 0:  return 1.0f / hf;
                case 1:  return ((h % 2) ? 1.6f : 0.6f) / hf;
                case 2:  return (h % 2) ? 1.0f / hf : 0.0f;
                default: return std::sin (juce::MathConstants<float>::pi * hf * 0.12f) / hf;
            }
        case 1: // METAL: resonant partial clusters
            switch (frame)
            {
                case 0:  return 1.0f / hf + (h >= 6 && h <= 10 ? 0.5f : 0.0f) / std::sqrt (hf);
                case 1:  return (h % 4 == 1) ? 1.0f / std::sqrt (hf) : 0.05f / hf;
                case 2:  return (h == 1 ? 1.0f : 0.0f) + 0.8f * (gauss (hf, 11, 1.2f) + gauss (hf, 17, 1.2f) + gauss (hf, 23, 1.5f) + gauss (hf, 31, 2.0f));
                default: return (h % 3 == 0 ? 1.0f : 0.15f) / std::sqrt (hf);
            }
        case 2: // VOCAL: formant envelopes (A, E, I, O) over a ~110 Hz fundamental
        {
            static const float formants[4][3] { { 800, 1150, 2900 }, { 400, 1600, 2700 }, { 350, 2300, 3000 }, { 450, 800, 2830 } };
            const float hz = hf * 110.0f;
            float env = 0.0f;
            for (int k = 0; k < 3; ++k)
                env += (k == 0 ? 1.0f : k == 1 ? 0.7f : 0.4f) * gauss (hz, formants[frame][k], 90.0f + 40.0f * (float) k);
            return (0.15f + 2.0f * env) / std::sqrt (hf);
        }
        default: // DIGITAL: harsh, irregular spectra
            switch (frame)
            {
                case 0:  return hashRand (11, h) / std::pow (hf, 0.7f);
                case 1:  return ((h & (h - 1)) == 0 ? 3.0f : 0.2f) / hf;
                case 2:  return hashRand (29, h) > 0.75f ? 1.0f / std::sqrt (hf) : 0.0f;
                default: return (1.0f + 0.8f * std::sin (hf * 0.7f)) / hf;
            }
    }
}
} // namespace

const WavetableBank& WavetableBank::get()
{
    static const WavetableBank bank;
    return bank;
}

WavetableBank::WavetableBank()
{
    data.resize ((size_t) (numSets * numFrames * numLevels * (tableSize + 1)));
    juce::dsp::FFT fft (11); // 2048
    std::vector<float> buf ((size_t) tableSize * 2);

    for (int set = 0; set < numSets; ++set)
        for (int frame = 0; frame < numFrames; ++frame)
        {
            float norm = 1.0f;
            for (int level = 0; level < numLevels; ++level)
            {
                const int maxH = (tableSize / 2) >> level;
                std::fill (buf.begin(), buf.end(), 0.0f);
                for (int h = 1; h < juce::jmin (maxH, tableSize / 2); ++h)
                {
                    // sine series: bin h imaginary part
                    buf[(size_t) (2 * h + 1)] = -harmonicAmp (set, frame, h);
                }
                fft.performRealOnlyInverseTransform (buf.data());

                auto* tbl = const_cast<float*> (table (set, frame, level));
                for (int i = 0; i < tableSize; ++i) tbl[i] = buf[(size_t) i];

                if (level == 0) // normalise every level by the full-band peak so loudness stays constant
                {
                    float peak = 0.0f;
                    for (int i = 0; i < tableSize; ++i) peak = juce::jmax (peak, std::abs (tbl[i]));
                    norm = peak > 0.0f ? 1.0f / peak : 1.0f;
                }
                for (int i = 0; i < tableSize; ++i) tbl[i] *= norm;
                tbl[tableSize] = tbl[0];
            }
        }
}
} // namespace ic
