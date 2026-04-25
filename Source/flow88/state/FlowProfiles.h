#pragma once

#include "FlowParameterLayout.h"
#include <array>
#include <vector>

namespace flow88::state {

struct MacroDefaults {
  float toneTiltDb = 0.0f;
  float punchPct = 0.0f;
  float stereoWidthPct = 100.0f;
  float lowEndFocusPct = 0.0f;
  float attitudePct = 0.0f;
};

struct AdvancedDefaults {
  float inputTrimDb = 0.0f;
  float outputTrimDb = 0.0f;
  float limiterCeilingDbTP = -1.0f;
  juce::String oversampling = "2x";
  float clipperAmountPct = 0.0f;
  float resonanceControlPct = 0.0f;
  bool referenceModeEnabled = false;
};

struct AnalysisTargets {
  float lufsIntegrated = -9.0f;
  float truePeakDbTP = -1.0f;
  float correlation = 0.2f;
  float tonalTilt = 0.0f;
  std::array<float, 7> spectralCurve{};
};

struct DspTuning {
  float toneScale = 1.0f;
  float punchScale = 1.0f;
  float widthScale = 1.0f;
  float lowEndFocusScale = 1.0f;
  float attitudeScale = 1.0f;
  float targetDriveBiasDb = 0.0f;
  float lowEndAnchorHz = 140.0f;
  float widthProtectHz = 155.0f;
  float dynamicsThresholdBiasDb = 0.0f;
  float limiterReleaseScale = 1.0f;
  float harshnessAmount = 0.0f;
};

struct SubgenreProfile {
  int schemaVersion = 0;
  juce::String id;
  juce::String displayName;
  juce::String assistantSummary;
  juce::String defaultLoudnessPreset;
  float defaultTargetLufs = -9.0f;
  MacroDefaults macroDefaults;
  AdvancedDefaults advancedDefaults;
  AnalysisTargets analysisTargets;
  DspTuning dspTuning;
  juce::String uiAccent;
};

struct StyleProfile {
  int schemaVersion = 0;
  juce::String id;
  juce::String displayName;
  juce::String assistantSummarySuffix;
  MacroDefaults macroDelta;
  AdvancedDefaults advancedDelta;
  DspTuning dspTuningDelta;
  juce::String uiAccentOverride;
};

struct EffectiveProfile {
  juce::String subgenreId;
  juce::String styleId;
  juce::String subgenreName;
  juce::String styleName;
  juce::String assistantSummary;
  juce::String loudnessPreset;
  float targetLufs = -9.0f;
  MacroDefaults macro;
  AdvancedDefaults advanced;
  AnalysisTargets analysisTargets;
  DspTuning dspTuning;
  juce::Colour accent = juce::Colour::fromString("ff4fd6ff");
};

class ProfileLibrary {
public:
  ProfileLibrary();

  const std::vector<SubgenreProfile> &getSubgenres() const noexcept {
    return subgenres_;
  }
  const std::vector<StyleProfile> &getStyles() const noexcept { return styles_; }

  const SubgenreProfile *findSubgenreById(const juce::String &id) const;
  const StyleProfile *findStyleById(const juce::String &id) const;
  EffectiveProfile compose(const juce::String &subgenreId,
                           const juce::String &styleId) const;

private:
  std::vector<SubgenreProfile> subgenres_;
  std::vector<StyleProfile> styles_;

  void loadBuiltIns();
};

void applyEffectiveProfile(juce::AudioProcessorValueTreeState &apvts,
                           const EffectiveProfile &profile);

} // namespace flow88::state
