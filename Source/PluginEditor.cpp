#include "PluginEditor.h"
#include "Presets.h"
#include "BinaryData.h"
#include <complex>

namespace colours
{
const juce::Colour bg       { 0xff0f1012 };
const juce::Colour panel    { 0xff1a1c20 };
const juce::Colour edge     { 0xff2c3036 };
const juce::Colour steel    { 0xff9aa3ad };
const juce::Colour text     { 0xffd8dde3 };
const juce::Colour hot      { 0xffff5a1f };
const juce::Colour hotDim   { 0xff7a2a0e };
}

//==============================================================================
IronLookAndFeel::IronLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, colours::text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, colours::steel);
    setColour (juce::ComboBox::backgroundColourId, colours::bg);
    setColour (juce::ComboBox::outlineColourId, colours::edge);
    setColour (juce::ComboBox::textColourId, colours::text);
    setColour (juce::ComboBox::arrowColourId, colours::hot);
    setColour (juce::PopupMenu::backgroundColourId, colours::panel);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, colours::hotDim);
    setColour (juce::TextButton::buttonColourId, colours::bg);
    setColour (juce::TextButton::textColourOffId, colours::text);
}

void IronLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                        float start, float end, juce::Slider& s)
{
    const auto b = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (4);
    const float r = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f;
    const auto c = b.getCentre();
    const float big = s.getProperties()["big"] ? 1.0f : 0.0f;
    const float thick = 3.0f + big * 3.0f;

    juce::Path track;
    track.addCentredArc (c.x, c.y, r - thick, r - thick, 0, start, end, true);
    g.setColour (colours::edge);
    g.strokePath (track, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const bool bipolar = s.getMinimum() < 0 && s.getMaximum() > 0;
    const float from = bipolar ? (start + end) * 0.5f : start;
    const float to = start + pos * (end - start);
    juce::Path arc;
    arc.addCentredArc (c.x, c.y, r - thick, r - thick, 0, juce::jmin (from, to), juce::jmax (from, to), true);
    g.setColour (colours::hot);
    g.strokePath (arc, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float inner = r - thick * 2.2f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a3f46), c.x, c.y - inner,
                                             juce::Colour (0xff15171a), c.x, c.y + inner, false));
    g.fillEllipse (juce::Rectangle<float> (inner * 2, inner * 2).withCentre (c));

    const float ang = to - juce::MathConstants<float>::halfPi;
    g.setColour (colours::text);
    g.drawLine (c.x + std::cos (ang) * inner * 0.25f, c.y + std::sin (ang) * inner * 0.25f,
                c.x + std::cos (ang) * inner * 0.9f, c.y + std::sin (ang) * inner * 0.9f, 2.0f + big);
}

void IronLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (2);
    g.setColour (b.getToggleState() ? colours::hot : (over ? colours::edge.brighter() : colours::edge));
    g.fillRoundedRectangle (r, 4);
    g.setColour (b.getToggleState() ? juce::Colours::black : colours::text);
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawText (b.getButtonText(), r, juce::Justification::centred);
}

//==============================================================================
RaveSequencer::RaveSequencer (IroncoreProcessor& p) : proc (p) { startTimerHz (30); }

void RaveSequencer::timerCallback()
{
    const int s = proc.uiStep.load();
    if (s != lastStepShown) { lastStepShown = s; repaint(); }
}

juce::Rectangle<float> RaveSequencer::cell (int lane, int step) const
{
    const float labelW = 74.0f;
    const float w = ((float) getWidth() - labelW) / (float) ic::numSteps;
    const float rowH[] { 1.0f, 1.0f, 1.0f, 1.0f, 0.8f, 0.5f, 0.5f };
    const float unit = (float) getHeight() / 5.8f;
    float y = 0;
    for (int i = 0; i < lane; ++i) y += rowH[i] * unit;
    return { labelW + (float) step * w, y, w, rowH[lane] * unit };
}

void RaveSequencer::paint (juce::Graphics& g)
{
    const char* names[] { "FILTER", "FM", "CHARACTER", "PITCH", "GATE", "ACCENT", "SLIDE" };
    const int master = (int) proc.apvts.getRawParameterValue ("seqlen")->load();
    const bool on = proc.apvts.getRawParameterValue ("seqon")->load() > 0.5f;

    for (int lane = 0; lane < ic::numLanes; ++lane)
    {
        auto row = cell (lane, 0);
        g.setColour (colours::steel);
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        g.drawText (names[lane], juce::Rectangle<float> (0, row.getY(), 70, row.getHeight()), juce::Justification::centredLeft);

        const int len = juce::jmin (master, (int) proc.apvts.getRawParameterValue (ic::laneLenId (ic::seqLanes[lane]))->load());
        const int laneCur = lastStepShown >= 0 ? lastStepShown % len : -1;
        if (len < master)
        {
            g.setColour (colours::hot);
            g.setFont (juce::FontOptions (9.0f, juce::Font::bold));
            g.drawText (juce::String (len), juce::Rectangle<float> (50, row.getY(), 22, row.getHeight()), juce::Justification::centredRight);
        }
        for (int s = 0; s < ic::numSteps; ++s)
        {
            auto r = cell (lane, s).reduced (2.0f);
            const bool active = s < len;
            const bool current = on && s == laneCur;
            g.setColour (current ? colours::edge.brighter (0.3f) : colours::bg);
            g.fillRect (r);
            if (s % 4 == 0) { g.setColour (colours::edge.brighter (0.2f)); g.drawRect (r, 1.0f); }

            const float v = proc.apvts.getRawParameterValue (ic::stepId (ic::seqLanes[lane], s))->load();
            auto col = active ? colours::hot : colours::hotDim.withAlpha (0.4f);
            const char ln = ic::seqLanes[lane];
            if (ic::isToggleLane (ln))
            {
                if (v > 0.5f) { g.setColour (ln == 's' ? col.withRotatedHue (0.1f) : col); g.fillRect (r.reduced (3.0f)); }
            }
            else if (ln == 'g')
            {
                g.setColour (col);
                g.fillRect (r.withTrimmedTop (r.getHeight() * (1.0f - v)).reduced (2.0f, 0));
            }
            else
            {
                const float mid = r.getCentreY();
                const float yv = r.getBottom() - v * r.getHeight();
                g.setColour (colours::edge);
                g.drawHorizontalLine ((int) mid, r.getX(), r.getRight());
                g.setColour (col);
                g.fillRect (juce::Rectangle<float>::leftTopRightBottom (r.getX() + 2, juce::jmin (mid, yv), r.getRight() - 2, juce::jmax (mid, yv) + 1));
                if (ln == 'p')
                {
                    const int st = ic::pitchSemis (v);
                    if (st != 0)
                    {
                        g.setColour (colours::text);
                        g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
                        g.drawText ((st > 0 ? "+" : "") + juce::String (st), r, st > 0 ? juce::Justification::centredBottom : juce::Justification::centredTop);
                    }
                }
            }
        }
    }
}

