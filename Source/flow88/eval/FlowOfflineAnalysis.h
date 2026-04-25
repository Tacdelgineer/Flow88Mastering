#pragma once

#include "flow88/FlowJuceIncludes.h"
#include "flow88/state/FlowProfiles.h"
#include <vector>

namespace flow88::eval {

struct AnalysisMetrics {
  float integratedLufs = -70.0f;
  float shortTermLufs = -70.0f;
  float truePeakDbTP = -60.0f;
  float stereoCorrelation = 0.0f;
  float stereoWidthPct = 0.0f;
  float tonalBalanceDeviation = 0.0f;
  float lowEndMonoStability = 100.0f;
  float lowEndWidthPct = 0.0f;
  float lowEndCorrelation = 1.0f;
  float crestFactorDb = 0.0f;
  float clippedSamplePct = 0.0f;
  float spectralTilt = 0.0f;
  std::array<float, 7> spectralCurve{};
};

struct EvaluationScores {
  float loudnessSafety = 100.0f;
  float tonalBalanceQuality = 100.0f;
  float widthRisk = 0.0f;
  float lowEndStability = 100.0f;
  float limiterDamageRisk = 0.0f;
};

struct EvaluationReport {
  juce::String sourcePath;
  juce::String sourceName;
  juce::String renderMode = "raw";
  juce::String subgenreId;
  juce::String styleId;
  AnalysisMetrics inputMetrics;
  AnalysisMetrics outputMetrics;
  EvaluationScores scores;
};

class OfflineAnalysisEngine {
public:
  void prepare(double sampleRate, int maxBlockSize);
  void reset();
  void pushBlock(const juce::AudioBuffer<float> &buffer);
  AnalysisMetrics finish(const state::AnalysisTargets *target = nullptr);

private:
  double sampleRate_ = 44100.0;
  int stepSamples_ = 4410;
  int stepCounter_ = 0;
  double stepEnergySum_ = 0.0;
  std::array<double, 7> spectralBandEnergy_{};
  std::array<float, 7> smoothedSpectrumDb_{};
  std::array<float, 30> shortTermHistory_{};
  std::array<float, 4> integratedWindow_{};
  std::vector<float> integratedBlockPowers_;
  size_t shortTermCount_ = 0;
  size_t shortTermWriteIndex_ = 0;
  double shortTermPowerSum_ = 0.0;
  size_t integratedWindowCount_ = 0;
  size_t integratedWindowWriteIndex_ = 0;
  double integratedWindowPowerSum_ = 0.0;
  size_t integratedBlockCount_ = 0;
  size_t integratedBlockWriteIndex_ = 0;
  double totalLeftEnergy_ = 0.0;
  double totalRightEnergy_ = 0.0;
  double totalCrossSum_ = 0.0;
  double totalMidEnergy_ = 0.0;
  double totalSideEnergy_ = 0.0;
  double lowLeftEnergy_ = 0.0;
  double lowRightEnergy_ = 0.0;
  double lowCrossSum_ = 0.0;
  double lowMidEnergy_ = 0.0;
  double lowSideEnergy_ = 0.0;
  double totalMonoEnergy_ = 0.0;
  int64_t totalSamples_ = 0;
  int64_t clippedSampleCount_ = 0;
  float peakSample_ = 0.0f;
  float truePeakSample_ = 0.0f;
  float shortTermLufs_ = -70.0f;
  std::array<juce::dsp::IIR::Filter<float>, 2> kWeightShelf_;
  std::array<juce::dsp::IIR::Filter<float>, 2> kWeightHighPass_;
  std::array<juce::dsp::IIR::Filter<float>, 7> spectralFilters_;
  std::array<juce::dsp::LinkwitzRileyFilter<float>, 2> lowMonoFilters_;
  std::unique_ptr<juce::dsp::Oversampling<float>> truePeakOversampler_;

  void finaliseStepWindow();
  float computeIntegratedLufs() const;
  static float powerToLufs(double power);
};

bool loadAudioFile(const juce::File &file, juce::AudioBuffer<float> &buffer,
                   double &sampleRate, juce::String &errorMessage);

AnalysisMetrics analyzeBuffer(const juce::AudioBuffer<float> &buffer, double sampleRate,
                              int blockSize,
                              const state::AnalysisTargets *target = nullptr);

AnalysisMetrics analyzeFile(const juce::File &file,
                            const state::AnalysisTargets *target, int blockSize,
                            juce::String *errorMessage = nullptr);

EvaluationScores scoreMetrics(const AnalysisMetrics &metrics,
                              const state::AnalysisTargets &target,
                              float limiterCeilingDbTP);

juce::String reportToJson(const std::vector<EvaluationReport> &reports);
juce::String reportToCsv(const std::vector<EvaluationReport> &reports);

} // namespace flow88::eval
