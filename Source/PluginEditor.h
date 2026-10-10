#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class IronLookAndFeel : public juce::LookAndFeel_V4
{
public:
    IronLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float start, float end, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool, bool) override;
};

class RaveSequencer : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    explicit RaveSequencer (IroncoreProcessor&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    void edit (const juce::MouseEvent&, bool first);
    juce::Rectangle<float> cell (int lane, int step) const;
    IroncoreProcessor& proc;
    int dragLane = -1, lastStepShown = -2;
    float accentPaint = 1.0f;
    juce::Array<juce::RangedAudioParameter*> gestures;
};

class ScopeView : public juce::Component, private juce::Timer
{
public:
    explicit ScopeView (IroncoreProcessor& p) : proc (p) { startTimerHz (30); }
    void paint (juce::Graphics&) override;
private:
    void timerCallback() override { repaint(); }
    IroncoreProcessor& proc;
    std::array<float, 1024> snap {};
};

class IroncoreEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit IroncoreEditor (IroncoreProcessor&);
    ~IroncoreEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    static constexpr int baseW = 1390, baseH = 1066;

private:
    struct Root : juce::Component
    {
        std::function<void (juce::Graphics&)> painter;
        void paint (juce::Graphics& g) override { if (painter) painter (g); }
    };
    void paintContent (juce::Graphics&);
    void layoutContent();
    void timerCallback() override;
    void showPresetMenu();
    void updatePatternButtons();
    Root root;
    juce::TooltipWindow tooltips { this, 600 };
    ScopeView scope;
    juce::TextButton panicBtn { "PANIC" };
    juce::ImageButton logoBtn;
    void showAbout();
    juce::Label statusLabel;
    juce::TextButton menuBtn { "..." }, copyBtn { "COPY" }, pasteBtn { "PASTE" }, clearBtn { "CLEAR" };
    std::array<juce::TextButton, ic::numPatterns> patternBtns;
    int shownPattern = -1;
    juce::Rectangle<int> displayArea;

    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
    };
    struct Choice
    {
        juce::ComboBox box;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> att;
    };
    struct Toggle
    {
        juce::ToggleButton button;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> att;
    };

    Knob& knob (const juce::String& id, const juce::String& name, bool big = false);
    Choice& choice (const juce::String& id, const juce::String& name);
    Toggle& toggle (const juce::String& id, const juce::String& name);
    void layoutKnobs (juce::Rectangle<int> area, const juce::StringArray& ids, int cols);
    void refreshPresetBox();

    IroncoreProcessor& proc;
    IronLookAndFeel lnf;
    std::map<juce::String, std::unique_ptr<Knob>> knobs;
    std::map<juce::String, std::unique_ptr<Choice>> choices;
    std::map<juce::String, std::unique_ptr<Toggle>> toggles;

    juce::ComboBox presetBox;
    juce::TextButton prevBtn { "<" }, nextBtn { ">" }, saveBtn { "SAVE" }, randomBtn { "RANDOM" };
    juce::Array<juce::File> userPresets;
    std::unique_ptr<juce::AlertWindow> saveDialog;
    void showSaveDialog();
    void selectPreset (int itemId);
    RaveSequencer sequencer;

    juce::Rectangle<int> charArea, analogArea, filt2Area, eqArea, stereoArea, matrixArea, macroArea, oscArea, filterArea, damageArea, envArea, seqArea, modArea, fxArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (IroncoreEditor)
};