void RaveSequencer::edit (const juce::MouseEvent& e, bool first)
{
    const auto pos = e.position;
    for (int lane = 0; lane < ic::numLanes; ++lane)
        for (int s = 0; s < ic::numSteps; ++s)
        {
            auto r = cell (lane, s);
            if (! r.contains (pos.x, first ? pos.y : r.getCentreY())) continue;
            if (first) dragLane = lane;
            if (lane != dragLane) continue;

            auto* p = proc.apvts.getParameter (ic::stepId (ic::seqLanes[lane], s));
            if (! gestures.contains (p)) { p->beginChangeGesture(); gestures.add (p); }
            float v;
            const char ln = ic::seqLanes[lane];
            if (ic::isToggleLane (ln))
            {
                if (first) accentPaint = p->getValue() > 0.5f ? 0.0f : 1.0f;
                v = accentPaint;
            }
            else
                v = juce::jlimit (0.0f, 1.0f, (r.getBottom() - pos.y) / r.getHeight());
            if (ln == 'p') v = 0.5f + (float) ic::pitchSemis (v) / 24.0f;
            if (e.mods.isAltDown() || e.getNumberOfClicks() > 1)
                v = ic::isToggleLane (ln) ? 0.0f : ln == 'g' ? 1.0f : 0.5f;
            p->setValueNotifyingHost (v);
            repaint();
            return;
        }
}

void RaveSequencer::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) // right-click: set this lane's length (polyrhythm)
    {
        for (int lane = 0; lane < ic::numLanes; ++lane)
            for (int s = 0; s < ic::numSteps; ++s)
                if (cell (lane, s).contains (e.position))
                {
                    auto* p = proc.apvts.getParameter (ic::laneLenId (ic::seqLanes[lane]));
                    const int cur = (int) proc.apvts.getRawParameterValue (ic::laneLenId (ic::seqLanes[lane]))->load();
                    const int newLen = (cur == s + 1) ? ic::numSteps : s + 1; // right-click same step again resets
                    p->beginChangeGesture();
                    p->setValueNotifyingHost (p->convertTo0to1 ((float) newLen));
                    p->endChangeGesture();
                    repaint();
                    return;
                }
        return;
    }
    edit (e, true);
}
void RaveSequencer::mouseDrag (const juce::MouseEvent& e) { if (! e.mods.isPopupMenu()) edit (e, false); }
void RaveSequencer::mouseUp (const juce::MouseEvent&)
{
    for (auto* p : gestures) p->endChangeGesture();
    gestures.clearQuick();
    dragLane = -1;
}

