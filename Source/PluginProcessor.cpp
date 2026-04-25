#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
#include <limits>

namespace {

enum StageFlag : uint32_t {
  inputTrimStage = 1u << 0,
  toneStage = 1u << 1,
  lowEndStage = 1u << 2,
  dynamicsStage = 1u << 3,
  widthStage = 1u << 4,
  saturationStage = 1u << 5,
  clipperStage = 1u << 6,
  limiterStage = 1u << 7,
  outputTrimStage = 1u << 8,
};

juce::String formatSigned(float value, int decimals = 1,
                          const juce::String &suffix = {}) {
  auto text = juce::String(value, decimals);
  if (value > 0.0f)
    text = "+" + text;
  return text + suffix;
}

constexpr float kUnknownABComparableLufs = -100.0f;

float chooseComparableLufsForAB(
    const flow88::dsp::FlowMeterSource::Snapshot &meters) {
  if (std::isfinite(meters.integratedLufs) && meters.integratedLufs > -55.0f)
    return meters.integratedLufs;
  if (std::isfinite(meters.shortTermLufs) && meters.shortTermLufs > -55.0f)
    return meters.shortTermLufs;
  return kUnknownABComparableLufs;
}

bool hasComparableLufsForAB(float value) {
  return std::isfinite(value) && value > -55.0f;
}

bool isHeroProfile(const flow88::state::EffectiveProfile &profile) {
  return profile.subgenreId == "progressive" && profile.styleId == "warm";
}

juce::String buildProfileGuide(
    const flow88::state::EffectiveProfile &profile) {
  auto guide = profile.assistantSummary;

  if (isHeroProfile(profile)) {
    return "Hero default. " + guide +
           " Use this as the first stop when you want the safest translation and "
           "least risky preset starting point.";
  }

  if (profile.styleId == "clean") {
    return guide + " Choose Clean when translation, restraint, and level headroom "
                   "matter more than density.";
  }

  if (profile.styleId == "club") {
    return guide + " Choose Club when the mix is already stable and you want a "
                   "denser finish without leaving the safe preset lane.";
  }

  return guide + " Choose Warm when you want the safer, softer contour before "
                 "making any louder or brighter decisions.";
}

juce::String describeSpectrum(const std::array<float, 7> &curve) {
  const auto lowEnergy =
      (curve[0] + curve[1] + curve[2]) / 3.0f;
  const auto highEnergy =
      (curve[4] + curve[5] + curve[6]) / 3.0f;
  const auto tilt = highEnergy - lowEnergy;

  if (lowEnergy > 0.22f)
    return "low-end is a little forward";
  if (highEnergy > 0.22f)
    return "top end is a little forward";
  if (tilt > 0.16f)
    return "the balance leans bright";
  if (tilt < -0.16f)
    return "the balance leans weighty";
  return "the balance is sitting fairly even";
}

float computeCurveDeviation(const std::array<float, 7> &lhs,
                            const std::array<float, 7> &rhs) {
  float deviation = 0.0f;
  for (size_t index = 0; index < lhs.size(); ++index)
    deviation += std::abs(lhs[index] - rhs[index]);
  return deviation / static_cast<float>(lhs.size());
}

juce::String describeReferenceGuidance(
    const flow88::dsp::FlowMeterSource::Snapshot &meters,
    const flow88::eval::AnalysisMetrics &referenceMetrics) {
  juce::StringArray parts;

  const auto loudnessDelta = meters.integratedLufs - referenceMetrics.integratedLufs;
  if (std::abs(loudnessDelta) > 0.6f) {
    parts.add(loudnessDelta < 0.0f
                  ? "push level about " + juce::String(std::abs(loudnessDelta), 1) +
                        " LU"
                  : "ease level about " + juce::String(std::abs(loudnessDelta), 1) +
                        " LU");
  }

  const auto widthDelta = meters.stereoWidthPct - referenceMetrics.stereoWidthPct;
  if (std::abs(widthDelta) > 8.0f)
    parts.add(widthDelta < 0.0f ? "open width slightly" : "trim width slightly");

  const auto liveLow =
      (meters.spectralCurve[0] + meters.spectralCurve[1] + meters.spectralCurve[2]) / 3.0f;
  const auto liveHigh =
      (meters.spectralCurve[4] + meters.spectralCurve[5] + meters.spectralCurve[6]) / 3.0f;
  const auto refLow = (referenceMetrics.spectralCurve[0] +
                       referenceMetrics.spectralCurve[1] +
                       referenceMetrics.spectralCurve[2]) /
                      3.0f;
  const auto refHigh = (referenceMetrics.spectralCurve[4] +
                        referenceMetrics.spectralCurve[5] +
                        referenceMetrics.spectralCurve[6]) /
                       3.0f;
  const auto lowDelta = liveLow - refLow;
  const auto highDelta = liveHigh - refHigh;
  if (lowDelta < -0.12f)
    parts.add("add a little more weight");
  else if (lowDelta > 0.12f)
    parts.add("tighten the low shelf");

  if (highDelta < -0.12f)
    parts.add("lift a touch of top air");
  else if (highDelta > 0.12f)
    parts.add("soften the top edge");

  const auto correlationDelta = meters.correlation - referenceMetrics.stereoCorrelation;
  if (correlationDelta < -0.1f)
    parts.add("keep the low sides tighter");

  if (parts.isEmpty())
    return "Current signal is tracking the reference closely.";

  return parts.joinIntoString(", ") + ".";
}

juce::String chooseSuggestedSubgenreId(
    const flow88::state::ProfileLibrary &library,
    const flow88::dsp::FlowMeterSource::Snapshot &meters) {
  auto bestScore = std::numeric_limits<float>::max();
  juce::String bestId;

  for (const auto &subgenre : library.getSubgenres()) {
    auto score = std::abs(meters.integratedLufs - subgenre.analysisTargets.lufsIntegrated) *
                     0.75f +
                 std::abs(meters.truePeakDbTP - subgenre.analysisTargets.truePeakDbTP) *
                     2.5f +
                 std::abs(meters.correlation - subgenre.analysisTargets.correlation) *
                     5.0f;

    for (size_t i = 0; i < meters.spectralCurve.size(); ++i)
      score += std::abs(meters.spectralCurve[i] -
                        subgenre.analysisTargets.spectralCurve[i]) *
               1.6f;

    if (score < bestScore) {
      bestScore = score;
      bestId = subgenre.id;
    }
  }

  return bestId;
}

} // namespace

