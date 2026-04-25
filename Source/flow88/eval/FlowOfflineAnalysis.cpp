#include "FlowOfflineAnalysis.h"
#include <cmath>
#include <limits>

namespace flow88::eval {

namespace {

constexpr double kAnalysisStepSeconds = 0.1;
constexpr size_t kIntegratedHistoryBlocks = 20 * 60 * 10;
constexpr float kLowMonoProbeHz = 125.0f;
constexpr std::array<double, 7> kSpectrumCentres{
    35.0, 90.0, 220.0, 560.0, 1400.0, 4200.0, 11000.0};

double lufsToPower(double lufs) {
  return std::pow(10.0, (lufs + 0.691) * 0.1);
}

float safeGainToDb(float gain) {
  return juce::Decibels::gainToDecibels(juce::jmax(gain, 1.0e-6f), -120.0f);
}

float clampScore(float value) { return juce::jlimit(0.0f, 100.0f, value); }

juce::var toSpectrumVar(const std::array<float, 7> &curve) {
  juce::Array<juce::var> array;
  for (const auto value : curve)
    array.add(value);
  return juce::var(array);
}

juce::var metricsToVar(const AnalysisMetrics &metrics) {
  auto object = std::make_unique<juce::DynamicObject>();
  object->setProperty("integratedLufs", metrics.integratedLufs);
  object->setProperty("shortTermLufs", metrics.shortTermLufs);
  object->setProperty("truePeakDbTP", metrics.truePeakDbTP);
  object->setProperty("stereoCorrelation", metrics.stereoCorrelation);
  object->setProperty("stereoWidthPct", metrics.stereoWidthPct);
  object->setProperty("tonalBalanceDeviation", metrics.tonalBalanceDeviation);
  object->setProperty("lowEndMonoStability", metrics.lowEndMonoStability);
  object->setProperty("lowEndWidthPct", metrics.lowEndWidthPct);
  object->setProperty("lowEndCorrelation", metrics.lowEndCorrelation);
  object->setProperty("crestFactorDb", metrics.crestFactorDb);
  object->setProperty("clippedSamplePct", metrics.clippedSamplePct);
  object->setProperty("spectralTilt", metrics.spectralTilt);
  object->setProperty("spectralCurve", toSpectrumVar(metrics.spectralCurve));
  return juce::var(object.release());
}

juce::var scoresToVar(const EvaluationScores &scores) {
  auto object = std::make_unique<juce::DynamicObject>();
  object->setProperty("loudnessSafety", scores.loudnessSafety);
  object->setProperty("tonalBalanceQuality", scores.tonalBalanceQuality);
  object->setProperty("widthRisk", scores.widthRisk);
  object->setProperty("lowEndStability", scores.lowEndStability);
  object->setProperty("limiterDamageRisk", scores.limiterDamageRisk);
  return juce::var(object.release());
}

} // namespace

void OfflineAnalysisEngine::prepare(double sampleRate, int maxBlockSize) {
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
    const auto frequency =
        static_cast<float>(juce::jlimit(20.0, sampleRate_ * 0.45, kSpectrumCentres[i]));
    spectralFilters_[i].coefficients =
        juce::dsp::IIR::Coefficients<float>::makeBandPass(sampleRate_, frequency, 0.85f);
    spectralFilters_[i].reset();
  }

  const juce::dsp::ProcessSpec monoSpec{
      sampleRate_, static_cast<juce::uint32>(juce::jmax(1, maxBlockSize)), 1};
  for (auto &filter : lowMonoFilters_) {
    filter.prepare(monoSpec);
    filter.setCutoffFrequency(kLowMonoProbeHz);
    filter.reset();
  }

  integratedBlockPowers_.assign(kIntegratedHistoryBlocks, 0.0f);
  reset();
}

void OfflineAnalysisEngine::reset() {
  stepCounter_ = 0;
  stepEnergySum_ = 0.0;
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
  totalLeftEnergy_ = 0.0;
  totalRightEnergy_ = 0.0;
  totalCrossSum_ = 0.0;
  totalMidEnergy_ = 0.0;
  totalSideEnergy_ = 0.0;
  lowLeftEnergy_ = 0.0;
  lowRightEnergy_ = 0.0;
  lowCrossSum_ = 0.0;
  lowMidEnergy_ = 0.0;
  lowSideEnergy_ = 0.0;
  totalMonoEnergy_ = 0.0;
  totalSamples_ = 0;
  clippedSampleCount_ = 0;
  peakSample_ = 0.0f;
  truePeakSample_ = 0.0f;
  shortTermLufs_ = -70.0f;

  for (auto &filter : kWeightShelf_)
    filter.reset();
  for (auto &filter : kWeightHighPass_)
    filter.reset();
  for (auto &filter : spectralFilters_)
    filter.reset();
  for (auto &filter : lowMonoFilters_)
    filter.reset();
  if (truePeakOversampler_ != nullptr)
    truePeakOversampler_->reset();
}