//==============================================================================
IroncoreEditor::IroncoreEditor (IroncoreProcessor& p) : AudioProcessorEditor (p), scope (p), proc (p), sequencer (p)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (root);
    root.painter = [this] (juce::Graphics& g) { paintContent (g); };
    root.addAndMakeVisible (scope);

    for (auto [id, name] : { std::pair { "heat", "HEAT" }, { "bite", "BITE" }, { "morph", "MORPH" },
                             { "movement", "MOVEMENT" }, { "density", "DENSITY" } })
        knob (id, name, true);

    for (auto [id, name] : { std::pair { "o1wave", "OSC1 SHAPE" }, { "o2wave", "OSC2 SHAPE" }, { "o2semi", "SEMI" },
                             { "o2fine", "FINE" }, { "o2mix", "OSC2" }, { "fm", "FM" }, { "ring", "RING" },
                             { "sub", "SUB" }, { "noise", "NOISE" },
                             { "cutoff", "CUTOFF" }, { "reso", "RESO" }, { "fdrive", "DRIVE" }, { "fenv", "ENV AMT" },
                             { "fkey", "KEYTRK" },
                             { "sat", "SATURATE" }, { "fold", "FOLD" }, { "clip", "CLIP" }, { "crush", "CRUSH" },
                             { "aa", "A" }, { "ad", "D" }, { "as", "S" }, { "ar", "R" },
                             { "fa", "F-A" }, { "fd", "F-D" }, { "fs", "F-S" }, { "fr", "F-R" },
                             { "l1amt", "LFO1>CUT" }, { "l2amt", "LFO2>CHAR" }, { "glide", "GLIDE" },
                             { "seqlen", "LENGTH" }, { "swing", "SWING" }, { "seqdepth", "DEPTH" }, { "accent", "ACCENT" }, { "mutate", "MUTATE" },
                             { "dfb", "DLY FB" }, { "dlevel", "DELAY" }, { "rsize", "SIZE" }, { "rlevel", "REVERB" },
                             { "fxmix", "DRY/FX" }, { "rdamp", "DAMP" }, { "rpre", "PRE-DLY" }, { "out", "OUTPUT" },
                             { "drift", "DRIFT" }, { "fmfb", "FM FDBK" }, { "f2cut", "CUTOFF 2" }, { "f2reso", "RESO 2" },
                             { "lowcut", "LOW CUT" }, { "mideq", "MID" }, { "tilt", "TILT" }, { "width", "WIDTH" }, { "pump", "PUMP" },
                             { "penv", "PITCH ENV" }, { "pdecay", "P-DECAY" }, { "env3a", "E3-A" }, { "env3d", "E3-D" }, { "env3s", "E3-S" }, { "env3r", "E3-R" },
                             { "l1hz", "LFO1 HZ" }, { "l2hz", "LFO2 HZ" }, { "cfxmix", "MIX" }, { "cfxp1", "PARAM 1" }, { "cfxp2", "PARAM 2" } })
        knob (id, name);

    for (auto [id, name] : { std::pair { "fmode", "MODE" }, { "dorder", "ORDER" }, { "l1rate", "LFO1" },
                             { "l2rate", "LFO2" }, { "vmode", "VOICE" }, { "seqrate", "RATE" },
                             { "evolve", "EVOLVE" }, { "dtime", "TIME" }, { "osctype", "TYPE" }, { "quality", "OVERS" },
                             { "unimode", "UNISON" }, { "dtype", "TYPE" }, { "drvpos", "DRIVE" }, { "f2mode", "MODE" },
                             { "f2route", "ROUTE" }, { "pumprate", "RATE" },
                             { "osc2type", "TYPE 2" }, { "l1shape", "SHAPE1" }, { "l2shape", "SHAPE2" }, { "cfxtype", "TYPE" }, { "fxorder", "ORDER" } })
        choice (id, name);

    for (int i = 0; i < ic::numModSlots; ++i)
    {
        choice (ic::modId ("src", i), "SRC");
        choice (ic::modId ("dst", i), "DEST");
        knob (ic::modId ("amt", i), "AMOUNT " + juce::String (i + 1));
    }

    toggle ("sync", "SYNC");
    toggle ("seqon", "SEQ ON");
    toggle ("seqretrig", "RETRIG");

    root.addAndMakeVisible (sequencer);

    root.addAndMakeVisible (presetBox);
    root.addAndMakeVisible (prevBtn);
    root.addAndMakeVisible (nextBtn);
    root.addAndMakeVisible (saveBtn);
    root.addAndMakeVisible (randomBtn);
    refreshPresetBox();
    presetBox.onChange = [this] { selectPreset (presetBox.getSelectedId()); };
    auto step = [this] (int d)
    {
        const int n = presetBox.getNumItems();
        if (n == 0) return;
        const int cur = juce::jmax (0, presetBox.getSelectedItemIndex());
        presetBox.setSelectedItemIndex ((cur + d + n) % n, juce::sendNotification);
    };
    prevBtn.onClick = [step] { step (-1); };
    nextBtn.onClick = [step] { step (1); };
    saveBtn.onClick = [this] { showSaveDialog(); };
    randomBtn.onClick = [this]
    {
        proc.randomize();
        presetBox.setSelectedId (0, juce::dontSendNotification);
        presetBox.setText ("Random", juce::dontSendNotification);
        sequencer.repaint();
    };

    root.addAndMakeVisible (menuBtn);
    root.addAndMakeVisible (panicBtn);
    {
        const auto logo = juce::ImageCache::getFromMemory (BinaryData::frequenzbetrieblogo_png, BinaryData::frequenzbetrieblogo_pngSize);
        logoBtn.setImages (false, true, true, logo, 0.85f, {}, logo, 1.0f, {}, logo, 0.7f, {});
        logoBtn.setTooltip ("IRONCORE ist ein Open-Source-Projekt von Frequenzbetrieb n.e.V. - www.frequenzbetrieb.de");
        logoBtn.setMouseCursor (juce::MouseCursor::PointingHandCursor);
        logoBtn.onClick = [] { juce::URL ("https://www.frequenzbetrieb.de").launchInDefaultBrowser(); };
        root.addAndMakeVisible (logoBtn);
    }
    root.addAndMakeVisible (statusLabel);
    panicBtn.setTooltip ("Stoppt alle Stimmen und loescht Delay- und Reverb-Fahnen sofort");
    panicBtn.onClick = [this] { proc.panicRequested.store (true); };
    statusLabel.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    statusLabel.setColour (juce::Label::textColourId, colours::steel);
    statusLabel.setJustificationType (juce::Justification::centredRight);
    menuBtn.setTooltip ("Preset umbenennen, loeschen, Ordner oeffnen");
    menuBtn.onClick = [this] { showPresetMenu(); };
    saveBtn.setTooltip ("Aktuellen Sound als eigenes Preset speichern");
    randomBtn.setTooltip ("Zufaelliger, musikalisch begrenzter Sound inkl. Pattern");
    sequencer.setTooltip ("Ziehen = Wert setzen | Doppelklick/Alt-Klick = Reset | Rechtsklick = Spurlaenge");

    for (int i = 0; i < ic::numPatterns; ++i)
    {
        auto& b = patternBtns[(size_t) i];
        b.setButtonText (juce::String (i + 1));
        b.setClickingTogglesState (false);
        b.setColour (juce::TextButton::buttonOnColourId, colours::hot);
        b.setColour (juce::TextButton::textColourOnId, juce::Colours::black);
        b.setTooltip ("Pattern " + juce::String (i + 1) + "  (MIDI-Note " + juce::MidiMessage::getMidiNoteName (i, true, true, 3) + " schaltet um)");
        b.onClick = [this, i] { proc.selectPattern (i); };
        root.addAndMakeVisible (b);
    }
    for (auto* b : { &copyBtn, &pasteBtn, &clearBtn }) root.addAndMakeVisible (*b);
    copyBtn.onClick = [this] { proc.copyPattern(); };
    pasteBtn.onClick = [this] { proc.pastePattern(); sequencer.repaint(); };
    clearBtn.onClick = [this] { proc.clearPattern(); sequencer.repaint(); };
    startTimerHz (15);

    int w = proc.editorWidth, h = proc.editorHeight; // read before the constrainer triggers resized()
    setResizable (true, true);
    setResizeLimits (baseW / 2, baseH / 2, baseW * 3 / 2, baseH * 3 / 2);
    getConstrainer()->setFixedAspectRatio ((double) baseW / (double) baseH);
    if (w <= 0 || h <= 0)
    {
        w = baseW; h = baseH;
        if (auto* d = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
        {
            const auto area = d->userArea;
            const double sc = juce::jmin (1.0, (area.getWidth() - 40) / (double) baseW, (area.getHeight() - 80) / (double) baseH);
            w = juce::roundToInt (baseW * sc); h = juce::roundToInt (baseH * sc);
        }
    }
    h = juce::roundToInt (w * (double) baseH / (double) baseW); // keep aspect if an older layout size was saved
    layoutContent();
    setSize (w, h);
}

