#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"
#include "Wavetables.h"

using namespace ic;

IroncoreProcessor::IroncoreProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "IRONCORE", createLayout())
{
    heldNotes.ensureStorageAllocated (128);

    for (int pat = 0; pat < numPatterns; ++pat)
        for (int l = 0; l < numLanes; ++l)
        {
            patLen (pat, l).store (numSteps);
            for (int st = 0; st < numSteps; ++st)
                patVal (pat, l, st).store (apvts.getRawParameterValue (stepId (seqLanes[l], st))->load());
        }
    for (auto lane : seqLanes)
    {
        apvts.addParameterListener (laneLenId (lane), this);
        for (int st = 0; st < numSteps; ++st)
            apvts.addParameterListener (stepId (lane, st), this);
    }
    startTimerHz (30);
}

//==============================================================================
void IroncoreProcessor::parameterChanged (const juce::String& id, float v)
{
    const int pat = activePattern.load();
    if (id.startsWith ("len"))
        patLen (pat, laneIndex ((char) id[3])).store (juce::roundToInt (v));
    else
        patVal (pat, laneIndex ((char) id[0]), id.substring (2).getIntValue()).store (v);
}

void IroncoreProcessor::timerCallback()
{
    const int pending = pendingPatternLoad.exchange (-1);
    if (pending >= 0) loadPatternIntoParams (pending);
}

void IroncoreProcessor::loadPatternIntoParams (int index)
{
    const juce::ScopedValueSetter<bool> guard (loadingPattern, true);

    for (int l = 0; l < numLanes; ++l)
    {
        if (auto* p = apvts.getParameter (laneLenId (seqLanes[l])))
            p->setValueNotifyingHost (p->convertTo0to1 ((float) patLen (index, l).load()));
        for (int st = 0; st < numSteps; ++st)
            if (auto* p = apvts.getParameter (stepId (seqLanes[l], st)))
                p->setValueNotifyingHost (patVal (index, l, st).load());
    }
}

void IroncoreProcessor::selectPattern (int index)
{
    index = juce::jlimit (0, numPatterns - 1, index);
    activePattern.store (index);
    pendingPatternLoad.store (-1);
    loadPatternIntoParams (index);
}

void IroncoreProcessor::copyPattern()
{
    const int pat = activePattern.load();
    for (int l = 0; l < numLanes; ++l)
    {
        clipboardLen[(size_t) l] = patLen (pat, l).load();
        for (int st = 0; st < numSteps; ++st) clipboard[(size_t) (l * numSteps + st)] = patVal (pat, l, st).load();
    }
    clipboardValid = true;
}

void IroncoreProcessor::pastePattern()
{
    if (! clipboardValid) return;
    const int pat = activePattern.load();
    for (int l = 0; l < numLanes; ++l)
    {
        patLen (pat, l).store (clipboardLen[(size_t) l]);
        for (int st = 0; st < numSteps; ++st) patVal (pat, l, st).store (clipboard[(size_t) (l * numSteps + st)]);
    }
    loadPatternIntoParams (pat);
}

void IroncoreProcessor::clearPattern()
{
    const int pat = activePattern.load();
    for (int l = 0; l < numLanes; ++l)
    {
        const char lane = seqLanes[l];
        patLen (pat, l).store (numSteps);
        for (int st = 0; st < numSteps; ++st)
            patVal (pat, l, st).store (isToggleLane (lane) ? 0.0f : lane == 'g' ? 1.0f : 0.5f);
    }
    loadPatternIntoParams (pat);
}

void IroncoreProcessor::fillAllPatternsFromActive()
{
    const int src = activePattern.load();
    for (int pat = 0; pat < numPatterns; ++pat)
        if (pat != src)
            for (int l = 0; l < numLanes; ++l)
            {
                patLen (pat, l).store (patLen (src, l).load());
                for (int st = 0; st < numSteps; ++st) patVal (pat, l, st).store (patVal (src, l, st).load());
            }
}

void IroncoreProcessor::writePatternsToState (juce::ValueTree& state)
{
    state.removeChild (state.getChildWithName ("PATTERNS"), nullptr);
    juce::ValueTree pats ("PATTERNS");
    for (int pat = 0; pat < numPatterns; ++pat)
    {
        juce::ValueTree pt ("P");
        juce::StringArray vals, lens;
        for (int l = 0; l < numLanes; ++l)
        {
            lens.add (juce::String (patLen (pat, l).load()));
            for (int st = 0; st < numSteps; ++st) vals.add (juce::String (patVal (pat, l, st).load(), 4));
        }
        pt.setProperty ("v", vals.joinIntoString (","), nullptr);
        pt.setProperty ("l", lens.joinIntoString (","), nullptr);
        pats.appendChild (pt, nullptr);
    }
    state.appendChild (pats, nullptr);
    state.setProperty ("activePattern", activePattern.load(), nullptr);
}

