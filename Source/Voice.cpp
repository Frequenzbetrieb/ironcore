#include "Voice.h"
#include "Wavetables.h"

namespace ic
{
namespace
{
constexpr float twoPi = juce::MathConstants<float>::twoPi;
constexpr float pi = juce::MathConstants<float>::pi;

inline float polyBlep (float t, float dt)
{
    if (t < dt) { t /= dt; return t + t - t * t - 1.0f; }
    if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
    return 0.0f;
}

inline float fastTanh (float x)
{
    x = juce::jlimit (-3.0f, 3.0f, x);
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

// Vowel formants A, E, I, O (Hz)
constexpr float vowels[4][3] { { 800, 1150, 2900 }, { 400, 1600, 2700 }, { 350, 2300, 3000 }, { 450, 800, 2830 } };
constexpr float formantGain[3] { 1.0f, 0.6f, 0.35f };
} // namespace

inline float softLimit (float y)
{
    const float a = std::abs (y);
    return a <= 1.0f ? y : std::copysign (1.0f + 0.5f * fastTanh (2.0f * (a - 1.0f)), y);
}

void Voice::prepare (double osRate)
{
    sr = osRate;
    ampEnv.setSampleRate (osRate);
    filtEnv.setSampleRate (osRate);
    env3.setSampleRate (osRate);
    for (auto& k : ks) { k.buf.assign ((size_t) (osRate / 15.0) + 8, 0.0f); k.pos = 0; k.lp = 0; k.excite = 0; }
    kill();
}

void Voice::setEnvelopes (const juce::ADSR::Parameters& amp, const juce::ADSR::Parameters& filtParams,
                          const juce::ADSR::Parameters& env3Params)
{
    ampEnv.setParameters (amp);
    filtEnv.setParameters (filtParams);
    env3.setParameters (env3Params);
}

void Voice::excite()
{
    // pluck: one period of noise into each resonator
    const int period = juce::jlimit (8, (int) ks[0].buf.size() - 4, (int) (sr / juce::jmax (20.0f, targetFreq)));
    for (auto& k : ks) k.excite = period;
}

float Voice::special (int osc, int type, float freq, float w)
{
    if (type == otResonator)
    {
        auto& k = ks[osc];
        const int len = (int) k.buf.size();
        const float d = juce::jlimit (2.0f, (float) len - 3.0f, (float) sr / juce::jmax (20.0f, freq) - 0.5f);
        float rp = (float) k.pos - d;
        if (rp < 0.0f) rp += (float) len;
        const int i0 = juce::jlimit (0, len - 1, (int) rp); // float rounding can yield exactly len -> clamp
        const float fr = rp - (float) i0;
        const float y = k.buf[(size_t) i0] + (k.buf[(size_t) ((i0 + 1) % len)] - k.buf[(size_t) i0]) * fr;
        const float bright = 0.12f + 0.86f * w;                 // SHAPE = brightness of the string/pipe
        k.lp += (y - k.lp) * bright;
        float exc = (rng.nextFloat() * 2.0f - 1.0f) * 0.012f;   // gentle "bowing" keeps it singing
        if (k.excite > 0) { exc += (rng.nextFloat() * 2.0f - 1.0f); --k.excite; }
        k.buf[(size_t) k.pos] = k.lp * 0.9985f + exc;
        if (++k.pos >= len) k.pos = 0;
        return y * 1.4f;
    }
    // Noise: white -> pitched band-pass noise as SHAPE increases
    const float white = rng.nextFloat() * 2.0f - 1.0f;
    const float kq = 2.0f - 1.96f * w;
    noiseSvf[osc].process (white, std::tan (pi * juce::jlimit (20.0f, (float) sr * 0.45f, freq) / (float) sr), kq);
    return white * (1.0f - w) * 0.6f + noiseSvf[osc].bp * std::sqrt (kq) * 2.0f * w;
}

void Voice::noteOn (int newNote, float vel, bool legato)
{
    note = newNote;
    velocity = vel;
    targetFreq = (float) juce::MidiMessage::getMidiNoteInHertz (newNote);

    if (legato && active && ! releasing)
        return; // glide only, envelopes keep running

    if (! active)
    {
        curFreq = targetFreq;
        for (int i = 0; i < maxUnison; ++i)
        {
            ph1[i] = i == 0 ? 0.0f : rng.nextFloat();
            ph2[i] = rng.nextFloat();
        }
        for (auto& st : filt) for (auto& s : st) s.reset();
        for (auto& c : formant) for (auto& s : c) s.reset();
        for (auto& c : ladder) for (auto& st : c) st = 0.0f;
        for (auto& k : ks) { std::fill (k.buf.begin(), k.buf.end(), 0.0f); k.lp = 0.0f; }
        dcX[0] = dcX[1] = dcY[0] = dcY[1] = 0;
    }
    active = true;
    releasing = false;
    ctrlCount = 0;
    noteTime = 0.0f;
    ampEnv.noteOn();
    filtEnv.noteOn();
    env3.noteOn();
    excite();
}

void Voice::retrigger()
{
    if (active && ! releasing)
    {
        ampEnv.noteOn();
        filtEnv.noteOn();
        env3.noteOn();
        noteTime = 0.0f;
        excite();
    }
}

void Voice::noteOff()
{
    releasing = true;
    ampEnv.noteOff();
    filtEnv.noteOff();
    env3.noteOff();
}

void Voice::kill()
{
    active = releasing = false;
    ampEnv.reset();
    filtEnv.reset();
    env3.reset();
}

float Voice::shape (float t, float dt, float w, int oscType) const
{
    if (oscType == otPD)
    {
        // Casio-CZ style phase distortion: bending the phase of a cosine
        const float d = 0.5f - w * 0.47f;
        const float tw = t < d ? 0.5f * t / d : 0.5f + 0.5f * (t - d) / (1.0f - d);
        return -std::cos (twoPi * tw);
    }
    if (oscType > 0 && oscType < otPD)
        return WavetableBank::get().lookup (oscType - 1, w, t, dt);

    // Wavetable-style morph: saw -> square -> narrow pulse
    const float saw = 2.0f * t - 1.0f - polyBlep (t, dt);
    const float pw = w < 0.5f ? 0.5f : 0.5f - (w - 0.5f) * 0.8f;
    float t2 = t + 1.0f - pw; t2 -= std::floor (t2);
    const float sq = (t < pw ? 1.0f : -1.0f) + polyBlep (t, dt) - polyBlep (t2, dt);
    return w < 0.5f ? saw + (sq - saw) * (w * 2.0f) : sq;
}

float Voice::damage (float x, int ch, const Frame& f)
{
    auto sat = [&] (float v)
    {
        if (f.sat <= 0.0f) return v;
        const float g = 1.0f + f.sat * 10.0f, bias = f.sat * 0.3f;
        float y;
        switch (f.dtype)
        {
            case 1: // Fuzz: exponential, gated-ish asymmetric
            {
                const float u = v * g * 1.5f + bias;
                y = (u >= 0.0f ? 1.0f - std::exp (-u) : -0.8f * (1.0f - std::exp (u * 1.4f)));
                break;
            }
            case 2: // Rectify: blends in full-wave rectification (octave-up grit)
            {
                const float t = fastTanh (v * g);
                y = t + (std::abs (t) * 1.4f - 0.7f - t) * juce::jmin (1.0f, f.sat * 1.2f);
                break;
            }
            case 3: // Diode: hard asymmetric clipping
            {
                const float u = v * g;
                y = juce::jlimit (-1.0f, 0.65f, u - 0.15f * u * u * u * (u > 0 ? 1.0f : 0.3f));
                break;
            }
            case 4: // Sine shaper: wraps around -> bright, glassy overtones
                y = std::sin (v * g * 0.9f);
                break;
            case 5: // Stairs: quantised transfer curve (lo-fi steps)
            {
                const float steps = 3.0f + (1.0f - f.sat) * 12.0f;
                y = std::round (fastTanh (v * g) * steps) / steps;
                break;
            }
            default: // Tube: asymmetric tanh
                y = fastTanh (v * g + bias) - fastTanh (bias);
        }
        return v + (y - v) * juce::jmin (1.0f, f.sat * 4.0f);
    };
    auto fold = [&] (float v)
    {
        if (f.fold <= 0.0f) return v;
        const float y = std::sin (v * (1.0f + f.fold * 5.0f) * pi * 0.5f);
        return v + (y - v) * juce::jmin (1.0f, f.fold * 4.0f);
    };
    auto clip = [&] (float v)
    {
        if (f.clip <= 0.0f) return v;
        return juce::jlimit (-1.0f, 1.0f, v * (1.0f + f.clip * 4.0f));
    };

    const bool clean = f.sat <= 0.0f && f.fold <= 0.0f && f.clip <= 0.0f && f.crush <= 0.0f && f.preGain <= 1.001f;
    if (clean) return softLimit (x); // clean path: no shaping at all

    x *= f.preGain;
    switch (f.dorder)
    {
        case 1:  x = clip (sat (fold (x))); break;
        case 2:  x = sat (fold (clip (x))); break;
        case 3:  x = fold (clip (sat (x))); break;
        default: x = clip (fold (sat (x))); break;
    }

    if (f.crush > 0.0f)
    {
        if (ch == 0)
        {
            crushCount += 1.0f;
            if (crushCount >= 1.0f + f.crush * f.crush * 24.0f) crushCount = 0.0f;
        }
        if (crushCount == 0.0f)
        {
            const float levels = std::exp2 (12.0f - f.crush * 9.0f);
            crushHold[ch] = std::round (x * levels) / levels;
        }
        x = crushHold[ch];
    }

    // DC blocker (asymmetric saturation creates offset)
    const float y = x - dcX[ch] + 0.995f * dcY[ch];
    dcX[ch] = x; dcY[ch] = y;
    return softLimit (y);
}

void Voice::updateControl (const Frame& frameIn, const Mods& m, float fe)
{
    // --- Mod matrix: per-voice sources -> offsets on a local copy of the frame ---
    Frame& f = cf;
    f = frameIn;
    float off[numModDst] {};
    for (int i = 0; i < numModSlots; ++i)
    {
        if (f.modSrc[i] == srcOff || f.modAmt[i] == 0.0f) continue;
        float v = 0.0f;
        switch (f.modSrc[i])
        {
            case srcLfo1:       v = m.lfo1; break;
            case srcLfo2:       v = m.lfo2; break;
            case srcFiltEnv:    v = fe; break;
            case srcVelocity:   v = velocity; break;
            case srcModWheel:   v = m.modWheel; break;
            case srcAftertouch: v = m.aftertouch; break;
            case srcSeqF:       v = m.seqF; break;
            case srcAccent:     v = m.accent; break;
            case srcRandom:     v = m.stepRandom; break;
            case srcEnv3:       v = env3Val; break;
            case srcKey:        v = (float) (note - 60) / 24.0f; break;
            default: break;
        }
        off[juce::jlimit (0, numModDst - 1, f.modDst[i])] += v * f.modAmt[i];
    }
    f.cutoff *= std::exp2 (off[dstCutoff] * 4.0f);
    f.reso = juce::jlimit (0.0f, 0.985f, f.reso + off[dstReso] * 0.5f);
    f.morph += off[dstChar];
    f.fm = juce::jlimit (0.0f, 1.0f, f.fm + off[dstFm] * 0.6f);
    f.preGain *= std::exp2 (off[dstDrive] * 2.0f);
    f.fdrive = juce::jlimit (0.0f, 1.0f, f.fdrive + off[dstDrive] * 0.5f);
    f.fold = juce::jlimit (0.0f, 1.0f, f.fold + off[dstFold]);
    f.o1wave = juce::jlimit (0.0f, 1.0f, f.o1wave + off[dstShape]);
    f.o2wave = juce::jlimit (0.0f, 1.0f, f.o2wave + off[dstShape]);
    f.o2ratio *= std::exp2 (off[dstO2Pitch]);
    f.pitchBendSemis += off[dstPitch] * 12.0f;
    f.f2cut *= std::exp2 (off[dstCut2] * 4.0f);
    f.drift = juce::jlimit (0.0f, 1.0f, f.drift + off[dstDrift]);
    f.fmfb = juce::jlimit (0.0f, 1.0f, f.fmfb + off[dstFmFb]);
    f.sat = juce::jlimit (0.0f, 1.0f, f.sat + off[dstSat]);
    f.detuneCents = juce::jmax (0.0f, f.detuneCents * (1.0f + off[dstDetune] * 2.0f));
    f.sub = juce::jlimit (0.0f, 1.0f, f.sub + off[dstSub]);
    f.noise = juce::jlimit (0.0f, 1.0f, f.noise + off[dstNoise]);
    f.o2mix = juce::jlimit (0.0f, 1.0f, f.o2mix + off[dstO2Level]);
    f.penvAmt += off[dstPenv] * 24.0f;
    const float panMod = juce::jlimit (-1.0f, 1.0f, off[dstPan]);
    cPanL = std::sqrt (1.0f - panMod);
    cPanR = std::sqrt (1.0f + panMod);
    cAmpMod = juce::jmax (0.0f, 1.0f + off[dstAmp]);
    // analog drift: slow random walks for pitch (per unison voice) and cutoff
    {
        constexpr float a = 0.0002f, sigma = 0.02f;
        for (int i = 0; i < f.unison; ++i) driftP[i] += -driftP[i] * a + sigma * (rng.nextFloat() * 2.0f - 1.0f) * 1.7f;
        driftF += -driftF * a + sigma * (rng.nextFloat() * 2.0f - 1.0f) * 1.7f;
    }
    // pitch envelope: exponential drop from penvAmt semitones (Schranz pitch impacts, lasers, kicks)
    const float penv = f.penvAmt != 0.0f ? f.penvAmt * std::exp (-noteTime / juce::jmax (0.001f, f.penvDecay)) : 0.0f;
    noteTime += 8.0f / (float) sr;
    cPitchMul = std::exp2 ((f.pitchBendSemis + m.pitch + penv) / 12.0f);

    // Character position: saw lead -> FM metal -> formant scream
    const float c = juce::jlimit (0.0f, 1.0f, f.morph + m.seqC * f.seqDepth * 0.6f + m.lfo2 * f.lfo2Amt * 0.5f);
    cFmMix = juce::jlimit (0.0f, 1.0f, f.fm * 0.5f + (c < 0.5f ? c * 2.0f : 1.0f - (c - 0.5f) * 1.2f));
    cFormMix = juce::jlimit (0.0f, 1.0f, (c - 0.5f) * 2.0f);
    cIndex = juce::jmax (0.0f, 1.0f + 10.0f * f.fm + 6.0f * m.seqM * f.seqDepth + m.accent * f.accentAmt * 4.0f);
    if (cFormMix > 0.001f)
    {
        const float vp = juce::jmin (2.999f, cFormMix * 3.0f);
        const int vi = (int) vp;
        const float fr = vp - (float) vi;
        for (int k = 0; k < 3; ++k)
        {
            const float hz = vowels[vi][k] + (vowels[vi + 1][k] - vowels[vi][k]) * fr;
            cFormG[k] = std::tan (pi * juce::jmin (hz, (float) sr * 0.45f) / (float) sr);
        }
    }

    // unison layout
    const int n = f.unison;
    static constexpr float superPos[7] { -1.0f, -0.62f, -0.28f, 0.0f, 0.3f, 0.64f, 0.97f }; // JP-style uneven spread
    float gainSum = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        const float pos = f.superSaw ? superPos[i] : (n > 1 ? (float) i / (float) (n - 1) * 2.0f - 1.0f : 0.0f);
        const float driftCents = f.drift * 12.0f * driftP[i];
        cUniRatio[i] = std::exp2 ((pos * f.detuneCents + driftCents) / 1200.0f);
        const float pan = pos * f.spread;
        const float gainU = (f.superSaw && i != 3) ? 0.72f : 1.0f; // centre voice dominates in supersaw mode
        cUniL[i] = std::sqrt (0.5f * (1.0f - pan)) * gainU;
        cUniR[i] = std::sqrt (0.5f * (1.0f + pan)) * gainU;
        gainSum += gainU * gainU;
    }
    cNorm = 1.0f / (std::sqrt (gainSum) * (1.0f + f.o2mix * 0.7f));