IroncoreEditor::~IroncoreEditor() { setLookAndFeel (nullptr); }

void IroncoreEditor::refreshPresetBox()
{
    presetBox.clear (juce::dontSendNotification);
    presetBox.addSectionHeading ("FACTORY");
    int selected = 0;
    for (int i = 0; i < proc.getNumPrograms(); ++i)
    {
        presetBox.addItem (proc.getProgramName (i), i + 1);
        if (proc.getProgramName (i) == proc.currentPresetName) selected = i + 1;
    }
    userPresets = proc.getUserPresets();
    if (! userPresets.isEmpty())
    {
        presetBox.addSectionHeading ("USER");
        for (int j = 0; j < userPresets.size(); ++j)
        {
            const auto& f = userPresets[j];
            presetBox.addItem (f.getParentDirectory().getFileName() + " / " + f.getFileNameWithoutExtension(), 1000 + j);
            if (f.getFileNameWithoutExtension() == proc.currentPresetName) selected = 1000 + j;
        }
    }
    if (selected > 0) presetBox.setSelectedId (selected, juce::dontSendNotification);
    else presetBox.setText (proc.currentPresetName, juce::dontSendNotification);
}

void IroncoreEditor::selectPreset (int id)
{
    if (id <= 0) return;
    if (id >= 1000)
    {
        if (juce::isPositiveAndBelow (id - 1000, userPresets.size()))
            proc.loadUserPreset (userPresets[id - 1000]);
    }
    else
        proc.setCurrentProgram (id - 1);
    sequencer.repaint();
}

void IroncoreEditor::showSaveDialog()
{
    saveDialog = std::make_unique<juce::AlertWindow> ("Save Preset", "Name und Kategorie fuer das Preset:", juce::MessageBoxIconType::NoIcon, this);
    saveDialog->addTextEditor ("name", proc.currentPresetName == "Random" ? "My Lead" : proc.currentPresetName, "Name");
    saveDialog->addComboBox ("cat", IroncoreProcessor::presetCategories(), "Kategorie");
    if (auto* cb = saveDialog->getComboBoxComponent ("cat")) cb->setSelectedItemIndex (IroncoreProcessor::presetCategories().size() - 1);
    saveDialog->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    saveDialog->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    saveDialog->enterModalState (true, juce::ModalCallbackFunction::create ([this] (int result)
    {
        if (result == 1 && saveDialog)
        {
            const auto name = saveDialog->getTextEditorContents ("name");
            const auto cat = saveDialog->getComboBoxComponent ("cat")->getText();
            if (proc.saveUserPreset (name, cat)) refreshPresetBox();
            else juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "IRONCORE", "Preset konnte nicht gespeichert werden.");
        }
        saveDialog.reset();
    }), false);
}

IroncoreEditor::Knob& IroncoreEditor::knob (const juce::String& id, const juce::String& name, bool big)
{
    auto k = std::make_unique<Knob>();
    k->slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k->slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    k->slider.setPopupDisplayEnabled (true, true, &root);
    k->slider.getProperties().set ("big", big);
    k->label.setText (name, juce::dontSendNotification);
    k->label.setJustificationType (juce::Justification::centred);
    k->label.setFont (juce::FontOptions (big ? 15.0f : 11.0f, juce::Font::bold));
    if (big) k->label.setColour (juce::Label::textColourId, colours::text);
    k->att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, k->slider);
    if (auto* p = proc.apvts.getParameter (id))
        k->slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
    root.addAndMakeVisible (k->slider);
    root.addAndMakeVisible (k->label);
    auto& ref = *k;
    knobs[id] = std::move (k);
    return ref;
}

IroncoreEditor::Choice& IroncoreEditor::choice (const juce::String& id, const juce::String& name)
{
    auto c = std::make_unique<Choice>();
    if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (id)))
        c->box.addItemList (p->choices, 1);
    c->label.setText (name, juce::dontSendNotification);
    c->label.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    c->att = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, id, c->box);
    root.addAndMakeVisible (c->box);
    root.addAndMakeVisible (c->label);
    auto& ref = *c;
    choices[id] = std::move (c);
    return ref;
}