void IroncoreProcessor::readPatternsFromState (const juce::ValueTree& state)
{
    auto pats = state.getChildWithName ("PATTERNS");
    const int active = juce::jlimit (0, numPatterns - 1, (int) state.getProperty ("activePattern", 0));
    activePattern.store (active);
    if (! pats.isValid())
    {
        fillAllPatternsFromActive(); // older state: all patterns = current steps
        return;
    }
    for (int pat = 0; pat < juce::jmin (numPatterns, pats.getNumChildren()); ++pat)
    {
        auto pt = pats.getChild (pat);
        juce::StringArray vals, lens;
        vals.addTokens (pt.getProperty ("v").toString(), ",", "");
        lens.addTokens (pt.getProperty ("l").toString(), ",", "");
        for (int l = 0; l < numLanes; ++l)
        {
            if (l < lens.size()) patLen (pat, l).store (juce::jlimit (1, numSteps, lens[l].getIntValue()));
            for (int st = 0; st < numSteps; ++st)
                if (l * numSteps + st < vals.size()) patVal (pat, l, st).store (vals[l * numSteps + st].getFloatValue());
        }
    }
    pendingPatternLoad.store (active); // push the active pattern into the step parameters
}

bool IroncoreProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void IroncoreProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    maxBlock = juce::jmax (1, samplesPerBlock);
    chunkMidi.ensureSize (4096);
    WavetableBank::get(); // build tables up front, not on the audio thread
    os2x = std::make_unique<juce::dsp::Oversampling<float>> (2, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true);
    os4x = std::make_unique<juce::dsp::Oversampling<float>> (2, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true);
    os2x->initProcessing ((size_t) samplesPerBlock);
    os4x->initProcessing ((size_t) samplesPerBlock);
    setOversampling (val ("quality") > 0.5f ? 4 : 2);
    heldNotes.clearQuick();

    delayBuf.setSize (2, (int) (sr * 4.0) + 1);
    delayBuf.clear();
    delayWrite = 0;
    fxBuf.setSize (2, samplesPerBlock);
    revIn.setSize (2, samplesPerBlock);
    revOut.setSize (2, samplesPerBlock);
    verb.prepare (sr);
    postFx.prepare (sr);
    charFx.prepare (sr);

    freeBeats = 0.0;
    lastAbsStep = -1;
}

void IroncoreProcessor::panic()
{
    for (auto& v : voices) v.kill();
    heldNotes.clearQuick();
    delayBuf.clear();
    verb.reset();
    charFx.reset();
    if (oversampling) oversampling->reset();
    pitchBend = 0.0f;
}

void IroncoreProcessor::setOversampling (int factor)
{
    osFactor = factor;
    oversampling = factor == 4 ? os4x.get() : os2x.get();
    oversampling->reset();
    osRate = sr * factor;
    setLatencySamples (juce::roundToInt (oversampling->getLatencyInSamples()));
    for (auto& v : voices) v.prepare (osRate);
}

//==============================================================================
float IroncoreProcessor::stepValue (char lane, int step, int cycle) const
{
    float v = const_cast<IroncoreProcessor*> (this)->patVal (activePattern.load(), laneIndex (lane), step).load();
    const float mutate = val ("mutate");
    if (mutate > 0.0f && lane != 'p' && lane != 'g' && lane != 's')
    {
        // deterministic hash -> repeatable mutation per (step, lane, cycle)
        uint32_t h = (uint32_t) step * 374761393u ^ (uint32_t) (unsigned char) lane * 668265263u ^ (uint32_t) cycle * 2246822519u;
        h = (h ^ (h >> 13)) * 1274126177u;
        h ^= h >> 16;
        const float r = (float) (h & 0xffff) / 65535.0f;
        if (lane == 'a')
            return (r < mutate * 0.3f) ? 1.0f - v : v;
        v = juce::jlimit (0.0f, 1.0f, v + (r * 2.0f - 1.0f) * mutate * 0.5f);
    }
    return v;
}

