#include "FlowProfiles.h"
#include "BinaryData.h"

namespace flow88::state {

namespace {

template <typename ValueType>
ValueType getNumber(const juce::var &value, ValueType fallback) {
  if (value.isDouble() || value.isInt() || value.isInt64() || value.isBool())
    return static_cast<ValueType>(static_cast<double>(value));
  return fallback;
}

juce::String getString(const juce::var &value, const juce::String &fallback = {}) {
  return value.isVoid() ? fallback : value.toString();
}

juce::DynamicObject *asObject(const juce::var &value) {
  return value.isObject() ? value.getDynamicObject() : nullptr;
}

juce::Colour parseColour(const juce::String &hex, juce::Colour fallback) {
  if (hex.isEmpty())
    return fallback;

  auto colourString = hex;
  if (colourString.startsWithChar('#'))
    colourString = "ff" + colourString.substring(1);
  return juce::Colour::fromString(colourString);
}

MacroDefaults parseMacroDefaults(const juce::var &value) {
  MacroDefaults defaults;
  if (auto *object = asObject(value)) {
    defaults.toneTiltDb = getNumber<float>(object->getProperty("toneTiltDb"), 0.0f);
    defaults.punchPct = getNumber<float>(object->getProperty("punchPct"), 0.0f);
    defaults.stereoWidthPct =
        getNumber<float>(object->getProperty("stereoWidthPct"), 100.0f);
    defaults.lowEndFocusPct =
        getNumber<float>(object->getProperty("lowEndFocusPct"), 0.0f);
    defaults.attitudePct =
        getNumber<float>(object->getProperty("attitudePct"), 0.0f);
  }
  return defaults;
}

MacroDefaults parseMacroDelta(const juce::var &value) {
  MacroDefaults defaults;
  if (auto *object = asObject(value)) {
    defaults.toneTiltDb = getNumber<float>(object->getProperty("toneTiltDb"), 0.0f);
    defaults.punchPct = getNumber<float>(object->getProperty("punchPct"), 0.0f);
    defaults.stereoWidthPct =
        getNumber<float>(object->getProperty("stereoWidthPct"), 0.0f);
    defaults.lowEndFocusPct =
        getNumber<float>(object->getProperty("lowEndFocusPct"), 0.0f);
    defaults.attitudePct =
        getNumber<float>(object->getProperty("attitudePct"), 0.0f);
  }
  return defaults;
}

AdvancedDefaults parseAdvancedDefaults(const juce::var &value) {
  AdvancedDefaults defaults;
  if (auto *object = asObject(value)) {
    defaults.inputTrimDb = getNumber<float>(object->getProperty("inputTrimDb"), 0.0f);
    defaults.outputTrimDb =
        getNumber<float>(object->getProperty("outputTrimDb"), 0.0f);
    defaults.limiterCeilingDbTP =
        getNumber<float>(object->getProperty("limiterCeilingDbTP"), -1.0f);
    defaults.oversampling =
        getString(object->getProperty("oversampling"), kOversamplingChoices[1]);
    defaults.clipperAmountPct =
        getNumber<float>(object->getProperty("clipperAmountPct"), 0.0f);
    defaults.resonanceControlPct =
        getNumber<float>(object->getProperty("resonanceControlPct"), 0.0f);
    defaults.referenceModeEnabled =
        static_cast<bool>(object->getProperty("referenceModeEnabled"));
  }
  return defaults;
}

AdvancedDefaults parseAdvancedDelta(const juce::var &value) {
  AdvancedDefaults defaults;
  defaults.oversampling = {};
  if (auto *object = asObject(value)) {
    defaults.inputTrimDb = getNumber<float>(object->getProperty("inputTrimDb"), 0.0f);
    defaults.outputTrimDb =
        getNumber<float>(object->getProperty("outputTrimDb"), 0.0f);
    defaults.limiterCeilingDbTP =
        getNumber<float>(object->getProperty("limiterCeilingDbTP"), 0.0f);
    defaults.oversampling =
        getString(object->getProperty("oversampling"), juce::String{});
    defaults.clipperAmountPct =
        getNumber<float>(object->getProperty("clipperAmountPct"), 0.0f);
    defaults.resonanceControlPct =
        getNumber<float>(object->getProperty("resonanceControlPct"), 0.0f);
    defaults.referenceModeEnabled =
        static_cast<bool>(object->getProperty("referenceModeEnabled"));
  }
  return defaults;
}

AnalysisTargets parseAnalysisTargets(const juce::var &value) {
  AnalysisTargets targets;
  if (auto *object = asObject(value)) {
    targets.lufsIntegrated =
        getNumber<float>(object->getProperty("lufsIntegrated"), -9.0f);
    targets.truePeakDbTP =
        getNumber<float>(object->getProperty("truePeakDbTP"), -1.0f);
    targets.correlation =
        getNumber<float>(object->getProperty("correlation"), 0.2f);
    targets.tonalTilt = getNumber<float>(object->getProperty("tonalTilt"), 0.0f);

    if (auto *curveArray = object->getProperty("spectralCurve").getArray()) {
      for (size_t i = 0; i < targets.spectralCurve.size(); ++i) {
        if (i < curveArray->size())
          targets.spectralCurve[i] =
              getNumber<float>((*curveArray)[static_cast<int>(i)], 0.0f);
      }
    }
  }
  return targets;
}

DspTuning parseDspTuning(const juce::var &value, bool treatAsDelta) {
  DspTuning tuning;
  if (treatAsDelta) {
    tuning.toneScale = 0.0f;
    tuning.punchScale = 0.0f;
    tuning.widthScale = 0.0f;
    tuning.lowEndFocusScale = 0.0f;
    tuning.attitudeScale = 0.0f;
    tuning.lowEndAnchorHz = 0.0f;
    tuning.widthProtectHz = 0.0f;
    tuning.limiterReleaseScale = 0.0f;
  }

  if (auto *object = asObject(value)) {
    tuning.toneScale = getNumber<float>(object->getProperty("toneScale"),
                                        tuning.toneScale);
    tuning.punchScale = getNumber<float>(object->getProperty("punchScale"),
                                         tuning.punchScale);
    tuning.widthScale = getNumber<float>(object->getProperty("widthScale"),
                                         tuning.widthScale);
    tuning.lowEndFocusScale =
        getNumber<float>(object->getProperty("lowEndFocusScale"),
                         tuning.lowEndFocusScale);
    tuning.attitudeScale = getNumber<float>(object->getProperty("attitudeScale"),
                                            tuning.attitudeScale);
    tuning.targetDriveBiasDb =
        getNumber<float>(object->getProperty("targetDriveBiasDb"),
                         tuning.targetDriveBiasDb);
    tuning.lowEndAnchorHz =
        getNumber<float>(object->getProperty("lowEndAnchorHz"),
                         tuning.lowEndAnchorHz);
    tuning.widthProtectHz =
        getNumber<float>(object->getProperty("widthProtectHz"),
                         tuning.widthProtectHz);
    tuning.dynamicsThresholdBiasDb =
        getNumber<float>(object->getProperty("dynamicsThresholdBiasDb"),
                         tuning.dynamicsThresholdBiasDb);
    tuning.limiterReleaseScale =
        getNumber<float>(object->getProperty("limiterReleaseScale"),
                         tuning.limiterReleaseScale);
    tuning.harshnessAmount =
        getNumber<float>(object->getProperty("harshnessAmount"),
                         tuning.harshnessAmount);
  }

  return tuning;
}

MacroDefaults clampMacro(MacroDefaults values) {
  values.toneTiltDb = juce::jlimit(-3.0f, 3.0f, values.toneTiltDb);
  values.punchPct = juce::jlimit(0.0f, 100.0f, values.punchPct);
  values.stereoWidthPct = juce::jlimit(50.0f, 150.0f, values.stereoWidthPct);
  values.lowEndFocusPct = juce::jlimit(0.0f, 100.0f, values.lowEndFocusPct);
  values.attitudePct = juce::jlimit(0.0f, 100.0f, values.attitudePct);
  return values;
}

AdvancedDefaults clampAdvanced(AdvancedDefaults values) {
  values.inputTrimDb = juce::jlimit(-12.0f, 12.0f, values.inputTrimDb);
  values.outputTrimDb = juce::jlimit(-12.0f, 12.0f, values.outputTrimDb);
  values.limiterCeilingDbTP =
      juce::jlimit(-1.5f, -0.1f, values.limiterCeilingDbTP);
  values.clipperAmountPct =
      juce::jlimit(0.0f, 100.0f, values.clipperAmountPct);
  values.resonanceControlPct =
      juce::jlimit(0.0f, 100.0f, values.resonanceControlPct);
  if (kOversamplingChoices.indexOf(values.oversampling) < 0)
    values.oversampling = kOversamplingChoices[1];
  return values;
}

DspTuning clampDspTuning(DspTuning tuning) {
  tuning.toneScale = juce::jlimit(0.75f, 1.3f, tuning.toneScale);
  tuning.punchScale = juce::jlimit(0.75f, 1.3f, tuning.punchScale);
  tuning.widthScale = juce::jlimit(0.75f, 1.3f, tuning.widthScale);
  tuning.lowEndFocusScale =
      juce::jlimit(0.75f, 1.3f, tuning.lowEndFocusScale);
  tuning.attitudeScale = juce::jlimit(0.75f, 1.3f, tuning.attitudeScale);
  tuning.targetDriveBiasDb =
      juce::jlimit(-1.5f, 1.5f, tuning.targetDriveBiasDb);
  tuning.lowEndAnchorHz = juce::jlimit(105.0f, 180.0f, tuning.lowEndAnchorHz);
  tuning.widthProtectHz =
      juce::jlimit(120.0f, 210.0f, tuning.widthProtectHz);
  tuning.dynamicsThresholdBiasDb =
      juce::jlimit(-2.5f, 2.5f, tuning.dynamicsThresholdBiasDb);
  tuning.limiterReleaseScale =
      juce::jlimit(0.7f, 1.4f, tuning.limiterReleaseScale);
  tuning.harshnessAmount = juce::jlimit(0.0f, 1.0f, tuning.harshnessAmount);
  return tuning;
}

SubgenreProfile parseSubgenreJson(const juce::String &jsonText) {
  SubgenreProfile profile;
  const auto parsed = juce::JSON::parse(jsonText);
  if (auto *object = asObject(parsed)) {
    profile.schemaVersion = getNumber<int>(object->getProperty("schemaVersion"), 0);
    profile.id = getString(object->getProperty("id"));
    profile.displayName = getString(object->getProperty("displayName"));
    profile.assistantSummary = getString(object->getProperty("assistantSummary"));
    profile.defaultLoudnessPreset =
        getString(object->getProperty("defaultLoudnessPreset"), "Club");
    profile.defaultTargetLufs =
        getNumber<float>(object->getProperty("defaultTargetLufs"), -9.0f);
    profile.macroDefaults =
        clampMacro(parseMacroDefaults(object->getProperty("macroDefaults")));
    profile.advancedDefaults = clampAdvanced(
        parseAdvancedDefaults(object->getProperty("advancedDefaults")));
    profile.analysisTargets =
        parseAnalysisTargets(object->getProperty("analysisTargets"));
    profile.dspTuning =
        clampDspTuning(parseDspTuning(object->getProperty("dspTuning"), false));
    profile.uiAccent = getString(object->getProperty("uiAccent"), "#4FD6FF");
  }
  return profile;
}

StyleProfile parseStyleJson(const juce::String &jsonText) {
  StyleProfile profile;
  const auto parsed = juce::JSON::parse(jsonText);
  if (auto *object = asObject(parsed)) {
    profile.schemaVersion = getNumber<int>(object->getProperty("schemaVersion"), 0);
    profile.id = getString(object->getProperty("id"));
    profile.displayName = getString(object->getProperty("displayName"));
    profile.assistantSummarySuffix =
        getString(object->getProperty("assistantSummarySuffix"));
    profile.macroDelta = parseMacroDelta(object->getProperty("macroDelta"));
    profile.advancedDelta = parseAdvancedDelta(object->getProperty("advancedDelta"));
    profile.dspTuningDelta =
        parseDspTuning(object->getProperty("dspTuningDelta"), true);
    profile.uiAccentOverride =
        getString(object->getProperty("uiAccentOverride"), {});
  }
  return profile;
}

juce::String loadText(const char *data, int size) {
  return juce::String::fromUTF8(data, size);
}

} // namespace

ProfileLibrary::ProfileLibrary() { loadBuiltIns(); }

void ProfileLibrary::loadBuiltIns() {
  subgenres_.clear();
  styles_.clear();

  subgenres_.push_back(parseSubgenreJson(
      loadText(BinaryData::house_json, BinaryData::house_jsonSize)));
  subgenres_.push_back(parseSubgenreJson(
      loadText(BinaryData::techno_json, BinaryData::techno_jsonSize)));
  subgenres_.push_back(parseSubgenreJson(
      loadText(BinaryData::melodic_json, BinaryData::melodic_jsonSize)));
  subgenres_.push_back(parseSubgenreJson(
      loadText(BinaryData::trance_json, BinaryData::trance_jsonSize)));
  subgenres_.push_back(parseSubgenreJson(
      loadText(BinaryData::progressive_json, BinaryData::progressive_jsonSize)));

  styles_.push_back(parseStyleJson(
      loadText(BinaryData::clean_json, BinaryData::clean_jsonSize)));
  styles_.push_back(parseStyleJson(
      loadText(BinaryData::club_json, BinaryData::club_jsonSize)));
  styles_.push_back(parseStyleJson(
      loadText(BinaryData::warm_json, BinaryData::warm_jsonSize)));
}

const SubgenreProfile *ProfileLibrary::findSubgenreById(const juce::String &id) const {
  const auto wantedId = id.toLowerCase();
  for (const auto &profile : subgenres_)
    if (profile.id == wantedId)
      return &profile;
  return subgenres_.empty() ? nullptr : &subgenres_.front();
}

const StyleProfile *ProfileLibrary::findStyleById(const juce::String &id) const {
  const auto wantedId = id.toLowerCase();
  for (const auto &profile : styles_)
    if (profile.id == wantedId)
      return &profile;
  return styles_.empty() ? nullptr : &styles_.front();
}

EffectiveProfile ProfileLibrary::compose(const juce::String &subgenreId,
                                         const juce::String &styleId) const {
  EffectiveProfile profile;

  const auto *subgenre = findSubgenreById(subgenreId);
  const auto *style = findStyleById(styleId);
  jassert(subgenre != nullptr && style != nullptr);

  if (subgenre == nullptr || style == nullptr)
    return profile;

  profile.subgenreId = subgenre->id;
  profile.styleId = style->id;
  profile.subgenreName = subgenre->displayName;
  profile.styleName = style->displayName;
  profile.loudnessPreset = subgenre->defaultLoudnessPreset;
  profile.targetLufs = subgenre->defaultTargetLufs;
  profile.macro.toneTiltDb =
      subgenre->macroDefaults.toneTiltDb + style->macroDelta.toneTiltDb;
  profile.macro.punchPct =
      subgenre->macroDefaults.punchPct + style->macroDelta.punchPct;
  profile.macro.stereoWidthPct =
      subgenre->macroDefaults.stereoWidthPct + style->macroDelta.stereoWidthPct;
  profile.macro.lowEndFocusPct =
      subgenre->macroDefaults.lowEndFocusPct + style->macroDelta.lowEndFocusPct;
  profile.macro.attitudePct =
      subgenre->macroDefaults.attitudePct + style->macroDelta.attitudePct;
  profile.macro = clampMacro(profile.macro);

  profile.advanced.inputTrimDb =
      subgenre->advancedDefaults.inputTrimDb + style->advancedDelta.inputTrimDb;
  profile.advanced.outputTrimDb =
      subgenre->advancedDefaults.outputTrimDb + style->advancedDelta.outputTrimDb;
  profile.advanced.limiterCeilingDbTP =
      subgenre->advancedDefaults.limiterCeilingDbTP +
      style->advancedDelta.limiterCeilingDbTP;
  profile.advanced.oversampling = style->advancedDelta.oversampling.isNotEmpty()
                                      ? style->advancedDelta.oversampling
                                      : subgenre->advancedDefaults.oversampling;
  profile.advanced.clipperAmountPct =
      subgenre->advancedDefaults.clipperAmountPct +
      style->advancedDelta.clipperAmountPct;
  profile.advanced.resonanceControlPct =
      subgenre->advancedDefaults.resonanceControlPct +
      style->advancedDelta.resonanceControlPct;
  profile.advanced.referenceModeEnabled =
      subgenre->advancedDefaults.referenceModeEnabled ||
      style->advancedDelta.referenceModeEnabled;
  profile.advanced = clampAdvanced(profile.advanced);

  profile.dspTuning.toneScale =
      subgenre->dspTuning.toneScale + style->dspTuningDelta.toneScale;
  profile.dspTuning.punchScale =
      subgenre->dspTuning.punchScale + style->dspTuningDelta.punchScale;
  profile.dspTuning.widthScale =
      subgenre->dspTuning.widthScale + style->dspTuningDelta.widthScale;
  profile.dspTuning.lowEndFocusScale =
      subgenre->dspTuning.lowEndFocusScale +
      style->dspTuningDelta.lowEndFocusScale;
  profile.dspTuning.attitudeScale =
      subgenre->dspTuning.attitudeScale + style->dspTuningDelta.attitudeScale;
  profile.dspTuning.targetDriveBiasDb =
      subgenre->dspTuning.targetDriveBiasDb +
      style->dspTuningDelta.targetDriveBiasDb;
  profile.dspTuning.lowEndAnchorHz =
      subgenre->dspTuning.lowEndAnchorHz +
      style->dspTuningDelta.lowEndAnchorHz;
  profile.dspTuning.widthProtectHz =
      subgenre->dspTuning.widthProtectHz +
      style->dspTuningDelta.widthProtectHz;
  profile.dspTuning.dynamicsThresholdBiasDb =
      subgenre->dspTuning.dynamicsThresholdBiasDb +
      style->dspTuningDelta.dynamicsThresholdBiasDb;
  profile.dspTuning.limiterReleaseScale =
      subgenre->dspTuning.limiterReleaseScale +
      style->dspTuningDelta.limiterReleaseScale;
  profile.dspTuning.harshnessAmount =
      subgenre->dspTuning.harshnessAmount +
      style->dspTuningDelta.harshnessAmount;
  profile.dspTuning = clampDspTuning(profile.dspTuning);

  profile.analysisTargets = subgenre->analysisTargets;
  profile.accent = parseColour(
      style->uiAccentOverride.isNotEmpty() ? style->uiAccentOverride
                                           : subgenre->uiAccent,
      juce::Colour::fromString("ff4fd6ff"));
  profile.assistantSummary = subgenre->assistantSummary + " " +
                             style->assistantSummarySuffix;
  return profile;
}

void applyEffectiveProfile(juce::AudioProcessorValueTreeState &apvts,
                           const EffectiveProfile &profile) {
  setChoiceParameter(apvts, paramIds::subgenre,
                     findChoiceIndex(kSubgenreChoices, profile.subgenreName));
  setChoiceParameter(apvts, paramIds::style,
                     findChoiceIndex(kStyleChoices, profile.styleName));
  setChoiceParameter(apvts, paramIds::loudnessPreset,
                     findChoiceIndex(kLoudnessPresetChoices, profile.loudnessPreset));
  setFloatParameter(apvts, paramIds::targetLufs, profile.targetLufs);
  setFloatParameter(apvts, paramIds::toneTiltDb, profile.macro.toneTiltDb);
  setFloatParameter(apvts, paramIds::punchPct, profile.macro.punchPct);
  setFloatParameter(apvts, paramIds::stereoWidthPct,
                    profile.macro.stereoWidthPct);
  setFloatParameter(apvts, paramIds::lowEndFocusPct,
                    profile.macro.lowEndFocusPct);
  setFloatParameter(apvts, paramIds::attitudePct, profile.macro.attitudePct);
  setFloatParameter(apvts, paramIds::inputTrimDb, profile.advanced.inputTrimDb);
  setFloatParameter(apvts, paramIds::outputTrimDb, profile.advanced.outputTrimDb);
  setFloatParameter(apvts, paramIds::limiterCeilingDbTP,
                    profile.advanced.limiterCeilingDbTP);
  setChoiceParameter(apvts, paramIds::oversampling,
                     findChoiceIndex(kOversamplingChoices,
                                     profile.advanced.oversampling));
  setFloatParameter(apvts, paramIds::clipperAmountPct,
                    profile.advanced.clipperAmountPct);
  setFloatParameter(apvts, paramIds::resonanceControlPct,
                    profile.advanced.resonanceControlPct);
  setBoolParameter(apvts, paramIds::referenceModeEnabled,
                   profile.advanced.referenceModeEnabled);
}

} // namespace flow88::state
