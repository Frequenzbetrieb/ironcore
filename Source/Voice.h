#pragma once
#include <JuceHeader.h>
#include "Params.h"
#include <vector>

namespace ic
{
// Values resolved once per block (macros already applied).
struct Frame
{
    float o1wave, o2wave, o2ratio, o2mix, fm, ring, sub, noise;
    bool sync;
    float cutoff, reso, fdrive, fenv, fkey;
    int fmode;
    float preGain, sat, fold, clip, crush;
    int dorder;
    float morph;
    int unison;
    float detuneCents, spread;
    float glideCoef, pitchBendSemis;
    float lfo1Amt, lfo2Amt, seqDepth, accentAmt;
    int oscType, osc2Type;
    float penvAmt, penvDecay;
    float drift, fmfb, f2cut, f2reso;
    bool superSaw, drvPost;
    int dtype, f2mode, f2route;
    int modSrc[numModSlots], modDst[numModSlots];
    float modAmt[numModSlots];
};

// Values that change every sample (global modulators).
struct Mods
{
    float lfo1, lfo2;          // -1..1
    float seqF, seqM, seqC;    // -1..1
    float accent;              // 0..1
    float pitch;               // semitones (sequencer pitch lane, slide-smoothed)
    float gate;                // 0..1 amplitude gate
    float modWheel, aftertouch; // 0..1
    float stepRandom;          // -1..1, new value each step
};

struct Svf
{
    float ic1 = 0, ic2 = 0, lp = 0, bp = 0, hp = 0;
    void process (float x, float g, float k)
    {
        const float a1 = 1.0f / (1.0f + g * (g + k));
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + g * a1 * v3;
        const float v2 = ic2 + g * v1;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        lp = v2; bp = v1; hp = x - k * v1 - v2;
    }
    void reset() { ic1 = ic2 = lp = bp = hp = 0; }
};

class Voice
{
public:
    void prepare (double osRate);
    void noteOn (int note, float velocity, bool legato);
    void noteOff();
    void kill();
    void retrigger();
    bool isActive() const { return active; }
    bool isReleasing() const { return releasing; }
    int getNote() const { return note; }
    uint64_t age = 0;
    float lastFc = 1000.0f; // for the filter display

    void setEnvelopes (const juce::ADSR::Parameters& amp, const juce::ADSR::Parameters& filtParams,
                       const juce::ADSR::Parameters& env3Params);
    void render (const Frame& f, const Mods& m, float& outL, float& outR);

private:
    float shape (float t, float dt, float w, int oscType) const;
    float special (int osc, int type, float freq, float w);   // resonator / noise oscillators
    static bool isSpecial (int type) { return type == otResonator || type == otNoise; }
    void excite();
    float damage (float x, int ch, const Frame& f);
    void updateControl (const Frame& f, const Mods& m, float fe);

    // control-rate cache
    Frame cf {};
    unsigned ctrlCount = 0;
    float cPitchMul = 1, cFmMix = 0, cFormMix = 0, cIndex = 0, cFormG[3] {}, cPanL = 1, cPanR = 1, cAmpMod = 1;
    float cUniRatio[maxUnison] {}, cUniL[maxUnison] {}, cUniR[maxUnison] {}, cNorm = 1;
    float cG2 = 0.1f, cK2 = 1.4f;
    // analog drift (Ornstein-Uhlenbeck random walks, unit variance)
    float driftP[maxUnison] {}, driftF = 0, lastMod = 0;
    float cG = 0.1f, cGl = 0.1f, cK = 1, cReso = 0, cDrive = 1, cGainStatic = 0;

    double sr = 88200.0;
    bool active = false, releasing = false;
    int note = 60;
    float velocity = 1.0f, curFreq = 440.0f, targetFreq = 440.0f;

    float ph1[maxUnison] {}, ph2[maxUnison] {};
    float phm = 0, phSub = 0;
    juce::Random rng;

    Svf filt[2][2];       // [stage][channel]
    Svf formant[2][3];    // [channel][formant]
    float ladder[2][4] {}; // [channel][stage]
    float crushHold[2] {}, crushCount = 0, dcX[2] {}, dcY[2] {};

    juce::ADSR ampEnv, filtEnv, env3;
    float noteTime = 0.0f, env3Val = 0.0f;

    // Karplus-Strong resonator per oscillator
    struct Ks { std::vector<float> buf; int pos = 0; float lp = 0; int excite = 0; };
    Ks ks[2];
    Svf noiseSvf[2];
};
} // namespace ic
