#pragma once
#include <JuceHeader.h>
#include <vector>

namespace ic
{
// Band-limited wavetable bank: 4 sets x 4 morph frames x 11 mip levels (one per octave).
class WavetableBank
{
public:
    static constexpr int tableSize = 2048;
    static constexpr int numSets = 4, numFrames = 4, numLevels = 11;

    static const WavetableBank& get();

    // set: 0..3, pos: 0..1 morph position across frames, t: phase 0..1, dt: phase increment
    float lookup (int set, float pos, float t, float dt) const noexcept
    {
        const int level = juce::jlimit (0, numLevels - 1, (int) std::ceil (std::log2 (juce::jmax (1.0e-6f, 2048.0f * dt))));
        const float fp = juce::jlimit (0.0f, (float) (numFrames - 1) - 1.0e-4f, pos * (float) (numFrames - 1));
        const int f0 = (int) fp;
        const float ff = fp - (float) f0;
        const float a = read (table (set, f0, level), t);
        const float b = read (table (set, f0 + 1, level), t);
        return a + (b - a) * ff;
    }

private:
    WavetableBank();
    const float* table (int set, int frame, int level) const noexcept
    {
        return data.data() + (size_t) (((set * numFrames + frame) * numLevels + level) * (tableSize + 1));
    }
    static float read (const float* tbl, float t) noexcept
    {
        const float x = t * (float) tableSize;
        const int i = juce::jlimit (0, tableSize - 1, (int) x);
        const float fr = x - (float) i;
        return tbl[i] + (tbl[i + 1] - tbl[i]) * fr;
    }
    std::vector<float> data;
};
} // namespace ic