void OfflineAnalysisEngine::pushBlock(const juce::AudioBuffer<float> &buffer) {
  if (buffer.getNumSamples() == 0 || buffer.getNumChannels() == 0)
    return;

  if (truePeakOversampler_ != nullptr && buffer.getNumChannels() >= 2) {
    const juce::dsp::AudioBlock<const float> inputBlock(buffer);
    const auto oversampledBlock = truePeakOversampler_->processSamplesUp(inputBlock);
    for (size_t channel = 0; channel < oversampledBlock.getNumChannels(); ++channel) {
      for (size_t sample = 0; sample < oversampledBlock.getNumSamples(); ++sample) {
        truePeakSample_ = juce::jmax(
            truePeakSample_, std::abs(oversampledBlock.getSample(channel, sample)));
      }
    }
  }

  const auto *left = buffer.getReadPointer(0);
  const auto *right = buffer.getReadPointer(juce::jmin(1, buffer.getNumChannels() - 1));
  const auto channelSamples = static_cast<int64_t>(buffer.getNumSamples()) * 2;
  totalSamples_ += channelSamples;

  for (int sample = 0; sample < buffer.getNumSamples(); ++sample) {
    const auto leftSample = left[sample];
    const auto rightSample = right[sample];
    const auto monoSample = 0.5f * (leftSample + rightSample);

    peakSample_ = juce::jmax(peakSample_,
                             juce::jmax(std::abs(leftSample), std::abs(rightSample)));
    if (std::abs(leftSample) > 0.985f)
      ++clippedSampleCount_;
    if (std::abs(rightSample) > 0.985f)
      ++clippedSampleCount_;

    totalLeftEnergy_ += static_cast<double>(leftSample) * leftSample;
    totalRightEnergy_ += static_cast<double>(rightSample) * rightSample;
    totalCrossSum_ += static_cast<double>(leftSample) * rightSample;
    totalMonoEnergy_ += static_cast<double>(monoSample) * monoSample;

    const auto midSample = 0.5f * (leftSample + rightSample);
    const auto sideSample = 0.5f * (leftSample - rightSample);
    totalMidEnergy_ += static_cast<double>(midSample) * midSample;
    totalSideEnergy_ += static_cast<double>(sideSample) * sideSample;

    const auto weightedLeft =
        kWeightHighPass_[0].processSample(kWeightShelf_[0].processSample(leftSample));
    const auto weightedRight =
        kWeightHighPass_[1].processSample(kWeightShelf_[1].processSample(rightSample));
    stepEnergySum_ += static_cast<double>(weightedLeft) * weightedLeft;
    stepEnergySum_ += static_cast<double>(weightedRight) * weightedRight;

    for (size_t band = 0; band < spectralFilters_.size(); ++band) {
      const auto filtered = spectralFilters_[band].processSample(monoSample);
      spectralBandEnergy_[band] += static_cast<double>(filtered) * filtered;
    }

    float lowLeft = 0.0f;
    float lowLeftHigh = 0.0f;
    float lowRight = 0.0f;
    float lowRightHigh = 0.0f;
    lowMonoFilters_[0].processSample(0, leftSample, lowLeft, lowLeftHigh);
    lowMonoFilters_[1].processSample(0, rightSample, lowRight, lowRightHigh);
    lowLeftEnergy_ += static_cast<double>(lowLeft) * lowLeft;
    lowRightEnergy_ += static_cast<double>(lowRight) * lowRight;
    lowCrossSum_ += static_cast<double>(lowLeft) * lowRight;
    const auto lowMid = 0.5f * (lowLeft + lowRight);
    const auto lowSide = 0.5f * (lowLeft - lowRight);
    lowMidEnergy_ += static_cast<double>(lowMid) * lowMid;
    lowSideEnergy_ += static_cast<double>(lowSide) * lowSide;

    ++stepCounter_;
    if (stepCounter_ >= stepSamples_)
      finaliseStepWindow();
  }
}