class Flow88MasterAudioProcessor::ReferenceAnalysisJob
    : public juce::ThreadPoolJob {
public:
  ReferenceAnalysisJob(Flow88MasterAudioProcessor &owner, int requestId,
                       juce::File file,
                       flow88::state::AnalysisTargets targets)
      : juce::ThreadPoolJob("Flow88 Reference Analysis"), owner_(owner),
        requestId_(requestId), file_(std::move(file)), targets_(targets) {}

  JobStatus runJob() override {
    juce::String errorMessage;
    const auto metrics =
        flow88::eval::analyzeFile(file_, &targets_, 1024, &errorMessage);
    if (shouldExit())
      return jobHasFinished;

    owner_.completeReferenceAnalysis(requestId_, file_, metrics, errorMessage);
    return jobHasFinished;
  }

private:
  Flow88MasterAudioProcessor &owner_;
  int requestId_ = 0;
  juce::File file_;
  flow88::state::AnalysisTargets targets_;
};

Flow88MasterAudioProcessor::Flow88MasterAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts_(*this, nullptr, "Parameters", flow88::state::createParameterLayout()) {
  inputTrimParam_ = apvts_.getRawParameterValue(flow88::state::paramIds::inputTrimDb);
  outputTrimParam_ =
      apvts_.getRawParameterValue(flow88::state::paramIds::outputTrimDb);
  bypassParam_ = apvts_.getRawParameterValue(flow88::state::paramIds::bypass);

  abLevelMatchGainDb_.store(0.0f);
  abLevelMatchReady_.store(false);
  for (auto &slotLufs : abSlotComparableLufs_)
    slotLufs.store(kUnknownABComparableLufs);

  applySelectionProfile(getSelectedSubgenreId(), getSelectedStyleId());
  sessionState_.setAssistantSummary(
      "Press Analyze to measure the incoming mix. Progressive / Warm is the "
      "default safe starting point.");
  ensureSnapshotDefaults();
}