IroncoreEditor::Toggle& IroncoreEditor::toggle (const juce::String& id, const juce::String& name)
{
    auto t = std::make_unique<Toggle>();
    t->button.setButtonText (name);
    t->att = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, id, t->button);
    t->button.onClick = [this] { sequencer.repaint(); };
    root.addAndMakeVisible (t->button);
    auto& ref = *t;
    toggles[id] = std::move (t);
    return ref;
}

void IroncoreEditor::layoutKnobs (juce::Rectangle<int> area, const juce::StringArray& ids, int cols)
{
    const int rows = (ids.size() + cols - 1) / cols;
    const int cw = area.getWidth() / cols, ch = area.getHeight() / rows;
    for (int i = 0; i < ids.size(); ++i)
    {
        auto& k = *knobs.at (ids[i]);
        juce::Rectangle<int> cellR (area.getX() + (i % cols) * cw, area.getY() + (i / cols) * ch, cw, ch);
        const bool big = (bool) k.slider.getProperties()["big"];
        k.label.setBounds (cellR.removeFromBottom (big ? 22 : 16));
        k.slider.setBounds (cellR.reduced (2));
    }
}

static void placeChoice (juce::ComboBox& box, juce::Label& label, juce::Rectangle<int> r, int labelW = 52)
{
    label.setBounds (r.removeFromLeft (labelW));
    box.setBounds (r.reduced (0, 2));
}

void IroncoreEditor::resized()
{
    const float sc = (float) getWidth() / (float) baseW;
    root.setBounds (0, 0, baseW, baseH);
    root.setTransform (juce::AffineTransform::scale (sc));
    proc.editorWidth = getWidth();
    proc.editorHeight = getHeight();
}

void IroncoreEditor::timerCallback()
{
    updatePatternButtons();
    {
        static const char* p1[] { "PARAM 1", "TUNE", "RATE", "SHIFT", "LENGTH" };
        static const char* p2[] { "PARAM 2", "FEEDBACK", "RESO", "FEEDBACK", "CHANCE" };
        const int t = juce::jlimit (0, 4, (int) proc.apvts.getRawParameterValue ("cfxtype")->load());
        knobs.at ("cfxp1")->label.setText (p1[t], juce::dontSendNotification);
        knobs.at ("cfxp2")->label.setText (p2[t], juce::dontSendNotification);
    }
    juce::String st = "VOICES " + juce::String (proc.uiVoices.load());
    if (const int r = proc.safetyResets.load(); r > 0) st << "  |  SAFETY " << r;
    statusLabel.setText (st, juce::dontSendNotification);
}

void IroncoreEditor::updatePatternButtons()
{
    const int pat = proc.getActivePattern();
    if (pat != shownPattern)
    {
        shownPattern = pat;
        for (int i = 0; i < ic::numPatterns; ++i) patternBtns[(size_t) i].setToggleState (i == pat, juce::dontSendNotification);
        sequencer.repaint();
    }
    pasteBtn.setEnabled (proc.hasPatternClipboard());
}

void IroncoreEditor::showPresetMenu()
{
    const int id = presetBox.getSelectedId();
    const bool isUser = id >= 1000 && juce::isPositiveAndBelow (id - 1000, userPresets.size());
    juce::PopupMenu m;
    m.addItem (1, "Umbenennen...", isUser);
    m.addItem (2, "Loeschen", isUser);
    m.addSeparator();
    m.addItem (3, "Preset-Ordner im Finder zeigen");
    m.addItem (4, "Liste aktualisieren");
    m.addSeparator();
    m.addItem (5, "Ueber IRONCORE ...");
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (menuBtn), [this, isUser, id] (int r)
    {
        if (r == 3) { auto d = IroncoreProcessor::getUserPresetDir(); d.createDirectory(); d.revealToUser(); }
        if (r == 4) refreshPresetBox();
        if (r == 5) showAbout();
        if (! isUser) return;
        const auto file = userPresets[id - 1000];
        if (r == 2)
        {
            juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon, "Preset loeschen",
                "\"" + file.getFileNameWithoutExtension() + "\" in den Papierkorb legen?", "Loeschen", "Abbrechen", this,
                juce::ModalCallbackFunction::create ([this, file] (int ok)
                {
                    if (ok && proc.deleteUserPreset (file)) refreshPresetBox();
                }));
        }
        if (r == 1)
        {
            saveDialog = std::make_unique<juce::AlertWindow> ("Preset umbenennen", "Neuer Name:", juce::MessageBoxIconType::NoIcon, this);
            saveDialog->addTextEditor ("name", file.getFileNameWithoutExtension(), "Name");
            saveDialog->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
            saveDialog->addButton ("Abbrechen", 0, juce::KeyPress (juce::KeyPress::escapeKey));
            saveDialog->enterModalState (true, juce::ModalCallbackFunction::create ([this, file] (int res)
            {
                if (res == 1 && saveDialog)
                {
                    if (proc.renameUserPreset (file, saveDialog->getTextEditorContents ("name"))) refreshPresetBox();
                    else juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "IRONCORE", "Umbenennen fehlgeschlagen (Name schon vergeben?).");
                }
                saveDialog.reset();
            }), false);
        }
    });
}

void IroncoreEditor::showAbout()
{
    juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "IRONCORE " IRONCORE_VERSION_STRING,
        "Industrial Lead Synthesizer\n\n"
        "Ein Open-Source-Projekt von Frequenzbetrieb n.e.V.\n"
        "www.frequenzbetrieb.de\n\n"
        "Quellcode: github.com/frequenzbetrieb/ironcore\n"
        "Lizenz: GNU AGPLv3 - frei nutzbar, veraenderbar und weitergebbar.\n\n"
        "Erstellt mit JUCE (AGPLv3). VST ist eine Marke der Steinberg Media Technologies GmbH.",
        "OK", this);
}

