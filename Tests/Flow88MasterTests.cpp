#include "PluginProcessor.h"
#include "flow88/eval/FlowOfflineAnalysis.h"
#include "flow88/eval/FlowPresetEvaluation.h"
#include "flow88/state/FlowParameterIDs.h"
#include "flow88/state/FlowParameterLayout.h"
#include "flow88/state/FlowProfiles.h"
#include <cmath>
#include <iostream>
#include <set>
#include <stdexcept>

namespace {

void require(bool condition, const juce::String &message) {
  if (!condition)
    throw std::runtime_error(message.toStdString());
}

void feedAnalysisSignal(Flow88MasterAudioProcessor &processor, double sampleRate,
                        double seconds) {
  constexpr int blockSize = 512;
  juce::AudioBuffer<float> buffer(2, blockSize);
  juce::MidiBuffer midi;

  auto phaseA = 0.0;
  auto phaseB = 0.3;
  auto phaseC = 0.0;
  const auto deltaA = juce::MathConstants<double>::twoPi * 56.0 / sampleRate;
  const auto deltaB = juce::MathConstants<double>::twoPi * 1730.0 / sampleRate;
  const auto deltaC = juce::MathConstants<double>::twoPi * 3100.0 / sampleRate;

  auto samplesRemaining = static_cast<int>(sampleRate * seconds);
  while (samplesRemaining > 0) {
    const auto numSamples = juce::jmin(blockSize, samplesRemaining);
    buffer.clear();

    auto *left = buffer.getWritePointer(0);
    auto *right = buffer.getWritePointer(1);
    for (int sample = 0; sample < numSamples; ++sample) {
      const auto low = std::sin(phaseA);
      const auto highLeft = std::sin(phaseB);
      const auto highRight = std::sin(phaseC + 0.55);

      left[sample] =
          static_cast<float>(0.28 * low + 0.08 * highLeft + 0.015 * std::sin(phaseC));
      right[sample] = static_cast<float>(0.25 * std::sin(phaseA + 0.18) +
                                         0.05 * highLeft + 0.07 * highRight);

      phaseA += deltaA;
      phaseB += deltaB;
      phaseC += deltaC;
    }

    processor.processBlock(buffer, midi);
    samplesRemaining -= numSamples;
  }
}

juce::AudioBuffer<float> makeAnalysisSignalBuffer(double sampleRate, double seconds) {
  constexpr int blockSize = 512;
  juce::AudioBuffer<float> rendered(2, static_cast<int>(sampleRate * seconds));
  juce::AudioBuffer<float> buffer(2, blockSize);

  auto phaseA = 0.0;
  auto phaseB = 0.3;
  auto phaseC = 0.0;
  const auto deltaA = juce::MathConstants<double>::twoPi * 56.0 / sampleRate;
  const auto deltaB = juce::MathConstants<double>::twoPi * 1730.0 / sampleRate;
  const auto deltaC = juce::MathConstants<double>::twoPi * 3100.0 / sampleRate;

  int writeOffset = 0;
  while (writeOffset < rendered.getNumSamples()) {
    const auto numSamples = juce::jmin(blockSize, rendered.getNumSamples() - writeOffset);
    buffer.clear();

    auto *left = buffer.getWritePointer(0);
    auto *right = buffer.getWritePointer(1);
    for (int sample = 0; sample < numSamples; ++sample) {
      const auto low = std::sin(phaseA);
      const auto highLeft = std::sin(phaseB);
      const auto highRight = std::sin(phaseC + 0.55);

      left[sample] =
          static_cast<float>(0.28 * low + 0.08 * highLeft + 0.015 * std::sin(phaseC));
      right[sample] = static_cast<float>(0.25 * std::sin(phaseA + 0.18) +
                                         0.05 * highLeft + 0.07 * highRight);

      phaseA += deltaA;
      phaseB += deltaB;
      phaseC += deltaC;
    }

    rendered.copyFrom(0, writeOffset, buffer, 0, 0, numSamples);
    rendered.copyFrom(1, writeOffset, buffer, 1, 0, numSamples);
    writeOffset += numSamples;
  }

  return rendered;
}

void writeWaveFile(const juce::File &file, const juce::AudioBuffer<float> &buffer,
                   double sampleRate) {
  juce::WavAudioFormat format;
  std::unique_ptr<juce::FileOutputStream> stream(file.createOutputStream());
  require(stream != nullptr, "Failed to create temporary wave file.");

  std::unique_ptr<juce::AudioFormatWriter> writer(
      format.createWriterFor(stream.get(), sampleRate,
                             static_cast<unsigned int>(buffer.getNumChannels()), 24, {},
                             0));
  require(writer != nullptr, "Failed to create a wave writer.");
  stream.release();

  const auto *channels = buffer.getArrayOfReadPointers();
  require(writer->writeFromFloatArrays(channels, buffer.getNumChannels(),
                                       buffer.getNumSamples()),
          "Failed to write the temporary wave file.");
}

void testParameterIds() {
  std::set<std::string> ids;
  for (const auto *id : flow88::state::kAllParameterIds) {
    require(id != nullptr && juce::String(id).isNotEmpty(),
            "Encountered an empty parameter ID.");
    const auto inserted = ids.insert(std::string(id)).second;
    require(inserted, "Duplicate parameter ID: " + juce::String(id));
  }
}

void testProfiles() {
  flow88::state::ProfileLibrary library;
  require(library.getSubgenres().size() == 5, "Expected 5 built-in subgenres.");
  require(library.getStyles().size() == 3, "Expected 3 built-in styles.");

  const auto profile = library.compose("house", "clean");
  const auto clubProfile = library.compose("techno", "club");
  const auto progressiveWarm = library.compose("progressive", "warm");
  require(profile.subgenreName == "House", "House profile failed to load.");
  require(profile.styleName == "Clean", "Clean style failed to load.");
  require(std::abs(profile.targetLufs - (-9.0f)) < 0.001f,
          "Unexpected default LUFS target.");
  require(std::abs(profile.macro.stereoWidthPct - 118.0f) < 0.001f,
          "Style delta failed to apply to stereo width.");
  require(profile.assistantSummary.isNotEmpty(),
          "Composed profile assistant summary should not be empty.");
  require(profile.dspTuning.widthScale < 1.0f,
          "Clean House profile should narrow macro width response slightly.");
  require(clubProfile.advanced.oversampling == "4x",
          "Club profiles should use 4x oversampling for safer finishing.");
  require(clubProfile.advanced.limiterCeilingDbTP <= -1.1f,
          "Club profiles should target a safer ceiling than the legacy default.");
  require(progressiveWarm.dspTuning.targetDriveBiasDb <
              clubProfile.dspTuning.targetDriveBiasDb,
          "Progressive Warm should remain the safer flagship than Club presets.");
}

void testHeroDefaults() {
  Flow88MasterAudioProcessor processor;
  const auto display = processor.buildDisplayState(0.0);

  require(display.currentProfileTitle == "Progressive / Warm",
          "Startup profile should default to Progressive / Warm.");
  require(display.currentProfileGuide.containsIgnoreCase("Hero default"),
          "Startup profile guide should explain the hero preset.");
  require(display.assistantSummary.containsIgnoreCase("Progressive / Warm"),
          "Startup assistant summary should point users at the safe flagship.");
}

void testStateRoundTripAndAB() {
  Flow88MasterAudioProcessor processor;
  processor.setSubgenreIndex(1);
  processor.setStyleIndex(1);
  processor.startAnalysisSimulation();
  processor.finishAnalysisSimulation();

  processor.switchABSlot(1);
  processor.setSubgenreIndex(4);
  processor.setStyleIndex(2);
  processor.switchABSlot(0);

  juce::MemoryBlock stateData;
  processor.getStateInformation(stateData);

  Flow88MasterAudioProcessor restored;
  restored.setStateInformation(stateData.getData(),
                               static_cast<int>(stateData.getSize()));

  require(restored.getSessionState().getActiveABSlot() == 0,
          "Active A/B slot did not restore.");

  auto displayA = restored.buildDisplayState(0.0);
  require(displayA.currentProfileTitle == "Techno / Club",
          "A slot profile did not restore.");

  restored.switchABSlot(1);
  auto displayB = restored.buildDisplayState(0.0);
  require(displayB.currentProfileTitle == "Progressive / Warm",
          "B slot profile did not restore.");
}

void testABLevelMatching() {
  Flow88MasterAudioProcessor processor;
  processor.prepareToPlay(48000.0, 512);

  feedAnalysisSignal(processor, 48000.0, 6.0);
  processor.switchABSlot(1);

  auto &apvts = processor.getAPVTS();
  flow88::state::setFloatParameter(apvts, flow88::state::paramIds::outputTrimDb,
                                   -3.0f);
  feedAnalysisSignal(processor, 48000.0, 6.0);

  processor.switchABSlot(0);
  const auto display = processor.buildDisplayState(0.0);

  require(display.currentProfileTitle == "Progressive / Warm",
          "Switching back to slot A should restore the hero profile.");
  require(display.abLevelMatchReady,
          "A/B level matching should become ready after both slots see signal.");
  require(std::abs(display.abLevelMatchGainDb) > 0.5f,
          "A/B level matching should apply a measurable correction when slots differ.");
  processor.releaseResources();
}

void testAnalyzeSummary() {
  Flow88MasterAudioProcessor processor;
  processor.setSubgenreIndex(0);
  processor.setStyleIndex(0);
  processor.startAnalysisSimulation();
  processor.finishAnalysisSimulation();

  const auto display = processor.buildDisplayState(0.5);
  require(display.analysisReady, "Analyze state did not complete.");
  require(display.suggestedProfileTitle == "House / Clean",
          "Analyze suggestion did not reflect the selected profile.");
  require(display.assistantSummary.isNotEmpty(),
          "Analyze summary should not be empty.");
}

void testRealAnalyzerMeters() {
  Flow88MasterAudioProcessor processor;
  processor.prepareToPlay(48000.0, 512);
  feedAnalysisSignal(processor, 48000.0, 6.0);
  processor.startAnalysisSimulation();
  processor.finishAnalysisSimulation();

  const auto display = processor.buildDisplayState(0.0);
  require(display.inputPeakDb > -24.0f, "Input peak meter did not react to audio.");
  require(display.outputPeakDb > -24.0f, "Output peak meter did not react to audio.");
  require(display.shortTermLufs > -40.0f,
          "Short-term loudness did not move into a usable range.");
  require(display.integratedLufs > -45.0f,
          "Integrated loudness did not accumulate from program material.");
  require(display.truePeakDbTP > -24.0f,
          "True peak estimate did not react to program material.");
  require(display.stereoWidthPct > 1.0f,
          "Stereo width analysis did not produce a measurable value.");

  auto spectrumMoved = false;
  for (const auto value : display.actualCurve)
    spectrumMoved = spectrumMoved || std::abs(value) > 0.02f;

  require(spectrumMoved, "Spectrum analysis curve remained at its placeholder baseline.");
  require(display.assistantSummary.containsIgnoreCase("LUFS"),
          "Measured analysis summary did not include loudness data.");
  processor.releaseResources();
}

void testMasteringChainMetrics() {
  Flow88MasterAudioProcessor processor;
  processor.prepareToPlay(48000.0, 512);

  auto &apvts = processor.getAPVTS();
  flow88::state::setFloatParameter(apvts, flow88::state::paramIds::inputTrimDb, 7.5f);
  flow88::state::setFloatParameter(apvts, flow88::state::paramIds::toneTiltDb, 2.2f);
  flow88::state::setFloatParameter(apvts, flow88::state::paramIds::punchPct, 88.0f);
  flow88::state::setFloatParameter(apvts, flow88::state::paramIds::stereoWidthPct,
                                   145.0f);
  flow88::state::setFloatParameter(apvts, flow88::state::paramIds::lowEndFocusPct,
                                   96.0f);
  flow88::state::setFloatParameter(apvts, flow88::state::paramIds::targetLufs, -7.4f);
  flow88::state::setFloatParameter(apvts, flow88::state::paramIds::attitudePct, 76.0f);
  flow88::state::setFloatParameter(apvts, flow88::state::paramIds::outputTrimDb, 1.0f);
  flow88::state::setFloatParameter(apvts, flow88::state::paramIds::clipperAmountPct,
                                   46.0f);

  feedAnalysisSignal(processor, 48000.0, 6.0);
  const auto display = processor.buildDisplayState(0.0);

  require(display.toneStageActive, "Tone stage should report as active.");
  require(display.lowEndStageActive, "Low-end stage should report as active.");
  require(display.dynamicsStageActive, "Dynamics stage should report as active.");
  require(display.widthStageActive, "Width stage should report as active.");
  require(display.saturationStageActive,
          "Saturation stage should report as active.");
  require(display.clipperStageActive, "Clipper stage should report as active.");
  require(display.limiterStageActive, "Limiter stage should report as active.");
  require(display.gainReductionDb > 0.1f,
          "Total gain reduction meter should react once the chain is driven.");
  require(display.compressorGainReductionDb > 0.05f,
          "Compressor gain reduction should react once punch is increased.");
  require(display.targetDriveDb > 0.1f,
          "Target LUFS should contribute real drive into the finishing chain.");
  require(display.truePeakDbTP < 0.2f,
          "Finishing chain should keep true peak under control when driven.");
  require(display.outputPeakDb > display.inputPeakDb - 6.0f,
          "Chain output should remain in a sensible mastered range.");
  processor.releaseResources();
}

void testOfflineAnalysisAndReporting() {
  const auto sampleRate = 48000.0;
  const auto signal = makeAnalysisSignalBuffer(sampleRate, 6.0);
  flow88::state::ProfileLibrary library;
  const auto profile = library.compose("house", "clean");
  const auto metrics =
      flow88::eval::analyzeBuffer(signal, sampleRate, 1024, &profile.analysisTargets);

  require(metrics.integratedLufs > -45.0f,
          "Offline analysis should produce integrated loudness.");
  require(metrics.truePeakDbTP > -24.0f,
          "Offline analysis should produce true peak data.");
  require(metrics.lowEndMonoStability > 0.0f,
          "Offline analysis should produce a low-end stability proxy.");

  flow88::eval::EvaluationReport report;
  report.sourcePath = "generated";
  report.sourceName = "generated.wav";
  report.subgenreId = profile.subgenreId;
  report.styleId = profile.styleId;
  report.inputMetrics = metrics;
  report.outputMetrics = metrics;
  report.scores = flow88::eval::scoreMetrics(metrics, profile.analysisTargets,
                                             profile.advanced.limiterCeilingDbTP);

  const auto json = flow88::eval::reportToJson({report});
  require(json.contains("\"aggregate\""),
          "Offline analysis JSON report should contain aggregate scores.");
  const auto csv = flow88::eval::reportToCsv({report});
  require(csv.contains("integrated_lufs"),
          "Offline analysis CSV report should include the metrics header.");
}

void testPresetEvaluationFramework() {
  const auto sampleRate = 48000.0;
  const auto signal = makeAnalysisSignalBuffer(sampleRate, 6.0);
  const auto tempFile = juce::File::getSpecialLocation(juce::File::tempDirectory)
                            .getNonexistentChildFile("flow88_eval", ".wav");
  writeWaveFile(tempFile, signal, sampleRate);

  flow88::state::ProfileLibrary library;
  flow88::eval::PresetEvaluationOptions options;
  options.subgenreId = "techno";
  options.styleId = "club";
  options.applyPreset = true;
  options.blockSize = 1024;

  flow88::eval::EvaluationReport report;
  juce::String errorMessage;
  const auto success =
      flow88::eval::evaluateFile(tempFile, library, options, report, errorMessage);
  tempFile.deleteFile();

  require(success, "Preset evaluation should complete for a rendered file.");
  require(errorMessage.isEmpty(),
          "Preset evaluation should not produce an error message.");
  require(report.renderMode == "preset",
          "Preset evaluation should mark the report as processed.");
  require(std::abs(report.outputMetrics.integratedLufs -
                   report.inputMetrics.integratedLufs) > 0.15f ||
              std::abs(report.outputMetrics.stereoWidthPct -
                       report.inputMetrics.stereoWidthPct) > 1.0f,
          "Preset evaluation should produce a measurable signal change.");
}

} // namespace

int main() {
  try {
    testParameterIds();
    testProfiles();
    testHeroDefaults();
    testStateRoundTripAndAB();
    testABLevelMatching();
    testAnalyzeSummary();
    testRealAnalyzerMeters();
    testMasteringChainMetrics();
    testOfflineAnalysisAndReporting();
    testPresetEvaluationFramework();
    std::cout << "Flow88MasterTests passed\n";
    return 0;
  } catch (const std::exception &exception) {
    std::cerr << "Flow88MasterTests failed: " << exception.what() << '\n';
    return 1;
  }
}