AnalysisMetrics OfflineAnalysisEngine::finish(const state::AnalysisTargets *target) {
  if (stepCounter_ > 0)
    finaliseStepWindow();

  AnalysisMetrics metrics;
  metrics.integratedLufs = computeIntegratedLufs();
  metrics.shortTermLufs = shortTermLufs_;
  metrics.truePeakDbTP =
      juce::jlimit(-60.0f, 6.0f, safeGainToDb(truePeakSample_));

  const auto correlationDenominator =
      std::sqrt(juce::jmax(1.0e-12, totalLeftEnergy_ * totalRightEnergy_));
  metrics.stereoCorrelation =
      correlationDenominator > 0.0
          ? juce::jlimit(-1.0f, 1.0f,
                         static_cast<float>(totalCrossSum_ / correlationDenominator))
          : 0.0f;

  const auto widthRatio =
      std::sqrt(totalSideEnergy_ / juce::jmax(1.0e-12, totalMidEnergy_));
  metrics.stereoWidthPct = juce::jlimit(
      0.0f, 220.0f, static_cast<float>(widthRatio * 100.0));

  for (size_t i = 0; i < metrics.spectralCurve.size(); ++i) {
    const auto averagePower =
        spectralBandEnergy_[i] / static_cast<double>(juce::jmax(stepSamples_, 1));
    juce::ignoreUnused(averagePower);
    metrics.spectralCurve[i] = juce::jlimit(-1.0f, 1.0f, smoothedSpectrumDb_[i] / 18.0f);
  }

  const auto lowEnergy =
      (metrics.spectralCurve[0] + metrics.spectralCurve[1] + metrics.spectralCurve[2]) /
      3.0f;
  const auto highEnergy =
      (metrics.spectralCurve[4] + metrics.spectralCurve[5] + metrics.spectralCurve[6]) /
      3.0f;
  metrics.spectralTilt = highEnergy - lowEnergy;

  if (target != nullptr) {
    float deviationSum = 0.0f;
    for (size_t i = 0; i < metrics.spectralCurve.size(); ++i)
      deviationSum += std::abs(metrics.spectralCurve[i] - target->spectralCurve[i]);
    deviationSum += std::abs(metrics.spectralTilt - target->tonalTilt) * 0.8f;
    metrics.tonalBalanceDeviation =
        deviationSum / static_cast<float>(metrics.spectralCurve.size());
  }

  const auto lowCorrelationDenominator =
      std::sqrt(juce::jmax(1.0e-12, lowLeftEnergy_ * lowRightEnergy_));
  metrics.lowEndCorrelation =
      lowCorrelationDenominator > 0.0
          ? juce::jlimit(-1.0f, 1.0f,
                         static_cast<float>(lowCrossSum_ / lowCorrelationDenominator))
          : 0.0f;
  metrics.lowEndWidthPct = juce::jlimit(
      0.0f, 150.0f,
      static_cast<float>(
          std::sqrt(lowSideEnergy_ / juce::jmax(1.0e-12, lowMidEnergy_)) * 100.0));

  const auto lowSideRatio =
      static_cast<float>(lowSideEnergy_ /
                         juce::jmax(1.0e-12, lowMidEnergy_ + lowSideEnergy_));
  metrics.lowEndMonoStability = juce::jlimit(
      0.0f, 100.0f,
      100.0f - metrics.lowEndWidthPct * 0.9f -
          juce::jmax(0.0f, 0.2f - metrics.lowEndCorrelation) * 120.0f -
          lowSideRatio * 38.0f);

  const auto rms = std::sqrt(totalMonoEnergy_ /
                             juce::jmax<int64_t>(1, totalSamples_ / 2));
  metrics.crestFactorDb = safeGainToDb(peakSample_ / juce::jmax(1.0e-6f, static_cast<float>(rms)));
  metrics.clippedSamplePct =
      static_cast<float>(clippedSampleCount_) * 100.0f /
      static_cast<float>(juce::jmax<int64_t>(1, totalSamples_));
  return metrics;
}

