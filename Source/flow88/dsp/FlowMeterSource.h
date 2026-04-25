#pragma once

#include "flow88/FlowJuceIncludes.h"

namespace flow88::dsp {

class FlowMeterSource {
public:
  static constexpr size_t kNumSpectrumBands = 7;

  struct Snapshot {
    float inputPeakDb = -60.0f;
    float outputPeakDb = -60.0f;
    float integratedLufs = -70.0f;
    float shortTermLufs = -70.0f;
    float truePeakDbTP = -60.0f;
    float gainReductionDb = 0.0f;
    float correlation = 0.0f;
    float stereoWidthPct = 0.0f;
    std::array<float, kNumSpectrumBands> spectralCurve{};
  };

  void prepare(double sampleRate, int maxBlockSize);
  void reset();
  void pushInputBuffer(const juce::AudioBuffer<float> &buffer);
  void pushOutputBuffer(const juce::AudioBuffer<float> &buffer);
  void setGainReductionDb(float gainReductionDb);
  Snapshot snapshot() const;

private:
  std::atomic<float> inputPeakDb_{-60.0f};
  std::atomic<float> outputPeakDb_{-60.0f};
  std::atomic<float> integratedLufs_{-70.0f};
  std::atomic<float> shortTermLufs_{-70.0f};
  std::atomic<float> truePeakDbTP_{-60.0f};
  std::atomic<float> gainReductionDb_{0.0f};
  std::atomic<float> correlation_{0.0f};
  std::atomic<float> stereoWidthPct_{0.0f};
  std::array<std::atomic<float>, kNumSpectrumBands> spectralCurve_{};

  double sampleRate_ = 44100.0;
  int stepSamples_ = 4410;
  int stepCounter_ = 0;
  double stepEnergySum_ = 0.0;
  std::array<double, kNumSpectrumBands> spectralBandEnergy_{};
  std::array<float, kNumSpectrumBands> smoothedSpectrumDb_{};
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
  double stepLeftEnergySum_ = 0.0;
  double stepRightEnergySum_ = 0.0;
  double stepCrossSum_ = 0.0;
  double stepMidEnergySum_ = 0.0;
  double stepSideEnergySum_ = 0.0;
  std::array<float, 2> previousOutputSamples_{};
  std::array<juce::dsp::IIR::Filter<float>, 2> kWeightShelf_;
  std::array<juce::dsp::IIR::Filter<float>, 2> kWeightHighPass_;
  std::array<juce::dsp::IIR::Filter<float>, kNumSpectrumBands> spectralFilters_;
  std::unique_ptr<juce::dsp::Oversampling<float>> truePeakOversampler_;

  static float computePeakDb(const juce::AudioBuffer<float> &buffer);
  static float applyDecay(float currentValue, float incomingValue);
  static float powerToLufs(double power);
  float computeIntegratedLufs() const;
  void finaliseStepWindow();
};

} // namespace flow88::dsp
