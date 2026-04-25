#include "PluginEditor.h"
#include <cmath>

namespace {

juce::String formatLufs(float value) {
  return juce::String(value, 1) + " LUFS";
}

juce::String formatDb(float value, const juce::String &suffix = " dB") {
  return juce::String(value, 1) + suffix;
}

juce::String formatSigned(float value, int decimals = 1,
                          const juce::String &suffix = {}) {
  auto text = juce::String(value, decimals);
  if (value > 0.0f)
    text = "+" + text;
  return text + suffix;
}

juce::String formatPercent(float value) {
  return juce::String(value, 0) + " %";
}

juce::String makeDiagnosticLine(const juce::String &label,
                                const juce::String &value,
                                const juce::String &delta = {}) {
  auto line = label + ": " + value;
  if (delta.isNotEmpty())
    line << " | " << delta;
  return line;
}

juce::String makeStageLine(const juce::String &name, const juce::String &status,
                           const juce::String &detail) {
  return name + ": " + status + " | " + detail;
}

juce::String stageStatusString(bool globalBypassed, bool isActive,
                               bool isImplemented = true) {
  if (!isImplemented)
    return "PLACEHOLDER";
  if (globalBypassed)
    return "BYPASSED";
  return isActive ? "ACTIVE" : "BYPASSED";
}

const juce::StringArray kStyleUiChoices{"Warm", "Clean", "Club"};
constexpr std::array<int, 5> kSubgenreUiOrder{4, 0, 1, 2, 3};
constexpr int kHeaderInsetY = 12;
constexpr int kHeaderGap = 10;
constexpr int kABControlWidth = 112;
constexpr int kABMatchWidth = 154;
constexpr int kBypassWidth = 144;
constexpr int kAssistantMinWidth = 288;
constexpr int kAssistantMaxWidth = 336;
constexpr int kTopRowMinHeight = 292;
constexpr int kTopRowMaxHeight = 324;
constexpr float kTopRowHeightRatio = 0.34f;
constexpr int kAssistantRowGap = 5;
constexpr int kAnalyzeRowHeight = 32;
constexpr int kProfileChipHeight = 26;
constexpr int kAssistantChipHeight = 24;
constexpr int kAssistantControlHeight = 30;
constexpr int kTargetChipWidth = 112;
constexpr int kSubgenreColumns = 3;
constexpr int kSubgenreButtonHeight = 24;
constexpr int kSubgenreButtonGap = 4;

juce::String makeAbMatchText(
    const Flow88MasterAudioProcessor::DisplayState &display) {
  if (!display.abLevelMatchReady)
    return "A/B Match Pending";

  return "A/B Match " + formatSigned(display.abLevelMatchGainDb, 1, " dB");
}

bool hasMeasuredSnapshotSummary(const juce::String &summary) {
  return summary.containsIgnoreCase("Measured") ||
         summary.containsIgnoreCase("No stable signal");
}

juce::String makeAnalysisChipText(
    const Flow88MasterAudioProcessor::DisplayState &display) {
  if (display.analysisInProgress)
    return "Measuring";
  if (hasMeasuredSnapshotSummary(display.assistantSummary))
    return "Ready";
  return "Idle";
}

bool shouldShowReferenceStatus(
    const Flow88MasterAudioProcessor::DisplayState &display) {
  return display.referenceReady || display.referenceAnalyzing ||
         display.referenceStatus == "Reference Error";
}

juce::String makeReferenceChipText(
    const Flow88MasterAudioProcessor::DisplayState &display) {
  if (display.referenceReady && display.referenceFileLabel.isNotEmpty())
    return "Reference: " + display.referenceFileLabel;
  return display.referenceStatus;
}

juce::String buildDiagnosticsReport(
    const Flow88MasterAudioProcessor::DisplayState &display,
    const juce::AudioProcessorValueTreeState &apvts) {
  const auto toneTiltDb = flow88::state::getFloatParameter(
      apvts, flow88::state::paramIds::toneTiltDb);
  const auto punchPct =
      flow88::state::getFloatParameter(apvts, flow88::state::paramIds::punchPct);
  const auto macroWidthPct = flow88::state::getFloatParameter(
      apvts, flow88::state::paramIds::stereoWidthPct);
  const auto lowEndFocusPct = flow88::state::getFloatParameter(
      apvts, flow88::state::paramIds::lowEndFocusPct);
  const auto attitudePct = flow88::state::getFloatParameter(
      apvts, flow88::state::paramIds::attitudePct);
  const auto inputTrimDb = flow88::state::getFloatParameter(
      apvts, flow88::state::paramIds::inputTrimDb);
  const auto outputTrimDb = flow88::state::getFloatParameter(
      apvts, flow88::state::paramIds::outputTrimDb);
  const auto limiterCeilingDbTP = flow88::state::getFloatParameter(
      apvts, flow88::state::paramIds::limiterCeilingDbTP);
  const auto clipperAmountPct = flow88::state::getFloatParameter(
      apvts, flow88::state::paramIds::clipperAmountPct);
  const auto resonanceControlPct = flow88::state::getFloatParameter(
      apvts, flow88::state::paramIds::resonanceControlPct);

  juce::StringArray lines;
  lines.add("FLOW88 MASTER ENGINEERING OVERLAY");
  lines.add("CURRENT PROFILE");
  lines.add(makeDiagnosticLine("Profile", display.currentProfileTitle));
  lines.add(makeDiagnosticLine("Suggested", display.suggestedProfileTitle));
  lines.add({});
  lines.add("LIVE VALUES");
  lines.add(makeDiagnosticLine("Input Peak",
                               formatDb(display.inputPeakDb),
                               "pre trim"));
  lines.add(makeDiagnosticLine(
      "Output Peak", formatDb(display.outputPeakDb),
      juce::String("delta ") +
          formatSigned(display.outputPeakDb - display.inputPeakDb, 1) + " dB"));
  lines.add(makeDiagnosticLine(
      "Short-Term", formatLufs(display.shortTermLufs),
      juce::String("vs target ") +
          formatSigned(display.shortTermLufs - display.targetLufs, 1) + " LU"));
  lines.add(makeDiagnosticLine(
      "Integrated", formatLufs(display.integratedLufs),
      juce::String("vs target ") +
          formatSigned(display.integratedLufs - display.targetLufs, 1) + " LU"));
  lines.add(makeDiagnosticLine(
      "True Peak", formatDb(display.truePeakDbTP, " dBTP"),
      juce::String("isp delta ") +
          formatSigned(display.truePeakDbTP - display.outputPeakDb, 1) + " dB"));
  lines.add(makeDiagnosticLine("Stereo Width", formatPercent(display.stereoWidthPct)));
  lines.add(makeDiagnosticLine("Correlation", formatSigned(display.correlation, 2)));
  lines.add({});
  if (display.referenceReady || display.referenceAnalyzing ||
      display.referenceFileLabel.isNotEmpty()) {
    lines.add("REFERENCE");
    lines.add(makeDiagnosticLine("Status", display.referenceStatus));
    lines.add(makeDiagnosticLine("Source", display.referenceFileLabel));
    if (display.referenceReady) {
      lines.add(makeDiagnosticLine("Ref LUFS",
                                   formatLufs(display.referenceIntegratedLufs),
                                   "delta " +
                                       formatSigned(display.integratedLufs -
                                                    display.referenceIntegratedLufs,
                                                    1) +
                                       " LU"));
      lines.add(makeDiagnosticLine(
          "Ref True Peak", formatDb(display.referenceTruePeakDbTP, " dBTP"),
          "delta " +
              formatSigned(display.truePeakDbTP - display.referenceTruePeakDbTP, 1) +
              " dB"));
      lines.add(makeDiagnosticLine(
          "Ref Stereo", formatSigned(display.referenceCorrelation, 2) + " / " +
                            juce::String(display.referenceWidthPct, 0) + "%",
          "curve dev " + juce::String(display.referenceTonalDeviation, 2)));
    }
    if (display.referenceGuidance.isNotEmpty())
      lines.add(makeDiagnosticLine("Guidance", display.referenceGuidance));
    lines.add({});
  }
  lines.add("MACROS");
  lines.add(makeDiagnosticLine("Tone", formatDb(toneTiltDb), "broad tilt"));
  lines.add(makeDiagnosticLine("Punch", formatPercent(punchPct), "glue amount"));
  lines.add(makeDiagnosticLine("Width", formatPercent(macroWidthPct), "high-side gain"));
  lines.add(makeDiagnosticLine("Low-End Focus", formatPercent(lowEndFocusPct),
                               "bass anchor"));
  lines.add(makeDiagnosticLine("Target LUFS", formatLufs(display.targetLufs),
                               formatSigned(display.targetDriveDb, 1) + " dB drive"));
  lines.add(makeDiagnosticLine("Attitude", formatPercent(attitudePct),
                               "sat / clip"));
  lines.add({});
  lines.add("STAGES");
  lines.add(makeStageLine(
      "Input Trim", stageStatusString(display.bypassed, display.inputTrimStageActive),
      formatSigned(inputTrimDb, 1) + " dB"));
  lines.add(makeStageLine(
      "Tone Shaping", stageStatusString(display.bypassed, display.toneStageActive),
      formatDb(toneTiltDb) + "  res " + formatPercent(resonanceControlPct)));
  lines.add(makeStageLine(
      "Low-End Focus",
      stageStatusString(display.bypassed, display.lowEndStageActive),
      formatPercent(lowEndFocusPct) + " anchor"));
  lines.add(makeStageLine(
      "Punch / Glue",
      stageStatusString(display.bypassed, display.dynamicsStageActive),
      "gr " + formatDb(display.compressorGainReductionDb)));
  lines.add(makeStageLine(
      "Stereo Width", stageStatusString(display.bypassed, display.widthStageActive),
      formatPercent(macroWidthPct)));
  lines.add(makeStageLine(
      "Saturation",
      stageStatusString(display.bypassed, display.saturationStageActive),
      "att " + formatPercent(attitudePct)));
  lines.add(makeStageLine(
      "Soft Clipper",
      stageStatusString(display.bypassed, display.clipperStageActive),
      formatPercent(clipperAmountPct)));
  lines.add(makeStageLine(
      "TP Limiter", stageStatusString(display.bypassed, display.limiterStageActive),
      "gr " + formatDb(display.limiterGainReductionDb) + "  ceil " +
          formatDb(limiterCeilingDbTP, " dBTP")));
  lines.add(makeStageLine(
      "Output Trim", stageStatusString(display.bypassed, display.outputTrimStageActive),
      formatSigned(outputTrimDb, 1) + " dB"));
  lines.add(makeStageLine(
      "Loudness Meter", stageStatusString(false, true),
      juce::String(display.shortTermLufs, 1) + " / " +
          juce::String(display.integratedLufs, 1) + " LUFS"));
  lines.add(makeStageLine(
      "True Peak Meter", stageStatusString(false, true),
      juce::String(display.truePeakDbTP, 1) + " dBTP"));
  lines.add(makeStageLine(
      "Stereo Analyzer", stageStatusString(false, true),
      "w " + juce::String(display.stereoWidthPct, 0) + "%  c " +
          formatSigned(display.correlation, 2)));
  lines.add(makeStageLine("Tonal Balance", stageStatusString(false, true),
                          "7-band curve live"));

  return lines.joinIntoString("\n");
}

float normaliseMeter(float value, float minimum, float maximum) {
  return juce::jlimit(0.0f, 1.0f, juce::jmap(value, minimum, maximum, 0.0f, 1.0f));
}

} // namespace