void IroncoreProcessor::buildFrame (double)
{
    const float heat = val ("heat"), bite = val ("bite"), mov = val ("movement"), dens = val ("density");
    auto& f = frame;
    f.o1wave = val ("o1wave");
    f.o2wave = val ("o2wave");
    f.o2ratio = std::exp2 ((val ("o2semi") + val ("o2fine") / 100.0f) / 12.0f);
    f.o2mix = val ("o2mix");
    f.sync = val ("sync") > 0.5f;
    f.fm = val ("fm");
    f.ring = val ("ring");
    f.sub = juce::jmin (1.0f, val ("sub") + dens * 0.2f);
    f.noise = val ("noise");

    f.cutoff = val ("cutoff") * std::exp2 (bite * 1.2f);
    f.reso = juce::jmin (0.98f, val ("reso") + bite * 0.35f);
    f.fmode = (int) val ("fmode");
    f.fdrive = juce::jmin (1.0f, val ("fdrive") + heat * 0.6f);
    f.fenv = val ("fenv");
    f.fkey = val ("fkey");

    f.preGain = 1.0f + heat * 3.0f;
    f.sat = juce::jmin (1.0f, val ("sat") + heat * 0.7f);
    f.fold = juce::jmin (1.0f, val ("fold") + heat * heat * 0.4f);
    f.clip = juce::jmin (1.0f, val ("clip") + bite * 0.3f);
    f.crush = val ("crush");
    f.dorder = (int) val ("dorder");

    f.morph = val ("morph");
    f.superSaw = val ("unimode") > 0.5f;
    f.unison = f.superSaw ? 7 : 1 + juce::roundToInt (dens * 4.0f);
    f.detuneCents = f.superSaw ? 4.0f + std::pow (dens, 1.5f) * 55.0f : 5.0f + dens * 30.0f;
    f.drift = val ("drift");
    f.fmfb = val ("fmfb");
    f.dtype = (int) val ("dtype");
    f.drvPost = val ("drvpos") > 0.5f;
    f.f2mode = (int) val ("f2mode");
    f.f2route = (int) val ("f2route");
    f.f2cut = val ("f2cut");
    f.f2reso = val ("f2reso");
    f.spread = dens;

    const float glide = val ("glide");
    f.glideCoef = glide < 0.001f ? 1.0f : 1.0f - std::exp (-1.0f / (glide * (float) osRate));
    f.pitchBendSemis = pitchBend * 2.0f;

    f.lfo1Amt = val ("l1amt") * (0.5f + mov);
    f.lfo2Amt = val ("l2amt") * (0.5f + mov);
    f.seqDepth = val ("seqon") > 0.5f ? juce::jmin (1.5f, val ("seqdepth") * (0.25f + 1.5f * mov)) : 0.0f;
    f.accentAmt = val ("accent");
    f.oscType = (int) val ("osctype");
    f.osc2Type = (int) val ("osc2type");
    f.penvAmt = val ("penv");
    f.penvDecay = val ("pdecay");
    for (int i = 0; i < numModSlots; ++i)
    {
        f.modSrc[i] = (int) apvts.getRawParameterValue (modId ("src", i))->load();
        f.modDst[i] = (int) apvts.getRawParameterValue (modId ("dst", i))->load();
        f.modAmt[i] = apvts.getRawParameterValue (modId ("amt", i))->load();
    }

    juce::ADSR::Parameters amp { val ("aa"), val ("ad"), val ("as"), val ("ar") };
    juce::ADSR::Parameters fil { val ("fa"), val ("fd"), val ("fs"), val ("fr") };
    juce::ADSR::Parameters e3 { val ("env3a"), val ("env3d"), val ("env3s"), val ("env3r") };
    for (auto& v : voices) v.setEnvelopes (amp, fil, e3);
}

void IroncoreProcessor::handleMidi (const juce::MidiMessage& msg)
{
    const bool mono = val ("vmode") < 0.5f;

    // Notes 0-7 (C-2 .. G-2) switch the sequencer pattern instead of playing
    if ((msg.isNoteOn() || msg.isNoteOff()) && msg.getNoteNumber() < numPatterns)
    {
        if (msg.isNoteOn())
        {
            activePattern.store (msg.getNoteNumber());
            pendingPatternLoad.store (msg.getNoteNumber());
        }
        return;
    }

    if (msg.isNoteOn())
    {
        const int n = msg.getNoteNumber();
        const float vel = msg.getFloatVelocity();
        const bool wasHeld = ! heldNotes.isEmpty();
        if (heldNotes.isEmpty())
        {
            freeBeats = 0.0; // free-running sequencer restarts with the phrase
            lastAbsStep = -1;
        }
        heldNotes.removeAllInstancesOf (n);
        heldNotes.add (n);

        if (mono)
        {
            voices[0].noteOn (n, vel, wasHeld);
            voices[0].age = ++voiceCounter;
            return;
        }

        Voice* target = nullptr;
        for (auto& v : voices) if (v.isActive() && v.getNote() == n) { target = &v; break; }
        if (! target) for (auto& v : voices) if (! v.isActive()) { target = &v; break; }
        if (! target)
        {
            uint64_t oldest = UINT64_MAX;
            for (auto& v : voices)
            {
                const uint64_t score = v.age - (v.isReleasing() ? (uint64_t) 1 << 40 : 0); // prefer releasing voices
                if (score < oldest) { oldest = score; target = &v; }
            }
            target->kill();
        }
        target->noteOn (n, vel, false);
        target->age = ++voiceCounter;
    }
    else if (msg.isNoteOff())
    {
        const int n = msg.getNoteNumber();
        heldNotes.removeAllInstancesOf (n);
        if (mono)
        {
            if (heldNotes.isEmpty()) voices[0].noteOff();
            else if (voices[0].getNote() == n) voices[0].noteOn (heldNotes.getLast(), 0.8f, true);
            return;
        }
        for (auto& v : voices)
            if (v.isActive() && ! v.isReleasing() && v.getNote() == n) v.noteOff();
    }
    else if (msg.isController() && msg.getControllerNumber() == 1)
    {
        modWheel = (float) msg.getControllerValue() / 127.0f;
    }
    else if (msg.isChannelPressure())
    {
        aftertouch = (float) msg.getChannelPressureValue() / 127.0f;
    }
    else if (msg.isPitchWheel())
    {
        pitchBend = (float) (msg.getPitchWheelValue() - 8192) / 8192.0f;
    }
    else if (msg.isAllNotesOff() || msg.isAllSoundOff())
    {
        heldNotes.clearQuick();
        for (auto& v : voices) v.kill();
    }
}

void IroncoreProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    // Hosts may send blocks larger than announced in prepareToPlay. The oversampler's buffers are
    // sized for maxBlock, so split oversized blocks into safe chunks instead of overrunning memory.
    const int total = buffer.getNumSamples();
    if (total <= maxBlock)
    {
        processChunk (buffer, midi, 0.0);
        return;
    }
    double beatOffset = 0.0;
    for (int start = 0; start < total; start += maxBlock)
    {
        const int n = juce::jmin (maxBlock, total - start);
        juce::AudioBuffer<float> sub (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), start, n);
        chunkMidi.clear();
        for (const auto meta : midi)
            if (meta.samplePosition >= start && meta.samplePosition < start + n)
                chunkMidi.addEvent (meta.data, meta.numBytes, meta.samplePosition - start);
        processChunk (sub, chunkMidi, beatOffset);
        beatOffset += n / sr * lastBpm / 60.0;
    }
}

void IroncoreProcessor::processChunk (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi, double beatOffset)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    buffer.clear();

    double bpm = 120.0, ppq = 0.0;
    bool playing = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = *b;
            if (auto p = pos->getPpqPosition()) ppq = *p + beatOffset;
            playing = pos->getIsPlaying();
        }

    if (! std::isfinite (bpm) || bpm < 20.0 || bpm > 400.0) bpm = 120.0;
    lastBpm = bpm;
    if (! std::isfinite (ppq)) ppq = 0.0;
    if (panicRequested.exchange (false)) panic();

    if (const int wanted = val ("quality") > 0.5f ? 4 : 2; wanted != osFactor)
        setOversampling (wanted); // switching quality resets voices

    buildFrame (bpm);

    // --- sequencer / LFO settings for this block ---
    const bool seqOn = val ("seqon") > 0.5f;
    const float stepBeats = seqRateBeats[(int) val ("seqrate")];
    const int seqLen = (int) val ("seqlen");
    const int evolve = evolveBars[(int) val ("evolve")];
    const int l1Idx = (int) val ("l1rate"), l2Idx = (int) val ("l2rate");
    const float l1Beats = syncRateBeats[juce::jmin (l1Idx, lfoFreeIndex - 1)];
    const float l2Beats = syncRateBeats[juce::jmin (l2Idx, lfoFreeIndex - 1)];
    lfoFree[0] = l1Idx == lfoFreeIndex; lfoFree[1] = l2Idx == lfoFreeIndex;
    lfoHz[0] = val ("l1hz"); lfoHz[1] = val ("l2hz");
    lfoShape[0] = (int) val ("l1shape"); lfoShape[1] = (int) val ("l2shape");
    const float lfoSmoothCoef = 1.0f - std::exp (-1.0f / (0.0015f * (float) osRate));
    const double beatsPerOsSample = bpm / 60.0 / osRate;
    const float smooth = 1.0f - std::exp (-1.0f / (0.002f * (float) osRate));
    const float gateCoef = 1.0f - std::exp (-1.0f / (0.0012f * (float) osRate));
    const float slideCoef = 1.0f - std::exp (-1.0f / (0.06f * (float) osRate));
    const bool retrig = val ("seqretrig") > 0.5f;
    const float split = 0.5f + val ("swing") * 0.25f;
    int laneLen[numLanes];
    for (int i = 0; i < numLanes; ++i)
        laneLen[i] = juce::jmin (seqLen, patLen (activePattern.load(), i).load());
    auto laneStep = [&] (int laneIdx, long long absStep)
    {
        const long long n = laneLen[laneIdx];
        return (int) (((absStep % n) + n) % n);
    };

    // --- render voices at 2x ---
    juce::dsp::AudioBlock<float> block (buffer);
    auto up = oversampling->processSamplesUp (block);
    up.clear();
    float* upL = up.getChannelPointer (0);
    float* upR = up.getNumChannels() > 1 ? up.getChannelPointer (1) : nullptr;

    double beat = playing ? ppq : freeBeats;
    const double beatStart = beat;
    auto midiIt = midi.cbegin();

    for (int s = 0; s < numSamples; ++s)
    {
        while (midiIt != midi.cend() && (*midiIt).samplePosition <= s)
        {
            const bool wasEmpty = heldNotes.isEmpty();
            handleMidi ((*midiIt).getMessage());
            if (! playing && wasEmpty && ! heldNotes.isEmpty()) { beat = 0.0; lastAbsStep = -1; }
            ++midiIt;
        }

        for (int o = 0; o < osFactor; ++o)
        {
            // sequencer step logic (swing: even steps longer, odd steps shorter)
            const double pair = beat / (2.0 * stepBeats);
            const double pairFloor = std::floor (pair);
            const double inPair = pair - pairFloor;
            const bool oddStep = inPair >= split;
            const long long absStep = (long long) pairFloor * 2 + (oddStep ? 1 : 0);
            const double inStep = oddStep ? (inPair - split) / (1.0 - split) : inPair / split;
            const int cycle = evolve > 0 ? (int) std::floor (beat / (4.0 * evolve)) : 0;
            if (absStep != lastAbsStep || cycle != lastCycle)
            {
                lastAbsStep = absStep;
                lastCycle = cycle;
                uiStep.store (seqOn ? (int) (((absStep % 720720) + 720720) % 720720) : -1);
                if (seqOn)
                {
                    seqTarget[0] = (stepValue ('f', laneStep (0, absStep), cycle) - 0.5f) * 2.0f;
                    seqTarget[1] = (stepValue ('m', laneStep (1, absStep), cycle) - 0.5f) * 2.0f;
                    seqTarget[2] = (stepValue ('c', laneStep (2, absStep), cycle) - 0.5f) * 2.0f;
                    pitchTarget = (float) pitchSemis (stepValue ('p', laneStep (3, absStep), cycle));
                    gateLen = stepValue ('g', laneStep (4, absStep), cycle);
                    seqTarget[3] = stepValue ('a', laneStep (5, absStep), cycle) > 0.5f ? 1.0f : 0.0f;
                    slideOn = stepValue ('s', laneStep (6, absStep), cycle) > 0.5f;
                    if (retrig && gateLen > 0.03f)
                        for (auto& v : voices) v.retrigger();
                }
                else
                {
                    seqTarget[0] = seqTarget[1] = seqTarget[2] = seqTarget[3] = 0.0f;
                    pitchTarget = 0.0f; gateLen = 1.0f; slideOn = false;
                }
                uint32_t h = (uint32_t) absStep * 2654435761u;
                h ^= h >> 15; h *= 2246822519u; h ^= h >> 13;
                stepRandom = (float) (h & 0xffff) / 32767.5f - 1.0f;
            }
            seqPitch = slideOn ? seqPitch + (pitchTarget - seqPitch) * slideCoef : pitchTarget;
            const bool open = gateLen >= 0.97f || (gateLen > 0.03f && inStep < gateLen);
            gateSmooth += ((open ? 1.0f : 0.0f) - gateSmooth) * gateCoef;
            for (int i = 0; i < 4; ++i)
                seqSmooth[i] += (seqTarget[i] - seqSmooth[i]) * (i == 3 ? smooth * 0.5f : smooth);

            Mods m;
            for (int li = 0; li < 2; ++li)
            {
                double ph;
                if (lfoFree[li]) { lfoFreePhase[li] += lfoHz[li] / osRate; lfoFreePhase[li] -= std::floor (lfoFreePhase[li]); ph = lfoFreePhase[li]; }
                else ph = beat / (li == 0 ? l1Beats : l2Beats);
                const double cyc = std::floor (ph);
                const float t = (float) (ph - cyc);
                float v;
                switch (lfoShape[li])
                {
                    case 1:  v = 1.0f - 4.0f * std::abs (t - 0.5f); break;            // triangle
                    case 2:  v = 2.0f * t - 1.0f; break;                               // saw
                    case 3:  v = t < 0.5f ? 1.0f : -1.0f; break;                        // square
                    case 4:                                                            // sample & hold
                        if ((long long) cyc != lfoLastCycle[li] || (lfoFree[li] && t < lfoPrevT[li])) { lfoLastCycle[li] = (long long) cyc; lfoHold[li] = lfoRng.nextFloat() * 2.0f - 1.0f; }
                        v = lfoHold[li]; break;
                    default: v = std::sin (t * juce::MathConstants<float>::twoPi);
                }
                lfoPrevT[li] = t;
                lfoSm[li] += (v - lfoSm[li]) * lfoSmoothCoef; // removes clicks from saw/square/S&H edges
            }
            m.lfo1 = lfoSm[0];
            m.lfo2 = lfoSm[1];
            m.seqF = seqSmooth[0]; m.seqM = seqSmooth[1]; m.seqC = seqSmooth[2]; m.accent = seqSmooth[3];
            m.pitch = seqPitch; m.gate = gateSmooth;
            m.modWheel = modWheel; m.aftertouch = aftertouch; m.stepRandom = stepRandom;

            float l = 0, r = 0;
            for (auto& v : voices) v.render (frame, m, l, r);
            if (! std::isfinite (l) || ! std::isfinite (r) || std::abs (l) > 50.0f || std::abs (r) > 50.0f)
            {
                for (auto& v : voices) v.kill(); // instability guard: never let garbage reach the output
                l = r = 0.0f;
                safetyResets.fetch_add (1);
            }

            const int idx = s * osFactor + o;
            if (upR) { upL[idx] = l; upR[idx] = r; }
            else upL[idx] = 0.5f * (l + r);
            beat += beatsPerOsSample;
        }
    }
    if (! playing) freeBeats = std::fmod (beat, 6144.0); // keeps LFO/step maths precise in long sessions
    {
        int n = 0;
        for (auto& v : voices) if (v.isActive()) ++n;
        uiVoices.store (n);
    }
    {
        const Voice* newest = nullptr;
        for (auto& v : voices) if (v.isActive() && (! newest || v.age > newest->age)) newest = &v;
        uiCutoff.store (newest ? newest->lastFc : frame.cutoff);
    }
    while (midiIt != midi.cend()) { handleMidi ((*midiIt).getMessage()); ++midiIt; }

    oversampling->processSamplesDown (block);
    dbgSynth.store (buffer.getMagnitude (0, numSamples));

    // --- Post EQ + stereo width (before the FX so delay/reverb inherit the tone) ---
    if (buffer.getNumChannels() > 1)
        postFx.process (buffer.getWritePointer (0), buffer.getWritePointer (1), numSamples,
                        val ("lowcut"), val ("mideq"), val ("tilt"), val ("width"), 0.0f, 0.0, 0.0, 1.0f);

    dbgPost.store (buffer.getMagnitude (0, numSamples));
    // --- Character FX (comb / phaser / freq shift / stutter), position chosen by FX ORDER ---
    const int cfxType = (int) val ("cfxtype");
    const int fxOrder = (int) val ("fxorder");
    const float cfxP1 = val ("cfxp1"), cfxP2 = val ("cfxp2"), cfxMix = val ("cfxmix");
    const double bpsBase = bpm / 60.0 / sr;
    auto runCharFx = [&] (float* l, float* r)
    {
        charFx.process (cfxType, l, r, numSamples, cfxP1, cfxP2, cfxMix, beatStart, bpsBase);
    };
    const bool stereo = buffer.getNumChannels() > 1;
    if (stereo && cfxType > 0 && fxOrder == 0)
        runCharFx (buffer.getWritePointer (0), buffer.getWritePointer (1));

    // --- FX: ping-pong delay + reverb ---
    const int nCh = buffer.getNumChannels();
    if (fxBuf.getNumSamples() < numSamples) fxBuf.setSize (2, numSamples, false, false, true);

    const float dLevel = val ("dlevel"), fb = val ("dfb"), fxMix = val ("fxmix");
    const int dSamples = juce::jlimit (1, delayBuf.getNumSamples() - 1,
                                       (int) (delayBeats[(int) val ("dtime")] * 60.0 / bpm * sr));
    const int dLen = delayBuf.getNumSamples();
    auto* dl = delayBuf.getWritePointer (0);
    auto* dr = delayBuf.getWritePointer (1);
    auto* inL = buffer.getReadPointer (0);
    auto* inR = buffer.getReadPointer (nCh > 1 ? 1 : 0);
    auto* fl = fxBuf.getWritePointer (0);
    auto* fr = fxBuf.getWritePointer (1);

    for (int s = 0; s < numSamples; ++s)
    {
        int rd = delayWrite - dSamples;
        if (rd < 0) rd += dLen;
        const float rL = dl[rd], rR = dr[rd];
        dl[delayWrite] = std::tanh (0.5f * (inL[s] + inR[s]) + rR * fb);
        dr[delayWrite] = std::tanh (rL * fb);
        if (++delayWrite >= dLen) delayWrite = 0;
        fl[s] = rL * dLevel; fr[s] = rR * dLevel;
    }

    if (revIn.getNumSamples() < numSamples) revIn.setSize (2, numSamples, false, false, true);
    if (revOut.getNumSamples() < numSamples) revOut.setSize (2, numSamples, false, false, true);
    revIn.copyFrom (0, 0, buffer, 0, 0, numSamples);
    revIn.copyFrom (1, 0, buffer, nCh > 1 ? 1 : 0, 0, numSamples);
    revIn.addFrom (0, 0, fxBuf, 0, 0, numSamples, 0.5f);
    revIn.addFrom (1, 0, fxBuf, 1, 0, numSamples, 0.5f);
    smoothedRevSize += (val ("rsize") - smoothedRevSize) * 0.05f; // avoid pitch sweeps from jumping delay lengths
    verb.setParameters (smoothedRevSize, val ("rdamp"), val ("rpre"));
    verb.process (revIn.getReadPointer (0), revIn.getReadPointer (1), revOut.getWritePointer (0), revOut.getWritePointer (1), numSamples);
    {
        // FX guard: if delay/reverb ever produce garbage, wipe their buffers instead of ringing on
        const float mag = revOut.getMagnitude (0, numSamples);
        if (! std::isfinite (mag) || mag > 8.0f) { verb.reset(); delayBuf.clear(); revOut.clear(); fxBuf.clear(); safetyResets.fetch_add (1); }
    }
    revOut.applyGain (val ("rlevel"));

    if (stereo && cfxType > 0 && fxOrder == 2) // character FX only on the wet (delay + reverb) signal
    {
        fxBuf.addFrom (0, 0, revOut, 0, 0, numSamples);
        fxBuf.addFrom (1, 0, revOut, 1, 0, numSamples);
        revOut.clear (0, numSamples);
        runCharFx (fxBuf.getWritePointer (0), fxBuf.getWritePointer (1));
    }

    const float outGain = juce::Decibels::decibelsToGain (val ("out"));
    const float pumpAmt = val ("pump");
    static constexpr float pumpBeatsTable[] { 1.0f, 2.0f, 0.5f };
    const float pumpBeats = pumpBeatsTable[(int) val ("pumprate")];
    const double beatsPerSample = bpm / 60.0 / sr;
    for (int ch = 0; ch < nCh; ++ch)
    {
        auto* d = buffer.getWritePointer (ch);
        const int src = juce::jmin (ch, 1);
        auto* dw = fxBuf.getReadPointer (nCh == 1 ? 0 : src);
        auto* rw = revOut.getReadPointer (nCh == 1 ? 0 : src);
        for (int s = 0; s < numSamples; ++s)
            d[s] = d[s] * (1.0f - fxMix * 0.4f) + (dw[s] + rw[s]) * fxMix;
    }

    dbgMix.store (buffer.getMagnitude (0, numSamples));
    if (stereo && cfxType > 0 && fxOrder == 1) // character FX after the whole mix (e.g. frequency-shifted reverb)
        runCharFx (buffer.getWritePointer (0), buffer.getWritePointer (1));

    for (int ch = 0; ch < nCh; ++ch)
    {
        auto* d = buffer.getWritePointer (ch);
        for (int s = 0; s < numSamples; ++s)
        {
            float x = d[s] * outGain;
            if (pumpAmt > 0.001f) // tempo-synced sidechain-style ducking on the full output (incl. FX)
            {
                double ph = std::fmod ((beatStart + s * beatsPerSample) / pumpBeats, 1.0);
                if (ph < 0) ph += 1.0;
                const float rise = juce::jmin (1.0f, (float) ph / 0.55f);
                x *= 1.0f - pumpAmt * (1.0f - rise) * (1.0f - rise);
            }
            if (! std::isfinite (x)) { x = 0.0f; panicRequested.store (true); safetyResets.fetch_add (1); }
            const float a = std::abs (x);
            if (a > 0.8f) x = std::copysign (0.8f + 0.2f * std::tanh ((a - 0.8f) / 0.2f), x); // safety
            d[s] = x;
            if (ch == 0)
            {
                const int w = scopeWrite.load (std::memory_order_relaxed);
                scopeBuf[(size_t) w] = x;
                scopeWrite.store ((w + 1) & (scopeSize - 1), std::memory_order_release);
            }
        }
    }
}

