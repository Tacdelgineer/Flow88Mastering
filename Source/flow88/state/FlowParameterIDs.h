#pragma once

#include "flow88/FlowJuceIncludes.h"
#include <array>

namespace flow88::state {

namespace paramIds {
inline constexpr auto inputTrimDb = "inputTrimDb";
inline constexpr auto subgenre = "subgenre";
inline constexpr auto style = "style";
inline constexpr auto loudnessPreset = "loudnessPreset";
inline constexpr auto targetLufs = "targetLufs";
inline constexpr auto toneTiltDb = "toneTiltDb";
inline constexpr auto punchPct = "punchPct";
inline constexpr auto stereoWidthPct = "stereoWidthPct";
inline constexpr auto lowEndFocusPct = "lowEndFocusPct";
inline constexpr auto attitudePct = "attitudePct";
inline constexpr auto bypass = "bypass";
inline constexpr auto outputTrimDb = "outputTrimDb";
inline constexpr auto limiterCeilingDbTP = "limiterCeilingDbTP";
inline constexpr auto oversampling = "oversampling";
inline constexpr auto clipperAmountPct = "clipperAmountPct";
inline constexpr auto resonanceControlPct = "resonanceControlPct";
inline constexpr auto referenceModeEnabled = "referenceModeEnabled";
} // namespace paramIds

inline const juce::StringArray kSubgenreChoices{
    "House", "Techno", "Melodic", "Trance", "Progressive"};
inline const juce::StringArray kStyleChoices{"Clean", "Club", "Warm"};
inline const juce::StringArray kLoudnessPresetChoices{"Streaming", "Club",
                                                      "Demo", "Exact"};
inline const juce::StringArray kOversamplingChoices{"1x", "2x", "4x"};

inline constexpr int kDefaultSubgenreIndex = 4;
inline constexpr int kDefaultStyleIndex = 2;
inline constexpr int kDefaultLoudnessPresetIndex = 2;
inline constexpr int kDefaultOversamplingIndex = 1;

inline constexpr std::array<const char *, 17> kAllParameterIds{
    paramIds::inputTrimDb,        paramIds::subgenre,
    paramIds::style,              paramIds::loudnessPreset,
    paramIds::targetLufs,         paramIds::toneTiltDb,
    paramIds::punchPct,           paramIds::stereoWidthPct,
    paramIds::lowEndFocusPct,     paramIds::attitudePct,
    paramIds::bypass,             paramIds::outputTrimDb,
    paramIds::limiterCeilingDbTP, paramIds::oversampling,
    paramIds::clipperAmountPct,   paramIds::resonanceControlPct,
    paramIds::referenceModeEnabled};

inline juce::String choiceLabelToId(const juce::String &label) {
  return label.toLowerCase().replaceCharacter(' ', '-');
}

} // namespace flow88::state