void IroncoreEditor::layoutContent()
{
    auto area = juce::Rectangle<int> (0, 0, baseW, baseH).reduced (12);
    auto header = area.removeFromTop (44);
    header.removeFromLeft (330);
    panicBtn.setBounds (header.removeFromLeft (70).reduced (2, 8));
    statusLabel.setBounds (header.removeFromLeft (150).reduced (2, 8));
    header.removeFromLeft (16);
    logoBtn.setBounds (header.removeFromLeft (230).reduced (0, 7));
    randomBtn.setBounds (header.removeFromRight (80).reduced (2, 8));
    saveBtn.setBounds (header.removeFromRight (64).reduced (2, 8));
    menuBtn.setBounds (header.removeFromRight (36).reduced (2, 8));
    header.removeFromRight (8);
    nextBtn.setBounds (header.removeFromRight (34).reduced (2, 8));
    presetBox.setBounds (header.removeFromRight (300).reduced (2, 8));
    prevBtn.setBounds (header.removeFromRight (34).reduced (2, 8));

    area.removeFromTop (6);
    macroArea = area.removeFromTop (160);
    area.removeFromTop (8);
    matrixArea = area.removeFromRight (262);
    area.removeFromRight (8);
    {
        auto m = macroArea;
        displayArea = m.removeFromRight (500).reduced (10, 10).withTrimmedTop (18);
        scope.setBounds (displayArea);
        layoutKnobs (m.reduced (30, 22).withTrimmedTop (4), { "heat", "bite", "morph", "movement", "density" }, 5);
    }

    auto row = area.removeFromTop (226);
    oscArea = row.removeFromLeft (340); row.removeFromLeft (8);
    filterArea = row.removeFromLeft (250); row.removeFromLeft (8);
    damageArea = row.removeFromLeft (230); row.removeFromLeft (8);
    envArea = row;

    area.removeFromTop (8);
    {
        auto row2 = area.removeFromTop (150);
        analogArea = row2.removeFromLeft (250); row2.removeFromLeft (8);
        filt2Area = row2.removeFromLeft (240); row2.removeFromLeft (8);
        eqArea = row2.removeFromLeft (170); row2.removeFromLeft (8);
        stereoArea = row2.removeFromLeft (140); row2.removeFromLeft (8);
        charArea = row2;
    }

    {
        auto r = oscArea.reduced (8).withTrimmedTop (22);
        auto bottom = r.removeFromBottom (26);
        toggles.at ("sync")->button.setBounds (bottom.removeFromLeft (80));
        bottom.removeFromLeft (12);
        auto& ot = *choices.at ("osctype");
        placeChoice (ot.box, ot.label, bottom.removeFromLeft (bottom.getWidth() / 2 - 4), 22);
        bottom.removeFromLeft (8);
        auto& ot2 = *choices.at ("osc2type");
        placeChoice (ot2.box, ot2.label, bottom, 22);
        ot.label.setText ("1", juce::dontSendNotification);
        ot2.label.setText ("2", juce::dontSendNotification);
        layoutKnobs (r, { "o1wave", "o2wave", "o2semi", "o2fine", "o2mix", "fm", "ring", "sub", "noise" }, 5);
    }
    {
        auto r = filterArea.reduced (8).withTrimmedTop (22);
        auto& c = *choices.at ("fmode");
        placeChoice (c.box, c.label, r.removeFromBottom (26));
        layoutKnobs (r, { "cutoff", "reso", "fdrive", "fenv", "fkey" }, 3);
    }
    {
        auto r = damageArea.reduced (8).withTrimmedTop (22);
        auto& dt = *choices.at ("dtype");
        placeChoice (dt.box, dt.label, r.removeFromBottom (26));
        auto& c = *choices.at ("dorder");
        placeChoice (c.box, c.label, r.removeFromBottom (26));
        layoutKnobs (r, { "sat", "fold", "clip", "crush" }, 2);
    }
    layoutKnobs (envArea.reduced (8).withTrimmedTop (22), { "aa", "ad", "as", "ar", "fa", "fd", "fs", "fr", "env3a", "env3d", "env3s", "env3r" }, 4);

    {
        auto r = analogArea.reduced (8).withTrimmedTop (22);
        auto& u = *choices.at ("unimode");
        placeChoice (u.box, u.label, r.removeFromBottom (26));
        u.box.setTooltip ("Supersaw: 7 Stimmen, DENSITY regelt die Verstimmung");
        layoutKnobs (r, { "drift", "fmfb", "penv", "pdecay" }, 4);
    }
    {
        auto r = filt2Area.reduced (8).withTrimmedTop (22);
        auto combos = r.removeFromRight (140);
        for (auto id : { "f2mode", "f2route", "drvpos" })
        {
            auto& c = *choices.at (id);
            placeChoice (c.box, c.label, combos.removeFromTop (30).reduced (0, 2));
        }
        layoutKnobs (r, { "f2cut", "f2reso" }, 2);
    }
    layoutKnobs (eqArea.reduced (8).withTrimmedTop (22), { "lowcut", "mideq", "tilt" }, 3);
    {
        auto r = stereoArea.reduced (8).withTrimmedTop (22);
        auto& pr = *choices.at ("pumprate");
        placeChoice (pr.box, pr.label, r.removeFromBottom (26));
        layoutKnobs (r, { "width", "pump" }, 2);
    }
    {
        auto r = charArea.reduced (8).withTrimmedTop (22);
        auto top = r.removeFromTop (24);
        auto& ct = *choices.at ("cfxtype");
        placeChoice (ct.box, ct.label, top.removeFromLeft (top.getWidth() / 2 - 4).reduced (0, 1), 34);
        top.removeFromLeft (8);
        auto& co = *choices.at ("fxorder");
        placeChoice (co.box, co.label, top.reduced (0, 1), 44);
        choices.at ("fxorder")->box.setTooltip ("Wo der Charakter-Effekt sitzt: vor Delay/Reverb, hinter dem ganzen Mix oder nur auf dem Effekt-Signal");
        layoutKnobs (r, { "cfxmix", "cfxp1", "cfxp2" }, 3);
    }

    area.removeFromTop (8);
    seqArea = area.removeFromLeft (690); area.removeFromLeft (8);
    modArea = area.removeFromLeft (160); area.removeFromLeft (8);
    fxArea = area;

    {
        {
            auto title = seqArea.reduced (8).removeFromTop (22).withTrimmedLeft (220);
            for (auto* b : { &clearBtn, &pasteBtn, &copyBtn }) b->setBounds (title.removeFromRight (54).reduced (2, 1));
            title.removeFromRight (10);
            for (int i = ic::numPatterns - 1; i >= 0; --i) patternBtns[(size_t) i].setBounds (title.removeFromRight (26).reduced (1, 1));
            title.removeFromRight (4);
        }
        auto r = seqArea.reduced (8).withTrimmedTop (26);
        auto controls = r.removeFromBottom (74);
        sequencer.setBounds (r.reduced (0, 4));
        auto left = controls.removeFromLeft (200);
        auto tg = left.removeFromTop (24);
        toggles.at ("seqon")->button.setBounds (tg.removeFromLeft (90));
        toggles.at ("seqretrig")->button.setBounds (tg.removeFromLeft (90).withTrimmedLeft (6));
        auto& rate = *choices.at ("seqrate");
        placeChoice (rate.box, rate.label, left.removeFromTop (24));
        auto& ev = *choices.at ("evolve");
        placeChoice (ev.box, ev.label, left.removeFromTop (24));
        layoutKnobs (controls, { "seqlen", "swing", "seqdepth", "accent", "mutate" }, 5);
    }
    {
        auto r = modArea.reduced (8).withTrimmedTop (22);
        for (auto id : { "l1rate", "l1shape", "l2rate", "l2shape", "vmode" })
        {
            auto& c = *choices.at (id);
            placeChoice (c.box, c.label, r.removeFromTop (26));
        }
        layoutKnobs (r, { "l1amt", "l2amt", "l1hz", "l2hz", "glide" }, 2);
    }
    {
        auto r = matrixArea.reduced (10).withTrimmedTop (22);
        const int slotH = r.getHeight() / ic::numModSlots;
        for (int i = 0; i < ic::numModSlots; ++i)
        {
            auto slot = r.removeFromTop (slotH).reduced (0, 4);
            auto combos = slot.removeFromLeft (150);
            combos = combos.withSizeKeepingCentre (150, 56);
            auto& src = *choices.at (ic::modId ("src", i));
            placeChoice (src.box, src.label, combos.removeFromTop (28));
            auto& dst = *choices.at (ic::modId ("dst", i));
            placeChoice (dst.box, dst.label, combos.removeFromTop (28));
            auto& k = *knobs.at (ic::modId ("amt", i));
            k.label.setBounds (slot.removeFromBottom (16));
            k.slider.setBounds (slot.reduced (4));
        }
    }
    {
        auto r = fxArea.reduced (8).withTrimmedTop (22);
        auto& c = *choices.at ("dtime");
        placeChoice (c.box, c.label, r.removeFromTop (26));
        auto& q = *choices.at ("quality");
        placeChoice (q.box, q.label, r.removeFromTop (26));
        q.box.setTooltip ("4x: sauberere Hoehen bei viel HEAT/FOLD, braucht ca. doppelte CPU");
        layoutKnobs (r, { "dlevel", "dfb", "rlevel", "rsize", "rdamp", "rpre", "fxmix", "out" }, 2);
    }
}

