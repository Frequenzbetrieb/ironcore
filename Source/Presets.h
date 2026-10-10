#pragma once
#include <JuceHeader.h>

namespace ic
{
int numPresets();
juce::String presetName (int index);
void applyPreset (int index, juce::AudioProcessorValueTreeState& apvts);
}