Flow88MasterAudioProcessorEditor::Flow88MasterAudioProcessorEditor(
    Flow88MasterAudioProcessor &processor)
    : AudioProcessorEditor(&processor), processor_(processor) {
  setLookAndFeel(&lookAndFeel_);
  setOpaque(true);
  setResizable(true, true);
  setResizeLimits(1280, 840, 1800, 1200);
  setSize(1560, 980);

  addAndMakeVisible(assistantCard_);
  assistantCard_.setEyebrow({});
  assistantCard_.setTitle("Profile");
  assistantCard_.setAccent(flow88::ui::theme::cyan);

  addAndMakeVisible(tonalCard_);
  tonalCard_.setEyebrow({});
  tonalCard_.setTitle("Spectrum");
  tonalCard_.setAccent(flow88::ui::theme::cyan);

  addAndMakeVisible(macroCard_);
  macroCard_.setEyebrow({});
  macroCard_.setTitle("Main Macros");
  macroCard_.setAccent(flow88::ui::theme::cyan);

  addAndMakeVisible(meterDockCard_);
  meterDockCard_.setElevated(true);

  addAndMakeVisible(advancedCard_);
  advancedCard_.setEyebrow("ADVANCED");
  advancedCard_.setTitle("Precision");
  advancedCard_.setSubtitle("Exact target, trims, ceiling, oversampling, and restrained reference checks.");
  advancedCard_.setAccent(flow88::ui::theme::blue);
  advancedCard_.setVisible(false);

  addAndMakeVisible(diagnosticsCard_);
  diagnosticsCard_.setEyebrow("ENGINEERING");
  diagnosticsCard_.setTitle("Diagnostics");
  diagnosticsCard_.setSubtitle("Live validation for meters, controls, and stage mapping.");
  diagnosticsCard_.setAccent(flow88::ui::theme::teal);
  diagnosticsCard_.setVisible(false);

  assistantCard_.addAndMakeVisible(currentProfileChip_);
  currentProfileChip_.setActive(true);

  addAndMakeVisible(abControl_);
  abControl_.setOptions({"A", "B"});
  abControl_.onSelectionChanged = [this](int index) { processor_.switchABSlot(index); };

  addAndMakeVisible(abMatchChip_);
  abMatchChip_.setAccent(flow88::ui::theme::amber);
  abMatchChip_.setActive(false);
  abMatchChip_.setVisible(false);

  addAndMakeVisible(bypassToggle_);
  addAndMakeVisible(diagnosticsButton_);
  diagnosticsButton_.onClick = [this]() { toggleDiagnosticsOverlay(); };
  addAndMakeVisible(settingsButton_);
  settingsButton_.onClick = [this]() { processor_.toggleAdvancedPanel(); };

  assistantCard_.addAndMakeVisible(analyzeButton_);
  analyzeButton_.setColour(juce::TextButton::buttonColourId,
                           flow88::ui::theme::cyan.withAlpha(0.86f));
  analyzeButton_.setColour(juce::TextButton::textColourOffId, flow88::ui::theme::bg);
  analyzeButton_.setButtonText("Analyze Playback");
  analyzeButton_.onClick = [this]() {
    analysisRunning_ = true;
    analysisStartedMs_ = juce::Time::getMillisecondCounter();
    processor_.startAnalysisSimulation();
  };

  assistantCard_.addAndMakeVisible(analysisChip_);
  assistantCard_.addAndMakeVisible(targetChip_);
  assistantCard_.addAndMakeVisible(referenceChip_);
  referenceChip_.setVisible(false);

  assistantCard_.addAndMakeVisible(styleControl_);
  styleControl_.setOptions(kStyleUiChoices);
  styleControl_.onSelectionChanged = [this](int index) {
    processor_.setStyleIndex(flow88::state::findChoiceIndex(
        flow88::state::kStyleChoices, kStyleUiChoices[index]));
  };

  assistantCard_.addAndMakeVisible(loudnessControl_);
  loudnessControl_.setOptions(flow88::state::kLoudnessPresetChoices);
  loudnessControl_.onSelectionChanged = [this](int index) {
    processor_.setLoudnessPresetIndex(index);
  };

  subgenreButtons_.reserve(static_cast<size_t>(flow88::state::kSubgenreChoices.size()));
  subgenreButtonSourceIndices_.reserve(
      static_cast<size_t>(flow88::state::kSubgenreChoices.size()));
  for (const auto sourceIndex : kSubgenreUiOrder) {
    auto button = std::make_unique<juce::TextButton>(
        flow88::state::kSubgenreChoices[sourceIndex]);
    button->setClickingTogglesState(false);
    button->onClick = [this, sourceIndex]() { processor_.setSubgenreIndex(sourceIndex); };
    assistantCard_.addAndMakeVisible(*button);
    subgenreButtonSourceIndices_.push_back(sourceIndex);
    subgenreButtons_.push_back(std::move(button));
  }

  tonalCard_.addAndMakeVisible(tonalView_);

  macroCard_.addAndMakeVisible(toneDial_);
  macroCard_.addAndMakeVisible(punchDial_);
  macroCard_.addAndMakeVisible(widthDial_);
  macroCard_.addAndMakeVisible(lowEndDial_);
  macroCard_.addAndMakeVisible(targetDial_);
  macroCard_.addAndMakeVisible(attitudeDial_);

  meterDockCard_.addAndMakeVisible(integratedMeter_);
  meterDockCard_.addAndMakeVisible(truePeakMeter_);
  meterDockCard_.addAndMakeVisible(gainReductionMeter_);
  meterDockCard_.addAndMakeVisible(correlationMeter_);
  meterDockCard_.addAndMakeVisible(ioMeter_);
  correlationMeter_.setBipolar(true);

  advancedCard_.addAndMakeVisible(closeAdvancedButton_);
  closeAdvancedButton_.onClick = [this]() { processor_.toggleAdvancedPanel(); };
  advancedCard_.addAndMakeVisible(loadReferenceButton_);
  advancedCard_.addAndMakeVisible(clearReferenceButton_);
  loadReferenceButton_.onClick = [this]() { launchReferenceChooser(); };
  clearReferenceButton_.onClick = [this]() { processor_.clearReferenceTrack(); };

  advancedCard_.addAndMakeVisible(exactTargetLabel_);
  exactTargetLabel_.setEditable(true, true, false);
  exactTargetLabel_.setJustificationType(juce::Justification::centred);
  exactTargetLabel_.setColour(juce::Label::backgroundColourId,
                              flow88::ui::theme::panel2);
  exactTargetLabel_.setColour(juce::Label::outlineColourId,
                              flow88::ui::theme::stroke);
  exactTargetLabel_.setColour(juce::Label::textColourId,
                              flow88::ui::theme::textMain);
  exactTargetLabel_.onEditorHide = [this]() { handleExactTargetCommit(); };

  configureAdvancedSlider(inputTrimSlider_, flow88::ui::theme::cyan);
  configureAdvancedSlider(outputTrimSlider_, flow88::ui::theme::blue);
  configureAdvancedSlider(limiterCeilingSlider_, flow88::ui::theme::amber);

  advancedCard_.addAndMakeVisible(inputTrimSlider_);
  advancedCard_.addAndMakeVisible(outputTrimSlider_);
  advancedCard_.addAndMakeVisible(limiterCeilingSlider_);

  advancedCard_.addAndMakeVisible(inputTrimCaption_);
  advancedCard_.addAndMakeVisible(outputTrimCaption_);
  advancedCard_.addAndMakeVisible(limiterCaption_);
  advancedCard_.addAndMakeVisible(oversamplingCaption_);
  inputTrimCaption_.setText("Input Trim", juce::dontSendNotification);
  outputTrimCaption_.setText("Output Trim", juce::dontSendNotification);
  limiterCaption_.setText("Limiter Ceiling", juce::dontSendNotification);
  oversamplingCaption_.setText("Oversampling", juce::dontSendNotification);
  for (auto *label : {&inputTrimCaption_, &outputTrimCaption_, &limiterCaption_,
                      &oversamplingCaption_}) {
    label->setJustificationType(juce::Justification::centredLeft);
    label->setColour(juce::Label::textColourId, flow88::ui::theme::textDim);
  }

  advancedCard_.addAndMakeVisible(oversamplingControl_);
  oversamplingControl_.setOptions(flow88::state::kOversamplingChoices);
  oversamplingControl_.onSelectionChanged = [this](int index) {
    processor_.setOversamplingIndex(index);
  };

  diagnosticsCard_.addAndMakeVisible(diagnosticsText_);
  diagnosticsText_.setMultiLine(true);
  diagnosticsText_.setReadOnly(true);
  diagnosticsText_.setScrollbarsShown(true);
  diagnosticsText_.setPopupMenuEnabled(false);
  diagnosticsText_.setCaretVisible(false);
  diagnosticsText_.setFont(juce::Font(juce::FontOptions(
      juce::Font::getDefaultMonospacedFontName(), 11.5f, juce::Font::plain)));
  diagnosticsText_.setColour(juce::TextEditor::backgroundColourId,
                             juce::Colours::transparentBlack);
  diagnosticsText_.setColour(juce::TextEditor::outlineColourId,
                             juce::Colours::transparentBlack);
  diagnosticsText_.setColour(juce::TextEditor::shadowColourId,
                             juce::Colours::transparentBlack);
  diagnosticsText_.setColour(juce::TextEditor::textColourId,
                             flow88::ui::theme::textMain);
  diagnosticsText_.setColour(juce::CaretComponent::caretColourId,
                             juce::Colours::transparentBlack);
  diagnosticsText_.setBorder(juce::BorderSize<int>(8, 8, 10, 8));

  auto &apvts = processor_.getAPVTS();
  toneAttachment_ = std::make_unique<SliderAttachment>(
      apvts, flow88::state::paramIds::toneTiltDb, toneDial_.getSlider());
  punchAttachment_ = std::make_unique<SliderAttachment>(
      apvts, flow88::state::paramIds::punchPct, punchDial_.getSlider());
  widthAttachment_ = std::make_unique<SliderAttachment>(
      apvts, flow88::state::paramIds::stereoWidthPct, widthDial_.getSlider());
  lowEndAttachment_ = std::make_unique<SliderAttachment>(
      apvts, flow88::state::paramIds::lowEndFocusPct, lowEndDial_.getSlider());
  targetAttachment_ = std::make_unique<SliderAttachment>(
      apvts, flow88::state::paramIds::targetLufs, targetDial_.getSlider());
  attitudeAttachment_ = std::make_unique<SliderAttachment>(
      apvts, flow88::state::paramIds::attitudePct, attitudeDial_.getSlider());
  bypassAttachment_ = std::make_unique<ButtonAttachment>(
      apvts, flow88::state::paramIds::bypass, bypassToggle_);
  inputTrimAttachment_ = std::make_unique<SliderAttachment>(
      apvts, flow88::state::paramIds::inputTrimDb, inputTrimSlider_);
  outputTrimAttachment_ = std::make_unique<SliderAttachment>(
      apvts, flow88::state::paramIds::outputTrimDb, outputTrimSlider_);
  limiterAttachment_ = std::make_unique<SliderAttachment>(
      apvts, flow88::state::paramIds::limiterCeilingDbTP, limiterCeilingSlider_);

  refreshFromProcessor();
  resized();
  startTimerHz(30);
}

