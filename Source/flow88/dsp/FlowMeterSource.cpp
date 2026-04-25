#include "FlowMeterSource.h"
#include <cmath>

namespace flow88::dsp {

namespace {

constexpr double kAnalysisStepSeconds = 0.1;
constexpr size_t kIntegratedHistoryBlocks = 20 * 60 * 10;
constexpr std::array<double, FlowMeterSource::kNumSpectrumBands> kSpectrumCentres{
    35.0, 90.0, 220.0, 560.0, 1400.0, 4200.0, 11000.0};

double lufsToPower(double lufs) {
  return std::pow(10.0, (lufs + 0.691) * 0.1);
}

} // namespace

void FlowMeterSource::prepare(double sampleRate, int maxBlockSize) {
  sampleRate_ = juce::jmax(1.0, sampleRate);
  stepSamples_ = juce::jmax(1, static_cast<int>(std::round(sampleRate_ * kAnalysisStepSeconds)));

  truePeakOversampler_ = std::make_unique<juce::dsp::Oversampling<float>>(
      2, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true,
      false);
  truePeakOversampler_->initProcessing(static_cast<size_t>(juce::jmax(1, maxBlockSize)));

  const auto shelf = juce::dsp::IIR::Coefficients<float>::makeHighShelf(
      sampleRate_, 1500.0f, 0.7071f, juce::Decibels::decibelsToGain(4.0f));
  const auto highPass = juce::dsp::IIR::Coefficients<float>::makeHighPass(
      sampleRate_, 38.0f, 0.5003f);

  for (auto &filter : kWeightShelf_) {
    filter.coefficients = shelf;
    filter.reset();
  }

  for (auto &filter : kWeightHighPass_) {
    filter.coefficients = highPass;
    filter.reset();
  }

  for (size_t i = 0; i < spectralFilters_.size(); ++i) {
    const auto frequency = static_cast<float>(
        juce::jlimit(20.0, sampleRate_ * 0.45, kSpectrumCentres[i]));
    spectralFilters_[i].coefficients =
        juce::dsp::IIR::Coefficients<float>::makeBandPass(sampleRate_, frequency, 0.85f);
    spectralFilters_[i].reset();
  }

  integratedBlockPowers_.assign(kIntegratedHistoryBlocks, 0.0f);
  reset();
}

void FlowMeterSource::reset() {
  inputPeakDb_.store(-60.0f);
  outputPeakDb_.store(-60.0f);
  integratedLufs_.store(-70.0f);
  shortTermLufs_.store(-70.0f);
  truePeakDbTP_.store(-60.0f);
  gainReductionDb_.store(0.0f);
  correlation_.store(0.0f);
  stereoWidthPct_.store(0.0f);

  for (auto &value : spectralCurve_)
    value.store(0.0f);

  stepCounter_ = 0;
  stepEnergySum_ = 0.0;
  stepLeftEnergySum_ = 0.0;
  stepRightEnergySum_ = 0.0;
  stepCrossSum_ = 0.0;
  stepMidEnergySum_ = 0.0;
  stepSideEnergySum_ = 0.0;
  spectralBandEnergy_.fill(0.0);
  smoothedSpectrumDb_.fill(-54.0f);
  shortTermHistory_.fill(0.0f);
  integratedWindow_.fill(0.0f);
  shortTermCount_ = 0;
  shortTermWriteIndex_ = 0;
  shortTermPowerSum_ = 0.0;
  integratedWindowCount_ = 0;
  integratedWindowWriteIndex_ = 0;
  integratedWindowPowerSum_ = 0.0;
  integratedBlockCount_ = 0;
  integratedBlockWriteIndex_ = 0;
  previousOutputSamples_.fill(0.0f);

  for (auto &filter : kWeightShelf_)
    filter.reset();

  for (auto &filter : kWeightHighPass_)
    filter.reset();

  for (auto &filter : spectralFilters_)
    filter.reset();

  if (truePeakOversampler_ != nullptr)
    truePeakOversampler_->reset();
}

void FlowMeterSource::pushInputBuffer(const juce::AudioBuffer<float> &buffer) {
  inputPeakDb_.store(applyDecay(inputPeakDb_.load(), computePeakDb(buffer)));
}

void FlowMeterSource::pushOutputBuffer(const juce::AudioBuffer<float> &buffer) {
  outputPeakDb_.store(applyDecay(outputPeakDb_.load(), computePeakDb(buffer)));

  if (buffer.getNumSamples() == 0 || buffer.getNumChannels() == 0)
    return;

  float truePeak = 0.0f;
  if (truePeakOversampler_ != nullptr && buffer.getNumChannels() >= 2) {
    const juce::dsp::AudioBlock<const float> inputBlock(buffer);
    const auto oversampledBlock = truePeakOversampler_->processSamplesUp(inputBlock);
    for (size_t channel = 0; channel < oversampledBlock.getNumChannels(); ++channel) {
      for (size_t sample = 0; sample < oversampledBlock.getNumSamples(); ++sample)
        truePeak = juce::jmax(truePeak, std::abs(oversampledBlock.getSample(channel, sample)));
    }
  } else {
    for (int channel = 0; channel < juce::jmin(2, buffer.getNumChannels()); ++channel)
      for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        truePeak = juce::jmax(truePeak, std::abs(buffer.getSample(channel, sample)));
  }

  truePeakDbTP_.store(applyDecay(
      truePeakDbTP_.load(),
      juce::jlimit(-60.0f, 6.0f, juce::Decibels::gainToDecibels(truePeak, -60.0f))));

  const auto *left = buffer.getReadPointer(0);
  const auto *right = buffer.getReadPointer(juce::jmin(1, buffer.getNumChannels() - 1));

  for (int sample = 0; sample < buffer.getNumSamples(); ++sample) {
    const auto leftSample = left[sample];
    const auto rightSample = right[sample];
    const auto monoSample = 0.5f * (leftSample + rightSample);

    stepLeftEnergySum_ += static_cast<double>(leftSample) * leftSample;
    stepRightEnergySum_ += static_cast<double>(rightSample) * rightSample;
    stepCrossSum_ += static_cast<double>(leftSample) * rightSample;

    const auto midSample = 0.5f * (leftSample + rightSample);
    const auto sideSample = 0.5f * (leftSample - rightSample);
    stepMidEnergySum_ += static_cast<double>(midSample) * midSample;
    stepSideEnergySum_ += static_cast<double>(sideSample) * sideSample;

    double weightedPower = 0.0;
    const auto weightedLeft =
        kWeightHighPass_[0].processSample(kWeightShelf_[0].processSample(leftSample));
    const auto weightedRight =
        kWeightHighPass_[1].processSample(kWeightShelf_[1].processSample(rightSample));
    weightedPower += static_cast<double>(weightedLeft) * weightedLeft;
    weightedPower += static_cast<double>(weightedRight) * weightedRight;
    stepEnergySum_ += weightedPower;

    for (size_t band = 0; band < spectralFilters_.size(); ++band) {
      const auto filtered = spectralFilters_[band].processSample(monoSample);
      spectralBandEnergy_[band] += static_cast<double>(filtered) * filtered;
    }

    ++stepCounter_;
    if (stepCounter_ >= stepSamples_)
      finaliseStepWindow();
  }
}

void FlowMeterSource::setGainReductionDb(float gainReductionDb) {
  gainReductionDb_.store(juce::jmax(0.0f, gainReductionDb));
}

FlowMeterSource::Snapshot FlowMeterSource::snapshot() const {
  Snapshot result;
  result.inputPeakDb = inputPeakDb_.load();
  result.outputPeakDb = outputPeakDb_.load();
  result.integratedLufs = integratedLufs_.load();
  result.shortTermLufs = shortTermLufs_.load();
  result.truePeakDbTP = truePeakDbTP_.load();
  result.gainReductionDb = gainReductionDb_.load();
  result.correlation = correlation_.load();
  result.stereoWidthPct = stereoWidthPct_.load();

  for (size_t i = 0; i < result.spectralCurve.size(); ++i)
    result.spectralCurve[i] = spectralCurve_[i].load();

  return result;
}

float FlowMeterSource::computePeakDb(const juce::AudioBuffer<float> &buffer) {
  float peak = 0.0f;
  for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    peak = juce::jmax(peak, buffer.getMagnitude(channel, 0, buffer.getNumSamples()));

  return juce::jlimit(-60.0f, 6.0f, juce::Decibels::gainToDecibels(peak, -60.0f));
}

float FlowMeterSource::applyDecay(float currentValue, float incomingValue) {
  if (incomingValue >= currentValue)
    return incomingValue;

  return juce::jmax(-60.0f, currentValue - 0.45f);
}

float FlowMeterSource::powerToLufs(double power) {
  if (power <= 0.0)
    return -70.0f;

  return juce::jlimit(-70.0f, 6.0f,
                      static_cast<float>(-0.691 + 10.0 * std::log10(power)));
}

float FlowMeterSource::computeIntegratedLufs() const {
  if (integratedBlockCount_ == 0 || integratedBlockPowers_.empty())
    return -70.0f;

  const auto absoluteGatePower = lufsToPower(-70.0);

  double absoluteSum = 0.0;
  size_t absoluteCount = 0;
  for (size_t i = 0; i < integratedBlockCount_; ++i) {
    const auto power = static_cast<double>(integratedBlockPowers_[i]);
    if (power > absoluteGatePower) {
      absoluteSum += power;
      ++absoluteCount;
    }
  }

  if (absoluteCount == 0)
    return -70.0f;

  const auto ungatedLufs = powerToLufs(absoluteSum / static_cast<double>(absoluteCount));
  const auto relativeGatePower = lufsToPower(ungatedLufs - 10.0);

  double gatedSum = 0.0;
  size_t gatedCount = 0;
  for (size_t i = 0; i < integratedBlockCount_; ++i) {
    const auto power = static_cast<double>(integratedBlockPowers_[i]);
    if (power > absoluteGatePower && power > relativeGatePower) {
      gatedSum += power;
      ++gatedCount;
    }
  }

  if (gatedCount == 0)
    return ungatedLufs;

  return powerToLufs(gatedSum / static_cast<double>(gatedCount));
}

void FlowMeterSource::finaliseStepWindow() {
  if (stepCounter_ <= 0)
    return;

  const auto stepPower = static_cast<float>(stepEnergySum_ / static_cast<double>(stepCounter_));
  const auto shortTermSize = shortTermHistory_.size();
  if (shortTermCount_ < shortTermSize) {
    shortTermHistory_[shortTermWriteIndex_] = stepPower;
    shortTermPowerSum_ += stepPower;
    ++shortTermCount_;
  } else {
    shortTermPowerSum_ -= shortTermHistory_[shortTermWriteIndex_];
    shortTermHistory_[shortTermWriteIndex_] = stepPower;
    shortTermPowerSum_ += stepPower;
  }
  shortTermWriteIndex_ = (shortTermWriteIndex_ + 1) % shortTermSize;

  const auto shortTermAveragePower =
      shortTermPowerSum_ / static_cast<double>(juce::jmax<size_t>(1, shortTermCount_));
  shortTermLufs_.store(powerToLufs(shortTermAveragePower));

  const auto integratedWindowSize = integratedWindow_.size();
  if (integratedWindowCount_ < integratedWindowSize) {
    integratedWindow_[integratedWindowWriteIndex_] = stepPower;
    integratedWindowPowerSum_ += stepPower;
    ++integratedWindowCount_;
  } else {
    integratedWindowPowerSum_ -= integratedWindow_[integratedWindowWriteIndex_];
    integratedWindow_[integratedWindowWriteIndex_] = stepPower;
    integratedWindowPowerSum_ += stepPower;
  }
  integratedWindowWriteIndex_ =
      (integratedWindowWriteIndex_ + 1) % integratedWindowSize;

  if (integratedWindowCount_ == integratedWindowSize && !integratedBlockPowers_.empty()) {
    const auto blockPower = static_cast<float>(
        integratedWindowPowerSum_ / static_cast<double>(integratedWindowSize));
    integratedBlockPowers_[integratedBlockWriteIndex_] = blockPower;
    integratedBlockWriteIndex_ =
        (integratedBlockWriteIndex_ + 1) % integratedBlockPowers_.size();
    integratedBlockCount_ =
        juce::jmin(integratedBlockPowers_.size(), integratedBlockCount_ + 1);
    integratedLufs_.store(computeIntegratedLufs());
  }

  const auto denominator =
      std::sqrt(juce::jmax(1.0e-12, stepLeftEnergySum_ * stepRightEnergySum_));
  const auto measuredCorrelation = denominator > 0.0
                                       ? static_cast<float>(stepCrossSum_ / denominator)
                                       : 0.0f;
  correlation_.store(juce::jlimit(
      -1.0f, 1.0f,
      0.82f * correlation_.load() + 0.18f * measuredCorrelation));

  const auto widthRatio =
      std::sqrt(stepSideEnergySum_ / juce::jmax(1.0e-12, stepMidEnergySum_));
  const auto measuredWidth = juce::jlimit(0.0f, 200.0f,
                                          static_cast<float>(widthRatio * 100.0));
  stereoWidthPct_.store(0.82f * stereoWidthPct_.load() + 0.18f * measuredWidth);

  float averageBandDb = 0.0f;
  for (size_t band = 0; band < spectralBandEnergy_.size(); ++band) {
    const auto bandPower =
        spectralBandEnergy_[band] / static_cast<double>(juce::jmax(stepCounter_, 1));
    const auto bandDb = static_cast<float>(
        10.0 * std::log10(juce::jmax(1.0e-9, bandPower)));
    smoothedSpectrumDb_[band] =
        0.84f * smoothedSpectrumDb_[band] + 0.16f * bandDb;
    averageBandDb += smoothedSpectrumDb_[band];
  }
  averageBandDb /= static_cast<float>(spectralBandEnergy_.size());

  for (size_t band = 0; band < spectralBandEnergy_.size(); ++band) {
    const auto relative =
        juce::jlimit(-1.0f, 1.0f, (smoothedSpectrumDb_[band] - averageBandDb) / 12.0f);
    spectralCurve_[band].store(relative);
  }

  stepCounter_ = 0;
  stepEnergySum_ = 0.0;
  stepLeftEnergySum_ = 0.0;
  stepRightEnergySum_ = 0.0;
  stepCrossSum_ = 0.0;
  stepMidEnergySum_ = 0.0;
  stepSideEnergySum_ = 0.0;
  spectralBandEnergy_.fill(0.0);
}

} // namespace flow88::dsp