Flow88MasterAudioProcessor::~Flow88MasterAudioProcessor() {
  referenceRequestId_.fetch_add(1);
  referenceThreadPool_.removeAllJobs(true, 4000);
}

void Flow88MasterAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
  inputGainSmoothed_.reset(sampleRate, 0.03);
  outputGainSmoothed_.reset(sampleRate, 0.03);
  abLevelMatchGainSmoothed_.reset(sampleRate, 0.04);
  inputGainSmoothed_.setCurrentAndTargetValue(
      juce::Decibels::decibelsToGain(inputTrimParam_->load()));
  outputGainSmoothed_.setCurrentAndTargetValue(
      juce::Decibels::decibelsToGain(outputTrimParam_->load()));
  abLevelMatchGainSmoothed_.setCurrentAndTargetValue(
      juce::Decibels::decibelsToGain(abLevelMatchGainDb_.load()));
  masteringChain_.prepare(sampleRate, samplesPerBlock, getTotalNumOutputChannels());
  meterSource_.prepare(sampleRate, samplesPerBlock);
}

void Flow88MasterAudioProcessor::releaseResources() {
  masteringChain_.reset();
  meterSource_.reset();
}

bool Flow88MasterAudioProcessor::isBusesLayoutSupported(
    const BusesLayout &layouts) const {
  return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo() &&
         layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void Flow88MasterAudioProcessor::processBlock(juce::AudioBuffer<float> &buffer,
                                              juce::MidiBuffer &) {
  juce::ScopedNoDenormals noDenormals;
  meterSource_.pushInputBuffer(buffer);

  if (bypassParam_ == nullptr || inputTrimParam_ == nullptr || outputTrimParam_ == nullptr) {
    meterSource_.pushOutputBuffer(buffer);
    return;
  }

  const auto bypassed = bypassParam_->load() >= 0.5f;

  inputGainSmoothed_.setTargetValue(
      juce::Decibels::decibelsToGain(inputTrimParam_->load()));
  outputGainSmoothed_.setTargetValue(
      juce::Decibels::decibelsToGain(outputTrimParam_->load()));
  abLevelMatchGainSmoothed_.setTargetValue(
      juce::Decibels::decibelsToGain(abLevelMatchGainDb_.load()));

  const auto inputStart = inputGainSmoothed_.getCurrentValue();
  const auto inputEnd = inputGainSmoothed_.skip(buffer.getNumSamples());
  const auto outputStart = outputGainSmoothed_.getCurrentValue();
  const auto outputEnd = outputGainSmoothed_.skip(buffer.getNumSamples());
  const auto abMatchStart = abLevelMatchGainSmoothed_.getCurrentValue();
  const auto abMatchEnd = abLevelMatchGainSmoothed_.skip(buffer.getNumSamples());

  if (!bypassed) {
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
      buffer.applyGainRamp(channel, 0, buffer.getNumSamples(), inputStart, inputEnd);

    flow88::dsp::FlowMasteringSettings settings;
    settings.toneTiltDb =
        flow88::state::getFloatParameter(apvts_, flow88::state::paramIds::toneTiltDb);
    settings.punchPct =
        flow88::state::getFloatParameter(apvts_, flow88::state::paramIds::punchPct);
    settings.stereoWidthPct = flow88::state::getFloatParameter(
        apvts_, flow88::state::paramIds::stereoWidthPct);
    settings.lowEndFocusPct = flow88::state::getFloatParameter(
        apvts_, flow88::state::paramIds::lowEndFocusPct);
    settings.targetLufs =
        flow88::state::getFloatParameter(apvts_, flow88::state::paramIds::targetLufs);
    settings.attitudePct = flow88::state::getFloatParameter(
        apvts_, flow88::state::paramIds::attitudePct);
    settings.outputTrimDb =
        flow88::state::getFloatParameter(apvts_, flow88::state::paramIds::outputTrimDb);
    settings.limiterCeilingDbTP = flow88::state::getFloatParameter(
        apvts_, flow88::state::paramIds::limiterCeilingDbTP);
    settings.clipperAmountPct = flow88::state::getFloatParameter(
        apvts_, flow88::state::paramIds::clipperAmountPct);
    settings.resonanceControlPct = flow88::state::getFloatParameter(
        apvts_, flow88::state::paramIds::resonanceControlPct);
    settings.toneScale = toneScale_.load();
    settings.punchScale = punchScale_.load();
    settings.widthScale = widthScale_.load();
    settings.lowEndFocusScale = lowEndFocusScale_.load();
    settings.attitudeScale = attitudeScale_.load();
    settings.targetDriveBiasDb = targetDriveBiasDb_.load();
    settings.lowEndAnchorHz = lowEndAnchorHz_.load();
    settings.widthProtectHz = widthProtectHz_.load();
    settings.dynamicsThresholdBiasDb = dynamicsThresholdBiasDb_.load();
    settings.limiterReleaseScale = limiterReleaseScale_.load();
    settings.harshnessAmount = harshnessAmount_.load();
    settings.oversamplingIndex = flow88::state::getChoiceParameterIndex(
        apvts_, flow88::state::paramIds::oversampling);

    const auto metrics = masteringChain_.process(buffer, settings);

    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
      buffer.applyGainRamp(channel, 0, buffer.getNumSamples(), outputStart,
                           outputEnd);

    compressorGainReductionDb_.store(metrics.compressorGainReductionDb);
    limiterGainReductionDb_.store(metrics.limiterGainReductionDb);
    targetDriveDb_.store(metrics.targetDriveDb);

    uint32_t flags = 0;
    if (std::abs(inputTrimParam_->load()) > 0.05f)
      flags |= inputTrimStage;
    if (metrics.toneStageActive)
      flags |= toneStage;
    if (metrics.lowEndStageActive)
      flags |= lowEndStage;
    if (metrics.dynamicsStageActive)
      flags |= dynamicsStage;
    if (metrics.widthStageActive)
      flags |= widthStage;
    if (metrics.saturationStageActive)
      flags |= saturationStage;
    if (metrics.clipperStageActive)
      flags |= clipperStage;
    if (metrics.limiterStageActive)
      flags |= limiterStage;
    if (std::abs(outputTrimParam_->load()) > 0.05f)
      flags |= outputTrimStage;
    stageFlags_.store(flags);
    meterSource_.setGainReductionDb(metrics.totalGainReductionDb);
  } else {
    compressorGainReductionDb_.store(0.0f);
    limiterGainReductionDb_.store(0.0f);
    targetDriveDb_.store(0.0f);
    stageFlags_.store(0u);
    meterSource_.setGainReductionDb(0.0f);
  }

  for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    buffer.applyGainRamp(channel, 0, buffer.getNumSamples(), abMatchStart,
                         abMatchEnd);
  meterSource_.pushOutputBuffer(buffer);
}

juce::AudioProcessorEditor *Flow88MasterAudioProcessor::createEditor() {
  return new Flow88MasterAudioProcessorEditor(*this);
}

void Flow88MasterAudioProcessor::getStateInformation(juce::MemoryBlock &destData) {
  auto state = flow88::state::createPluginState(apvts_, sessionState_);
  std::unique_ptr<juce::XmlElement> xml(state.createXml());
  copyXmlToBinary(*xml, destData);
}

void Flow88MasterAudioProcessor::setStateInformation(const void *data,
                                                     int sizeInBytes) {
  std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
  if (xml == nullptr)
    return;

  flow88::state::restorePluginState(juce::ValueTree::fromXml(*xml), apvts_,
                                    sessionState_);
  ensureSnapshotDefaults();
  cacheProfileTuning(getCurrentProfile());
  abLevelMatchGainDb_.store(0.0f);
  abLevelMatchReady_.store(false);
  for (auto &slotLufs : abSlotComparableLufs_)
    slotLufs.store(kUnknownABComparableLufs);
  refreshReferenceStatus();
}

Flow88MasterAudioProcessor::DisplayState
Flow88MasterAudioProcessor::buildDisplayState(double animationSeconds) const {
  juce::ignoreUnused(animationSeconds);
  DisplayState state;

  const auto profile = getCurrentProfile();
  const auto suggested =
      profileLibrary_.compose(sessionState_.getSuggestedSubgenre(),
                              sessionState_.getSuggestedStyle());
  const auto meters = meterSource_.snapshot();
  const auto reference = copyReferenceSnapshot();
  const auto referenceEnabled = flow88::state::getBoolParameter(
      apvts_, flow88::state::paramIds::referenceModeEnabled);

  state.currentProfileTitle = profile.subgenreName + " / " + profile.styleName;
  state.suggestedProfileTitle =
      suggested.subgenreName + " / " + suggested.styleName;
  state.assistantSummary = sessionState_.getAssistantSummary();
  state.currentProfileGuide = buildProfileGuide(profile);
  state.analysisInProgress = sessionState_.isAnalysisInProgress();
  state.analysisReady = sessionState_.isAnalysisReady();
  state.analysisStateText =
      state.analysisInProgress ? "Analyzing"
                               : (state.analysisReady ? "Ready" : "Idle");
  state.loudnessPresetText = flow88::state::kLoudnessPresetChoices[
      flow88::state::getChoiceParameterIndex(apvts_,
                                             flow88::state::paramIds::loudnessPreset)];
  state.accent = profile.accent;
  state.advancedPanelOpen = sessionState_.isAdvancedPanelOpen();
  state.activeABSlot = sessionState_.getActiveABSlot();
  state.bypassed = flow88::state::getBoolParameter(apvts_,
                                                   flow88::state::paramIds::bypass);
  state.referenceAnalyzing = reference.analyzing;
  state.referenceReady = referenceEnabled && reference.ready &&
                         reference.errorMessage.isEmpty();
  state.referenceFileLabel = reference.fileLabel;
  state.targetLufs =
      flow88::state::getFloatParameter(apvts_, flow88::state::paramIds::targetLufs);
  state.integratedLufs = meters.integratedLufs;
  state.shortTermLufs = meters.shortTermLufs;
  state.truePeakDbTP = meters.truePeakDbTP;
  state.gainReductionDb = meters.gainReductionDb;
  state.compressorGainReductionDb = compressorGainReductionDb_.load();
  state.limiterGainReductionDb = limiterGainReductionDb_.load();
  state.correlation = meters.correlation;
  state.stereoWidthPct = meters.stereoWidthPct;
  state.inputPeakDb = meters.inputPeakDb;
  state.outputPeakDb = meters.outputPeakDb;
  state.targetDriveDb = targetDriveDb_.load();
  state.referenceIntegratedLufs = reference.metrics.integratedLufs;
  state.referenceTruePeakDbTP = reference.metrics.truePeakDbTP;
  state.referenceCorrelation = reference.metrics.stereoCorrelation;
  state.referenceWidthPct = reference.metrics.stereoWidthPct;
  state.referenceTonalDeviation =
      reference.ready
          ? computeCurveDeviation(meters.spectralCurve, reference.metrics.spectralCurve)
          : 0.0f;
  state.abLevelMatchReady = abLevelMatchReady_.load();
  state.abLevelMatchGainDb = abLevelMatchGainDb_.load();

  const auto flags = stageFlags_.load();
  state.inputTrimStageActive = (flags & inputTrimStage) != 0;
  state.toneStageActive = (flags & toneStage) != 0;
  state.lowEndStageActive = (flags & lowEndStage) != 0;
  state.dynamicsStageActive = (flags & dynamicsStage) != 0;
  state.widthStageActive = (flags & widthStage) != 0;
  state.saturationStageActive = (flags & saturationStage) != 0;
  state.clipperStageActive = (flags & clipperStage) != 0;
  state.limiterStageActive = (flags & limiterStage) != 0;
  state.outputTrimStageActive = (flags & outputTrimStage) != 0;
  if (!referenceEnabled)
    state.referenceStatus = "Reference Off";
  else if (reference.filePath.isEmpty())
    state.referenceStatus = "Load Reference";
  else if (reference.analyzing)
    state.referenceStatus = "Analyzing Reference";
  else if (!reference.errorMessage.isEmpty())
    state.referenceStatus = "Reference Error";
  else if (reference.ready)
    state.referenceStatus = "Reference Ready";
  else
    state.referenceStatus = "Reference Pending";

  if (state.referenceReady)
    state.referenceGuidance = describeReferenceGuidance(meters, reference.metrics);
  else if (!reference.errorMessage.isEmpty())
    state.referenceGuidance = reference.errorMessage;

  state.targetCurve = state.referenceReady ? reference.metrics.spectralCurve
                                           : profile.analysisTargets.spectralCurve;
  state.actualCurve = meters.spectralCurve;

  return state;
}

void Flow88MasterAudioProcessor::setSubgenreIndex(int index) {
  const auto clamped =
      juce::jlimit(0, flow88::state::kSubgenreChoices.size() - 1, index);
  applySelectionProfile(flow88::state::choiceLabelToId(
                            flow88::state::kSubgenreChoices[clamped]),
                        getSelectedStyleId());
}

void Flow88MasterAudioProcessor::setStyleIndex(int index) {
  const auto clamped =
      juce::jlimit(0, flow88::state::kStyleChoices.size() - 1, index);
  applySelectionProfile(getSelectedSubgenreId(),
                        flow88::state::choiceLabelToId(
                            flow88::state::kStyleChoices[clamped]));
}

void Flow88MasterAudioProcessor::setLoudnessPresetIndex(int index) {
  const auto clamped =
      juce::jlimit(0, flow88::state::kLoudnessPresetChoices.size() - 1, index);
  flow88::state::setChoiceParameter(apvts_, flow88::state::paramIds::loudnessPreset,
                                    clamped);
  if (clamped != 3)
    flow88::state::setFloatParameter(apvts_, flow88::state::paramIds::targetLufs,
                                     targetForLoudnessPreset(clamped));
}

void Flow88MasterAudioProcessor::setTargetLufsExact(float value) {
  flow88::state::setChoiceParameter(apvts_, flow88::state::paramIds::loudnessPreset,
                                    3);
  flow88::state::setFloatParameter(apvts_, flow88::state::paramIds::targetLufs,
                                   juce::jlimit(-16.0f, -7.0f, value));
}

void Flow88MasterAudioProcessor::setOversamplingIndex(int index) {
  flow88::state::setChoiceParameter(apvts_, flow88::state::paramIds::oversampling,
                                    index);
}

void Flow88MasterAudioProcessor::toggleAdvancedPanel() {
  sessionState_.setAdvancedPanelOpen(!sessionState_.isAdvancedPanelOpen());
}

void Flow88MasterAudioProcessor::switchABSlot(int slotIndex) {
  const auto targetSlot = juce::jlimit(0, 1, slotIndex);
  const auto currentSlot = sessionState_.getActiveABSlot();

  if (targetSlot == currentSlot)
    return;

  const auto currentComparableLufs = chooseComparableLufsForAB(meterSource_.snapshot());
  sessionState_.captureSlot(currentSlot, apvts_.copyState());
  if (hasComparableLufsForAB(currentComparableLufs))
    abSlotComparableLufs_[static_cast<size_t>(currentSlot)].store(currentComparableLufs);

  const auto targetHasSnapshot = sessionState_.hasSnapshotForSlot(targetSlot);
  if (!targetHasSnapshot) {
    sessionState_.captureSlot(targetSlot, apvts_.copyState());
    if (hasComparableLufsForAB(currentComparableLufs))
      abSlotComparableLufs_[static_cast<size_t>(targetSlot)].store(currentComparableLufs);
  }

  const auto targetComparableLufs =
      abSlotComparableLufs_[static_cast<size_t>(targetSlot)].load();
  if (hasComparableLufsForAB(currentComparableLufs) &&
      hasComparableLufsForAB(targetComparableLufs)) {
    abLevelMatchGainDb_.store(juce::jlimit(
        -6.0f, 6.0f, currentComparableLufs - targetComparableLufs));
    abLevelMatchReady_.store(true);
  } else {
    abLevelMatchGainDb_.store(0.0f);
    abLevelMatchReady_.store(false);
  }

  if (const auto restored = sessionState_.restoreSlot(targetSlot); restored.isValid())
    apvts_.replaceState(restored);

  sessionState_.setActiveABSlot(targetSlot);
  cacheProfileTuning(getCurrentProfile());
  refreshReferenceStatus();
}

void Flow88MasterAudioProcessor::startAnalysisSimulation() {
  sessionState_.setAnalysisState("analyzing");
  sessionState_.setAssistantSummary(
      "Measuring loudness, peak headroom, spectral balance, and stereo consistency.");
}

void Flow88MasterAudioProcessor::finishAnalysisSimulation() {
  const auto profile = getCurrentProfile();
  const auto meters = meterSource_.snapshot();
  sessionState_.setAnalysisState("ready");

  auto suggestedSubgenreId = profile.subgenreId;
  if (meters.outputPeakDb > -55.0f)
    suggestedSubgenreId = chooseSuggestedSubgenreId(profileLibrary_, meters);

  sessionState_.setSuggestedProfile(suggestedSubgenreId, profile.styleId);

  if (meters.outputPeakDb <= -55.0f) {
    sessionState_.setAssistantSummary(
        "No stable signal was captured yet. Start playback and run Analyze again for measured loudness and tonal readings.");
    return;
  }

  const auto widthText = juce::String(meters.stereoWidthPct, 0) + "%";
  const auto summary =
      "Measured " + juce::String(meters.integratedLufs, 1) + " LUFS integrated, " +
      juce::String(meters.truePeakDbTP, 1) + " dBTP true peak, " +
      widthText + " width, correlation " + formatSigned(meters.correlation, 2) +
      ". " + describeSpectrum(meters.spectralCurve) + ".";
  sessionState_.setAssistantSummary(summary);
}

juce::String Flow88MasterAudioProcessor::getSelectedSubgenreId() const {
  const auto index = flow88::state::getChoiceParameterIndex(
      apvts_, flow88::state::paramIds::subgenre);
  return flow88::state::choiceLabelToId(flow88::state::kSubgenreChoices[index]);
}

juce::String Flow88MasterAudioProcessor::getSelectedStyleId() const {
  const auto index =
      flow88::state::getChoiceParameterIndex(apvts_, flow88::state::paramIds::style);
  return flow88::state::choiceLabelToId(flow88::state::kStyleChoices[index]);
}

flow88::state::EffectiveProfile Flow88MasterAudioProcessor::getCurrentProfile() const {
  return profileLibrary_.compose(getSelectedSubgenreId(), getSelectedStyleId());
}

void Flow88MasterAudioProcessor::applySelectionProfile(const juce::String &subgenreId,
                                                       const juce::String &styleId) {
  const auto profile = profileLibrary_.compose(subgenreId, styleId);
  flow88::state::applyEffectiveProfile(apvts_, profile);
  cacheProfileTuning(profile);
  sessionState_.setSuggestedProfile(subgenreId, styleId);
  if (!sessionState_.isAnalysisInProgress())
    sessionState_.setAssistantSummary(profile.assistantSummary);
  refreshReferenceStatus();
}

void Flow88MasterAudioProcessor::refreshReferenceStatus() {
  const auto referenceEnabled = flow88::state::getBoolParameter(
      apvts_, flow88::state::paramIds::referenceModeEnabled);
  const auto referencePath = sessionState_.getReferencePath();
  const auto snapshot = copyReferenceSnapshot();

  if (!referenceEnabled) {
    sessionState_.setReferenceStatus("Reference Off");
    return;
  }

  if (referencePath.isEmpty()) {
    sessionState_.setReferenceStatus("Load Reference");
    return;
  }

  if (snapshot.filePath != referencePath || (!snapshot.ready && !snapshot.analyzing &&
                                             snapshot.errorMessage.isEmpty())) {
    beginReferenceAnalysis(juce::File(referencePath));
    sessionState_.setReferenceStatus("Analyzing Reference");
    return;
  }

  if (snapshot.analyzing) {
    sessionState_.setReferenceStatus("Analyzing Reference");
    return;
  }

  if (!snapshot.errorMessage.isEmpty()) {
    sessionState_.setReferenceStatus("Reference Error");
    return;
  }

  sessionState_.setReferenceStatus(snapshot.ready ? "Reference Ready"
                                                  : "Reference Pending");
}

void Flow88MasterAudioProcessor::cacheProfileTuning(
    const flow88::state::EffectiveProfile &profile) {
  toneScale_.store(profile.dspTuning.toneScale);
  punchScale_.store(profile.dspTuning.punchScale);
  widthScale_.store(profile.dspTuning.widthScale);
  lowEndFocusScale_.store(profile.dspTuning.lowEndFocusScale);
  attitudeScale_.store(profile.dspTuning.attitudeScale);
  targetDriveBiasDb_.store(profile.dspTuning.targetDriveBiasDb);
  lowEndAnchorHz_.store(profile.dspTuning.lowEndAnchorHz);
  widthProtectHz_.store(profile.dspTuning.widthProtectHz);
  dynamicsThresholdBiasDb_.store(profile.dspTuning.dynamicsThresholdBiasDb);
  limiterReleaseScale_.store(profile.dspTuning.limiterReleaseScale);
  harshnessAmount_.store(profile.dspTuning.harshnessAmount);
}

Flow88MasterAudioProcessor::ReferenceSnapshot
Flow88MasterAudioProcessor::copyReferenceSnapshot() const {
  const juce::ScopedLock lock(referenceLock_);
  return referenceSnapshot_;
}

void Flow88MasterAudioProcessor::loadReferenceTrack(const juce::File &file) {
  if (!file.existsAsFile())
    return;

  sessionState_.setReferencePath(file.getFullPathName());
  flow88::state::setBoolParameter(apvts_, flow88::state::paramIds::referenceModeEnabled,
                                  true);
  beginReferenceAnalysis(file);
  refreshReferenceStatus();
}

void Flow88MasterAudioProcessor::clearReferenceTrack() {
  referenceRequestId_.fetch_add(1);
  {
    const juce::ScopedLock lock(referenceLock_);
    referenceSnapshot_ = {};
  }

  sessionState_.setReferencePath({});
  flow88::state::setBoolParameter(apvts_, flow88::state::paramIds::referenceModeEnabled,
                                  false);
  refreshReferenceStatus();
}

void Flow88MasterAudioProcessor::beginReferenceAnalysis(const juce::File &file) {
  if (!file.existsAsFile())
    return;

  const auto requestId = referenceRequestId_.fetch_add(1) + 1;
  {
    const juce::ScopedLock lock(referenceLock_);
    referenceSnapshot_.analyzing = true;
    referenceSnapshot_.ready = false;
    referenceSnapshot_.filePath = file.getFullPathName();
    referenceSnapshot_.fileLabel = file.getFileNameWithoutExtension();
    referenceSnapshot_.errorMessage.clear();
    referenceSnapshot_.metrics = {};
  }

  const auto profile = getCurrentProfile();
  referenceThreadPool_.addJob(
      new ReferenceAnalysisJob(*this, requestId, file, profile.analysisTargets),
      true);
}

void Flow88MasterAudioProcessor::completeReferenceAnalysis(
    int requestId, const juce::File &file,
    const flow88::eval::AnalysisMetrics &metrics,
    const juce::String &errorMessage) {
  if (requestId != referenceRequestId_.load())
    return;

  {
    const juce::ScopedLock lock(referenceLock_);
    referenceSnapshot_.analyzing = false;
    referenceSnapshot_.ready = errorMessage.isEmpty();
    referenceSnapshot_.filePath = file.getFullPathName();
    referenceSnapshot_.fileLabel = file.getFileNameWithoutExtension();
    referenceSnapshot_.errorMessage = errorMessage;
    referenceSnapshot_.metrics = metrics;
  }
}

void Flow88MasterAudioProcessor::ensureSnapshotDefaults() {
  const auto activeSlot = sessionState_.getActiveABSlot();
  const auto inactiveSlot = activeSlot == 0 ? 1 : 0;

  if (!sessionState_.hasSnapshotForSlot(activeSlot))
    sessionState_.captureSlot(activeSlot, apvts_.copyState());

  if (!sessionState_.hasSnapshotForSlot(inactiveSlot))
    sessionState_.captureSlot(inactiveSlot, apvts_.copyState());

  if (const auto restored = sessionState_.restoreSlot(activeSlot); restored.isValid())
    apvts_.replaceState(restored);
}

float Flow88MasterAudioProcessor::targetForLoudnessPreset(int index) {
  switch (index) {
  case 0:
    return -14.0f;
  case 1:
    return -9.0f;
  case 2:
    return -11.0f;
  default:
    return -9.0f;
  }
}

juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter() {
  return new Flow88MasterAudioProcessor();
}
