#include "FlowPresetEvaluation.h"
#include "PluginProcessor.h"
#include <cmath>

namespace flow88::eval {

namespace {

int choiceIndexForId(const juce::StringArray &choices, const juce::String &id) {
  const auto wanted = flow88::state::choiceLabelToId(id);
  for (int index = 0; index < choices.size(); ++index) {
    if (flow88::state::choiceLabelToId(choices[index]) == wanted)
      return index;
  }
  return 0;
}

} // namespace

bool evaluateFile(const juce::File &file, const state::ProfileLibrary &profileLibrary,
                  const PresetEvaluationOptions &options,
                  EvaluationReport &report, juce::String &errorMessage) {
  juce::AudioBuffer<float> inputBuffer;
  double sampleRate = 0.0;
  if (!loadAudioFile(file, inputBuffer, sampleRate, errorMessage))
    return false;

  const auto profile = profileLibrary.compose(options.subgenreId, options.styleId);
  report.sourcePath = file.getFullPathName();
  report.sourceName = file.getFileName();
  report.renderMode = options.applyPreset ? "preset" : "raw";
  report.subgenreId = profile.subgenreId;
  report.styleId = profile.styleId;
  report.inputMetrics =
      analyzeBuffer(inputBuffer, sampleRate, options.blockSize, &profile.analysisTargets);

  juce::AudioBuffer<float> outputBuffer(inputBuffer.getNumChannels(),
                                        inputBuffer.getNumSamples());
  outputBuffer.makeCopyOf(inputBuffer);
  if (options.applyPreset) {
    Flow88MasterAudioProcessor processor;
    processor.prepareToPlay(sampleRate, options.blockSize);
    processor.setSubgenreIndex(choiceIndexForId(flow88::state::kSubgenreChoices,
                                               profile.subgenreName));
    processor.setStyleIndex(choiceIndexForId(flow88::state::kStyleChoices,
                                            profile.styleName));
    if (std::isfinite(options.exactTargetLufs))
      processor.setTargetLufsExact(options.exactTargetLufs);

    juce::MidiBuffer midi;
    int sampleOffset = 0;
    while (sampleOffset < inputBuffer.getNumSamples()) {
      const auto numThisBlock =
          juce::jmin(options.blockSize, inputBuffer.getNumSamples() - sampleOffset);
      juce::AudioBuffer<float> block(inputBuffer.getNumChannels(), numThisBlock);
      for (int channel = 0; channel < inputBuffer.getNumChannels(); ++channel)
        block.copyFrom(channel, 0, inputBuffer, channel, sampleOffset, numThisBlock);

      processor.processBlock(block, midi);
      for (int channel = 0; channel < outputBuffer.getNumChannels(); ++channel)
        outputBuffer.copyFrom(channel, sampleOffset, block, channel, 0, numThisBlock);
      sampleOffset += numThisBlock;
    }

    processor.releaseResources();
  }

  report.outputMetrics =
      analyzeBuffer(outputBuffer, sampleRate, options.blockSize, &profile.analysisTargets);
  report.scores = scoreMetrics(report.outputMetrics, profile.analysisTargets,
                               profile.advanced.limiterCeilingDbTP);
  errorMessage.clear();
  return true;
}

} // namespace flow88::eval