void IroncoreEditor::paint (juce::Graphics& g) { g.fillAll (colours::bg); }

void IroncoreEditor::paintContent (juce::Graphics& g)
{
    g.fillAll (colours::bg);

    // header
    g.setColour (colours::hot);
    g.setFont (juce::FontOptions (30.0f, juce::Font::bold));
    g.drawText ("IRONCORE", 14, 10, 200, 40, juce::Justification::centredLeft);
    g.setColour (colours::steel);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.drawText ("INDUSTRIAL LEAD SYNTHESIZER", 186, 24, 200, 20, juce::Justification::centredLeft);

    auto drawPanel = [&] (juce::Rectangle<int> r, const juce::String& title)
    {
        g.setColour (colours::panel);
        g.fillRoundedRectangle (r.toFloat(), 6);
        g.setColour (colours::edge);
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 6, 1);
        g.setColour (colours::hot);
        g.fillRect (r.getX() + 10, r.getY() + 10, 3, 12);
        g.setColour (colours::text);
        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        g.drawText (title, r.getX() + 18, r.getY() + 7, r.getWidth() - 24, 18, juce::Justification::centredLeft);
    };
    drawPanel (macroArea, "PERFORMANCE MACROS");
    drawPanel (oscArea, "1  CORE OSCILLATOR");
    drawPanel (filterArea, "2  IRON FILTER");
    drawPanel (damageArea, "3  DAMAGE CIRCUIT");
    drawPanel (envArea, "ENVELOPES  AMP / FILTER / ENV 3");
    drawPanel (seqArea, "4  RAVE MOTION SEQUENCER");
    drawPanel (modArea, "LFO / VOICE");
    drawPanel (fxArea, "FX / OUTPUT");
    drawPanel (matrixArea, "MOD MATRIX");
    drawPanel (analogArea, "ANALOG / PITCH ENV");
    drawPanel (filt2Area, "FILTER 2 / DRIVE");
    drawPanel (eqArea, "POST EQ");
    drawPanel (stereoArea, "STEREO / PUMP");
    drawPanel (charArea, "CHARACTER FX");
    g.setColour (colours::edge);
    for (int i = 1; i < ic::numModSlots; ++i)
    {
        const int y = matrixArea.getY() + 32 + (matrixArea.getHeight() - 42) * i / ic::numModSlots;
        g.drawHorizontalLine (y, (float) matrixArea.getX() + 10, (float) matrixArea.getRight() - 10);
    }
    g.setColour (colours::steel);
    g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
    g.drawText ("PATTERN", patternBtns[0].getX() - 56, patternBtns[0].getY(), 52, patternBtns[0].getHeight(), juce::Justification::centredRight);
    g.drawText ("SCOPE", displayArea.getX(), displayArea.getY() - 16, 100, 14, juce::Justification::centredLeft);
    g.drawText ("FILTER", displayArea.getCentreX() + 6, displayArea.getY() - 16, 100, 14, juce::Justification::centredLeft);

    // morph legend under the MORPH macro
    g.setColour (colours::steel.withAlpha (0.7f));
    g.setFont (juce::FontOptions (10.0f));
    if (auto it = knobs.find ("morph"); it != knobs.end())
        g.drawText ("SAW  >  FM METAL  >  FORMANT", it->second->label.getBounds().translated (0, 14).expanded (40, 0),
                    juce::Justification::centred);
}

