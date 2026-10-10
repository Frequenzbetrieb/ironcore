// IRONCORE stress test: random parameters + notes, detects NaN/Inf and loud bursts.
#include <JuceHeader.h>
#include "../Source/PluginProcessor.h"

struct FakePlayHead : juce::AudioPlayHead
{
    double ppq = 0.0, bpm = 140.0;
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo info; info.setBpm (bpm); info.setPpqPosition (ppq); info.setIsPlaying (true);
        return info;
    }
};

// Mode "presets": play every factory preset with transport running and report loud bursts.
static int presetScan()
{
    const double sr = 48000.0; const int block = 256;
    IroncoreProcessor proc0;
    juce::String trims;
    std::printf ("%-26s %8s %8s %8s %s\n", "Preset", "medRMS", "maxRMS", "ratio", "laute Bloecke");
    for (int i = 0; i < proc0.getNumPrograms(); ++i)
    {
        IroncoreProcessor proc;
        FakePlayHead ph;
        proc.setPlayHead (&ph);
        proc.setPlayConfigDetails (0, 2, sr, block);
        proc.setCurrentProgram (i);
        proc.prepareToPlay (sr, block);
        juce::AudioBuffer<float> buf (2, block);
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 48, (juce::uint8) 100), 0);
        std::vector<double> rmsList;
        int loud = 0;
        for (int b = 0; b < (int) (16 * sr / block); ++b) // 16 s
        {
            proc.processBlock (buf, midi);
            midi.clear();
            ph.ppq += block / sr * ph.bpm / 60.0;
            double sum = 0, peak = 0;
            for (int ch = 0; ch < 2; ++ch) for (int s = 0; s < block; ++s) { const float x = buf.getSample (ch, s); sum += x * x; peak = std::max (peak, (double) std::abs (x)); }
            rmsList.push_back (std::sqrt (sum / (2 * block)));
            if (peak > 0.97) ++loud;
        }
        auto sorted = rmsList; std::sort (sorted.begin(), sorted.end());
        const double med = sorted[sorted.size() / 2], mx = sorted.back();
        std::printf ("%-26s %8.3f %8.3f %8.1f %d\n", proc.getProgramName (i).toRawUTF8(), med, mx, mx / (med + 1e-6), loud);
        // current trim is already applied: suggest the total trim relative to -3 dB output
        const float outDb = proc.apvts.getRawParameterValue ("out")->load();
        const double total = (outDb + 3.0) + 20.0 * std::log10 (0.09 / std::max (mx, 1e-4));
        trims << "        { \"" << proc.getProgramName (i) << "\", " << juce::String (juce::jlimit (-9.0, 9.0, total), 1) << "f },\n";
    }
    juce::File (IRONCORE_SOURCE_DIR "/Source/PresetTrims.inc").replaceWithText (trims);
    return 0;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    if (argc > 1 && juce::String (argv[1]) == "presets") return presetScan();
    if (argc > 1 && juce::String (argv[1]) == "oscdiff")
    {
        // Timbre change caused by oscillator controls, measured on the harmonic profile (h1..h24, dB rel. h1).
        // One voice, no drift/sequencer/FX so the result is deterministic; preset distortion & filter stay as they are.
        const double sr = 48000.0; const int block = 512, N = 16384;
        juce::dsp::FFT fft (14);
        auto profile = [&] (const juce::String& preset, std::vector<std::pair<juce::String, float>> sets)
        {
            IroncoreProcessor proc;
            proc.setPlayConfigDetails (0, 2, sr, block);
            for (int i = 0; i < proc.getNumPrograms(); ++i) if (proc.getProgramName (i) == preset) proc.setCurrentProgram (i);
            std::vector<std::pair<juce::String, float>> base { { "seqon", 0 }, { "density", 0 }, { "drift", 0 }, { "fxmix", 0 }, { "unimode", 0 }, { "l1amt", 0 }, { "l2amt", 0 }, { "noise", 0 } };
            for (auto& x : sets) base.push_back (x);
            for (auto& [id, v] : base) for (auto* p : proc.getParameters()) if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p)) if (rp->getParameterID() == id) rp->setValueNotifyingHost (rp->convertTo0to1 (v));
            proc.prepareToPlay (sr, block);
            juce::AudioBuffer<float> buf (2, block); juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage::noteOn (1, 45, (juce::uint8) 100), 0);
            std::vector<float> rec;
            for (int b = 0; b < 60; ++b) { proc.processBlock (buf, midi); midi.clear(); if (b >= 20) for (int s2 = 0; s2 < block; ++s2) rec.push_back (buf.getSample (0, s2)); }
            std::vector<float> spec (2 * N, 0.0f);
            for (int i = 0; i < N; ++i) spec[(size_t) i] = rec[(size_t) i] * (0.5f - 0.5f * std::cos (6.2831853f * i / N));
            fft.performFrequencyOnlyForwardTransform (spec.data());
            auto peak = [&] (double hz) { const int c = (int) (hz / sr * N); float m = 0; for (int i = c - 3; i <= c + 3; ++i) m = std::max (m, spec[(size_t) i]); return m; };
            std::vector<float> h; float tot = 0; for (int k = 1; k <= 24; ++k) tot += peak (110.0 * k);
            for (int k = 1; k <= 24; ++k) h.push_back (juce::jlimit (-40.0f, 0.0f, juce::Decibels::gainToDecibels (peak (110.0 * k) / (tot + 1e-9f), -99.0f)));
            return h;
        };
        auto d = [] (const std::vector<float>& a, const std::vector<float>& b) { double s2 = 0; for (size_t i = 0; i < a.size(); ++i) s2 += std::abs (a[i] - b[i]); return s2 / (double) a.size(); };
        const std::vector<std::pair<const char*, std::vector<std::pair<juce::String, float>>>> tests {
            { "Leerlauf", {} }, { "OSC1 SHAPE 0 -> 0.5", { { "o1wave", 0.5f } } }, { "OSC1 SHAPE 0 -> 1", { { "o1wave", 1.0f } } },
            { "SEMI 7 -> 0", { { "o2semi", 0.0f } } }, { "OSC2 Level -> 0", { { "o2mix", 0.0f } } },
            { "FM 0 -> 0.6", { { "fm", 0.6f } } }, { "RING 0 -> 0.6", { { "ring", 0.6f } } },
            { "TYPE 1 -> WT Metal", { { "osctype", 2.0f } } }, { "TYPE 1 -> Phase Dist", { { "osctype", 5.0f } } } };
        double total = 0; int cnt = 0;
        for (const char* preset : { "INIT Iron", "SCHRANZ Saw Burner", "SCHRANZ Grinder", "INDUSTRIAL Foundry", "ACID Reactor", "HARD TECHNO Drill" })
        {
            const auto ref = profile (preset, { { "o1wave", 0.0f }, { "osctype", 0.0f }, { "fm", 0.0f }, { "ring", 0.0f } });
            std::printf ("%-22s", preset);
            for (auto& [name, sets] : tests)
            {
                auto s2 = sets; s2.insert (s2.begin(), { { "o1wave", 0.0f }, { "osctype", 0.0f }, { "fm", 0.0f }, { "ring", 0.0f } });
                const double v = d (ref, profile (preset, s2)); total += v; ++cnt;
                std::printf (" %5.1f", v);
            }
            std::printf ("\n");
        }
        std::printf ("Mittelwert: %.1f dB  (Spalten: Leerlauf Shape.5 Shape1 Semi O2Lvl FM Ring WTMetal PD)\n", total / cnt);
        return 0;
    }
    if (argc > 1 && juce::String (argv[1]) == "harmonics")
    {
        // Level of harmonics 1..12 (dB rel. fundamental), one voice, filter wide open, no FX
        const double sr = 48000.0; const int block = 512, N = 16384;
        juce::dsp::FFT fft (14);
        const juce::String preset = argc > 2 ? argv[2] : "INIT Iron";
        auto run = [&] (const char* label, std::vector<std::pair<juce::String, float>> sets)
        {
            IroncoreProcessor proc;
            proc.setPlayConfigDetails (0, 2, sr, block);
            for (int i = 0; i < proc.getNumPrograms(); ++i) if (proc.getProgramName (i) == preset) proc.setCurrentProgram (i);
            std::vector<std::pair<juce::String, float>> base { { "seqon", 0 }, { "cutoff", 18000 }, { "fenv", 0 }, { "reso", 0 }, { "density", 0 }, { "drift", 0 },
                                                             { "fxmix", 0 }, { "unimode", 0 }, { "bite", 0 }, { "movement", 0 }, { "l1amt", 0 }, { "l2amt", 0 } };
            for (auto& x : sets) base.push_back (x);
            for (auto& [id, v] : base) for (auto* p : proc.getParameters()) if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p)) if (rp->getParameterID() == id) rp->setValueNotifyingHost (rp->convertTo0to1 (v));
            proc.prepareToPlay (sr, block);
            juce::AudioBuffer<float> buf (2, block); juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage::noteOn (1, 45, (juce::uint8) 100), 0); // A2 = 110 Hz
            std::vector<float> rec;
            for (int b = 0; b < 60; ++b) { proc.processBlock (buf, midi); midi.clear(); if (b >= 20) for (int s2 = 0; s2 < block; ++s2) rec.push_back (buf.getSample (0, s2)); }
            std::vector<float> spec (2 * N, 0.0f);
            for (int i = 0; i < N; ++i) spec[(size_t) i] = rec[(size_t) i] * (0.5f - 0.5f * std::cos (6.2831853f * i / N));
            fft.performFrequencyOnlyForwardTransform (spec.data());
            auto peak = [&] (double hz) { const int c = (int) (hz / sr * N); float m = 0; for (int i = c - 3; i <= c + 3; ++i) m = std::max (m, spec[(size_t) i]); return m; };
            const float f0 = peak (110.0);
            std::printf ("%-28s", label);
            for (int h = 1; h <= 12; ++h) std::printf ("%6.0f", juce::Decibels::gainToDecibels (peak (110.0 * h) / (f0 + 1e-9f), -99.0f));
            std::printf ("   peak %.2f\n", buf.getMagnitude (0, block));
        };
        std::printf ("%-28s", "Harmonische ->"); for (int h = 1; h <= 12; ++h) std::printf ("%6d", h); std::printf ("\n");
        run ("Saw, Osc2 aus, Sub aus", { { "o2mix", 0 }, { "sub", 0 } });
        run ("Square (SHAPE 0.5)", { { "o2mix", 0 }, { "sub", 0 }, { "o1wave", 0.5f } });
        run ("Puls (SHAPE 1)", { { "o2mix", 0 }, { "sub", 0 }, { "o1wave", 1.0f } });
        run ("Saw, HEAT 0, SAT 0", { { "o2mix", 0 }, { "sub", 0 }, { "heat", 0 }, { "sat", 0 }, { "clip", 0 }, { "fold", 0 } });
        run ("Square, HEAT 0, SAT 0", { { "o2mix", 0 }, { "sub", 0 }, { "o1wave", 0.5f }, { "heat", 0 }, { "sat", 0 }, { "clip", 0 }, { "fold", 0 } });
        run ("Phase Dist, clean", { { "o2mix", 0 }, { "sub", 0 }, { "osctype", 5 }, { "o1wave", 0.0f }, { "heat", 0 }, { "sat", 0 }, { "clip", 0 } });
        run ("Saw, nur CLIP aus", { { "o2mix", 0 }, { "sub", 0 }, { "clip", 0 } });
        run ("Saw, nur SAT aus", { { "o2mix", 0 }, { "sub", 0 }, { "sat", 0 } });
        run ("Saw, nur HEAT aus", { { "o2mix", 0 }, { "sub", 0 }, { "heat", 0 } });
        run ("Saw, Filter LP12 statt Ladder", { { "o2mix", 0 }, { "sub", 0 }, { "fmode", 1 } });
        run ("Saw, Ladder, alles clean", { { "o2mix", 0 }, { "sub", 0 }, { "heat", 0 }, { "sat", 0 }, { "clip", 0 }, { "fold", 0 }, { "fdrive", 0 } });
        run ("Saw, LP12, alles clean", { { "o2mix", 0 }, { "sub", 0 }, { "heat", 0 }, { "sat", 0 }, { "clip", 0 }, { "fold", 0 }, { "fdrive", 0 }, { "fmode", 1 } });
        return 0;
    }
    if (argc > 1 && juce::String (argv[1]) == "oscimpact")
    {
        // How much does each oscillator control change the sound? Spectral distance (dB) vs. reference.
        const double sr = 48000.0; const int block = 512, N = 8192;
        juce::dsp::FFT fft (13);
        auto render = [&] (const juce::String& preset, std::vector<std::pair<juce::String, float>> sets)
        {
            IroncoreProcessor proc;
            proc.setPlayConfigDetails (0, 2, sr, block);
            for (int i = 0; i < proc.getNumPrograms(); ++i) if (proc.getProgramName (i) == preset) proc.setCurrentProgram (i);
            for (auto& [id, v] : sets) for (auto* p : proc.getParameters()) if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p)) if (rp->getParameterID() == id) rp->setValueNotifyingHost (rp->convertTo0to1 (v));
            proc.prepareToPlay (sr, block);
            juce::AudioBuffer<float> buf (2, block); juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage::noteOn (1, 48, (juce::uint8) 100), 0);
            std::vector<float> rec;
            for (int b = 0; b < 60; ++b) { proc.processBlock (buf, midi); midi.clear(); if (b >= 20) for (int s2 = 0; s2 < block; ++s2) rec.push_back (buf.getSample (0, s2)); }
            std::vector<float> spec (2 * N, 0.0f);
            for (int i = 0; i < N; ++i) spec[(size_t) i] = rec[(size_t) i] * (0.5f - 0.5f * std::cos (6.2831853f * i / N));
            fft.performFrequencyOnlyForwardTransform (spec.data());
            std::vector<float> db (N / 2);
            for (int i = 0; i < N / 2; ++i) db[(size_t) i] = juce::Decibels::gainToDecibels (spec[(size_t) i] + 1e-9f, -100.0f);
            return db;
        };
        // third-octave band levels from the magnitude spectrum (dB), like the ear groups frequencies
        auto bands = [&] (const std::vector<float>& db)
        {
            std::vector<double> out;
            for (double fc = 50.0; fc < 16000.0; fc *= std::pow (2.0, 1.0 / 3.0))
            {
                const int lo = (int) (fc / std::pow (2.0, 1.0 / 6.0) / sr * N), hi = std::max (lo + 1, (int) (fc * std::pow (2.0, 1.0 / 6.0) / sr * N));
                double e = 0; for (int i = lo; i < hi && i < (int) db.size(); ++i) e += std::pow (10.0, db[(size_t) i] / 10.0);
                out.push_back (10.0 * std::log10 (e + 1e-12));
            }
            return out;
        };
        auto dist = [&] (const std::vector<float>& a, const std::vector<float>& b)
        {
            const auto A = bands (a), B = bands (b);
            const double offs = [&] { double s2 = 0; for (size_t i = 0; i < A.size(); ++i) s2 += A[i] - B[i]; return s2 / (double) A.size(); }();
            double sum = 0; for (size_t i = 0; i < A.size(); ++i) sum += std::abs (A[i] - B[i] - offs); // level-independent: timbre only
            return sum / (double) A.size();
        };
        struct Test { const char* name; std::vector<std::pair<juce::String, float>> sets; };
        const std::vector<Test> tests {
            { "(Grundrauschen: gleiche Einstellung)", {} },
            { "OSC1 SHAPE 0 -> 1",   { { "o1wave", 1.0f } } },
            { "OSC2 SHAPE 0 -> 1",   { { "o2wave", 1.0f } } },
            { "SEMI 7 -> 0",         { { "o2semi", 0.0f } } },
            { "OSC2 Level -> 0",     { { "o2mix", 0.0f } } },
            { "FM 0 -> 0.6",         { { "fm", 0.6f } } },
            { "RING 0 -> 0.6",       { { "ring", 0.6f } } },
            { "SUB -> 0",            { { "sub", 0.0f } } },
            { "NOISE 0 -> 0.5",      { { "noise", 0.5f } } },
            { "SYNC an (SEMI 19)",   { { "sync", 1.0f }, { "o2semi", 19.0f } } },
            { "TYPE 1 -> WT Metal",  { { "osctype", 2.0f } } },
            { "TYPE 1 -> Phase Dist",{ { "osctype", 5.0f } } },
            { "TYPE 1 -> Resonator", { { "osctype", 6.0f } } },
            { "zum Vergleich: CUTOFF 1800 -> 600", { { "cutoff", 600.0f } } },
        };
        for (const char* preset : { "INIT Iron", "SCHRANZ Saw Burner" })
            for (bool open : { false, true })
            {
                std::vector<std::pair<juce::String, float>> base { { "seqon", 0.0f } };
                if (open) { base.push_back ({ "cutoff", 18000.0f }); base.push_back ({ "fenv", 0.0f }); base.push_back ({ "reso", 0.0f }); }
                const auto ref = render (preset, base);
                std::printf ("\n== %s, Filter %s ==\n", preset, open ? "OFFEN" : "wie im Preset");
                for (auto& t : tests)
                {
                    auto sets = base; for (auto& x : t.sets) sets.push_back (x);
                    std::printf ("  %-36s %5.1f dB\n", t.name, dist (ref, render (preset, sets)));
                }
            }
        return 0;
    }
    if (argc > 1 && juce::String (argv[1]) == "postfx")
    {
        const double sr = 48000.0; const int block = 512;
        juce::Random r (5);
        for (float width : { 0.0f, 0.3f, 0.66f })
            for (float lowcut : { 20.0f, 150.0f })
                for (float tilt : { 0.0f, 0.25f })
                {
                    ic::PostFX fx; fx.prepare (sr);
                    std::vector<float> L (block), R (block);
                    double late = 0, at = 0;
                    for (int b = 0; b < (int) (35 * sr / block); ++b)
                    {
                        for (int s2 = 0; s2 < block; ++s2) { const bool on = b * block < 4 * sr; L[(size_t) s2] = on ? r.nextFloat() - 0.5f : 0.0f; R[(size_t) s2] = L[(size_t) s2]; }
                        fx.process (L.data(), R.data(), block, lowcut, 0.0f, tilt, width, 0.0f, 0.0, 0.0, 1.0f);
                        if (b * block > 6 * sr) for (int s2 = 0; s2 < block; ++s2) if (std::abs (L[(size_t) s2]) > late) { late = std::abs (L[(size_t) s2]); at = (b * block + s2) / sr; }
                    }
                    std::printf ("width %.2f lowcut %3.0f tilt %.2f -> max nach Stille %.4f bei %.2f s\n", width, lowcut, tilt, late, at);
                }
        return 0;
    }
    if (argc > 1 && juce::String (argv[1]) == "bisect")
    {
        const double sr = 48000.0; const int block = 512;
        const char* kills[] { "none", "width", "eq", "delay", "reverb", "fxmix", "pump" };
        int hits[7] {}, runsPer[7] {};
        juce::Random r (99);
        for (int run = 0; run < 280; ++run)
        {
            const int k = run % 7;
            IroncoreProcessor proc;
            proc.setPlayConfigDetails (0, 2, sr, block);
            for (int i = 0; i < proc.getNumPrograms(); ++i) if (proc.getProgramName (i) == "RAVE Hoover Lead") proc.setCurrentProgram (i);
            auto set = [&] (const char* id, float v) { for (auto* p : proc.getParameters()) if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p)) if (rp->getParameterID() == id) rp->setValueNotifyingHost (rp->convertTo0to1 (v)); };
            set ("seqon", 0.0f);
            switch (k)
            {
                case 1: set ("width", 0.0f); break;
                case 2: set ("lowcut", 20.0f); set ("tilt", 0.0f); set ("mideq", 0.0f); break;
                case 3: set ("dlevel", 0.0f); break;
                case 4: set ("rlevel", 0.0f); break;
                case 5: set ("fxmix", 0.0f); break;
                case 6: set ("pump", 0.0f); break;
                default: break;
            }
            proc.prepareToPlay (sr, block);
            juce::AudioBuffer<float> buf (2, block); juce::MidiBuffer midi;
            for (int b = 0; b < (int) (4 * sr / block); ++b)
            {
                midi.clear();
                if (b % 20 == 0) midi.addEvent (juce::MidiMessage::noteOn (1, 40 + r.nextInt (30), (juce::uint8) 100), 0);
                if (b % 20 == 15) for (int n = 8; n < 128; ++n) midi.addEvent (juce::MidiMessage::noteOff (1, n), 0);
                proc.processBlock (buf, midi);
            }
            midi.clear();
            for (int n = 8; n < 128; ++n) midi.addEvent (juce::MidiMessage::noteOff (1, n), 0);
            bool hit = false;
            for (int b = 0; b < (int) (26 * sr / block); ++b)
            {
                proc.processBlock (buf, midi); midi.clear();
                if (b * block > 15 * sr && buf.getMagnitude (0, block) > 0.01f) hit = true;
            }
            ++runsPer[k]; if (hit) ++hits[k];
        }
        for (int k = 0; k < 7; ++k) std::printf ("%-7s aus: %d/%d Laeufe mit Knall\n", kills[k], hits[k], runsPer[k]);
        return 0;
    }
    if (argc > 1 && juce::String (argv[1]) == "isolate")
    {
        // Which stage keeps ringing? Play a preset, stop, then zero one stage at a time.
        const double sr = 48000.0; const int block = 512;
        const char* presetName = argc > 2 ? argv[2] : "RAVE Siren Morph";
        for (const char* kill : { "none", "width", "lowcut", "tilt", "fxmix", "rlevel", "dlevel", "pump" })
        {
            IroncoreProcessor proc;
            proc.setPlayConfigDetails (0, 2, sr, block);
            for (int i = 0; i < proc.getNumPrograms(); ++i) if (proc.getProgramName (i) == presetName) proc.setCurrentProgram (i);
            auto set = [&] (const char* id, float v) { for (auto* p : proc.getParameters()) if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p)) if (rp->getParameterID() == id) rp->setValueNotifyingHost (rp->convertTo0to1 (v)); };
            set ("seqon", 0.0f);
            if (juce::String (kill) == "lowcut") set (kill, 20.0f); else if (juce::String (kill) != "none") set (kill, 0.0f);
            proc.prepareToPlay (sr, block);
            juce::AudioBuffer<float> buf (2, block); juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
            for (int b = 0; b < (int) (2 * sr / block); ++b) { proc.processBlock (buf, midi); midi.clear(); }
            midi.addEvent (juce::MidiMessage::noteOff (1, 60), 0);
            double late = 0;
            for (int b = 0; b < (int) (30 * sr / block); ++b) { proc.processBlock (buf, midi); midi.clear(); if (b * block > 25 * sr) late = std::max (late, (double) buf.getMagnitude (0, block)); }
            std::printf ("%-8s aus -> Pegel nach 25 s: %.4f\n", kill, late);
        }
        return 0;
    }
    if (argc > 1 && juce::String (argv[1]) == "fxdecay")
    {
        // Worst-case FX settings with 32-sample blocks: one note, then 60 s silence. Tails must die.
        IroncoreProcessor proc;
        proc.setPlayConfigDetails (0, 2, 48000.0, 32);
        auto set = [&] (const char* id, float v) { for (auto* p : proc.getParameters()) if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p)) if (rp->getParameterID() == id) rp->setValueNotifyingHost (rp->convertTo0to1 (v)); };
        set ("rsize", 1.0f); set ("rdamp", 0.0f); set ("rlevel", 1.0f); set ("dfb", 0.9f); set ("dlevel", 1.0f); set ("fxmix", 1.0f); set ("heat", 1.0f);
        proc.prepareToPlay (48000.0, 32);
        juce::AudioBuffer<float> buf (2, 32);
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 127), 0);
        for (int b = 0; b < 48000 / 32 * 61; ++b)
        {
            if (b == 48000 / 32) midi.addEvent (juce::MidiMessage::noteOff (1, 60), 0);
            proc.processBlock (buf, midi);
            midi.clear();
            if (b % (48000 / 32 * 5) == 0)
                std::printf ("t=%2ds  Pegel %.5f\n", b * 32 / 48000, buf.getMagnitude (0, 32));
        }
        return 0;
    }
    if (argc > 1 && juce::String (argv[1]) == "oversize")
    {
        // Host announces 32-sample blocks but occasionally sends bigger ones (no notes played!)
        IroncoreProcessor proc;
        proc.setPlayConfigDetails (0, 2, 48000.0, 32);
        proc.prepareToPlay (48000.0, 32);
        juce::AudioBuffer<float> buf (2, 1024);
        juce::MidiBuffer midi;
        double peak = 0; int bad = 0;
        for (int i = 0; i < 4000; ++i)
        {
            const int n = (i % 50 == 0) ? 512 : 32;
            buf.setSize (2, n, false, false, true);
            proc.processBlock (buf, midi);
            for (int ch = 0; ch < 2; ++ch) for (int s2 = 0; s2 < n; ++s2)
            { const float x = buf.getSample (ch, s2); if (! std::isfinite (x)) ++bad; else peak = std::max (peak, (double) std::abs (x)); }
        }
        std::printf ("Oversize-Test ohne Noten: Spitzenpegel %.3f, NaN %d\n", peak, bad);
        return 0;
    }
    if (argc > 1 && juce::String (argv[1]) == "idle")
    {
        // Play notes, stop everything, then render silence: anything audible after the tails is a bug.
        const double sr = 48000.0; const int block = 512;
        juce::Random r (argc > 2 ? atoi (argv[2]) : 3);
        int found = 0;
        for (int run = 0; run < 200 && found < 10; ++run)
        {
            IroncoreProcessor proc;
            proc.setPlayConfigDetails (0, 2, sr, block);
            proc.setCurrentProgram (r.nextInt (30));
            juce::String changes;
            for (int k = 0; k < 8; ++k)
                if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (proc.getParameters()[r.nextInt (proc.getParameters().size())]))
                {
                    if (rp->getParameterID() == "out") continue;
                    const float v = r.nextFloat(); rp->setValueNotifyingHost (v);
                    changes << rp->getParameterID() << "=" << juce::String (rp->convertFrom0to1 (v), 2) << " ";
                }
            for (auto* p : proc.getParameters()) if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p)) if (rp->getParameterID() == "seqon") rp->setValueNotifyingHost (0.0f);
            proc.prepareToPlay (sr, block);
            juce::AudioBuffer<float> buf (2, block);
            juce::MidiBuffer midi;
            // 4 s of playing
            for (int b = 0; b < (int) (4 * sr / block); ++b)
            {
                midi.clear();
                if (b % 20 == 0) midi.addEvent (juce::MidiMessage::noteOn (1, 40 + r.nextInt (30), (juce::uint8) 100), 0);
                if (b % 20 == 15) for (int n = 8; n < 128; ++n) midi.addEvent (juce::MidiMessage::noteOff (1, n), 0);
                proc.processBlock (buf, midi);
            }
            midi.clear();
            for (int n = 8; n < 128; ++n) midi.addEvent (juce::MidiMessage::noteOff (1, n), 0);
            // 40 s of silence
            double lateMax = 0.0, lateAt = 0.0; int lateBlocks = 0, voicesAtMax = -1; float dS = 0, dP = 0, dM = 0;
            for (int b = 0; b < (int) (40 * sr / block); ++b)
            {
                proc.processBlock (buf, midi);
                midi.clear();
                if (b * block > 20 * sr)
                {
                    const double mg = buf.getMagnitude (0, block);
                    if (mg > lateMax) { lateMax = mg; lateAt = b * block / sr; voicesAtMax = proc.uiVoices.load(); dS = proc.dbgSynth.load(); dP = proc.dbgPost.load(); dM = proc.dbgMix.load(); }
                    if (mg > 0.001) ++lateBlocks;
                }
            }
            if (lateMax > 0.001)
            {
                // diagnose: voices still active? does disabling FX silence it?
                const int voicesLeft = proc.uiVoices.load();
                double dc = 0; for (int s2 = 0; s2 < block; ++s2) dc += buf.getSample (0, s2); dc /= block;
                std::printf ("Run %d (%s): nach 20 s Stille noch Pegel %.3f, Stimmen aktiv: %d, DC: %.3f, Max bei %.2f s (Stimmen dort %d) [Synth %.3f Post %.3f Mix %.3f], laute Bloecke %d | %s\n", run, proc.getProgramName (proc.getCurrentProgram()).toRawUTF8(), lateMax, voicesLeft, dc, lateAt, voicesAtMax, dS, dP, dM, lateBlocks, changes.toRawUTF8());
                ++found;
                auto setp = [&] (const char* id, float v) { for (auto* p : proc.getParameters()) if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p)) if (rp->getParameterID() == id) rp->setValueNotifyingHost (rp->convertTo0to1 (v)); };
                std::pair<const char*, float> kills[] { { "cfxmix", 0.0f }, { "pump", 0.0f }, { "width", 0.0f }, { "tilt", 0.0f }, { "mideq", 0.0f }, { "lowcut", 20.0f }, { "rlevel", 0.0f }, { "dlevel", 0.0f }, { "fxmix", 0.0f } };
                for (auto& [id, v] : kills)
                {
                    setp (id, v);
                    double lvl = 0;
                    for (int b = 0; b < (int) (1.0 * sr / block); ++b) { proc.processBlock (buf, midi); lvl = std::max (lvl, (double) buf.getMagnitude (0, block)); }
                    std::printf ("    %s=0 -> %.4f\n", id, lvl);
                }
            }
        }
        std::printf ("Idle-Test fertig, %d Faelle\n", found);
        return 0;
    }
    if (argc > 1 && juce::String (argv[1]) == "host")
    {
        // Host-like conditions: variable block sizes, loop jumps, quality switch, sample rates
        for (double sr : { 44100.0, 48000.0, 96000.0 })
        {
            const int maxBlock = 512;
            IroncoreProcessor proc;
            FakePlayHead ph;
            proc.setPlayHead (&ph);
            proc.setPlayConfigDetails (0, 2, sr, maxBlock);
            proc.prepareToPlay (sr, maxBlock);
            juce::Random r (7);
            juce::AudioBuffer<float> buf (2, maxBlock);
            juce::MidiBuffer midi;
            int bad = 0, loud = 0;
            double t = 0;
            for (int i = 0; i < 40000; ++i)
            {
                const int n = 1 + r.nextInt (maxBlock);
                buf.setSize (2, n, false, false, true);
                midi.clear();
                if (i % 150 == 0) proc.setCurrentProgram (r.nextInt (30));
                if (i % 40 == 0) midi.addEvent (juce::MidiMessage::noteOn (1, 36 + r.nextInt (40), (juce::uint8) 100), 0);
                if (i % 40 == 20) midi.addEvent (juce::MidiMessage::allNotesOff (1), 0);
                if (i % 333 == 0) { auto* q = proc.getParameters()[0]; (void) q;
                    for (auto* p : proc.getParameters()) if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p)) if (rp->getParameterID() == "quality") rp->setValueNotifyingHost (r.nextBool() ? 1.0f : 0.0f); }
                if (i % 500 == 0) ph.ppq = 0.0; // loop jump
                proc.processBlock (buf, midi);
                ph.ppq += n / sr * ph.bpm / 60.0;
                t += n / sr;
                for (int ch = 0; ch < 2; ++ch) for (int s2 = 0; s2 < n; ++s2)
                {
                    const float x = buf.getSample (ch, s2);
                    if (! std::isfinite (x)) ++bad;
                    else if (std::abs (x) > 0.97f) ++loud;
                }
            }
            std::printf ("SR %6.0f: %.0f s gerendert, NaN-Samples: %d, Samples >0.97: %d\n", sr, t, bad, loud);
        }
        return 0;
    }
    const double sr = 48000.0;
    const int block = 256;
    const int seconds = argc > 1 ? atoi (argv[1]) : 600;
    juce::Random rng (argc > 2 ? atoi (argv[2]) : 1234);

    IroncoreProcessor proc;
    proc.setPlayConfigDetails (0, 2, sr, block);
    proc.prepareToPlay (sr, block);
    juce::AudioBuffer<float> buf (2, block);
    juce::MidiBuffer midi;

    auto params = proc.getParameters();
    int issues = 0;
    double rmsSlow = 0.0;
    int note = -1;
    juce::String lastChange;
    const int blocks = (int) (seconds * sr / block);

    for (int b = 0; b < blocks; ++b)
    {
        midi.clear();
        // every ~0.5 s: change a few random parameters
        if (b % 90 == 0)
        {
            lastChange.clear();
            for (int k = 0; k < 6; ++k)
            {
                auto* p = params[rng.nextInt (params.size())];
                if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (p))
                {
                    if (r->getParameterID() == "quality" || r->getParameterID() == "out") continue;
                    const float v = rng.nextFloat();
                    r->setValueNotifyingHost (v);
                    lastChange << r->getParameterID() << "=" << juce::String (r->convertFrom0to1 (v), 3) << " ";
                }
            }
        }
        if (b % 60 == 0)
        {
            if (note >= 0) midi.addEvent (juce::MidiMessage::noteOff (1, note), 0);
            note = 30 + rng.nextInt (60);
            midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) (40 + rng.nextInt (87))), 1);
        }
        if (b % 400 == 0) midi.addEvent (juce::MidiMessage::noteOn (1, rng.nextInt (8), (juce::uint8) 100), 2);

        proc.processBlock (buf, midi);

        double sum = 0.0;
        bool bad = false;
        for (int ch = 0; ch < 2; ++ch)
            for (int s = 0; s < block; ++s)
            {
                const float x = buf.getSample (ch, s);
                if (! std::isfinite (x)) bad = true;
                sum += (double) x * x;
            }
        const double rms = std::sqrt (sum / (2 * block));
        if (bad)
        {
            std::printf ("[%.2fs] NaN/Inf!  letzte Aenderung: %s\n", b * block / sr, lastChange.toRawUTF8());
            if (++issues > 15) break;
            proc.prepareToPlay (sr, block);
        }
        else if (rms > 0.5 && rms > rmsSlow * 6.0 + 0.05)
        {
            std::printf ("[%.2fs] Pegelsprung RMS %.2f (vorher %.3f)  letzte Aenderung: %s\n", b * block / sr, rms, rmsSlow, lastChange.toRawUTF8());
            if (++issues > 15) break;
        }
        rmsSlow += (rms - rmsSlow) * 0.02;
    }
    std::printf ("Fertig: %d Auffaelligkeiten in %d s\n", issues, seconds);
    return 0;
}