bool loadAudioFile(const juce::File &file, juce::AudioBuffer<float> &buffer,
                   double &sampleRate, juce::String &errorMessage) {
  if (!file.existsAsFile()) {
    errorMessage = "Input file was not found.";
    return false;
  }

  juce::AudioFormatManager formatManager;
  formatManager.registerBasicFormats();
  std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
  if (reader == nullptr) {
    errorMessage = "Unsupported or unreadable audio file.";
    return false;
  }

  const auto lengthInSamples = static_cast<int>(juce::jmin<int64_t>(
      reader->lengthInSamples, static_cast<int64_t>(std::numeric_limits<int>::max())));
  if (lengthInSamples <= 0) {
    errorMessage = "Input file contained no readable samples.";
    return false;
  }

  juce::AudioBuffer<float> sourceBuffer(juce::jmax(1, static_cast<int>(reader->numChannels)),
                                        lengthInSamples);
  if (!reader->read(&sourceBuffer, 0, lengthInSamples, 0, true, true)) {
    errorMessage = "Failed to read audio data from the file.";
    return false;
  }

  buffer.setSize(2, lengthInSamples);
  buffer.clear();
  buffer.copyFrom(0, 0, sourceBuffer, 0, 0, lengthInSamples);
  if (sourceBuffer.getNumChannels() > 1)
    buffer.copyFrom(1, 0, sourceBuffer, 1, 0, lengthInSamples);
  else
    buffer.copyFrom(1, 0, sourceBuffer, 0, 0, lengthInSamples);

  sampleRate = reader->sampleRate;
  errorMessage.clear();
  return true;
}

AnalysisMetrics analyzeBuffer(const juce::AudioBuffer<float> &buffer, double sampleRate,
                              int blockSize, const state::AnalysisTargets *target) {
  OfflineAnalysisEngine engine;
  engine.prepare(sampleRate, blockSize);

  int sampleOffset = 0;
  while (sampleOffset < buffer.getNumSamples()) {
    const auto numThisBlock = juce::jmin(blockSize, buffer.getNumSamples() - sampleOffset);
    juce::AudioBuffer<float> block(buffer.getNumChannels(), numThisBlock);
    block.clear();
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel) {
      block.copyFrom(channel, 0, buffer, channel, sampleOffset, numThisBlock);
    }
    engine.pushBlock(block);
    sampleOffset += numThisBlock;
  }

  return engine.finish(target);
}

AnalysisMetrics analyzeFile(const juce::File &file,
                            const state::AnalysisTargets *target, int blockSize,
                            juce::String *errorMessage) {
  juce::AudioBuffer<float> buffer;
  double sampleRate = 0.0;
  juce::String error;
  if (!loadAudioFile(file, buffer, sampleRate, error)) {
    if (errorMessage != nullptr)
      *errorMessage = error;
    return {};
  }

  if (errorMessage != nullptr)
    errorMessage->clear();
  return analyzeBuffer(buffer, sampleRate, blockSize, target);
}

EvaluationScores scoreMetrics(const AnalysisMetrics &metrics,
                              const state::AnalysisTargets &target,
                              float limiterCeilingDbTP) {
  EvaluationScores scores;
  scores.loudnessSafety = clampScore(
      100.0f - std::abs(metrics.integratedLufs - target.lufsIntegrated) * 11.0f -
      std::abs(metrics.shortTermLufs - target.lufsIntegrated) * 4.0f -
      juce::jmax(0.0f, metrics.truePeakDbTP - limiterCeilingDbTP) * 42.0f);

  scores.tonalBalanceQuality = clampScore(
      100.0f - metrics.tonalBalanceDeviation * 82.0f -
      std::abs(metrics.spectralTilt - target.tonalTilt) * 20.0f);

  scores.widthRisk = clampScore(
      juce::jmax(0.0f, metrics.stereoWidthPct - 140.0f) * 1.55f +
      juce::jmax(0.0f, 0.08f - metrics.stereoCorrelation) * 180.0f +
      juce::jmax(0.0f, 78.0f - metrics.lowEndMonoStability) * 1.2f);

  scores.lowEndStability = clampScore(
      100.0f - juce::jmax(0.0f, 85.0f - metrics.lowEndMonoStability) * 1.4f -
      juce::jmax(0.0f, metrics.lowEndWidthPct - 20.0f) * 1.2f -
      juce::jmax(0.0f, 0.25f - metrics.lowEndCorrelation) * 90.0f);

  scores.limiterDamageRisk = clampScore(
      juce::jmax(0.0f, metrics.truePeakDbTP - limiterCeilingDbTP) * 55.0f +
      juce::jmax(0.0f, 7.5f - metrics.crestFactorDb) * 8.5f +
      metrics.clippedSamplePct * 220.0f);
  return scores;
}