Flow88MasterAudioProcessorEditor::~Flow88MasterAudioProcessorEditor() {
  stopTimer();
  setLookAndFeel(nullptr);
}

void Flow88MasterAudioProcessorEditor::paint(juce::Graphics &g) {
  g.fillAll(flow88::ui::theme::bg);

  auto bounds = getLocalBounds();
  auto header = bounds.removeFromTop(flow88::ui::theme::headerHeight).toFloat();

  g.setColour(flow88::ui::theme::panel.darker(0.08f));
  g.fillRect(header);
  g.setColour(flow88::ui::theme::stroke);
  g.drawHorizontalLine(static_cast<int>(header.getBottom()) - 1, header.getX(),
                       header.getRight());

  g.setColour(flow88::ui::theme::cyan);
  g.setFont(flow88::ui::theme::titleFont(22.0f));
  g.drawText("Flow88 Master", 28, 18, 240, 32, juce::Justification::centredLeft,
             false);
}

void Flow88MasterAudioProcessorEditor::resized() {
  auto bounds = getLocalBounds().reduced(18);
  auto header = bounds.removeFromTop(flow88::ui::theme::headerHeight - 12);
  auto footer = bounds.removeFromBottom(flow88::ui::theme::meterDockHeight);
  footer.removeFromTop(flow88::ui::theme::gapSmall);

  auto headerRow = header.reduced(0, kHeaderInsetY);
  settingsButton_.setBounds(headerRow.removeFromRight(114));
  headerRow.removeFromRight(kHeaderGap);
  diagnosticsButton_.setBounds(headerRow.removeFromRight(108));
  headerRow.removeFromRight(16);
  bypassToggle_.setBounds(headerRow.removeFromRight(kBypassWidth));
  headerRow.removeFromRight(kHeaderGap);

  if (abMatchChip_.isVisible()) {
    abMatchChip_.setBounds(headerRow.removeFromRight(kABMatchWidth));
    headerRow.removeFromRight(12);
  } else {
    abMatchChip_.setBounds({});
  }

  abControl_.setBounds(headerRow.removeFromRight(kABControlWidth));

  const auto topRowHeight = juce::jlimit(
      kTopRowMinHeight, kTopRowMaxHeight,
      static_cast<int>(bounds.getHeight() * kTopRowHeightRatio));
  auto topRow = bounds.removeFromTop(topRowHeight);
  bounds.removeFromTop(flow88::ui::theme::gap);

  const auto assistantWidth = juce::jlimit(
      kAssistantMinWidth, kAssistantMaxWidth,
      static_cast<int>(topRow.getWidth() * 0.29f));
  assistantCard_.setBounds(topRow.removeFromLeft(assistantWidth));
  topRow.removeFromLeft(flow88::ui::theme::gap);
  tonalCard_.setBounds(topRow);
  macroCard_.setBounds(bounds);
  meterDockCard_.setBounds(footer);

  if (advancedCard_.isVisible()) {
    auto overlay = macroCard_.getBounds().reduced(26);
    overlay.setWidth(360);
    overlay.setHeight(438);
    overlay.setX(macroCard_.getRight() - overlay.getWidth() - 22);
    overlay.setY(macroCard_.getY() + 24);
    advancedCard_.setBounds(overlay);
  }

  if (diagnosticsCard_.isVisible()) {
    auto overlay = macroCard_.getBounds().reduced(26);
    const auto reservedForAdvanced = advancedCard_.isVisible() ? 392 : 0;
    const auto availableWidth =
        juce::jmax(460, overlay.getWidth() - reservedForAdvanced);
    overlay.setWidth(juce::jmin(620, availableWidth));
    overlay.setHeight(juce::jmin(540, overlay.getHeight() - 4));
    overlay.setX(macroCard_.getX() + 24);
    overlay.setY(macroCard_.getY() + 24);
    diagnosticsCard_.setBounds(overlay);
    diagnosticsText_.setBounds(diagnosticsCard_.getContentBounds().reduced(4, 0));
  }

  auto assistantBounds = assistantCard_.getContentBounds();
  auto analyzeRow = assistantBounds.removeFromTop(kAnalyzeRowHeight);
  auto analysisStatus = analyzeRow.removeFromRight(110);
  analyzeRow.removeFromRight(kAssistantRowGap);
  analyzeButton_.setBounds(analyzeRow);
  analysisChip_.setBounds(analysisStatus);
  assistantBounds.removeFromTop(kAssistantRowGap);

  currentProfileChip_.setBounds(assistantBounds.removeFromTop(kProfileChipHeight));
  assistantBounds.removeFromTop(kAssistantRowGap);

  if (referenceChip_.isVisible()) {
    referenceChip_.setBounds(assistantBounds.removeFromTop(kAssistantChipHeight));
    assistantBounds.removeFromTop(kAssistantRowGap);
  } else {
    referenceChip_.setBounds({});
  }

  auto styleRow = assistantBounds.removeFromTop(kAssistantControlHeight);
  auto targetBounds = styleRow.removeFromRight(kTargetChipWidth);
  styleRow.removeFromRight(kAssistantRowGap);
  styleControl_.setBounds(styleRow);
  targetChip_.setBounds(targetBounds);
  assistantBounds.removeFromTop(kAssistantRowGap);

  loudnessControl_.setBounds(assistantBounds.removeFromTop(kAssistantControlHeight));
  assistantBounds.removeFromTop(kAssistantRowGap);

  const auto subgenreRows =
      juce::jmax(1, (static_cast<int>(subgenreButtons_.size()) + kSubgenreColumns - 1) /
                        kSubgenreColumns);
  const auto subgenreHeight = subgenreRows * kSubgenreButtonHeight +
                              (subgenreRows - 1) * kSubgenreButtonGap;
  auto subgenreBounds = assistantBounds.removeFromTop(subgenreHeight);

  const auto buttonWidth =
      (subgenreBounds.getWidth() - (kSubgenreColumns - 1) * kSubgenreButtonGap) /
      kSubgenreColumns;
  for (size_t i = 0; i < subgenreButtons_.size(); ++i)
    subgenreButtons_[i]->setBounds({});

  int buttonIndex = 0;
  for (int row = 0; row < subgenreRows; ++row) {
    const auto remainingButtons =
        static_cast<int>(subgenreButtons_.size()) - buttonIndex;
    const auto buttonsInRow = juce::jmin(kSubgenreColumns, remainingButtons);
    const auto rowWidth = buttonsInRow * buttonWidth +
                          (buttonsInRow - 1) * kSubgenreButtonGap;
    const auto startX =
        subgenreBounds.getX() + (subgenreBounds.getWidth() - rowWidth) / 2;
    const auto y =
        subgenreBounds.getY() + row * (kSubgenreButtonHeight + kSubgenreButtonGap);

    for (int col = 0; col < buttonsInRow; ++col) {
      subgenreButtons_[static_cast<size_t>(buttonIndex++)]->setBounds(
          startX + col * (buttonWidth + kSubgenreButtonGap), y, buttonWidth,
          kSubgenreButtonHeight);
    }
  }

  tonalView_.setBounds(tonalCard_.getContentBounds());

  auto macroBounds = macroCard_.getContentBounds().reduced(8, 6);
  const int tileGap = 6;
  const int tileWidth = (macroBounds.getWidth() - tileGap * 5) / 6;
  for (auto *component :
       {static_cast<juce::Component *>(&toneDial_),
        static_cast<juce::Component *>(&punchDial_),
        static_cast<juce::Component *>(&widthDial_),
        static_cast<juce::Component *>(&lowEndDial_),
        static_cast<juce::Component *>(&targetDial_),
        static_cast<juce::Component *>(&attitudeDial_)}) {
    component->setBounds(macroBounds.removeFromLeft(tileWidth));
    macroBounds.removeFromLeft(tileGap);
  }

  auto meterBounds = meterDockCard_.getContentBounds();
  const int meterGap = 10;
  const int meterWidth = (meterBounds.getWidth() - meterGap * 4) / 5;
  for (auto *meter : {static_cast<juce::Component *>(&integratedMeter_),
                      static_cast<juce::Component *>(&truePeakMeter_),
                      static_cast<juce::Component *>(&gainReductionMeter_),
                      static_cast<juce::Component *>(&correlationMeter_),
                      static_cast<juce::Component *>(&ioMeter_)}) {
    meter->setBounds(meterBounds.removeFromLeft(meterWidth));
    meterBounds.removeFromLeft(meterGap);
  }

  if (advancedCard_.isVisible()) {
    auto advancedBounds = advancedCard_.getContentBounds();
    auto advancedHeader = advancedCard_.getHeaderBounds();
    closeAdvancedButton_.setBounds(
        advancedHeader.removeFromRight(82).removeFromTop(28));

    exactTargetLabel_.setBounds(advancedBounds.removeFromTop(42));
    advancedBounds.removeFromTop(12);

    auto row = advancedBounds.removeFromTop(54);
    inputTrimCaption_.setBounds(row.removeFromLeft(120));
    inputTrimSlider_.setBounds(row.reduced(0, 10));
    advancedBounds.removeFromTop(8);

    row = advancedBounds.removeFromTop(54);
    outputTrimCaption_.setBounds(row.removeFromLeft(120));
    outputTrimSlider_.setBounds(row.reduced(0, 10));
    advancedBounds.removeFromTop(8);

    row = advancedBounds.removeFromTop(54);
    limiterCaption_.setBounds(row.removeFromLeft(120));
    limiterCeilingSlider_.setBounds(row.reduced(0, 10));
    advancedBounds.removeFromTop(10);

    oversamplingCaption_.setBounds(advancedBounds.removeFromTop(22));
    oversamplingControl_.setBounds(advancedBounds.removeFromTop(40));
    advancedBounds.removeFromTop(14);

    auto referenceRow = advancedBounds.removeFromTop(38);
    loadReferenceButton_.setBounds(referenceRow.removeFromLeft(168));
    referenceRow.removeFromLeft(10);
    clearReferenceButton_.setBounds(referenceRow.removeFromLeft(92));
  }
}

