// IRONCORE CPU benchmark: renders 10 s of audio offline and reports % of one CPU core.
#include <JuceHeader.h>
#include "../Source/PluginProcessor.h"

static void setParam (juce::AudioProcessor& p, const juce::String& id, float plain)
{
    for (auto* prm : p.getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm))
            if (r->getParameterID() == id) { r->setValueNotifyingHost (r->convertTo0to1 (plain)); return; }
}

struct Scenario { const char* name; int poly; float density, morph; int quality, notes, osctype; };

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    const double sr = 48000.0;
    const int block = 512, seconds = 10;
    const Scenario scenarios[] {
        { "Mono, 1 Note, Density 0.3",          0, 0.3f, 0.0f, 0, 1, 0 },
        { "Mono, 1 Note, Density 1.0",          0, 1.0f, 0.0f, 0, 1, 0 },
        { "Poly, 4 Noten, Density 0.3",         1, 0.3f, 0.0f, 0, 4, 0 },
        { "Poly, 8 Noten, Density 0.3",         1, 0.3f, 0.0f, 0, 8, 0 },
        { "Poly, 8 Noten, Density 1.0",         1, 1.0f, 0.0f, 0, 8, 0 },
        { "Poly, 8 Noten, Density 1.0, Formant",1, 1.0f, 1.0f, 0, 8, 0 },
        { "Poly, 8 Noten, Density 1.0, WT",     1, 1.0f, 0.0f, 0, 8, 2 },
        { "Mono, 1 Note, Density 0.3, 4x",      0, 0.3f, 0.0f, 1, 1, 0 },
        { "Poly, 8 Noten, Density 1.0, 4x",     1, 1.0f, 0.0f, 1, 8, 0 },
        { "Poly, 8 Noten, D 1.0, Formant, 4x",  1, 1.0f, 1.0f, 1, 8, 0 },
    };

    std::printf ("%-40s %10s %12s\n", "Szenario", "CPU-Kern", "Echtzeit-x");
    for (auto& sc : scenarios)
    {
        IroncoreProcessor proc;
        proc.setPlayConfigDetails (0, 2, sr, block);
        proc.setCurrentProgram (0);
        setParam (proc, "vmode", (float) sc.poly);
        setParam (proc, "density", sc.density);
        setParam (proc, "morph", sc.morph);
        setParam (proc, "quality", (float) sc.quality);
        setParam (proc, "osctype", (float) sc.osctype);
        setParam (proc, "ar", 2.0f);
        proc.prepareToPlay (sr, block);

        juce::AudioBuffer<float> buf (2, block);
        juce::MidiBuffer midi;
        for (int n = 0; n < sc.notes; ++n) midi.addEvent (juce::MidiMessage::noteOn (1, 48 + n * 3, (juce::uint8) 110), 0);
        proc.processBlock (buf, midi); // warm-up + note-ons
        midi.clear();

        const int blocks = (int) (seconds * sr / block);
        const auto t0 = juce::Time::getHighResolutionTicks();
        for (int b = 0; b < blocks; ++b) proc.processBlock (buf, midi);
        const double elapsed = juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - t0);
        const double audio = blocks * block / sr;
        std::printf ("%-40s %9.2f%% %11.1fx\n", sc.name, 100.0 * elapsed / audio, audio / elapsed);
    }
    return 0;
}
