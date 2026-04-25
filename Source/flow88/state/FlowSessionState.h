#pragma once

#include "flow88/FlowJuceIncludes.h"

namespace flow88::state {

class FlowSessionState {
public:
  static constexpr const char *kRootType = "Flow88Session";

  FlowSessionState();

  const juce::ValueTree &getState() const noexcept { return state_; }
  juce::ValueTree copyState() const { return state_.createCopy(); }
  void replaceState(const juce::ValueTree &newState);

  void setAnalysisState(const juce::String &value);
  juce::String getAnalysisState() const;
  bool isAnalysisReady() const;
  bool isAnalysisInProgress() const;

  void setSuggestedProfile(const juce::String &subgenreId,
                           const juce::String &styleId);
  juce::String getSuggestedSubgenre() const;
  juce::String getSuggestedStyle() const;

  void setAssistantSummary(const juce::String &summary);
  juce::String getAssistantSummary() const;

  void setActiveABSlot(int slotIndex);
  int getActiveABSlot() const;

  void captureSlot(int slotIndex, const juce::ValueTree &parameterState);
  bool hasSnapshotForSlot(int slotIndex) const;
  juce::ValueTree restoreSlot(int slotIndex) const;

  void setAdvancedPanelOpen(bool isOpen);
  bool isAdvancedPanelOpen() const;

  void setReferenceStatus(const juce::String &status);
  juce::String getReferenceStatus() const;
  void setReferencePath(const juce::String &path);
  juce::String getReferencePath() const;

private:
  juce::ValueTree state_;

  static juce::String slotKeyForIndex(int slotIndex);
  void initialiseDefaults();
};

} // namespace flow88::state