void Flow88MasterAudioProcessorEditor::timerCallback() {
  if (analysisRunning_ &&
      juce::Time::getMillisecondCounter() - analysisStartedMs_ > 1200) {
    analysisRunning_ = false;
    processor_.finishAnalysisSimulation();
  }

  refreshFromProcessor();
}

void Flow88MasterAudioProcessorEditor::refreshFromProcessor() {
  const auto display =
      processor_.buildDisplayState(juce::Time::getMillisecondCounterHiRes() * 0.001);
  const auto showAbMatch = display.abLevelMatchReady;
  const auto abMatchVisibilityChanged = abMatchChip_.isVisible() != showAbMatch;
  const auto showReference = shouldShowReferenceStatus(display);
  const auto referenceVisibilityChanged =
      referenceChip_.isVisible() != showReference;
  const auto hasMeasuredSnapshot =
      hasMeasuredSnapshotSummary(display.assistantSummary);

  currentProfileChip_.setText(display.currentProfileTitle);
  currentProfileChip_.setAccent(display.accent);
  currentProfileChip_.setActive(true);

  abMatchChip_.setText(makeAbMatchText(display));
  abMatchChip_.setAccent(flow88::ui::theme::amber);
  abMatchChip_.setActive(display.abLevelMatchReady);
  abMatchChip_.setVisible(showAbMatch);

  analysisChip_.setText(makeAnalysisChipText(display));
  analysisChip_.setAccent(display.accent);
  analysisChip_.setActive(display.analysisInProgress || hasMeasuredSnapshot);

  targetChip_.setText("Target " + formatLufs(display.targetLufs));
  targetChip_.setAccent(flow88::ui::theme::blue);
  targetChip_.setActive(true);

  referenceChip_.setText(makeReferenceChipText(display));
  referenceChip_.setAccent(flow88::ui::theme::amber);
  referenceChip_.setActive(showReference);
  referenceChip_.setVisible(showReference);

  tonalView_.setAccent(display.accent);
  tonalView_.setCurves(display.actualCurve, display.targetCurve);

  abControl_.setSelectedIndex(display.activeABSlot);
  const auto selectedStyleIndex = flow88::state::getChoiceParameterIndex(
      processor_.getAPVTS(), flow88::state::paramIds::style);
  const auto selectedStyleUiIndex =
      kStyleUiChoices.indexOf(flow88::state::kStyleChoices[selectedStyleIndex]);
  styleControl_.setSelectedIndex(juce::jmax(0, selectedStyleUiIndex));
  loudnessControl_.setSelectedIndex(flow88::state::getChoiceParameterIndex(
      processor_.getAPVTS(), flow88::state::paramIds::loudnessPreset));
  oversamplingControl_.setSelectedIndex(flow88::state::getChoiceParameterIndex(
      processor_.getAPVTS(), flow88::state::paramIds::oversampling));
  syncSubgenreButtons(flow88::state::getChoiceParameterIndex(
      processor_.getAPVTS(), flow88::state::paramIds::subgenre));

  integratedMeter_.setAccent(display.accent);
  integratedMeter_.setValueText(juce::String(display.integratedLufs, 1) + " / " +
                                juce::String(display.shortTermLufs, 1) + " LUFS");
  integratedMeter_.setNormalisedValue(
      normaliseMeter(display.shortTermLufs, -16.0f, -7.0f));

  truePeakMeter_.setAccent(flow88::ui::theme::amber);
  truePeakMeter_.setValueText(formatDb(display.truePeakDbTP, " dBTP"));
  truePeakMeter_.setNormalisedValue(normaliseMeter(display.truePeakDbTP, -1.5f, 0.0f));

  gainReductionMeter_.setAccent(flow88::ui::theme::violet);
  gainReductionMeter_.setValueText(formatDb(display.gainReductionDb));
  gainReductionMeter_.setNormalisedValue(normaliseMeter(display.gainReductionDb, 0.0f, 6.0f));

  correlationMeter_.setAccent(flow88::ui::theme::cyan);
  correlationMeter_.setValueText(formatSigned(display.correlation, 2) + " / " +
                                 juce::String(display.stereoWidthPct, 0) + "%");
  correlationMeter_.setNormalisedValue((display.correlation + 1.0f) * 0.5f);

  ioMeter_.setAccent(flow88::ui::theme::blue);
  ioMeter_.setValueText(juce::String(display.inputPeakDb, 1) + " / " +
                        juce::String(display.outputPeakDb, 1) + " dB");
  ioMeter_.setNormalisedValue(normaliseMeter(display.outputPeakDb, -60.0f, 0.0f));

  analyzeButton_.setButtonText(display.analysisInProgress ? "Analyzing..."
                                                          : "Analyze Playback");
  diagnosticsButton_.setButtonText(diagnosticsVisible_ ? "Hide Inspect" : "Inspect");
  diagnosticsCard_.setVisible(diagnosticsVisible_);
  advancedCard_.setVisible(display.advancedPanelOpen);
  const auto diagnosticsReport =
      buildDiagnosticsReport(display, processor_.getAPVTS());
  if (diagnosticsText_.getText() != diagnosticsReport)
    diagnosticsText_.setText(diagnosticsReport, false);
  if (display.advancedPanelOpen || diagnosticsVisible_ ||
      abMatchVisibilityChanged || referenceVisibilityChanged)
    resized();

  if (!exactTargetSync_ && !exactTargetLabel_.isBeingEdited()) {
    exactTargetSync_ = true;
    exactTargetLabel_.setText(formatLufs(display.targetLufs),
                              juce::dontSendNotification);
    exactTargetSync_ = false;
  }

  repaint();
}

