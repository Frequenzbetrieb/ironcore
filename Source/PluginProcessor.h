#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include "Params.h"
#include "Voice.h"
#include "IronVerb.h"
#include "PostFX.h"
#include "CharFX.h"

// IRONCORE — Industrial Lead Synthesizer
class IroncoreProcessor : public juce::AudioProcessor,
                          private juce::AudioProcessorValueTreeState::Listener,
                          private juce::Timer
{
public:
    IroncoreProcessor();
    ~IroncoreProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "IRONCORE"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<int> uiStep { -1 };

    // --- Preset management (message thread only) ---
    static juce::StringArray presetCategories() { return { "Schranz", "Industrial", "Acid", "Rave", "Hard Techno", "FX", "User" }; }
    static juce::File getUserPresetDir();
    juce::Array<juce::File> getUserPresets() const;
    bool saveUserPreset (const juce::String& name, const juce::String& category);
    bool loadUserPreset (const juce::File&);
    void randomize();
    juce::String currentPresetName { "INIT" };
    bool deleteUserPreset (const juce::File&);
    bool renameUserPreset (const juce::File&, const juce::String& newName);

    // --- Pattern memory (8 patterns; audio thread reads the store directly) ---
    int getActivePattern() const { return activePattern.load(); }
    void selectPattern (int index);         // message thread
    void copyPattern();
    void pastePattern();
    void clearPattern();
    void fillAllPatternsFromActive();
    bool hasPatternClipboard() const { return clipboardValid; }

    // --- Display feeds ---
    static constexpr int scopeSize = 4096;
    std::array<float, scopeSize> scopeBuf {};
    std::atomic<int> scopeWrite { 0 };
    std::atomic<float> uiCutoff { 1000.0f };
    std::atomic<int> uiVoices { 0 };
    std::atomic<bool> panicRequested { false };   // set from UI, handled on the audio thread
    std::atomic<int> safetyResets { 0 };
    std::atomic<float> dbgSynth { 0 }, dbgPost { 0 }, dbgMix { 0 }; // per-block stage peaks (diagnostics)          // counts NaN/instability recoveries

    // editor size persistence
    int editorWidth = 0, editorHeight = 0;

private:
    void parameterChanged (const juce::String& id, float newValue) override;
    void timerCallback() override;
    void loadPatternIntoParams (int index);
    void writePatternsToState (juce::ValueTree&);
    void readPatternsFromState (const juce::ValueTree&);
    std::atomic<float>& patVal (int pat, int lane, int step) { return patStore[(size_t) ((pat * numLanesC + lane) * numStepsC + step)]; }
    std::atomic<int>& patLen (int pat, int lane) { return patLenStore[(size_t) (pat * numLanesC + lane)]; }
    static constexpr int numLanesC = ic::numLanes, numStepsC = ic::numSteps;
    std::array<std::atomic<float>, ic::numPatterns * numLanesC * numStepsC> patStore;
    std::array<std::atomic<int>, ic::numPatterns * numLanesC> patLenStore;
    std::atomic<int> activePattern { 0 }, pendingPatternLoad { -1 };
    std::array<float, numLanesC * numStepsC> clipboard {};
    std::array<int, numLanesC> clipboardLen {};
    bool clipboardValid = false;
    bool loadingPattern = false;

    float val (const char* id) const { return apvts.getRawParameterValue (id)->load(); }
    void handleMidi (const juce::MidiMessage&);
    void buildFrame (double bpm);
    float stepValue (char lane, int step, int cycle) const;

    ic::Frame frame {};
    std::array<ic::Voice, ic::maxVoices> voices;
    uint64_t voiceCounter = 0;
    juce::Array<int> heldNotes;      // mono note stack
    float pitchBend = 0.0f;
    int currentProgram = 0;

    std::unique_ptr<juce::dsp::Oversampling<float>> os2x, os4x;
    juce::dsp::Oversampling<float>* oversampling = nullptr;
    int osFactor = 2;
    void setOversampling (int factor);
    void panic();
    void processChunk (juce::AudioBuffer<float>&, juce::MidiBuffer&, double beatOffset);
    int maxBlock = 512;
    double lastBpm = 120.0;
    juce::MidiBuffer chunkMidi;
    float smoothedRevSize = 0.5f;
    double sr = 44100.0, osRate = 88200.0;

    // sequencer / LFO state (base rate)
    double freeBeats = 0.0;
    float seqTarget[4] {}, seqSmooth[4] {};
    long long lastAbsStep = -1;
    int lastCycle = -1;
    float stepRandom = 0.0f, modWheel = 0.0f, aftertouch = 0.0f;
    float seqPitch = 0, pitchTarget = 0, gateLen = 1, gateSmooth = 1;
    bool slideOn = false;

    // FX
    juce::AudioBuffer<float> delayBuf;
    int delayWrite = 0;
    ic::IronVerb verb;
    ic::PostFX postFx;
    ic::CharFX charFx;
    // LFO state
    bool lfoFree[2] {};
    int lfoShape[2] {};
    float lfoHz[2] { 1, 1 }, lfoSm[2] {}, lfoHold[2] {}, lfoPrevT[2] {};
    double lfoFreePhase[2] {};
    long long lfoLastCycle[2] { -1, -1 };
    juce::Random lfoRng;
    juce::AudioBuffer<float> fxBuf, revIn, revOut;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (IroncoreProcessor)
};
