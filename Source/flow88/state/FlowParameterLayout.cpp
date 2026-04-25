#include "FlowParameterLayout.h"

namespace flow88::state {

namespace {

juce::String formatDb(float value, int decimals = 1,
                      const juce::String &suffix = " dB") {
  return juce::String(value, decimals) + suffix;
}

juce::String formatPercent(float value) {
  return juce::String(value, 0) + " %";
}

} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout() {
  using namespace juce;

  std::vector<std::unique_ptr<RangedAudioParameter>> params;

  auto trimRange = NormalisableRange<float>(-12.0f, 12.0f, 0.01f);
  auto targetLufsRange = NormalisableRange<float>(-16.0f, -7.0f, 0.1f);
  auto toneRange = NormalisableRange<float>(-3.0f, 3.0f, 0.01f);
  auto percentRange = NormalisableRange<float>(0.0f, 100.0f, 0.1f);
  auto widthRange = NormalisableRange<float>(50.0f, 150.0f, 0.1f);
  auto ceilingRange = NormalisableRange<float>(-1.5f, -0.1f, 0.01f);

  params.push_back(std::make_unique<AudioParameterFloat>(
      ParameterID{paramIds::inputTrimDb, 1}, "Input Trim", trimRange, 0.0f,
      AudioParameterFloatAttributes().withStringFromValueFunction(
          [](float value, int) { return formatDb(value); })));

  params.push_back(std::make_unique<AudioParameterChoice>(
      ParameterID{paramIds::subgenre, 1}, "Subgenre", kSubgenreChoices,
      kDefaultSubgenreIndex));

  params.push_back(std::make_unique<AudioParameterChoice>(
      ParameterID{paramIds::style, 1}, "Style", kStyleChoices,
      kDefaultStyleIndex));

  params.push_back(std::make_unique<AudioParameterChoice>(
      ParameterID{paramIds::loudnessPreset, 1}, "Loudness Preset",
      kLoudnessPresetChoices, kDefaultLoudnessPresetIndex));

  params.push_back(std::make_unique<AudioParameterFloat>(
      ParameterID{paramIds::targetLufs, 1}, "Target LUFS", targetLufsRange,
      -10.0f, AudioParameterFloatAttributes().withStringFromValueFunction(
                 [](float value, int) {
                   return juce::String(value, 1) + " LUFS";
                 })));

  params.push_back(std::make_unique<AudioParameterFloat>(
      ParameterID{paramIds::toneTiltDb, 1}, "Tone", toneRange, -0.1f,
      AudioParameterFloatAttributes().withStringFromValueFunction(
          [](float value, int) { return formatDb(value); })));

  params.push_back(std::make_unique<AudioParameterFloat>(
      ParameterID{paramIds::punchPct, 1}, "Punch", percentRange, 53.0f,
      AudioParameterFloatAttributes().withStringFromValueFunction(
          [](float value, int) { return formatPercent(value); })));

  params.push_back(std::make_unique<AudioParameterFloat>(
      ParameterID{paramIds::stereoWidthPct, 1}, "Width", widthRange, 114.0f,
      AudioParameterFloatAttributes().withStringFromValueFunction(
          [](float value, int) { return formatPercent(value); })));

  params.push_back(std::make_unique<AudioParameterFloat>(
      ParameterID{paramIds::lowEndFocusPct, 1}, "Low-End Focus", percentRange,
      80.0f, AudioParameterFloatAttributes().withStringFromValueFunction(
                [](float value, int) { return formatPercent(value); })));

  params.push_back(std::make_unique<AudioParameterFloat>(
      ParameterID{paramIds::attitudePct, 1}, "Attitude", percentRange, 13.0f,
      AudioParameterFloatAttributes().withStringFromValueFunction(
          [](float value, int) { return formatPercent(value); })));

  params.push_back(std::make_unique<AudioParameterBool>(
      ParameterID{paramIds::bypass, 1}, "Bypass", false));

  params.push_back(std::make_unique<AudioParameterFloat>(
      ParameterID{paramIds::outputTrimDb, 1}, "Output Trim", trimRange, 0.0f,
      AudioParameterFloatAttributes().withStringFromValueFunction(
          [](float value, int) { return formatDb(value); })));

  params.push_back(std::make_unique<AudioParameterFloat>(
      ParameterID{paramIds::limiterCeilingDbTP, 1}, "Limiter Ceiling",
      ceilingRange, -1.05f, AudioParameterFloatAttributes()
                              .withStringFromValueFunction([](float value, int) {
                                return formatDb(value, 2, " dBTP");
                              })));

  params.push_back(std::make_unique<AudioParameterChoice>(
      ParameterID{paramIds::oversampling, 1}, "Oversampling",
      kOversamplingChoices, kDefaultOversamplingIndex));

  params.push_back(std::make_unique<AudioParameterFloat>(
      ParameterID{paramIds::clipperAmountPct, 1}, "Clipper Amount",
      percentRange, 12.0f,
      AudioParameterFloatAttributes().withStringFromValueFunction(
          [](float value, int) { return formatPercent(value); })));

  params.push_back(std::make_unique<AudioParameterFloat>(
      ParameterID{paramIds::resonanceControlPct, 1}, "Resonance Control",
      percentRange, 21.0f,
      AudioParameterFloatAttributes().withStringFromValueFunction(
          [](float value, int) { return formatPercent(value); })));

  params.push_back(std::make_unique<AudioParameterBool>(
      ParameterID{paramIds::referenceModeEnabled, 1}, "Reference Mode",
      false));

  return {params.begin(), params.end()};
}

int findChoiceIndex(const juce::StringArray &choices, const juce::String &value) {
  const auto index = choices.indexOf(value);
  return index >= 0 ? index : 0;
}

void setChoiceParameter(juce::AudioProcessorValueTreeState &apvts,
                        const juce::String &parameterId, int index) {
  if (auto *choice = dynamic_cast<juce::AudioParameterChoice *>(
          apvts.getParameter(parameterId))) {
    const auto clamped = juce::jlimit(0, choice->choices.size() - 1, index);
    choice->setValueNotifyingHost(choice->convertTo0to1(static_cast<float>(clamped)));
  }
}

void setFloatParameter(juce::AudioProcessorValueTreeState &apvts,
                       const juce::String &parameterId, float value) {
  if (auto *parameter = dynamic_cast<juce::RangedAudioParameter *>(
          apvts.getParameter(parameterId))) {
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
  }
}

void setBoolParameter(juce::AudioProcessorValueTreeState &apvts,
                      const juce::String &parameterId, bool value) {
  if (auto *parameter = dynamic_cast<juce::AudioParameterBool *>(
          apvts.getParameter(parameterId))) {
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value ? 1.0f : 0.0f));
  }
}

int getChoiceParameterIndex(const juce::AudioProcessorValueTreeState &apvts,
                            const juce::String &parameterId) {
  if (auto *raw = apvts.getRawParameterValue(parameterId))
    return static_cast<int>(raw->load());
  return 0;
}

float getFloatParameter(const juce::AudioProcessorValueTreeState &apvts,
                        const juce::String &parameterId) {
  if (auto *raw = apvts.getRawParameterValue(parameterId))
    return raw->load();
  return 0.0f;
}

bool getBoolParameter(const juce::AudioProcessorValueTreeState &apvts,
                      const juce::String &parameterId) {
  if (auto *raw = apvts.getRawParameterValue(parameterId))
    return raw->load() >= 0.5f;
  return false;
}

} // namespace flow88::state
