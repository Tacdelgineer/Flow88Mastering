#pragma once

#include "FlowParameterIDs.h"

namespace flow88::state {

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

int findChoiceIndex(const juce::StringArray &choices, const juce::String &value);

void setChoiceParameter(juce::AudioProcessorValueTreeState &apvts,
                        const juce::String &parameterId, int index);
void setFloatParameter(juce::AudioProcessorValueTreeState &apvts,
                       const juce::String &parameterId, float value);
void setBoolParameter(juce::AudioProcessorValueTreeState &apvts,
                      const juce::String &parameterId, bool value);

int getChoiceParameterIndex(const juce::AudioProcessorValueTreeState &apvts,
                            const juce::String &parameterId);
float getFloatParameter(const juce::AudioProcessorValueTreeState &apvts,
                        const juce::String &parameterId);
bool getBoolParameter(const juce::AudioProcessorValueTreeState &apvts,
                      const juce::String &parameterId);

} // namespace flow88::state