//==============================================================================
void ScopeView::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    auto scopeR = b.removeFromLeft (b.getWidth() * 0.5f - 6.0f);
    b.removeFromLeft (12.0f);
    auto filtR = b;

    for (auto r : { scopeR, filtR })
    {
        g.setColour (colours::bg);
        g.fillRoundedRectangle (r, 4);
        g.setColour (colours::edge);
        g.drawRoundedRectangle (r, 4, 1);
    }

    // --- oscilloscope: trigger on rising zero crossing ---
    const int w = proc.scopeWrite.load (std::memory_order_acquire);
    const int n = (int) snap.size();
    for (int i = 0; i < n; ++i)
        snap[(size_t) i] = proc.scopeBuf[(size_t) ((w - n + i + IroncoreProcessor::scopeSize) & (IroncoreProcessor::scopeSize - 1))];
    int start = 0;
    for (int i = 1; i < n / 2; ++i)
        if (snap[(size_t) i - 1] <= 0.0f && snap[(size_t) i] > 0.0f) { start = i; break; }
    const int len = n / 2;
    juce::Path wave;
    for (int i = 0; i < len; ++i)
    {
        const float x = scopeR.getX() + scopeR.getWidth() * (float) i / (float) (len - 1);
        const float y = scopeR.getCentreY() - juce::jlimit (-1.0f, 1.0f, snap[(size_t) (start + i)]) * scopeR.getHeight() * 0.45f;
        if (i == 0) wave.startNewSubPath (x, y); else wave.lineTo (x, y);
    }
    g.setColour (colours::hot);
    g.strokePath (wave, juce::PathStrokeType (1.5f));

    // --- filter response (analytic, at the current effective cutoff) ---
    auto& s = proc.apvts;
    const float fc = proc.uiCutoff.load();
    const float reso = juce::jmin (0.985f, s.getRawParameterValue ("reso")->load() + s.getRawParameterValue ("bite")->load() * 0.35f);
    const int mode = (int) s.getRawParameterValue ("fmode")->load();
    const float k = 2.0f - 2.0f * reso;
    juce::Path curve;
    const int pts = 120;
    for (int i = 0; i < pts; ++i)
    {
        const float hz = 20.0f * std::pow (1000.0f, (float) i / (float) (pts - 1));
        const std::complex<float> sj (0.0f, hz / fc);
        std::complex<float> h;
        if (mode == 0)
        {
            const float fb = reso * 4.1f;
            h = (1.0f + fb * 0.25f) / (std::pow (1.0f + sj, 4.0f) + fb);
        }
        else
        {
            const auto den = sj * sj + k * sj + 1.0f;
            h = mode == 1 ? 1.0f / den : mode == 2 ? (sj * (0.3f + k)) / den : (sj * sj) / den;
        }
        const float db = juce::jlimit (-36.0f, 24.0f, juce::Decibels::gainToDecibels (std::abs (h), -60.0f));
        const float x = filtR.getX() + filtR.getWidth() * (float) i / (float) (pts - 1);
        const float y = filtR.getY() + filtR.getHeight() * (1.0f - (db + 36.0f) / 60.0f);
        if (i == 0) curve.startNewSubPath (x, y); else curve.lineTo (x, y);
    }
    g.setColour (colours::edge);
    const float zeroY = filtR.getY() + filtR.getHeight() * (1.0f - 36.0f / 60.0f);
    g.drawHorizontalLine ((int) zeroY, filtR.getX(), filtR.getRight());
    juce::Path fill (curve);
    fill.lineTo (filtR.getRight(), filtR.getBottom());
    fill.lineTo (filtR.getX(), filtR.getBottom());
    fill.closeSubPath();
    g.setColour (colours::hot.withAlpha (0.15f));
    g.fillPath (fill);
    g.setColour (colours::hot);
    g.strokePath (curve, juce::PathStrokeType (1.5f));
    g.setColour (colours::steel);
    g.setFont (juce::FontOptions (10.0f));
    g.drawText (juce::String (juce::roundToInt (fc)) + " Hz", filtR.reduced (4), juce::Justification::topRight);
}