juce::String reportToJson(const std::vector<EvaluationReport> &reports) {
  auto root = std::make_unique<juce::DynamicObject>();
  root->setProperty("generatedAt", juce::Time::getCurrentTime().toISO8601(true));
  root->setProperty("count", static_cast<int>(reports.size()));

  juce::Array<juce::var> records;
  float loudnessSafetySum = 0.0f;
  float tonalSum = 0.0f;
  float widthRiskSum = 0.0f;
  float lowEndSum = 0.0f;
  float limiterRiskSum = 0.0f;

  for (const auto &report : reports) {
    auto object = std::make_unique<juce::DynamicObject>();
    object->setProperty("sourcePath", report.sourcePath);
    object->setProperty("sourceName", report.sourceName);
    object->setProperty("renderMode", report.renderMode);
    object->setProperty("subgenreId", report.subgenreId);
    object->setProperty("styleId", report.styleId);
    object->setProperty("inputMetrics", metricsToVar(report.inputMetrics));
    object->setProperty("outputMetrics", metricsToVar(report.outputMetrics));
    object->setProperty("scores", scoresToVar(report.scores));
    records.add(juce::var(object.release()));

    loudnessSafetySum += report.scores.loudnessSafety;
    tonalSum += report.scores.tonalBalanceQuality;
    widthRiskSum += report.scores.widthRisk;
    lowEndSum += report.scores.lowEndStability;
    limiterRiskSum += report.scores.limiterDamageRisk;
  }

  root->setProperty("reports", juce::var(records));

  auto aggregate = std::make_unique<juce::DynamicObject>();
  const auto count = juce::jmax(1.0f, static_cast<float>(reports.size()));
  aggregate->setProperty("loudnessSafety", loudnessSafetySum / count);
  aggregate->setProperty("tonalBalanceQuality", tonalSum / count);
  aggregate->setProperty("widthRisk", widthRiskSum / count);
  aggregate->setProperty("lowEndStability", lowEndSum / count);
  aggregate->setProperty("limiterDamageRisk", limiterRiskSum / count);
  root->setProperty("aggregate", juce::var(aggregate.release()));

  return juce::JSON::toString(juce::var(root.release()), true);
}

juce::String reportToCsv(const std::vector<EvaluationReport> &reports) {
  juce::StringArray lines;
  lines.add("source,mode,subgenre,style,integrated_lufs,short_term_lufs,true_peak_dbtp,"
            "stereo_correlation,stereo_width_pct,tonal_balance_deviation,"
            "low_end_mono_stability,loudness_safety,tonal_balance_quality,width_risk,"
            "low_end_stability,limiter_damage_risk");

  for (const auto &report : reports) {
    const auto &metrics = report.outputMetrics;
    const auto &scores = report.scores;
    lines.add(juce::StringArray{
                  report.sourceName,
                  report.renderMode,
                  report.subgenreId,
                  report.styleId,
                  juce::String(metrics.integratedLufs, 2),
                  juce::String(metrics.shortTermLufs, 2),
                  juce::String(metrics.truePeakDbTP, 2),
                  juce::String(metrics.stereoCorrelation, 3),
                  juce::String(metrics.stereoWidthPct, 2),
                  juce::String(metrics.tonalBalanceDeviation, 3),
                  juce::String(metrics.lowEndMonoStability, 2),
                  juce::String(scores.loudnessSafety, 2),
                  juce::String(scores.tonalBalanceQuality, 2),
                  juce::String(scores.widthRisk, 2),
                  juce::String(scores.lowEndStability, 2),
                  juce::String(scores.limiterDamageRisk, 2)}
                  .joinIntoString(","));
  }

  return lines.joinIntoString("\n");
}

float OfflineAnalysisEngine::powerToLufs(double power) {
  if (power <= 0.0)
    return -70.0f;

  return juce::jlimit(-70.0f, 6.0f,
                      static_cast<float>(-0.691 + 10.0 * std::log10(power)));
}

float OfflineAnalysisEngine::computeIntegratedLufs() const {
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

void OfflineAnalysisEngine::finaliseStepWindow() {
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
  shortTermLufs_ =
      powerToLufs(shortTermPowerSum_ / static_cast<double>(juce::jmax<size_t>(1, shortTermCount_)));

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
  }

  float averageBandDb = 0.0f;
  for (size_t band = 0; band < spectralBandEnergy_.size(); ++band) {
    const auto bandPower =
        spectralBandEnergy_[band] / static_cast<double>(juce::jmax(stepCounter_, 1));
    const auto bandDb =
        static_cast<float>(10.0 * std::log10(juce::jmax(1.0e-9, bandPower)));
    smoothedSpectrumDb_[band] = 0.84f * smoothedSpectrumDb_[band] + 0.16f * bandDb;
    averageBandDb += smoothedSpectrumDb_[band];
  }

  averageBandDb /= static_cast<float>(spectralBandEnergy_.size());
  for (auto &value : smoothedSpectrumDb_)
    value -= averageBandDb;

  stepCounter_ = 0;
  stepEnergySum_ = 0.0;
  spectralBandEnergy_.fill(0.0);
}

} // namespace flow88::eval
