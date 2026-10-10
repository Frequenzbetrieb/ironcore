#include "Presets.h"
#include "Params.h"
#include <map>

namespace ic
{
namespace
{
struct Preset
{
    const char* name;
    float heat, bite, morph, movement, density;
    float o1wave; int o2semi; float o2fine; bool sync; float fm, ring;
    float cutoff, reso; int fmode; float fenv, fdecay;
    float sat, fold, clip, crush; int dorder;
    int seqRate, pattern; float seqDepth, mutate, glide, lfo1, fxmix;
    std::vector<std::pair<const char*, float>> extra {}; // any further parameter overrides (plain values)
};

// 8 pattern shapes for the RAVE SEQUENCER (filter / fm / character / accent)
float semis (int st) { return 0.5f + (float) st / 24.0f; }

float melodicValue (int pattern, char lane, int s)
{
    static const int acidRiff[16] { 0, 0, 12, 0, 3, 0, -2, 0, 0, 12, 0, 7, 0, 3, 10, 0 };
    static const int chaosRiff[16] { 0, 0, 7, 0, 12, 0, 5, -5, 0, 0, 7, 12, 0, -12, 5, 0 };
    switch (pattern)
    {
        case 1: // acid: riff with slides, short gates
            if (lane == 'p') return semis (acidRiff[s]);
            if (lane == 's') return (s % 4 == 2) ? 1.0f : 0.0f;
            return (s % 2) ? 0.5f : 0.85f;
        case 4: // schranz gallop: chopped gates
            if (lane == 'p') return semis ((s % 8 == 7) ? 12 : 0);
            if (lane == 's') return 0.0f;
            return (s % 4 == 1) ? 0.0f : 0.6f;
        case 0: // pumping: short stabs
            if (lane == 'p') return semis (0);
            if (lane == 's') return 0.0f;
            return (s % 4 == 0) ? 0.0f : 0.7f;
        case 6: // chaos riff
            if (lane == 'p') return semis (chaosRiff[s]);
            if (lane == 's') return (s % 5 == 3) ? 1.0f : 0.0f;
            return (s % 3 == 2) ? 0.35f : 0.9f;
        default:
            return lane == 'p' ? 0.5f : lane == 'g' ? 1.0f : 0.0f;
    }
}

// Patterns 8-11 (v0.6): full definitions for all lanes
float patternValueV2 (int pattern, char lane, int s)
{
    static const int rollRiff[16]  { 0, 12, 0, 0, 12, 0, 0, 12, 0, 12, 0, 0, 12, 0, 7, 12 };
    static const int acid2Riff[16] { 0, 0, -12, 0, 3, 0, 5, 3, 0, 0, -12, 7, 0, 3, -2, 0 };
    static const int trance[16]    { 1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0 };
    switch (pattern)
    {
        case 8: // rolling 16th octave line
            switch (lane) { case 'p': return semis (rollRiff[s]); case 'g': return 0.75f; case 'a': return (s % 4 == 2) ? 1.0f : 0.0f;
                            case 'f': return (s % 4 == 2) ? 0.85f : 0.45f; case 's': return 0.0f; default: return 0.5f; }
        case 9: // trance gate
            switch (lane) { case 'g': return trance[s] ? 0.9f : 0.0f; case 'a': return (s % 8 == 0) ? 1.0f : 0.0f;
                            case 'f': return 0.5f + 0.2f * std::sin ((float) s * 0.4f); case 'p': return semis (0); case 's': return 0.0f; default: return 0.5f; }
        case 10: // industrial stutter
            switch (lane) { case 'g': return (s % 3 == 0) ? 0.25f : (s % 5 == 0 ? 0.0f : 0.15f); case 'a': return (s % 3 == 0) ? 1.0f : 0.0f;
                            case 'p': return semis ((s % 7 == 6) ? -12 : 0); case 'm': return (s % 3 == 0) ? 0.9f : 0.4f;
                            case 'c': return (s % 6 < 3) ? 0.3f : 0.7f; case 'f': return 0.6f; default: return 0.0f; }
        default: // 11: acid riff 2 with slides
            switch (lane) { case 'p': return semis (acid2Riff[s]); case 's': return (s % 5 == 2 || s == 13) ? 1.0f : 0.0f;
                            case 'g': return (s % 4 == 3) ? 0.0f : 0.8f; case 'a': return (s % 4 == 0 || s == 7) ? 1.0f : 0.0f;
                            case 'f': return 0.25f + 0.04f * (float) (s % 8); default: return 0.5f; }
    }
}

float patternValue (int pattern, char lane, int s)
{
    if (pattern >= 8) return patternValueV2 (pattern, lane, s);
    if (lane == 'p' || lane == 'g' || lane == 's')
        return melodicValue (pattern, lane, s);
    const float x = (float) s;
    switch (pattern)
    {
        case 0: // pumping offbeat
            if (lane == 'f') return (s % 4 == 2) ? 0.95f : (s % 2 ? 0.55f : 0.25f);
            if (lane == 'a') return (s % 4 == 2) ? 1.0f : 0.0f;
            return 0.5f;
        case 1: // acid climb
            if (lane == 'f') return 0.2f + 0.05f * (float) (s % 8) + (s % 3 == 0 ? 0.3f : 0.0f);
            if (lane == 'm') return 0.5f;
            if (lane == 'a') return (s % 3 == 0) ? 1.0f : 0.0f;
            return 0.5f;
        case 2: // metal hits
            if (lane == 'm') return (s % 4 == 0) ? 1.0f : 0.35f;
            if (lane == 'f') return 0.5f + 0.3f * std::sin (x * 0.8f);
            if (lane == 'a') return (s % 8 == 0 || s % 8 == 6) ? 1.0f : 0.0f;
            return 0.5f;
        case 3: // character switch
            if (lane == 'c') return (s % 8 < 4) ? 0.2f : 0.9f;
            if (lane == 'f') return (s % 2) ? 0.7f : 0.4f;
            if (lane == 'a') return (s % 4 == 0) ? 1.0f : 0.0f;
            return 0.5f;
        case 4: // schranz gallop
            if (lane == 'f') return (s % 4 == 0) ? 0.9f : (s % 4 == 3 ? 0.7f : 0.3f);
            if (lane == 'm') return (s % 4 == 3) ? 0.85f : 0.5f;
            if (lane == 'a') return (s % 4 == 0 || s % 4 == 3) ? 1.0f : 0.0f;
            return 0.5f;
        case 5: // vowel talk
            if (lane == 'c') return 0.5f + 0.45f * std::sin (x * 0.7f);
            if (lane == 'f') return 0.55f;
            if (lane == 'a') return (s % 4 == 2) ? 1.0f : 0.0f;
            return 0.5f;
        case 6: // full chaos (all lanes)
            if (lane == 'a') return ((s * 7) % 5 == 0) ? 1.0f : 0.0f;
            return 0.5f + 0.45f * std::sin (x * (lane == 'f' ? 1.3f : lane == 'm' ? 2.1f : 0.9f));
        default: // static (no movement)
            return lane == 'a' ? 0.0f : 0.5f;
    }
}

//  name                    heat bite morph mov  dens  o1w  semi fine  sync  fm   ring  cut   reso mode fenv fdec  sat  fold clip crush ord rate pat depth mut  glide lfo1 fx
const Preset presets[] {
    { "SCHRANZ Saw Burner",    0.55f,0.50f,0.00f,0.60f,0.45f, 0.0f, 7,  8, false,0.0f,0.0f,1500,0.50f,0,0.55f,0.25f,0.4f,0.0f,0.2f,0.0f,0,1,4,0.7f,0.0f,0.02f,0.0f,0.20f },
    { "SCHRANZ Gallop Lead",   0.70f,0.60f,0.10f,0.70f,0.35f, 0.2f, 12, 5, false,0.1f,0.0f,1200,0.55f,0,0.60f,0.20f,0.5f,0.1f,0.3f,0.0f,0,1,4,0.8f,0.1f,0.02f,0.0f,0.15f },
    { "SCHRANZ Grinder",       0.85f,0.55f,0.20f,0.65f,0.30f, 0.4f, 7,  0, true, 0.2f,0.1f,1000,0.45f,0,0.50f,0.20f,0.6f,0.3f,0.4f,0.0f,1,1,0,0.7f,0.0f,0.01f,0.0f,0.10f },
    { "SCHRANZ Sync Screamer", 0.60f,0.70f,0.00f,0.75f,0.25f, 0.0f, 19, 0, true, 0.0f,0.0f,2200,0.60f,2,0.40f,0.18f,0.4f,0.0f,0.2f,0.0f,0,2,4,0.8f,0.0f,0.03f,0.2f,0.15f },
    { "SCHRANZ Tool Stab",     0.75f,0.65f,0.30f,0.50f,0.20f, 0.0f, 12, 0, false,0.3f,0.2f, 800,0.50f,0,0.80f,0.12f,0.6f,0.2f,0.3f,0.0f,0,1,0,0.6f,0.0f,0.00f,0.0f,0.25f },
    { "SCHRANZ Hammer Loop",   0.90f,0.60f,0.15f,0.80f,0.40f, 0.3f, 5,  10,false,0.2f,0.0f,1100,0.55f,0,0.60f,0.20f,0.7f,0.2f,0.5f,0.1f,3,1,4,0.9f,0.2f,0.02f,0.0f,0.10f },
    { "INDUSTRIAL Metal Bar",  0.50f,0.50f,0.45f,0.50f,0.10f, 0.0f, 17, 0, false,0.6f,0.3f,3000,0.40f,2,0.30f,0.20f,0.3f,0.0f,0.1f,0.0f,0,1,2,0.7f,0.0f,0.00f,0.0f,0.30f },
    { "INDUSTRIAL Rust FM",    0.65f,0.40f,0.50f,0.60f,0.20f, 0.0f, 9,  30,false,0.7f,0.4f,2400,0.35f,0,0.40f,0.30f,0.5f,0.2f,0.2f,0.2f,2,1,2,0.8f,0.2f,0.00f,0.0f,0.25f },
    { "INDUSTRIAL Bitcrusher", 0.70f,0.50f,0.20f,0.55f,0.30f, 0.5f, 12, 0, false,0.2f,0.0f,1800,0.45f,0,0.50f,0.25f,0.5f,0.1f,0.3f,0.6f,0,1,6,0.6f,0.3f,0.00f,0.0f,0.15f },
    { "INDUSTRIAL Foundry",    0.95f,0.45f,0.40f,0.70f,0.50f, 0.7f, 7,  0, true, 0.4f,0.3f, 900,0.50f,0,0.55f,0.30f,0.8f,0.6f,0.5f,0.0f,1,0,2,0.7f,0.1f,0.01f,0.3f,0.20f },
    { "INDUSTRIAL Clang",      0.40f,0.60f,0.48f,0.40f,0.00f, 0.0f, 23, 40,false,0.8f,0.6f,5000,0.30f,3,0.20f,0.10f,0.3f,0.1f,0.1f,0.0f,0,1,2,0.6f,0.0f,0.00f,0.0f,0.35f },
    { "INDUSTRIAL Pipe Scream",0.60f,0.80f,0.75f,0.60f,0.20f, 0.2f, 12, 0, false,0.3f,0.0f,2000,0.65f,2,0.40f,0.20f,0.5f,0.2f,0.2f,0.0f,0,1,5,0.8f,0.0f,0.03f,0.0f,0.20f },
    { "ACID Reactor",          0.55f,0.80f,0.00f,0.70f,0.00f, 0.0f, 0,  0, false,0.0f,0.0f, 600,0.85f,0,0.70f,0.20f,0.5f,0.0f,0.2f,0.0f,0,1,1,0.9f,0.0f,0.06f,0.0f,0.20f },
    { "ACID Square 303 Burn",  0.65f,0.85f,0.00f,0.70f,0.00f, 0.5f, 0,  0, false,0.0f,0.0f, 500,0.88f,0,0.75f,0.18f,0.6f,0.1f,0.3f,0.0f,0,1,1,0.9f,0.1f,0.08f,0.0f,0.15f },
    { "ACID Squelch Fold",     0.80f,0.75f,0.05f,0.75f,0.00f, 0.0f, 0,  0, false,0.0f,0.0f, 700,0.80f,0,0.65f,0.15f,0.4f,0.5f,0.2f,0.0f,1,1,1,0.9f,0.2f,0.06f,0.0f,0.15f },
    { "ACID Talker",           0.55f,0.70f,0.70f,0.70f,0.00f, 0.0f, 0,  0, false,0.1f,0.0f, 900,0.75f,1,0.50f,0.20f,0.5f,0.0f,0.2f,0.0f,0,1,5,0.9f,0.0f,0.05f,0.0f,0.20f },
    { "ACID Crushed Line",     0.60f,0.75f,0.10f,0.65f,0.10f, 0.3f, 0,  0, false,0.0f,0.0f, 700,0.80f,0,0.65f,0.20f,0.5f,0.0f,0.2f,0.5f,0,1,1,0.8f,0.2f,0.05f,0.0f,0.15f },
    { "RAVE Hoover Lead",      0.45f,0.40f,0.10f,0.50f,0.90f, 0.2f, 12, 15,false,0.0f,0.0f,3000,0.30f,1,0.30f,0.40f,0.3f,0.0f,0.1f,0.0f,0,1,7,0.3f,0.0f,0.10f,0.4f,0.35f },
    { "RAVE Supersaw Riot",    0.50f,0.45f,0.00f,0.55f,1.00f, 0.0f, 7,  12,false,0.0f,0.0f,4000,0.30f,0,0.30f,0.30f,0.4f,0.0f,0.2f,0.0f,0,1,0,0.5f,0.0f,0.02f,0.0f,0.35f },
    { "RAVE Stab Alarm",       0.60f,0.60f,0.25f,0.60f,0.70f, 0.4f, 12, 0, false,0.2f,0.1f,2500,0.45f,0,0.60f,0.15f,0.5f,0.1f,0.3f,0.0f,0,1,0,0.6f,0.0f,0.00f,0.0f,0.30f },
    { "RAVE Siren Morph",      0.45f,0.55f,0.50f,0.80f,0.60f, 0.2f, 12, 0, false,0.3f,0.0f,2200,0.50f,1,0.30f,0.40f,0.4f,0.1f,0.2f,0.0f,0,0,3,0.8f,0.0f,0.15f,0.5f,0.30f },
    { "RAVE Vowel Anthem",     0.40f,0.55f,0.85f,0.60f,0.70f, 0.1f, 12, 8, false,0.1f,0.0f,2600,0.45f,1,0.30f,0.30f,0.4f,0.0f,0.2f,0.0f,0,1,5,0.7f,0.0f,0.04f,0.0f,0.35f },
    { "HARD TECHNO Drill",     0.80f,0.65f,0.25f,0.70f,0.30f, 0.6f, 12, 0, true, 0.2f,0.1f,1600,0.55f,2,0.40f,0.15f,0.6f,0.2f,0.4f,0.1f,2,2,4,0.8f,0.2f,0.00f,0.0f,0.15f },
    { "HARD TECHNO Pulse Tool",0.70f,0.55f,0.20f,0.60f,0.20f, 0.9f, 7,  0, false,0.1f,0.0f,1300,0.50f,0,0.60f,0.15f,0.5f,0.1f,0.3f,0.0f,0,1,0,0.7f,0.0f,0.00f,0.2f,0.15f },
    { "HARD TECHNO Character", 0.70f,0.60f,0.40f,0.80f,0.40f, 0.3f, 12, 6, false,0.3f,0.0f,1500,0.55f,0,0.50f,0.20f,0.6f,0.2f,0.3f,0.0f,0,1,3,0.9f,0.3f,0.02f,0.0f,0.20f },
    { "HARD TECHNO Evolve",    0.75f,0.60f,0.30f,0.85f,0.40f, 0.2f, 7,  9, false,0.3f,0.2f,1400,0.55f,0,0.55f,0.20f,0.6f,0.2f,0.3f,0.1f,3,1,6,0.9f,0.6f,0.02f,0.2f,0.20f },
    { "HARD TECHNO Bass Rip",  0.75f,0.45f,0.10f,0.50f,0.20f, 0.0f, -12,0, false,0.1f,0.0f, 500,0.40f,0,0.50f,0.20f,0.6f,0.2f,0.3f,0.0f,0,1,0,0.5f,0.0f,0.03f,0.0f,0.05f },
    { "FX Riser Chaos",        0.60f,0.60f,0.60f,1.00f,0.60f, 0.4f, 19, 20,true, 0.5f,0.3f,2000,0.55f,2,0.50f,0.30f,0.5f,0.3f,0.3f,0.2f,1,2,6,1.0f,0.8f,0.00f,0.6f,0.40f },
    { "FX Machine Talk",       0.50f,0.60f,0.80f,0.90f,0.20f, 0.3f, 12, 0, false,0.4f,0.2f,1800,0.55f,2,0.40f,0.20f,0.5f,0.1f,0.2f,0.3f,0,2,6,0.9f,0.5f,0.00f,0.0f,0.30f },
    // ---------------- v0.6 additions ----------------
    { "SCHRANZ Steel Roller",  0.70f,0.55f,0.10f,0.70f,0.30f, 0.0f, 12, 6, false,0.1f,0.0f,1300,0.50f,0,0.60f,0.18f,0.5f,0.1f,0.3f,0.0f,0,1,8,0.8f,0.0f,0.02f,0.0f,0.15f, { { "osctype", 1 }, { "seqretrig", 1 } } },
    { "SCHRANZ Jackhammer",    0.90f,0.60f,0.20f,0.75f,0.20f, 0.5f, 7,  0, true, 0.2f,0.1f,1500,0.55f,2,0.55f,0.12f,0.7f,0.3f,0.5f,0.0f,2,2,10,0.9f,0.1f,0.00f,0.0f,0.10f, { { "seqretrig", 1 } } },
    { "SCHRANZ Loop Saw Wall", 0.60f,0.45f,0.00f,0.60f,0.85f, 0.0f, 7,  10,false,0.0f,0.0f,1700,0.45f,0,0.50f,0.25f,0.5f,0.0f,0.3f,0.0f,0,1,9,0.7f,0.0f,0.02f,0.0f,0.20f },
    { "SCHRANZ Detroit Rage",  0.80f,0.65f,0.15f,0.70f,0.40f, 0.2f, 5,  0, false,0.2f,0.0f,1200,0.60f,0,0.65f,0.15f,0.6f,0.2f,0.4f,0.1f,3,1,8,0.8f,0.2f,0.02f,0.0f,0.15f, { { "swing", 0.15f } } },
    { "SCHRANZ Acid Smash",    0.85f,0.80f,0.05f,0.75f,0.10f, 0.0f, 0,  0, false,0.0f,0.0f, 650,0.85f,0,0.70f,0.18f,0.7f,0.2f,0.4f,0.0f,0,1,11,0.9f,0.1f,0.05f,0.0f,0.12f },
    { "INDUSTRIAL Anvil",      0.70f,0.55f,0.45f,0.40f,0.10f, 0.3f, 19, 0, false,0.7f,0.5f,2500,0.40f,2,0.40f,0.12f,0.5f,0.3f,0.2f,0.0f,1,1,10,0.7f,0.0f,0.00f,0.0f,0.30f, { { "osctype", 2 }, { "seqretrig", 1 } } },
    { "INDUSTRIAL Conveyor",   0.65f,0.50f,0.35f,0.80f,0.30f, 0.4f, 7,  15,false,0.4f,0.2f,1600,0.50f,0,0.45f,0.20f,0.5f,0.2f,0.3f,0.2f,2,1,6,0.9f,0.4f,0.00f,0.3f,0.20f, { { "osctype", 4 }, { "lenf", 11 }, { "lenm", 7 }, { "lenc", 5 } } },
    { "INDUSTRIAL Gas Leak",   0.55f,0.70f,0.70f,0.60f,0.20f, 0.0f, 12, 0, false,0.2f,0.0f,3000,0.55f,3,0.30f,0.30f,0.4f,0.1f,0.2f,0.0f,0,0,5,0.7f,0.0f,0.05f,0.4f,0.35f, { { "noise", 0.35f } } },
    { "INDUSTRIAL Rebar",      0.75f,0.60f,0.30f,0.50f,0.20f, 0.6f, 24, 0, true, 0.3f,0.3f,2000,0.50f,2,0.45f,0.15f,0.6f,0.4f,0.3f,0.0f,1,1,2,0.7f,0.1f,0.00f,0.0f,0.20f, { { "osctype", 2 } } },
    { "INDUSTRIAL Static Mill",0.60f,0.55f,0.25f,0.70f,0.40f, 0.5f, 7,  0, false,0.2f,0.0f,1500,0.50f,0,0.50f,0.20f,0.5f,0.1f,0.3f,0.7f,0,2,10,0.8f,0.2f,0.00f,0.0f,0.15f, { { "osctype", 4 } } },
    { "ACID Night Shift",      0.50f,0.80f,0.00f,0.70f,0.00f, 0.0f, 0,  0, false,0.0f,0.0f, 550,0.88f,0,0.75f,0.20f,0.4f,0.0f,0.2f,0.0f,0,1,11,0.9f,0.0f,0.06f,0.0f,0.20f },
    { "ACID Warehouse 303",    0.60f,0.85f,0.00f,0.75f,0.00f, 0.5f, 0,  0, false,0.0f,0.0f, 500,0.90f,0,0.80f,0.16f,0.5f,0.1f,0.2f,0.0f,0,1,11,1.0f,0.1f,0.07f,0.0f,0.15f, { { "swing", 0.3f } } },
    { "ACID Rubber Band",      0.45f,0.75f,0.15f,0.65f,0.10f, 0.2f, 0,  0, false,0.1f,0.0f, 800,0.80f,1,0.60f,0.25f,0.4f,0.3f,0.1f,0.0f,1,1,8,0.8f,0.0f,0.08f,0.0f,0.20f },
    { "ACID Vocoder Squelch",  0.55f,0.80f,0.75f,0.70f,0.00f, 0.0f, 0,  0, false,0.0f,0.0f, 900,0.80f,1,0.60f,0.20f,0.5f,0.0f,0.2f,0.0f,0,1,11,0.9f,0.0f,0.05f,0.0f,0.20f, { { "osctype", 3 } } },
    { "ACID Fold Bubble",      0.70f,0.70f,0.05f,0.70f,0.00f, 0.0f, 0,  0, false,0.0f,0.0f, 700,0.82f,0,0.70f,0.15f,0.3f,0.7f,0.1f,0.0f,1,2,1,0.8f,0.1f,0.04f,0.0f,0.15f },
    { "RAVE 92 Hoover",        0.50f,0.40f,0.15f,0.50f,1.00f, 0.3f, 12, 20,false,0.1f,0.0f,3200,0.30f,1,0.30f,0.40f,0.3f,0.0f,0.1f,0.0f,0,1,7,0.3f,0.0f,0.20f,0.5f,0.35f, { { "ar", 0.6f }, { "vmode", 1 } } },
    { "RAVE Mentasm Stab",     0.55f,0.55f,0.30f,0.50f,0.80f, 0.6f, 12, 25,false,0.2f,0.0f,2600,0.45f,0,0.70f,0.12f,0.4f,0.1f,0.2f,0.0f,0,1,0,0.5f,0.0f,0.00f,0.0f,0.30f, { { "vmode", 1 }, { "ad", 0.3f }, { "as", 0.0f } } },
    { "RAVE Trance Gate",      0.40f,0.45f,0.10f,0.60f,0.90f, 0.0f, 7,  12,false,0.0f,0.0f,3500,0.35f,1,0.30f,0.30f,0.3f,0.0f,0.1f,0.0f,0,1,9,0.5f,0.0f,0.02f,0.0f,0.35f, { { "vmode", 1 } } },
    { "RAVE Piano Riot",       0.35f,0.50f,0.50f,0.40f,0.30f, 0.0f, 12, 0, false,0.5f,0.2f,4000,0.30f,1,0.50f,0.20f,0.3f,0.0f,0.1f,0.0f,0,1,7,0.2f,0.0f,0.00f,0.0f,0.30f, { { "vmode", 1 }, { "ad", 0.6f }, { "as", 0.1f }, { "osctype", 2 } } },
    { "RAVE Alarm Siren",      0.60f,0.65f,0.20f,0.90f,0.40f, 0.5f, 12, 0, true, 0.1f,0.0f,2500,0.55f,2,0.30f,0.30f,0.5f,0.1f,0.3f,0.0f,0,0,3,0.8f,0.0f,0.10f,0.0f,0.25f, { { "mmsrc0", 1 }, { "mmdst0", 2 }, { "mmamt0", 0.4f } } },
    { "HARD TECHNO Rumble Lead",0.75f,0.50f,0.15f,0.60f,0.30f, 0.0f, -12,0, false,0.1f,0.0f, 700,0.45f,0,0.55f,0.18f,0.6f,0.2f,0.3f,0.0f,0,1,8,0.7f,0.0f,0.03f,0.0f,0.10f, { { "sub", 0.6f } } },
    { "HARD TECHNO Poly Grind",0.80f,0.60f,0.35f,0.80f,0.40f, 0.4f, 7,  7, false,0.3f,0.1f,1400,0.55f,0,0.55f,0.20f,0.6f,0.3f,0.4f,0.1f,3,1,6,0.9f,0.5f,0.02f,0.2f,0.20f, { { "osctype", 1 }, { "lenf", 12 }, { "lenm", 9 }, { "lenc", 7 }, { "lenp", 5 } } },
    { "HARD TECHNO Stutter",   0.70f,0.60f,0.25f,0.70f,0.30f, 0.3f, 12, 0, false,0.2f,0.0f,1600,0.55f,2,0.50f,0.12f,0.5f,0.2f,0.3f,0.0f,0,2,10,0.8f,0.0f,0.00f,0.0f,0.15f, { { "seqretrig", 1 } } },
    { "HARD TECHNO Metal Pulse",0.65f,0.55f,0.40f,0.60f,0.20f, 0.5f, 19, 0, false,0.5f,0.3f,2200,0.50f,0,0.45f,0.15f,0.5f,0.2f,0.3f,0.0f,0,1,2,0.7f,0.0f,0.00f,0.0f,0.20f, { { "osctype", 2 }, { "mmsrc0", 9 }, { "mmdst0", 7 }, { "mmamt0", 0.5f } } },
    { "HARD TECHNO Shuffle",   0.70f,0.55f,0.20f,0.70f,0.30f, 0.2f, 7,  0, false,0.1f,0.0f,1400,0.50f,0,0.55f,0.18f,0.6f,0.1f,0.3f,0.0f,0,1,4,0.8f,0.1f,0.02f,0.0f,0.15f, { { "swing", 0.45f } } },
    { "FX Laser Drop",         0.60f,0.70f,0.30f,1.00f,0.30f, 0.3f, 24, 0, true, 0.4f,0.0f,3000,0.60f,2,0.40f,0.40f,0.5f,0.2f,0.3f,0.0f,0,2,6,1.0f,0.6f,0.00f,0.0f,0.40f, { { "mmsrc0", 3 }, { "mmdst0", 2 }, { "mmamt0", 1.0f } } },
    { "FX Broken Radio",       0.55f,0.60f,0.60f,0.90f,0.30f, 0.5f, 12, 0, false,0.3f,0.4f,2000,0.55f,2,0.40f,0.30f,0.5f,0.2f,0.2f,0.8f,0,2,6,0.9f,0.7f,0.00f,0.0f,0.35f, { { "osctype", 4 }, { "noise", 0.2f } } },
    { "FX Space Debris",       0.45f,0.50f,0.70f,0.90f,0.60f, 0.4f, 19, 30,false,0.5f,0.3f,2500,0.50f,1,0.40f,0.50f,0.4f,0.2f,0.2f,0.1f,1,0,6,0.9f,0.8f,0.00f,0.6f,0.60f, { { "rsize", 0.9f }, { "rlevel", 0.6f }, { "osctype", 3 } } },
    { "FX Riser Engine",       0.60f,0.55f,0.40f,0.90f,0.80f, 0.2f, 12, 10,false,0.2f,0.0f,1500,0.55f,0,0.60f,2.00f,0.5f,0.2f,0.3f,0.0f,0,1,7,0.0f,0.0f,0.00f,0.0f,0.40f, { { "fa", 2.0f }, { "fs", 1.0f }, { "mmsrc0", 3 }, { "mmdst0", 2 }, { "mmamt0", 0.5f } } },
    { "FX Tunnel Echo",        0.55f,0.55f,0.30f,0.70f,0.30f, 0.3f, 7,  0, false,0.2f,0.0f,1800,0.55f,2,0.50f,0.15f,0.5f,0.1f,0.3f,0.0f,0,1,10,0.7f,0.0f,0.00f,0.0f,0.60f, { { "dfb", 0.75f }, { "dlevel", 0.7f }, { "dtime", 2 } } },
    // ---------------- v0.8: new engines, pitch env, character FX, clean sounds ----------------
    { "PLUCK Steel String",    0.00f,0.30f,0.00f,0.40f,0.00f, 0.6f, 12, 0, false,0.0f,0.0f,6000,0.20f,1,0.20f,0.30f,0.0f,0.0f,0.0f,0.0f,0,1,7,0.3f,0.0f,0.00f,0.0f,0.30f, { { "osctype", 6 }, { "osc2type", 6 }, { "o2mix", 0.3f }, { "ad", 1.2f }, { "as", 0.0f }, { "ar", 0.6f }, { "vmode", 1 }, { "sub", 0.0f } } },
    { "PLUCK Rusty Harp",      0.10f,0.30f,0.00f,0.50f,0.00f, 0.3f, 7,  0, false,0.0f,0.0f,4000,0.30f,1,0.30f,0.40f,0.1f,0.0f,0.0f,0.0f,0,1,8,0.4f,0.0f,0.00f,0.0f,0.35f, { { "osctype", 6 }, { "osc2type", 6 }, { "o2mix", 0.4f }, { "ad", 1.5f }, { "as", 0.0f }, { "cfxtype", 1 }, { "cfxp1", 0.35f }, { "cfxp2", 0.8f }, { "cfxmix", 0.5f }, { "sub", 0.0f } } },
    { "PLUCK Pipe Bell",       0.00f,0.25f,0.00f,0.30f,0.00f, 0.9f, 19, 0, false,0.0f,0.3f,8000,0.10f,1,0.00f,0.30f,0.0f,0.0f,0.0f,0.0f,0,1,7,0.2f,0.0f,0.00f,0.0f,0.40f, { { "osctype", 6 }, { "osc2type", 6 }, { "o2mix", 0.5f }, { "ad", 2.5f }, { "as", 0.0f }, { "ar", 1.5f }, { "vmode", 1 }, { "sub", 0.0f }, { "rlevel", 0.4f } } },
    { "INDUSTRIAL Pipe Strike",0.60f,0.45f,0.00f,0.60f,0.00f, 0.4f, 7,  0, false,0.0f,0.2f,3000,0.40f,0,0.40f,0.20f,0.6f,0.0f,0.2f,0.0f,0,1,10,0.7f,0.0f,0.00f,0.0f,0.30f, { { "osctype", 6 }, { "dtype", 1 }, { "seqretrig", 1 }, { "cfxtype", 3 }, { "cfxp1", 0.62f }, { "cfxp2", 0.3f }, { "cfxmix", 0.4f } } },
    { "CLEAN CZ Lead",         0.00f,0.30f,0.00f,0.40f,0.20f, 0.6f, 12, 4, false,0.0f,0.0f,9000,0.10f,1,0.30f,0.30f,0.0f,0.0f,0.0f,0.0f,0,1,7,0.3f,0.0f,0.04f,0.0f,0.25f, { { "osctype", 5 }, { "osc2type", 5 }, { "o2wave", 0.3f }, { "o2mix", 0.4f }, { "mmsrc0", 3 }, { "mmdst0", 7 }, { "mmamt0", 0.6f } } },
    { "CLEAN PD Brass",        0.05f,0.30f,0.00f,0.40f,0.40f, 0.2f, 0,  6, false,0.0f,0.0f,1500,0.20f,1,0.60f,0.40f,0.0f,0.0f,0.0f,0.0f,0,1,7,0.2f,0.0f,0.03f,0.0f,0.30f, { { "osctype", 5 }, { "osc2type", 5 }, { "aa", 0.04f }, { "vmode", 1 }, { "mmsrc0", 3 }, { "mmdst0", 7 }, { "mmamt0", 0.8f } } },
    { "CLEAN Pure Saw",        0.00f,0.20f,0.00f,0.30f,0.20f, 0.0f, 0,  7, false,0.0f,0.0f,7000,0.15f,1,0.20f,0.40f,0.0f,0.0f,0.0f,0.0f,0,1,7,0.2f,0.0f,0.02f,0.0f,0.25f, { { "drift", 0.3f } } },
    { "CLEAN Glass Sine",      0.00f,0.20f,0.25f,0.40f,0.00f, 0.0f, 12, 0, false,0.2f,0.0f,9000,0.10f,1,0.00f,0.30f,0.0f,0.0f,0.0f,0.0f,0,1,7,0.2f,0.0f,0.03f,0.0f,0.40f, { { "osctype", 5 }, { "o1wave", 0.0f }, { "fmfb", 0.0f }, { "rlevel", 0.35f } } },
    { "CLEAN Square Lead",     0.00f,0.35f,0.00f,0.50f,0.10f, 0.5f, 12, 3, false,0.0f,0.0f,3000,0.30f,1,0.40f,0.25f,0.0f,0.0f,0.0f,0.0f,0,1,8,0.5f,0.0f,0.04f,0.0f,0.20f, {} },
    { "PERC Noise Snare Lead", 0.30f,0.50f,0.00f,0.60f,0.00f, 0.0f, 0,  0, false,0.0f,0.0f,5000,0.30f,2,0.30f,0.10f,0.2f,0.0f,0.2f,0.0f,0,1,0,0.6f,0.0f,0.00f,0.0f,0.20f, { { "osctype", 7 }, { "o1wave", 0.6f }, { "o2mix", 0.4f }, { "penv", 24 }, { "pdecay", 0.03f }, { "ad", 0.15f }, { "as", 0.0f }, { "seqretrig", 1 } } },
    { "PERC Steam Hit",        0.20f,0.40f,0.00f,0.50f,0.00f, 0.0f, 0,  0, false,0.0f,0.0f,7000,0.20f,3,0.20f,0.10f,0.1f,0.0f,0.0f,0.0f,0,2,10,0.6f,0.0f,0.00f,0.0f,0.25f, { { "osctype", 7 }, { "osc2type", 7 }, { "o1wave", 0.2f }, { "ad", 0.08f }, { "as", 0.0f }, { "seqretrig", 1 } } },
    { "PERC Tom Lead",         0.30f,0.30f,0.00f,0.50f,0.00f, 0.0f, 0,  0, false,0.0f,0.0f,2500,0.20f,1,0.20f,0.20f,0.3f,0.0f,0.0f,0.0f,0,1,8,0.5f,0.0f,0.00f,0.0f,0.20f, { { "osctype", 5 }, { "o1wave", 0.1f }, { "o2mix", 0.0f }, { "penv", 18 }, { "pdecay", 0.08f }, { "ad", 0.3f }, { "as", 0.0f }, { "seqretrig", 1 } } },
    { "SCHRANZ Pitch Impact",  0.75f,0.55f,0.10f,0.70f,0.30f, 0.0f, 7,  6, false,0.1f,0.0f,1400,0.50f,0,0.60f,0.15f,0.6f,0.1f,0.3f,0.0f,0,1,4,0.8f,0.0f,0.00f,0.0f,0.15f, { { "penv", 24 }, { "pdecay", 0.025f }, { "seqretrig", 1 } } },
    { "HARD TECHNO Laser Stab",0.55f,0.60f,0.20f,0.60f,0.30f, 0.3f, 12, 0, true, 0.2f,0.0f,2500,0.55f,2,0.40f,0.15f,0.4f,0.2f,0.2f,0.0f,0,1,0,0.6f,0.0f,0.00f,0.0f,0.30f, { { "penv", 36 }, { "pdecay", 0.12f }, { "seqretrig", 1 } } },
    { "FX Zap Down",           0.40f,0.50f,0.30f,0.60f,0.20f, 0.0f, 12, 0, false,0.3f,0.0f,4000,0.40f,1,0.20f,0.30f,0.3f,0.0f,0.1f,0.0f,0,2,6,0.5f,0.3f,0.00f,0.0f,0.40f, { { "penv", 48 }, { "pdecay", 0.2f }, { "seqretrig", 1 }, { "osctype", 5 } } },
    { "FX Shifted Space",      0.20f,0.40f,0.40f,0.60f,0.40f, 0.3f, 12, 0, false,0.2f,0.0f,3000,0.40f,1,0.30f,0.40f,0.2f,0.0f,0.1f,0.0f,0,1,7,0.4f,0.0f,0.05f,0.3f,0.60f, { { "cfxtype", 3 }, { "fxorder", 2 }, { "cfxp1", 0.7f }, { "cfxp2", 0.5f }, { "cfxmix", 1.0f }, { "rsize", 0.8f }, { "rlevel", 0.6f } } },
    { "FX Wind Tunnel",        0.00f,0.30f,0.00f,0.80f,0.00f, 0.0f, 0,  0, false,0.0f,0.0f,1200,0.70f,2,0.00f,0.30f,0.0f,0.0f,0.0f,0.0f,0,1,7,0.0f,0.0f,0.00f,0.6f,0.50f, { { "osctype", 7 }, { "osc2type", 7 }, { "o1wave", 0.3f }, { "l1rate", 8 }, { "l1hz", 0.15f }, { "l1amt", 0.8f }, { "cfxtype", 2 }, { "cfxp1", 0.2f }, { "cfxp2", 0.7f }, { "cfxmix", 0.6f } } },
    { "FX S&H Computer",       0.20f,0.60f,0.30f,0.80f,0.00f, 0.5f, 19, 0, false,0.3f,0.2f,2000,0.70f,2,0.20f,0.20f,0.2f,0.0f,0.1f,0.0f,0,1,7,0.0f,0.0f,0.00f,0.9f,0.30f, { { "l1shape", 4 }, { "l1rate", 4 }, { "l2shape", 4 }, { "l2rate", 3 }, { "l2amt", 0.6f } } },
    { "INDUSTRIAL Comb Grinder",0.70f,0.50f,0.20f,0.60f,0.30f, 0.4f, 7,  0, false,0.2f,0.0f,1800,0.50f,0,0.50f,0.20f,0.5f,0.2f,0.3f,0.0f,1,1,10,0.7f,0.0f,0.00f,0.0f,0.20f, { { "cfxtype", 1 }, { "cfxp1", 0.15f }, { "cfxp2", 0.95f }, { "cfxmix", 0.7f }, { "dtype", 4 } } },
    { "HARD TECHNO Stutter Machine",0.70f,0.55f,0.20f,0.70f,0.30f, 0.2f, 7, 0, false,0.1f,0.0f,1500,0.55f,0,0.55f,0.18f,0.5f,0.1f,0.3f,0.0f,0,1,8,0.8f,0.0f,0.00f,0.0f,0.20f, { { "cfxtype", 4 }, { "cfxp1", 0.3f }, { "cfxp2", 0.4f }, { "cfxmix", 1.0f } } },
    { "RAVE Phaser Lead",      0.20f,0.40f,0.10f,0.50f,0.70f, 0.0f, 7,  10,false,0.0f,0.0f,3500,0.30f,1,0.30f,0.30f,0.2f,0.0f,0.1f,0.0f,0,1,7,0.3f,0.0f,0.05f,0.0f,0.30f, { { "unimode", 1 }, { "cfxtype", 2 }, { "cfxp1", 0.35f }, { "cfxp2", 0.6f }, { "cfxmix", 0.7f } } },
    { "ACID Env3 Sweep",       0.40f,0.80f,0.00f,0.70f,0.00f, 0.0f, 0,  0, false,0.0f,0.0f, 500,0.85f,0,0.50f,0.20f,0.3f,0.0f,0.1f,0.0f,0,1,11,0.9f,0.0f,0.06f,0.0f,0.20f, { { "env3a", 2.0f }, { "env3s", 1.0f }, { "mmsrc2", 10 }, { "mmdst2", 0 }, { "mmamt2", 0.5f }, { "mmsrc3", 10 }, { "mmdst3", 5 }, { "mmamt3", 0.4f } } },
    { "INIT Iron",             0.00f,0.40f,0.00f,0.50f,0.30f, 0.0f, 7,  8, false,0.0f,0.0f,1800,0.45f,0,0.50f,0.25f,0.3f,0.0f,0.1f,0.0f,0,1,7,0.6f,0.0f,0.03f,0.0f,0.25f },
};

float presetTrimDb (const juce::String& name)
{
    static const std::map<juce::String, float> trims {
#include "PresetTrims.inc"
    };
    auto it = trims.find (name);
    return it != trims.end() ? it->second : 0.0f;
}

void set (juce::AudioProcessorValueTreeState& s, const juce::String& id, float v)
{
    if (auto* p = s.getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 (v));
}
} // namespace

int numPresets() { return (int) std::size (presets); }
juce::String presetName (int i) { return juce::isPositiveAndBelow (i, numPresets()) ? presets[i].name : ""; }

void applyPreset (int i, juce::AudioProcessorValueTreeState& s)
{
    if (! juce::isPositiveAndBelow (i, numPresets())) return;
    for (auto* p : s.processor.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            rp->setValueNotifyingHost (rp->getDefaultValue());

    const auto& p = presets[i];
    set (s, "heat", p.heat);   set (s, "bite", p.bite);   set (s, "morph", p.morph);
    set (s, "movement", p.movement); set (s, "density", p.density);
    set (s, "o1wave", p.o1wave); set (s, "o2semi", (float) p.o2semi); set (s, "o2fine", p.o2fine);
    set (s, "sync", p.sync ? 1.0f : 0.0f); set (s, "fm", p.fm); set (s, "ring", p.ring);
    set (s, "cutoff", p.cutoff); set (s, "reso", p.reso); set (s, "fmode", (float) p.fmode);
    set (s, "fenv", p.fenv); set (s, "fd", p.fdecay);
    set (s, "sat", p.sat); set (s, "fold", p.fold); set (s, "clip", p.clip); set (s, "crush", p.crush);
    set (s, "dorder", (float) p.dorder);
    set (s, "seqrate", (float) p.seqRate); set (s, "seqdepth", p.seqDepth); set (s, "mutate", p.mutate);
    set (s, "evolve", p.mutate > 0.0f ? 3.0f : 0.0f);
    set (s, "glide", p.glide); set (s, "l1amt", p.lfo1); set (s, "fxmix", p.fxmix);

    for (auto lane : seqLanes)
        for (int st = 0; st < numSteps; ++st)
            set (s, stepId (lane, st), patternValue (p.pattern, lane, st));

    // v0.3 extras: wavetables, swing, polyrhythmic lanes, mod matrix
    auto mod = [&] (int slot, int src, int dst, float amt)
    {
        set (s, modId ("src", slot), (float) src); set (s, modId ("dst", slot), (float) dst); set (s, modId ("amt", slot), amt);
    };
    const juce::String n (p.name);
    if (n.contains ("Metal") || n.contains ("Clang") || n.contains ("Rust"))  set (s, "osctype", 2.0f);
    if (n.contains ("Vowel") || n.contains ("Talk") || n.contains ("Pipe"))  set (s, "osctype", 3.0f);
    if (n.contains ("Bitcrusher") || n.contains ("Machine") || n.contains ("Chaos")) set (s, "osctype", 4.0f);
    if (n.contains ("Grinder") || n.contains ("Foundry") || n.contains ("Drill")) set (s, "osctype", 1.0f);
    if (n.startsWith ("ACID")) { set (s, "swing", 0.25f); mod (0, srcVelocity, dstCutoff, 0.35f); mod (1, srcModWheel, dstReso, 0.5f); }
    if (n.contains ("Evolve") || n.contains ("Chaos"))
    {
        set (s, laneLenId ('f'), 12.0f); set (s, laneLenId ('m'), 7.0f); set (s, laneLenId ('c'), 10.0f);
        mod (0, srcRandom, dstShape, 0.4f); mod (1, srcLfo2, dstPan, 0.6f);
    }
    if (n.contains ("Character")) { set (s, laneLenId ('c'), 6.0f); mod (0, srcAccent, dstFold, 0.5f); }
    if (n.startsWith ("SCHRANZ")) { mod (0, srcModWheel, dstDrive, 0.6f); mod (1, srcAccent, dstPitch, 0.04f); }
    if (n.startsWith ("RAVE")) { mod (0, srcModWheel, dstChar, 0.6f); mod (1, srcLfo1, dstPitch, 0.01f); }
    if (n.startsWith ("HARD TECHNO")) mod (0, srcFiltEnv, dstFm, 0.4f);

    // v0.7 upgrades: supersaw, FM feedback, drive types, filter 2, post EQ, width, pump
    if (n.contains ("Supersaw") || n.contains ("Hoover") || n.contains ("Trance") || n.contains ("Saw Wall") || n.contains ("Anthem"))
        { set (s, "unimode", 1.0f); set (s, "width", 0.5f); }
    if (n.contains ("FM") || n.contains ("Metal") || n.contains ("Clang") || n.contains ("Anvil") || n.contains ("Rebar"))
        set (s, "fmfb", 0.45f);
    if (n.contains ("Fuzz") || n.contains ("Grinder") || n.contains ("Jackhammer") || n.contains ("Foundry")) set (s, "dtype", 1.0f);
    if (n.contains ("Drill") || n.contains ("Rust") || n.contains ("Static")) set (s, "dtype", 2.0f);
    if (n.contains ("Hammer") || n.contains ("Smash") || n.contains ("Rage")) set (s, "dtype", 3.0f);
    if (n.contains ("Pipe") || n.contains ("Talk") || n.contains ("Vocoder")) { set (s, "f2mode", 2.0f); set (s, "f2route", 1.0f); set (s, "f2cut", 1400.0f); set (s, "f2reso", 0.6f); }
    if (n.contains ("Sync Screamer") || n.contains ("Siren")) { set (s, "f2mode", 4.0f); set (s, "f2cut", 2500.0f); }
    if (n.startsWith ("ACID")) set (s, "drvpos", 1.0f);
    if (n.startsWith ("SCHRANZ") || n.startsWith ("HARD TECHNO")) { set (s, "lowcut", 120.0f); set (s, "mideq", 3.0f); }
    if (n.startsWith ("RAVE") && ! n.contains ("Alarm")) { set (s, "heat", p.heat * 0.4f); set (s, "sat", p.sat * 0.4f); set (s, "clip", 0.0f); }
    if (n.startsWith ("RAVE")) { set (s, "lowcut", 150.0f); set (s, "tilt", 0.25f); set (s, "width", juce::jmax (0.3f, s.getRawParameterValue ("width")->load())); }
    if (n.contains ("Trance") || n.contains ("Riot") || n.contains ("Loop Saw")) set (s, "pump", 0.6f);
    set (s, "drift", n.startsWith ("RAVE") ? 0.3f : 0.15f);

    // stabs and acid lines re-pluck their envelopes on every step
    if (n.startsWith ("ACID") || n.contains ("Stab") || n.contains ("Gallop") || n.contains ("Pulse Tool") || n.contains ("Shuffle") || n.contains ("Steel"))
        set (s, "seqretrig", 1.0f);

    for (const auto& [id, v] : p.extra) set (s, id, v); // per-preset overrides win

    // loudness match (measured with Tools/Stress presets, target peak RMS ~0.09)
    set (s, "out", juce::jlimit (-12.0f, 6.0f, -3.0f + presetTrimDb (n)));
}
} // namespace ic
