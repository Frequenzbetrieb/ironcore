#pragma once
#include <JuceHeader.h>

namespace ic
{
constexpr int numSteps = 16;
constexpr int maxVoices = 8;
constexpr int maxUnison = 7;

inline const juce::StringArray syncRateNames { "1/1", "1/2", "1/4", "1/8", "1/16", "1/4T", "1/8T", "1/2D", "Free Hz" };
constexpr int lfoFreeIndex = 8;
inline const juce::StringArray lfoShapes { "Sine", "Triangle", "Saw", "Square", "S&H" };
inline const float syncRateBeats[] { 4.0f, 2.0f, 1.0f, 0.5f, 0.25f, 2.0f / 3.0f, 1.0f / 3.0f, 3.0f };

inline const juce::StringArray seqRateNames { "1/8", "1/16", "1/32", "1/16T" };
inline const float seqRateBeats[] { 0.5f, 0.25f, 0.125f, 1.0f / 6.0f };

inline const juce::StringArray delayNames { "1/16", "1/8", "3/16", "1/4", "3/8", "1/2" };
inline const float delayBeats[] { 0.25f, 0.5f, 0.75f, 1.0f, 1.5f, 2.0f };

inline const juce::StringArray filterModes { "LP24 Ladder", "LP12", "BP Scream", "HP" };
inline const juce::StringArray damageOrders { "SAT>FOLD>CLIP", "FOLD>SAT>CLIP", "CLIP>FOLD>SAT", "SAT>CLIP>FOLD" };
inline const juce::StringArray evolveNames { "Off", "1 Bar", "2 Bars", "4 Bars", "8 Bars" };
inline const int evolveBars[] { 0, 1, 2, 4, 8 };

inline const juce::StringArray oscTypes { "Classic", "WT Iron", "WT Metal", "WT Vocal", "WT Digital", "Phase Dist", "Resonator", "Noise" };
enum OscType { otClassic = 0, otPD = 5, otResonator = 6, otNoise = 7 };

constexpr int numModSlots = 8;
inline const juce::StringArray modSources { "Off", "LFO 1", "LFO 2", "Filter Env", "Velocity", "Mod Wheel", "Aftertouch", "Seq Filter", "Accent", "Step Random", "Env 3", "Keytrack" };
enum ModSrc { srcOff, srcLfo1, srcLfo2, srcFiltEnv, srcVelocity, srcModWheel, srcAftertouch, srcSeqF, srcAccent, srcRandom, srcEnv3, srcKey, numModSrc };
inline const juce::StringArray modDests { "Cutoff", "Resonance", "Pitch", "Character", "FM", "Drive", "Fold", "Osc Shape", "Osc2 Pitch", "Pan", "Amp",
                                                "Cutoff 2", "Drift", "FM Fdbk", "Saturation", "Detune", "Sub", "Noise", "Osc2 Level", "Pitch Env" };
enum ModDst { dstCutoff, dstReso, dstPitch, dstChar, dstFm, dstDrive, dstFold, dstShape, dstO2Pitch, dstPan, dstAmp,
              dstCut2, dstDrift, dstFmFb, dstSat, dstDetune, dstSub, dstNoise, dstO2Level, dstPenv, numModDst };
inline juce::String modId (const char* what, int slot) { return juce::String ("mm") + what + juce::String (slot); }

inline juce::String laneLenId (char lane) { return "len" + juce::String::charToString (lane); }

inline juce::String stepId (char lane, int step) { return juce::String::charToString (lane) + "s" + juce::String (step); }

// Sequencer lanes: f = filter, m = FM amount, c = character, p = pitch, g = gate, a = accent, s = slide
inline const char seqLanes[] { 'f', 'm', 'c', 'p', 'g', 'a', 's' };
constexpr int numLanes = 7;
constexpr int numPatterns = 8;
inline int laneIndex (char lane) { for (int i = 0; i < numLanes; ++i) if (seqLanes[i] == lane) return i; return 0; }
inline bool isToggleLane (char lane) { return lane == 'a' || lane == 's'; }
inline int pitchSemis (float v) { return juce::roundToInt ((v - 0.5f) * 24.0f); }

// A parameter that changes other parameters (the pattern selector rewrites all step values).
// Hosts (AU in particular) must be told via the meta flag.
struct MetaChoice : juce::AudioParameterChoice
{
    using juce::AudioParameterChoice::AudioParameterChoice;
    bool isMetaParameter() const override { return true; }
};

inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    auto f = [&] (const char* id, const char* name, float lo, float hi, float def, float skewCentre = -1.0f)
    {
        NormalisableRange<float> r (lo, hi);
        if (skewCentre > 0) r.setSkewForCentre (skewCentre);
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, def));
    };
    auto c = [&] (const char* id, const char* name, const StringArray& opts, int def)
    { l.add (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, opts, def)); };
    auto b = [&] (const char* id, const char* name, bool def)
    { l.add (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def)); };

    // Macros
    f ("heat", "HEAT", 0, 1, 0.35f);
    f ("bite", "BITE", 0, 1, 0.4f);
    f ("morph", "MORPH", 0, 1, 0.0f);
    f ("movement", "MOVEMENT", 0, 1, 0.5f);
    f ("density", "DENSITY", 0, 1, 0.3f);

    // Core oscillator
    c ("osctype", "Osc1 Type", oscTypes, 0);
    c ("osc2type", "Osc2 Type", oscTypes, 0);
    f ("o1wave", "Osc1 Shape", 0, 1, 0.0f);
    f ("o2wave", "Osc2 Shape", 0, 1, 0.0f);
    l.add (std::make_unique<AudioParameterInt> (ParameterID { "o2semi", 1 }, "Osc2 Semi", -24, 24, 7));
    f ("o2fine", "Osc2 Fine", -100, 100, 8);
    f ("o2mix", "Osc2 Level", 0, 1, 0.6f);
    b ("sync", "Hard Sync", false);
    f ("fm", "FM Amount", 0, 1, 0.0f);
    f ("ring", "Ring Mod", 0, 1, 0.0f);
    f ("sub", "Sub", 0, 1, 0.3f);
    f ("noise", "Noise", 0, 1, 0.0f);

    // Iron filter
    f ("cutoff", "Cutoff", 30, 18000, 1800, 1000);
    f ("reso", "Resonance", 0, 1, 0.45f);
    c ("fmode", "Filter Mode", filterModes, 0);
    f ("fdrive", "Filter Drive", 0, 1, 0.0f);
    f ("fenv", "Filter Env", -1, 1, 0.5f);
    f ("fkey", "Key Track", 0, 1, 0.5f);

    // Envelopes
    f ("aa", "Amp Attack", 0.0005f, 2, 0.002f, 0.1f);
    f ("ad", "Amp Decay", 0.005f, 3, 0.4f, 0.4f);
    f ("as", "Amp Sustain", 0, 1, 0.8f);
    f ("ar", "Amp Release", 0.005f, 4, 0.12f, 0.4f);
    f ("fa", "Filter Attack", 0.0005f, 2, 0.001f, 0.1f);
    f ("fd", "Filter Decay", 0.005f, 3, 0.25f, 0.4f);
    f ("fs", "Filter Sustain", 0, 1, 0.2f);
    f ("fr", "Filter Release", 0.005f, 4, 0.15f, 0.4f);

    // Damage circuit
    f ("sat", "Saturation", 0, 1, 0.0f);
    f ("fold", "Wavefold", 0, 1, 0.0f);
    f ("clip", "Clip", 0, 1, 0.0f);
    f ("crush", "Bit Crush", 0, 1, 0.0f);
    c ("dorder", "Damage Order", damageOrders, 0);

    // LFOs
    c ("l1rate", "LFO1 Rate", syncRateNames, 3);
    f ("l1amt", "LFO1 > Cutoff", 0, 1, 0.0f);
    c ("l2rate", "LFO2 Rate", syncRateNames, 2);
    c ("l1shape", "LFO1 Shape", lfoShapes, 0);
    c ("l2shape", "LFO2 Shape", lfoShapes, 0);
    f ("l1hz", "LFO1 Free Rate", 0.05f, 30, 2, 2);
    f ("l2hz", "LFO2 Free Rate", 0.05f, 30, 0.5f, 2);
    f ("l2amt", "LFO2 > Character", 0, 1, 0.0f);

    // Voice
    c ("vmode", "Voice Mode", { "Mono", "Poly" }, 0);
    f ("glide", "Glide", 0, 1, 0.03f, 0.15f);

    // Rave sequencer
    b ("seqon", "Sequencer On", true);
    c ("seqrate", "Seq Rate", seqRateNames, 1);
    l.add (std::make_unique<AudioParameterInt> (ParameterID { "seqlen", 1 }, "Seq Length", 1, numSteps, 16));
    f ("seqdepth", "Seq Depth", 0, 1, 0.6f);
    f ("accent", "Accent Logic", 0, 1, 0.6f);
    f ("mutate", "Step Mutation", 0, 1, 0.0f);
    c ("evolve", "Pattern Evolution", evolveNames, 0);
    b ("seqretrig", "Step Retrigger", false);
    f ("swing", "Swing", 0, 0.75f, 0.0f);
    for (auto lane : seqLanes)
        l.add (std::make_unique<AudioParameterInt> (ParameterID { laneLenId (lane), 1 },
               "Lane Length " + juce::String::charToString (lane).toUpperCase(), 1, numSteps, numSteps));

    for (int i = 0; i < numModSlots; ++i)
    {
        l.add (std::make_unique<AudioParameterChoice> (ParameterID { modId ("src", i), 1 }, "Mod " + String (i + 1) + " Source", modSources, 0));
        l.add (std::make_unique<AudioParameterChoice> (ParameterID { modId ("dst", i), 1 }, "Mod " + String (i + 1) + " Dest", modDests, 0));
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { modId ("amt", i), 1 }, "Mod " + String (i + 1) + " Amount", NormalisableRange<float> (-1, 1), 0.0f));
    }

    for (auto lane : seqLanes)
        for (int s = 0; s < numSteps; ++s)
        {
            auto id = stepId (lane, s);
            auto name = juce::String ("Seq ") + juce::String::charToString (lane).toUpperCase() + " " + juce::String (s + 1);
            float def = lane == 'a' ? ((s % 4 == 0) ? 1.0f : 0.0f) : lane == 'g' ? 1.0f : lane == 's' ? 0.0f : 0.5f;
            if (lane == 'f') def = 0.5f + 0.4f * std::sin ((float) s * 0.9f) * ((s % 2) ? 1.0f : -0.6f);
            l.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (0, 1), def));
        }

    // FX & output
    c ("dtime", "Delay Time", delayNames, 2);
    f ("dfb", "Delay Feedback", 0, 0.9f, 0.35f);
    f ("dlevel", "Delay Level", 0, 1, 0.3f);
    f ("rsize", "Reverb Size", 0, 1, 0.5f);
    f ("rlevel", "Reverb Level", 0, 1, 0.2f);
    f ("rdamp", "Reverb Damping", 0, 1, 0.5f);
    f ("rpre", "Reverb Pre-Delay", 0, 200, 20);
    c ("quality", "Oversampling", { "2x Eco", "4x High" }, 0);
    f ("fxmix", "Dry/FX Mix", 0, 1, 0.25f);
    f ("out", "Output", -24, 6, -3);

    // v0.7 sound upgrades
    f ("drift", "Analog Drift", 0, 1, 0.15f);
    c ("unimode", "Unison Mode", { "Classic", "Supersaw" }, 0);
    f ("fmfb", "FM Feedback", 0, 1, 0.0f);
    c ("dtype", "Drive Type", { "Tube", "Fuzz", "Rectify", "Diode", "Sine Shaper", "Stairs" }, 0);
    c ("drvpos", "Drive Position", { "Pre Filter", "Post Filter" }, 0);
    c ("f2mode", "Filter 2", { "Off", "LP", "BP", "HP", "Notch" }, 0);
    c ("f2route", "Filter Routing", { "Serial", "Parallel" }, 0);
    f ("f2cut", "Filter 2 Cutoff", 30, 18000, 3000, 1000);
    f ("f2reso", "Filter 2 Reso", 0, 1, 0.3f);
    f ("lowcut", "Low Cut", 20, 600, 20, 120);
    f ("mideq", "Mid Boost", -6, 12, 0);
    f ("tilt", "Tilt EQ", -1, 1, 0);
    f ("width", "Stereo Width", 0, 1, 0.0f);
    f ("pump", "Pump Amount", 0, 1, 0.0f);
    c ("pumprate", "Pump Rate", { "1/4", "1/2", "1/8" }, 0);

    // v0.8: pitch envelope, env 3, character FX, FX order
    f ("penv", "Pitch Env Amount", -48, 48, 0);
    f ("pdecay", "Pitch Env Decay", 0.002f, 1.5f, 0.06f, 0.08f);
    f ("env3a", "Env3 Attack", 0.0005f, 4, 0.01f, 0.2f);
    f ("env3d", "Env3 Decay", 0.005f, 4, 0.5f, 0.5f);
    f ("env3s", "Env3 Sustain", 0, 1, 0.0f);
    f ("env3r", "Env3 Release", 0.005f, 4, 0.3f, 0.5f);
    c ("cfxtype", "Character FX", { "Off", "Comb", "Phaser", "Freq Shift", "Stutter" }, 0);
    f ("cfxmix", "Char FX Mix", 0, 1, 0.5f);
    f ("cfxp1", "Char FX Param 1", 0, 1, 0.5f);
    f ("cfxp2", "Char FX Param 2", 0, 1, 0.5f);
    c ("fxorder", "FX Order", { "Char > Delay > Verb", "Delay > Verb > Char", "Char on Wet only" }, 0);
    return l;
}
} // namespace ic
