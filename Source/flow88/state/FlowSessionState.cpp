#include "FlowSessionState.h"

namespace flow88::state {

namespace {
constexpr auto kAnalysisState = "analysisState";
constexpr auto kSuggestedSubgenre = "suggestedSubgenre";
constexpr auto kSuggestedStyle = "suggestedStyle";
constexpr auto kAssistantSummary = "assistantSummary";
constexpr auto kActiveABSlot = "activeABSlot";
constexpr auto kAdvancedPanelOpen = "advancedPanelOpen";
constexpr auto kReferenceStatus = "referenceStatus";
constexpr auto kReferencePath = "referencePath";
} // namespace

FlowSessionState::FlowSessionState() : state_(kRootType) { initialiseDefaults(); }

void FlowSessionState::replaceState(const juce::ValueTree &newState) {
  if (newState.hasType(kRootType))
    state_ = newState.createCopy();
  else
    initialiseDefaults();
}

void FlowSessionState::setAnalysisState(const juce::String &value) {
  state_.setProperty(kAnalysisState, value, nullptr);
}

juce::String FlowSessionState::getAnalysisState() const {
  return state_[kAnalysisState].toString();
}

bool FlowSessionState::isAnalysisReady() const {
  return getAnalysisState().equalsIgnoreCase("ready");
}

bool FlowSessionState::isAnalysisInProgress() const {
  return getAnalysisState().equalsIgnoreCase("analyzing");
}

void FlowSessionState::setSuggestedProfile(const juce::String &subgenreId,
                                           const juce::String &styleId) {
  state_.setProperty(kSuggestedSubgenre, subgenreId, nullptr);
  state_.setProperty(kSuggestedStyle, styleId, nullptr);
}

juce::String FlowSessionState::getSuggestedSubgenre() const {
  return state_[kSuggestedSubgenre].toString();
}

juce::String FlowSessionState::getSuggestedStyle() const {
  return state_[kSuggestedStyle].toString();
}

void FlowSessionState::setAssistantSummary(const juce::String &summary) {
  state_.setProperty(kAssistantSummary, summary, nullptr);
}

juce::String FlowSessionState::getAssistantSummary() const {
  return state_[kAssistantSummary].toString();
}

void FlowSessionState::setActiveABSlot(int slotIndex) {
  state_.setProperty(kActiveABSlot, juce::jlimit(0, 1, slotIndex), nullptr);
}

int FlowSessionState::getActiveABSlot() const {
  return static_cast<int>(state_[kActiveABSlot]);
}

void FlowSessionState::captureSlot(int slotIndex,
                                   const juce::ValueTree &parameterState) {
  const auto snapshotKey = slotKeyForIndex(slotIndex);
  std::unique_ptr<juce::XmlElement> xml(parameterState.createXml());
  state_.setProperty(snapshotKey, xml != nullptr ? xml->toString() : juce::String{},
                     nullptr);
}

bool FlowSessionState::hasSnapshotForSlot(int slotIndex) const {
  return state_[slotKeyForIndex(slotIndex)].toString().isNotEmpty();
}

juce::ValueTree FlowSessionState::restoreSlot(int slotIndex) const {
  const auto xmlText = state_[slotKeyForIndex(slotIndex)].toString();
  if (xmlText.isEmpty())
    return {};

  const auto xml = juce::parseXML(xmlText);
  return xml != nullptr ? juce::ValueTree::fromXml(*xml) : juce::ValueTree{};
}

void FlowSessionState::setAdvancedPanelOpen(bool isOpen) {
  state_.setProperty(kAdvancedPanelOpen, isOpen, nullptr);
}

bool FlowSessionState::isAdvancedPanelOpen() const {
  return static_cast<bool>(state_[kAdvancedPanelOpen]);
}

void FlowSessionState::setReferenceStatus(const juce::String &status) {
  state_.setProperty(kReferenceStatus, status, nullptr);
}

juce::String FlowSessionState::getReferenceStatus() const {
  return state_[kReferenceStatus].toString();
}

void FlowSessionState::setReferencePath(const juce::String &path) {
  state_.setProperty(kReferencePath, path, nullptr);
}

juce::String FlowSessionState::getReferencePath() const {
  return state_[kReferencePath].toString();
}

juce::String FlowSessionState::slotKeyForIndex(int slotIndex) {
  return slotIndex == 0 ? "slotAState" : "slotBState";
}

void FlowSessionState::initialiseDefaults() {
  state_ = juce::ValueTree(kRootType);
  state_.setProperty(kAnalysisState, "idle", nullptr);
  state_.setProperty(kSuggestedSubgenre, "progressive", nullptr);
  state_.setProperty(kSuggestedStyle, "warm", nullptr);
  state_.setProperty(kAssistantSummary,
                     "Hero default: Progressive / Warm. Start here when you want the safest translation-first finish before pushing into brighter or denser options.",
                     nullptr);
  state_.setProperty(kActiveABSlot, 0, nullptr);
  state_.setProperty("slotAState", "", nullptr);
  state_.setProperty("slotBState", "", nullptr);
  state_.setProperty(kAdvancedPanelOpen, false, nullptr);
  state_.setProperty(kReferenceStatus, "Reference Off", nullptr);
  state_.setProperty(kReferencePath, "", nullptr);
}

} // namespace flow88::state