//==============================================================================
int IroncoreProcessor::getNumPrograms() { return numPresets(); }
const juce::String IroncoreProcessor::getProgramName (int i) { return presetName (i); }

void IroncoreProcessor::setCurrentProgram (int i)
{
    if (i < 0 || i >= numPresets()) return;
    currentProgram = i;
    applyPreset (i, apvts);
    fillAllPatternsFromActive();
    currentPresetName = presetName (i);
}

//==============================================================================
juce::File IroncoreProcessor::getUserPresetDir()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("IRONCORE/Presets");
}

juce::Array<juce::File> IroncoreProcessor::getUserPresets() const
{
    auto files = getUserPresetDir().findChildFiles (juce::File::findFiles, true, "*.ironpreset");
    std::sort (files.begin(), files.end(), [] (const juce::File& a, const juce::File& b)
    {
        const auto ka = a.getParentDirectory().getFileName() + "/" + a.getFileNameWithoutExtension();
        const auto kb = b.getParentDirectory().getFileName() + "/" + b.getFileNameWithoutExtension();
        return ka.compareIgnoreCase (kb) < 0;
    });
    return files;
}

bool IroncoreProcessor::saveUserPreset (const juce::String& rawName, const juce::String& category)
{
    const auto name = juce::File::createLegalFileName (rawName.trim());
    if (name.isEmpty()) return false;
    auto dir = getUserPresetDir().getChildFile (juce::File::createLegalFileName (category));
    if (! dir.createDirectory()) return false;

    auto state = apvts.copyState();
    writePatternsToState (state);
    state.setProperty ("presetName", name, nullptr);
    state.setProperty ("category", category, nullptr);
    auto xml = state.createXml();
    if (! xml || ! xml->writeTo (dir.getChildFile (name + ".ironpreset"))) return false;
    currentPresetName = name;
    return true;
}

