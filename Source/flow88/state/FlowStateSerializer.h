#pragma once

#include "FlowSessionState.h"

namespace flow88::state {

juce::ValueTree createPluginState(
    juce::AudioProcessorValueTreeState &apvts,
    const FlowSessionState &sessionState);

void restorePluginState(const juce::ValueTree &root,
                        juce::AudioProcessorValueTreeState &apvts,
                        FlowSessionState &sessionState);

} // namespace flow88::state
