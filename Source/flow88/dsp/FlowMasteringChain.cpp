#include "FlowMasteringChain.h"
#include <cmath>

namespace flow88::dsp {

namespace {

constexpr float kToneLowCutoffHz = 220.0f;
constexpr float kToneHighCutoffHz = 3200.0f;
constexpr float kLowEndAnchorCutoffHz = 140.0f;
constexpr float kWidthProtectCutoffHz = 155.0f;
constexpr float kDynamicsSidechainCutoffHz = 95.0f;
constexpr float kResonanceCentreHz = 3600.0f;

float clampPercent(float value) {
  return juce::jlimit(0.0f, 1.0f, value * 0.01f);
}

} // namespace

void FlowMasteringChain::prepare(double sampleRate, int maxBlockSize,
                                 int numChannels) {
  sampleRate_ = juce::jmax(1.0, sampleRate);
  numChannels_ = juce::jlimit(1, 2, numChannels);
  lastOversamplingIndex_ = 1;

  const juce::dsp::ProcessSpec stereoSpec{
      sampleRate_, static_cast<juce::uint32>(juce::jmax(1, maxBlockSize)),
      static_cast<juce::uint32>(numChannels_)};
  const juce::dsp::ProcessSpec monoSpec{
      sampleRate_, static_cast<juce::uint32>(juce::jmax(1, maxBlockSize)), 1};

  for (auto &filter : toneLowFilters_) {
    filter.setType(juce::dsp::FirstOrderTPTFilterType::lowpass);
    filter.prepare(stereoSpec);
    filter.setCutoffFrequency(kToneLowCutoffHz);
  }

  for (auto &filter : toneHighFilters_) {
    filter.setType(juce::dsp::FirstOrderTPTFilterType::highpass);
    filter.prepare(stereoSpec);
    filter.setCutoffFrequency(kToneHighCutoffHz);
  }

  for (auto &filter : resonanceFilters_) {
    filter.setType(juce::dsp::StateVariableTPTFilterType::bandpass);
    filter.prepare(stereoSpec);
    filter.setCutoffFrequency(kResonanceCentreHz);
    filter.setResonance(1.2f);
  }

  sideSplitFilter_.prepare(monoSpec);
  sideSplitFilter_.setCutoffFrequency(juce::jmax(kLowEndAnchorCutoffHz, kWidthProtectCutoffHz));

  detectorHighpass_.setType(juce::dsp::FirstOrderTPTFilterType::highpass);
  detectorHighpass_.prepare(monoSpec);
  detectorHighpass_.setCutoffFrequency(kDynamicsSidechainCutoffHz);

  oversamplers_[0] = std::make_unique<juce::dsp::Oversampling<float>>(
      static_cast<size_t>(numChannels_), 1,
      juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false);
  oversamplers_[1] = std::make_unique<juce::dsp::Oversampling<float>>(
      static_cast<size_t>(numChannels_), 2,
      juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false);

  for (auto &oversampler : oversamplers_)
    oversampler->initProcessing(static_cast<size_t>(juce::jmax(1, maxBlockSize)));

  reset();
}

void FlowMasteringChain::reset() {
  compressorEnvelope_ = 0.0f;
  limiterGain_ = 1.0f;
  harshnessEnvelope_.fill(0.0f);

  for (auto &filter : toneLowFilters_)
    filter.reset();

  for (auto &filter : toneHighFilters_)
    filter.reset();

  for (auto &filter : resonanceFilters_)
    filter.reset();

  sideSplitFilter_.reset();
  detectorHighpass_.reset();

  for (auto &oversampler : oversamplers_)
    if (oversampler != nullptr)
      oversampler->reset();
}

FlowMasteringMetrics
FlowMasteringChain::process(juce::AudioBuffer<float> &buffer,
                            const FlowMasteringSettings &settings) {
  FlowMasteringMetrics metrics;

  if (buffer.getNumChannels() < 2 || buffer.getNumSamples() == 0)
    return metrics;

  if (settings.oversamplingIndex != lastOversamplingIndex_) {
    limiterGain_ = 1.0f;
    lastOversamplingIndex_ = settings.oversamplingIndex;
    for (auto &oversampler : oversamplers_)
      if (oversampler != nullptr)
        oversampler->reset();
  }

  updateFilterSettings(settings);
  processToneAndSpatial(buffer, settings, metrics);
  processGlue(buffer, settings, metrics);

  const auto oversamplingFactor = settings.oversamplingIndex == 2 ? 4 : 2;
  if (settings.oversamplingIndex > 0) {
    auto &oversampler = oversamplers_[static_cast<size_t>(settings.oversamplingIndex - 1)];
    juce::dsp::AudioBlock<const float> inputBlock(buffer);
    auto oversampledBlock = oversampler->processSamplesUp(inputBlock);
    processNonlinearBlock(oversampledBlock, settings, oversamplingFactor, metrics);
    juce::dsp::AudioBlock<float> outputBlock(buffer);
    oversampler->processSamplesDown(outputBlock);
  } else {
    juce::dsp::AudioBlock<float> block(buffer);
    processNonlinearBlock(block, settings, 1, metrics);
  }

  metrics.totalGainReductionDb =
      metrics.compressorGainReductionDb + metrics.limiterGainReductionDb;
  return metrics;
}

void FlowMasteringChain::updateFilterSettings(
    const FlowMasteringSettings &settings) {
  for (auto &filter : toneLowFilters_)
    filter.setCutoffFrequency(kToneLowCutoffHz);

  for (auto &filter : toneHighFilters_)
    filter.setCutoffFrequency(kToneHighCutoffHz);

  for (auto &filter : resonanceFilters_) {
    filter.setCutoffFrequency(kResonanceCentreHz);
    filter.setResonance(1.0f + 0.25f * clampPercent(settings.resonanceControlPct));
  }

  sideSplitFilter_.setCutoffFrequency(juce::jmax(
      settings.lowEndAnchorHz > 0.0f ? settings.lowEndAnchorHz
                                     : kLowEndAnchorCutoffHz,
      settings.widthProtectHz > 0.0f ? settings.widthProtectHz
                                     : kWidthProtectCutoffHz));
  detectorHighpass_.setCutoffFrequency(kDynamicsSidechainCutoffHz);
}

void FlowMasteringChain::processToneAndSpatial(
    juce::AudioBuffer<float> &buffer, const FlowMasteringSettings &settings,
    FlowMasteringMetrics &metrics) {
  const auto toneScaledDb =
      juce::jlimit(-3.6f, 3.6f, settings.toneTiltDb * settings.toneScale);
  const auto toneNorm = juce::jlimit(-1.0f, 1.0f, toneScaledDb / 3.0f);
  const auto resonanceNorm = clampPercent(settings.resonanceControlPct);
  const auto harshnessNorm = juce::jlimit(
      0.0f, 1.0f,
      settings.harshnessAmount *
          (0.45f + 0.55f * juce::jmax(0.0f, (toneNorm + 1.0f) * 0.5f)));
  const auto effectiveLowEndFocusPct = juce::jlimit(
      0.0f, 100.0f,
      50.0f + (settings.lowEndFocusPct - 50.0f) * settings.lowEndFocusScale);
  const auto lowEndAnchorAmount =
      juce::jmap(clampPercent(effectiveLowEndFocusPct), 0.0f, 1.0f, 0.0f, 0.84f);
  const auto effectiveWidthPct = juce::jlimit(
      50.0f, 150.0f,
      100.0f + (settings.stereoWidthPct - 100.0f) * settings.widthScale);
  const auto widthGain =
      juce::jmap(effectiveWidthPct, 50.0f, 150.0f, 0.82f, 1.18f);

  metrics.toneStageActive =
      std::abs(toneScaledDb) > 0.03f || resonanceNorm > 0.01f || harshnessNorm > 0.02f;
  metrics.lowEndStageActive = lowEndAnchorAmount > 0.01f;
  metrics.widthStageActive = std::abs(widthGain - 1.0f) > 0.01f;

  for (int sample = 0; sample < buffer.getNumSamples(); ++sample) {
    for (int channel = 0; channel < juce::jmin(2, buffer.getNumChannels()); ++channel) {
      const auto input = buffer.getSample(channel, sample);
      const auto lowBand = toneLowFilters_[static_cast<size_t>(channel)].processSample(
          channel, input);
      const auto highBand = toneHighFilters_[static_cast<size_t>(channel)].processSample(
          channel, input);
      const auto resonanceBand =
          resonanceFilters_[static_cast<size_t>(channel)].processSample(channel, input);

      harshnessEnvelope_[static_cast<size_t>(channel)] =
          juce::jmax(std::abs(resonanceBand),
                     0.9915f * harshnessEnvelope_[static_cast<size_t>(channel)]);
      const auto dynamicHarshness = juce::jlimit(
          0.0f, 1.0f,
          (harshnessEnvelope_[static_cast<size_t>(channel)] - 0.04f) * 15.0f) *
          harshnessNorm;

      auto shaped = input + toneNorm * (0.56f * highBand - 0.34f * lowBand);
      shaped -= (resonanceNorm * 0.12f + dynamicHarshness * 0.18f) * resonanceBand;
      buffer.setSample(channel, sample, shaped);
    }

    const auto left = buffer.getSample(0, sample);
    const auto right = buffer.getSample(1, sample);
    const auto mid = 0.5f * (left + right);
    const auto side = 0.5f * (left - right);

    float lowSide = 0.0f;
    float highSide = 0.0f;
    sideSplitFilter_.processSample(0, side, lowSide, highSide);

    const auto outputSide =
        lowSide * (1.0f - 0.82f * lowEndAnchorAmount) + highSide * widthGain;
    buffer.setSample(0, sample, mid + outputSide);
    buffer.setSample(1, sample, mid - outputSide);
  }
}

void FlowMasteringChain::processGlue(juce::AudioBuffer<float> &buffer,
                                     const FlowMasteringSettings &settings,
                                     FlowMasteringMetrics &metrics) {
  const auto effectivePunchPct = juce::jlimit(
      0.0f, 100.0f,
      50.0f + (settings.punchPct - 50.0f) * settings.punchScale);
  const auto punchNorm = clampPercent(effectivePunchPct);
  const auto parallelMix = 0.03f + 0.22f * punchNorm;
  const auto thresholdDb =
      -20.0f + 5.4f * punchNorm + settings.dynamicsThresholdBiasDb;
  const auto ratio = 1.18f + 1.02f * punchNorm;
  const auto attackMs = 24.0f + 22.0f * punchNorm;
  const auto releaseMs = 145.0f - 34.0f * punchNorm;
  const auto wetMakeup = dbToGain(0.35f + 0.85f * punchNorm);
  const auto attackCoeff =
      std::exp(-1.0f / (juce::jmax(0.001f, attackMs * 0.001f) * static_cast<float>(sampleRate_)));
  const auto releaseCoeff =
      std::exp(-1.0f / (juce::jmax(0.001f, releaseMs * 0.001f) * static_cast<float>(sampleRate_)));

  metrics.dynamicsStageActive = parallelMix > 0.05f;

  float peakGrDb = 0.0f;
  for (int sample = 0; sample < buffer.getNumSamples(); ++sample) {
    auto left = buffer.getSample(0, sample);
    auto right = buffer.getSample(1, sample);
    const auto mono = 0.5f * (left + right);
    const auto sidechain = std::abs(detectorHighpass_.processSample(0, mono)) * 0.8f +
                           std::abs(mono) * 0.2f;

    const auto coeff = sidechain > compressorEnvelope_ ? attackCoeff : releaseCoeff;
    compressorEnvelope_ =
        coeff * compressorEnvelope_ + (1.0f - coeff) * sidechain;

    const auto envelopeDb = gainToDb(compressorEnvelope_);
    const auto overDb = juce::jmax(0.0f, envelopeDb - thresholdDb);
    const auto grDb = overDb * (1.0f - (1.0f / ratio));
    const auto compressorGain = dbToGain(-grDb);
    peakGrDb = juce::jmax(peakGrDb, grDb);

    const auto compressedLeft = left * compressorGain * wetMakeup;
    const auto compressedRight = right * compressorGain * wetMakeup;
    left = left * (1.0f - parallelMix) + compressedLeft * parallelMix;
    right = right * (1.0f - parallelMix) + compressedRight * parallelMix;

    buffer.setSample(0, sample, left);
    buffer.setSample(1, sample, right);
  }

  metrics.compressorGainReductionDb = peakGrDb;
}

void FlowMasteringChain::processNonlinearBlock(
    juce::dsp::AudioBlock<float> block, const FlowMasteringSettings &settings,
    int oversamplingFactor, FlowMasteringMetrics &metrics) {
  const auto targetDriveDb = juce::jlimit(
      0.0f, 4.9f,
      (11.5f + settings.targetLufs) * 0.86f + settings.targetDriveBiasDb);
  const auto targetGain = dbToGain(targetDriveDb);
  const auto drivePressure =
      juce::jlimit(0.0f, 1.0f, (targetDriveDb - 1.35f) / 2.85f);
  const auto effectiveAttitudePct = juce::jlimit(
      0.0f, 100.0f,
      50.0f + (settings.attitudePct - 50.0f) * settings.attitudeScale);
  const auto attitudeNorm = clampPercent(effectiveAttitudePct);
  const auto clipperNorm = juce::jlimit(
      0.0f, 1.0f,
      0.22f * attitudeNorm + 0.72f * clampPercent(settings.clipperAmountPct) +
          0.08f * juce::jmin(1.0f, targetDriveDb / 4.9f));
  const auto saturationDrive =
      1.0f + 1.3f * attitudeNorm + 0.18f * targetDriveDb;
  const auto saturationNormaliser = 1.0f / std::tanh(saturationDrive);
  const auto saturationMix =
      0.04f + 0.16f * attitudeNorm + 0.018f * targetDriveDb;
  const auto clipDrive = 1.0f + 2.8f * clipperNorm + 0.35f * targetDriveDb;
  const auto clipperMix =
      0.035f + 0.18f * clipperNorm + 0.015f * drivePressure;
  const auto limiterSafetyMarginDb =
      0.16f + 0.18f * clipperNorm + 0.13f * drivePressure;
  const auto effectiveCeilingDb =
      settings.limiterCeilingDbTP - juce::jmax(0.0f, settings.outputTrimDb) -
      limiterSafetyMarginDb;
  const auto limiterCeilingGain = dbToGain(effectiveCeilingDb);
  const auto clipperCeilingGain =
      dbToGain(effectiveCeilingDb - 0.12f - 0.12f * clipperNorm);
  const auto limiterReleaseSeconds =
      juce::jlimit(0.055f, 0.185f,
                   (0.084f + 0.022f * drivePressure + 0.004f * clipperNorm) *
                       settings.limiterReleaseScale);
  const auto limiterReleaseCoeff =
      std::exp(-1.0f / (limiterReleaseSeconds *
                        static_cast<float>(sampleRate_ * oversamplingFactor)));

  metrics.targetDriveDb = targetDriveDb;
  metrics.saturationStageActive = saturationMix > 0.055f;
  metrics.clipperStageActive = clipperNorm > 0.02f || targetDriveDb > 1.15f;
  metrics.limiterStageActive = true;

  float limiterPeakGrDb = 0.0f;
  for (size_t sample = 0; sample < block.getNumSamples(); ++sample) {
    auto left = block.getSample(0, sample) * targetGain;
    auto right = block.getSample(1, sample) * targetGain;

    if (metrics.saturationStageActive) {
      const auto saturatedLeft =
          std::tanh(left * saturationDrive) * saturationNormaliser;
      const auto saturatedRight =
          std::tanh(right * saturationDrive) * saturationNormaliser;
      left = juce::jmap(saturationMix, left, saturatedLeft);
      right = juce::jmap(saturationMix, right, saturatedRight);
    }

    if (metrics.clipperStageActive) {
      const auto clippedLeft = std::atan(left * clipDrive) / std::atan(clipDrive);
      const auto clippedRight = std::atan(right * clipDrive) / std::atan(clipDrive);
      left = juce::jmap(clipperMix, left, clippedLeft);
      right = juce::jmap(clipperMix, right, clippedRight);

      const auto softenPeak = [clipperCeilingGain](float value) {
        const auto magnitude = std::abs(value);
        if (magnitude <= clipperCeilingGain)
          return value;

        const auto over = magnitude - clipperCeilingGain;
        const auto softened =
            clipperCeilingGain + std::tanh(over * 6.0f) / 6.0f;
        return juce::jlimit(-clipperCeilingGain * 1.01f,
                            clipperCeilingGain * 1.01f,
                            std::copysign(softened, value));
      };

      left = softenPeak(left);
      right = softenPeak(right);
    }

    const auto peak = juce::jmax(std::abs(left), std::abs(right));
    const auto desiredGain =
        peak > limiterCeilingGain ? limiterCeilingGain / peak : 1.0f;
    if (desiredGain < limiterGain_)
      limiterGain_ = desiredGain;
    else
      limiterGain_ = desiredGain + limiterReleaseCoeff * (limiterGain_ - desiredGain);

    const auto limiterGrDb = -gainToDb(limiterGain_);
    limiterPeakGrDb = juce::jmax(limiterPeakGrDb, limiterGrDb);
    block.setSample(0, sample, left * limiterGain_);
    block.setSample(1, sample, right * limiterGain_);
  }

  metrics.limiterGainReductionDb = limiterPeakGrDb;
}

float FlowMasteringChain::dbToGain(float valueDb) {
  return juce::Decibels::decibelsToGain(valueDb);
}

float FlowMasteringChain::gainToDb(float gain) {
  return juce::Decibels::gainToDecibels(juce::jmax(gain, 1.0e-6f), -120.0f);
}

} // namespace flow88::dsp