void Flow88MasterAudioProcessorEditor::toggleDiagnosticsOverlay() {
  diagnosticsVisible_ = !diagnosticsVisible_;
  diagnosticsCard_.setVisible(diagnosticsVisible_);
  resized();
  repaint();
}

void Flow88MasterAudioProcessorEditor::handleExactTargetCommit() {
  if (exactTargetSync_)
    return;

  auto text = exactTargetLabel_.getText().upToFirstOccurrenceOf(" ", false, false);
  const auto value = text.getFloatValue();
  processor_.setTargetLufsExact(value);
}

void Flow88MasterAudioProcessorEditor::launchReferenceChooser() {
  referenceChooser_ = std::make_unique<juce::FileChooser>(
      "Choose a reference track", juce::File{},
      "*.wav;*.aif;*.aiff;*.flac");
  referenceChooser_->launchAsync(
      juce::FileBrowserComponent::openMode |
          juce::FileBrowserComponent::canSelectFiles,
      [this](const juce::FileChooser &chooser) {
        const auto file = chooser.getResult();
        if (file.existsAsFile())
          processor_.loadReferenceTrack(file);
      });
}

void Flow88MasterAudioProcessorEditor::syncSubgenreButtons(int selectedIndex) {
  for (int i = 0; i < static_cast<int>(subgenreButtons_.size()); ++i)
    subgenreButtons_[static_cast<size_t>(i)]->setToggleState(
        subgenreButtonSourceIndices_[static_cast<size_t>(i)] == selectedIndex,
        juce::dontSendNotification);
}

void Flow88MasterAudioProcessorEditor::configureAdvancedSlider(juce::Slider &slider,
                                                               juce::Colour accent) {
  slider.setSliderStyle(juce::Slider::LinearHorizontal);
  slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 72, 22);
  slider.setColour(juce::Slider::rotarySliderFillColourId, accent);
  slider.setColour(juce::Slider::textBoxTextColourId, flow88::ui::theme::textMain);
  slider.setColour(juce::Slider::textBoxBackgroundColourId,
                   flow88::ui::theme::panel2);
}
