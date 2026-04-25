#include "FlowStateSerializer.h"

namespace flow88::state {

namespace {
constexpr auto kRootStateType = "Flow88MasterState";
} // namespace

juce::ValueTree createPluginState(
    juce::AudioProcessorValueTreeState &apvts,
    const FlowSessionState &sessionState) {
  juce::ValueTree root(kRootStateType);
  root.addChild(apvts.copyState(), -1, nullptr);
  root.addChild(sessionState.copyState(), -1, nullptr);
  return root;
}

void restorePluginState(const juce::ValueTree &root,
                        juce::AudioProcessorValueTreeState &apvts,
                        FlowSessionState &sessionState) {
  if (!root.hasType(kRootStateType))
    return;

  if (const auto parameterState = root.getChildWithName(apvts.state.getType());
      parameterState.isValid())
    apvts.replaceState(parameterState);

  if (const auto session = root.getChildWithName(FlowSessionState::kRootType);
      session.isValid())
    sessionState.replaceState(session);
}

} // namespace flow88::state