    if (f.f2mode > 0)
    {
        cG2 = std::tan (pi * juce::jlimit (20.0f, (float) sr * 0.45f, f.f2cut) / (float) sr);
        cK2 = 2.0f - 2.0f * juce::jmin (0.97f, f.f2reso);
    }

    // filter
    const float oct = f.fenv * fe * 6.0f + m.lfo1 * f.lfo1Amt * 3.0f + m.seqF * f.seqDepth * 3.0f
                    + f.fkey * (float) (note - 60) / 12.0f + m.accent * f.accentAmt * 1.2f
                    + f.drift * 0.25f * driftF;
    const float fc = juce::jlimit (20.0f, (float) sr * 0.45f, f.cutoff * std::exp2 (oct));
    lastFc = fc;
    cG = std::tan (pi * fc / (float) sr);
    cGl = 1.0f - std::exp (-twoPi * fc / (float) sr);
    cReso = juce::jmin (0.985f, f.reso + m.accent * f.accentAmt * 0.2f);
    cK = 2.0f - 2.0f * cReso;
    cDrive = 1.0f + f.fdrive * 5.0f;
    cGainStatic = (0.5f + 0.5f * velocity) * (1.0f + m.accent * f.accentAmt * 0.4f) * 0.22f * cAmpMod;
}

void Voice::render (const Frame& frameIn, const Mods& m, float& outL, float& outR)
{
    if (! active) return;

    const float fe = filtEnv.getNextSample();
    const float amp = ampEnv.getNextSample();
    env3Val = env3.getNextSample();
    if (! ampEnv.isActive()) { active = false; return; }

    // control-rate update (every 8 oversampled samples, ~0.1 ms)
    if ((ctrlCount++ & 7) == 0) updateControl (frameIn, m, fe);
    const Frame& f = cf;

    curFreq += (targetFreq - curFreq) * f.glideCoef;
    const float base = curFreq * cPitchMul;
    const float invSr = 1.0f / (float) sr;

    // --- Core oscillators with unison (resonator/noise types run once, centred) ---
    const bool sp1 = isSpecial (f.oscType), sp2 = isSpecial (f.osc2Type);
    float coreL = 0, coreR = 0;
    const int n = f.unison;
    for (int i = 0; i < n; ++i)
    {
        const float f1 = base * cUniRatio[i];
        const float dt1 = juce::jmin (0.45f, f1 * invSr), dt2 = juce::jmin (0.45f, f1 * f.o2ratio * invSr);

        ph1[i] += dt1;
        const bool wrapped = ph1[i] >= 1.0f;
        if (wrapped) ph1[i] -= 1.0f;
        ph2[i] += dt2;
        if (f.sync && wrapped) ph2[i] = ph1[i] * (dt2 / dt1);
        ph2[i] -= std::floor (ph2[i]);

        const float s1 = sp1 ? 0.0f : shape (ph1[i], dt1, f.o1wave, f.oscType);
        const float s2 = sp2 ? 0.0f : shape (ph2[i], dt2, f.o2wave, f.osc2Type);
        const float v = s1 + s2 * f.o2mix + f.ring * s1 * s2;
        coreL += v * cUniL[i];
        coreR += v * cUniR[i];
    }
    coreL *= cNorm; coreR *= cNorm;
    if (sp1 || sp2)
    {
        const float a = sp1 ? special (0, f.oscType, base, f.o1wave) : 0.0f;
        const float b = sp2 ? special (1, f.osc2Type, base * f.o2ratio, f.o2wave) * f.o2mix : 0.0f;
        const float c = (a + b) * 0.8f / (1.0f + f.o2mix * 0.7f);
        coreL += c; coreR += c;
    }

    float sL = coreL, sR = coreR;

    // --- FM layer (phase modulation, metallic) ---
    if (cFmMix > 0.001f)
    {
        phm += base * f.o2ratio * invSr;
        phm -= std::floor (phm);
        const float mod = std::sin (twoPi * phm + f.fmfb * 2.6f * lastMod); // self-feedback -> harsher, saw-like modulator
        lastMod = mod;
        const float fmS = std::sin (twoPi * ph1[0] + cIndex * mod);
        sL += (fmS - coreL) * cFmMix;
        sR += (fmS - coreR) * cFmMix;
    }

    // --- Formant layer (screaming vowels) ---
    if (cFormMix > 0.001f)
    {
        float outs[2] {};
        const float in[2] { sL, sR };
        for (int k = 0; k < 3; ++k)
            for (int chn = 0; chn < 2; ++chn)
            {
                formant[chn][k].process (in[chn], cFormG[k], 0.12f);
                outs[chn] += formant[chn][k].bp * formantGain[k] * 0.35f;
            }
        sL += (outs[0] - sL) * cFormMix;
        sR += (outs[1] - sR) * cFormMix;
    }

    // --- Sub & noise ---
    phSub += base * 0.5f * invSr;
    phSub -= std::floor (phSub);
    const float sub = (phSub < 0.5f ? 1.0f : -1.0f) * f.sub * 0.7f;
    const float nz = f.noise > 0.0f ? (rng.nextFloat() * 2.0f - 1.0f) * f.noise * 0.5f : 0.0f;
    sL += sub + nz; sR += sub + nz;

    // --- Iron filter (+ optional Filter 2, serial or parallel; drive pre or post) ---
    float io[2] { sL, sR };
    const float preDrive = f.drvPost ? 1.0f : cDrive;
    for (int chn = 0; chn < 2; ++chn)
    {
        const float xin = io[chn];
        const float xd = preDrive > 1.001f ? fastTanh (xin * preDrive) : xin; // clean when DRIVE = 0
        float y;
        switch (f.fmode)
        {
            case 0:
            {
                // Non-linear 4-pole transistor ladder: resonance eats the lows, tanh per stage
                auto& st = ladder[chn];
                const float fbk = cReso * 4.1f;
                // half-scaled tanh: nearly linear at normal levels (clean), saturates only when pushed
                float in = 2.0f * fastTanh (0.5f * (xin * preDrive - fbk * (st[3] - 0.25f * xin)));
                for (int i = 0; i < 4; ++i)
                {
                    st[i] += cGl * (2.0f * fastTanh (0.5f * in) - 2.0f * fastTanh (0.5f * st[i]));
                    in = st[i];
                }
                y = st[3] * (1.0f + cReso * 1.5f); // partial passband gain compensation
                break;
            }
            case 1:  filt[1][chn].process (xd, cG, cK); y = filt[1][chn].lp; break;
            case 2:  filt[1][chn].process (xd, cG, cK); y = filt[1][chn].bp * (0.3f + cK); break;
            default: filt[1][chn].process (xd, cG, cK); y = filt[1][chn].hp; break;
        }
        // saturating resonance: keeps screaming peaks under control
        filt[1][chn].ic1 = 2.0f * fastTanh (filt[1][chn].ic1 * 0.5f);

        if (f.f2mode > 0)
        {
            auto& f2 = filt[0][chn];
            f2.process (f.f2route == 0 ? y : xd, cG2, cK2);
            f2.ic1 = 2.0f * fastTanh (f2.ic1 * 0.5f);
            float y2;
            switch (f.f2mode)
            {
                case 1:  y2 = f2.lp; break;
                case 2:  y2 = f2.bp * (0.3f + cK2); break;
                case 3:  y2 = f2.hp; break;
                default: y2 = f2.lp + f2.hp; break; // notch
            }
            y = f.f2route == 0 ? y2 : 0.6f * (y + y2);
        }
        if (f.drvPost && cDrive > 1.001f) y = fastTanh (y * cDrive);
        io[chn] = damage (y, chn, f);
    }

    const float gain = amp * m.gate * cGainStatic;
    outL += io[0] * gain * cPanL;
    outR += io[1] * gain * cPanR;
}
} // namespace ic
