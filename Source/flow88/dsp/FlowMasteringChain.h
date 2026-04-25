#pragma once

#include "flow88/FlowJuceIncludes.h"
#include <array>
#include <memory>

namespace flow88::dsp {

struct FlowMasteringSettings {
  float toneTiltDb = 0.0f;
  float punchPct = 0.0f;
  float stereoWidthPct = 100.0f;
  float lowEndFocusPct = 0.0f;
  float targetLufs = -9.0f;
  float attitudePct = 0.0f;
  float outputTrimDb = 0.0f;
  float limiterCeilingDbTP = -1.0f;
  float clipperAmountPct = 0.0f;
  float resonanceControlPct = 0.0f;
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
  int oversamplingIndex = 1;
};

struct FlowMasteringMetrics {
  float compressorGainReductionDb = 0.0f;
  float limiterGainReductionDb = 0.0f;
  float totalGainReductionDb = 0.0f;
  float targetDriveDb = 0.0f;
  bool toneStageActive = false;
  bool lowEndStageActive = false;
  bool dynamicsStageActive = false;
  bool widthStageActive = false;
  bool saturationStageActive = false;
  bool clipperStageActive = false;
  bool limiterStageActive = false;
};

class FlowMasteringChain {
public:
  void prepare(double sampleRate, int maxBlockSize, int numChannels);
  void reset();
  FlowMasteringMetrics process(juce::AudioBuffer<float> &buffer,
                               const FlowMasteringSettings &settings);

private:
  double sampleRate_ = 44100.0;
  int numChannels_ = 2;
  int lastOversamplingIndex_ = 1;
  float compressorEnvelope_ = 0.0f;
  float limiterGain_ = 1.0f;
  std::array<float, 2> harshnessEnvelope_{};

  std::array<juce::dsp::FirstOrderTPTFilter<float>, 2> toneLowFilters_;
  std::array<juce::dsp::FirstOrderTPTFilter<float>, 2> toneHighFilters_;
  std::array<juce::dsp::StateVariableTPTFilter<float>, 2> resonanceFilters_;
  juce::dsp::LinkwitzRileyFilter<float> sideSplitFilter_;
  juce::dsp::FirstOrderTPTFilter<float> detectorHighpass_;
  std::array<std::unique_ptr<juce::dsp::Oversampling<float>>, 2> oversamplers_;

  void updateFilterSettings(const FlowMasteringSettings &settings);
  void processToneAndSpatial(juce::AudioBuffer<float> &buffer,
                             const FlowMasteringSettings &settings,
                             FlowMasteringMetrics &metrics);
  void processGlue(juce::AudioBuffer<float> &buffer,
                   const FlowMasteringSettings &settings,
                   FlowMasteringMetrics &metrics);
  void processNonlinearBlock(juce::dsp::AudioBlock<float> block,
                             const FlowMasteringSettings &settings,
                             int oversamplingFactor,
                             FlowMasteringMetrics &metrics);

  static float dbToGain(float valueDb);
  static float gainToDb(float gain);
};

} // namespace flow88::dsp