bool IroncoreProcessor::loadUserPreset (const juce::File& file)
{
    auto xml = juce::XmlDocument::parse (file);
    if (! xml || ! xml->hasTagName (apvts.state.getType())) return false;
    auto tree = juce::ValueTree::fromXml (*xml);
    apvts.replaceState (tree);
    readPatternsFromState (tree);
    currentPresetName = file.getFileNameWithoutExtension();
    return true;
}

bool IroncoreProcessor::deleteUserPreset (const juce::File& f)
{
    return f.isAChildOf (getUserPresetDir()) && f.moveToTrash();
}

bool IroncoreProcessor::renameUserPreset (const juce::File& f, const juce::String& newName)
{
    const auto name = juce::File::createLegalFileName (newName.trim());
    if (name.isEmpty() || ! f.isAChildOf (getUserPresetDir())) return false;
    auto target = f.getSiblingFile (name + ".ironpreset");
    if (target.exists()) return false;
    if (! f.moveFileTo (target)) return false;
    if (currentPresetName == f.getFileNameWithoutExtension()) currentPresetName = name;
    return true;
}

void IroncoreProcessor::randomize()
{
    juce::Random r;
    auto set = [this] (const juce::String& id, float v)
    {
        if (auto* p = apvts.getParameter (id)) p->setValueNotifyingHost (p->convertTo0to1 (v));
    };
    auto rf = [&r] (float lo, float hi) { return lo + r.nextFloat() * (hi - lo); };

    set ("heat", rf (0.3f, 0.9f));   set ("bite", rf (0.3f, 0.85f)); set ("morph", r.nextFloat() < 0.5f ? rf (0, 0.3f) : rf (0.3f, 0.95f));
    set ("movement", rf (0.4f, 0.9f)); set ("density", rf (0, 0.8f));
    set ("o1wave", rf (0, 1)); set ("o2wave", rf (0, 1));
    const int intervals[] { 0, 5, 7, 12, -12, 19, 24 };
    set ("o2semi", (float) intervals[r.nextInt (7)]); set ("o2fine", rf (-15, 15));
    set ("sync", r.nextFloat() < 0.25f ? 1.0f : 0.0f);
    set ("fm", r.nextFloat() < 0.4f ? rf (0, 0.6f) : 0.0f); set ("ring", r.nextFloat() < 0.25f ? rf (0, 0.4f) : 0.0f);
    set ("cutoff", rf (400, 3000)); set ("reso", rf (0.3f, 0.85f)); set ("fmode", (float) r.nextInt (3));
    set ("fenv", rf (0.2f, 0.8f)); set ("fd", rf (0.08f, 0.5f));
    set ("sat", rf (0.2f, 0.8f)); set ("fold", r.nextFloat() < 0.4f ? rf (0, 0.5f) : 0.0f);
    set ("clip", rf (0, 0.5f)); set ("crush", r.nextFloat() < 0.15f ? rf (0, 0.6f) : 0.0f);
    set ("dorder", (float) r.nextInt (4));

    const int scale[] { 0, 0, 0, 0, 12, 3, 7, 10, -12, 5 };
    for (int st = 0; st < numSteps; ++st)
    {
        set (stepId ('f', st), rf (0.1f, 0.95f));
        set (stepId ('m', st), r.nextFloat() < 0.5f ? 0.5f : rf (0, 1));
        set (stepId ('c', st), r.nextFloat() < 0.6f ? 0.5f : rf (0, 1));
        set (stepId ('p', st), 0.5f + (float) scale[r.nextInt (10)] / 24.0f);
        set (stepId ('g', st), r.nextFloat() < 0.15f ? 0.0f : rf (0.4f, 1.0f));
        set (stepId ('a', st), r.nextFloat() < 0.3f ? 1.0f : 0.0f);
        set (stepId ('s', st), r.nextFloat() < 0.15f ? 1.0f : 0.0f);
    }
    currentPresetName = "Random";
}

void IroncoreProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    state.setProperty ("presetName", currentPresetName, nullptr);
    state.setProperty ("editorW", editorWidth, nullptr);
    state.setProperty ("editorH", editorHeight, nullptr);
    writePatternsToState (state);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void IroncoreProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            currentProgram = tree.getProperty ("program", 0);
            currentPresetName = tree.getProperty ("presetName", "INIT").toString();
            editorWidth = tree.getProperty ("editorW", 0);
            editorHeight = tree.getProperty ("editorH", 0);
            apvts.replaceState (tree);
            // replaceState skips parameters whose snapped value already matches (e.g. a bool holding 0.15),
            // so push every saved value explicitly to guarantee an exact restore.
            for (auto* p : getParameters())
                if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
                {
                    auto child = tree.getChildWithProperty ("id", rp->getParameterID());
                    if (child.isValid() && child.hasProperty ("value"))
                        rp->setValueNotifyingHost (rp->convertTo0to1 ((float) child.getProperty ("value")));
                }
            readPatternsFromState (tree);
        }
}

juce::AudioProcessorEditor* IroncoreProcessor::createEditor() { return new IroncoreEditor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new IroncoreProcessor(); }
