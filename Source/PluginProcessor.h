#pragma once

#include "flow88/dsp/FlowMasteringChain.h"
#include "flow88/dsp/FlowMeterSource.h"
#include "flow88/eval/FlowOfflineAnalysis.h"
#include "flow88/FlowJuceIncludes.h"
#include "flow88/state/FlowParameterLayout.h"
#include "flow88/state/FlowProfiles.h"
#include "flow88/state/FlowSessionState.h"
#include "flow88/state/FlowStateSerializer.h"

class Flow88MasterAudioProcessor : public juce::AudioProcessor {
public:
  struct DisplayState {
    juce::String currentProfileTitle;
    juce::String suggestedProfileTitle;
    juce::String assistantSummary;
    juce::String currentProfileGuide;
    juce::String referenceStatus;
    juce::String referenceGuidance;
    juce::String referenceFileLabel;
    juce::String analysisStateText;
    juce::String loudnessPresetText;
    juce::Colour accent = juce::Colour::fromString("ff4fd6ff");
    bool analysisInProgress = false;
    bool analysisReady = false;
    bool bypassed = false;
    bool advancedPanelOpen = false;
    bool referenceAnalyzing = false;
    bool referenceReady = false;
    bool abLevelMatchReady = false;
    int activeABSlot = 0;
    float targetLufs = -9.0f;
    float integratedLufs = -9.0f;
    float shortTermLufs = -9.0f;
    float truePeakDbTP = -1.0f;
    float gainReductionDb = 0.0f;
    float compressorGainReductionDb = 0.0f;
    float limiterGainReductionDb = 0.0f;
    float correlation = 0.2f;
    float stereoWidthPct = 100.0f;
    float inputPeakDb = -60.0f;
    float outputPeakDb = -60.0f;
    float targetDriveDb = 0.0f;
    float referenceIntegratedLufs = -70.0f;
    float referenceTruePeakDbTP = -60.0f;
    float referenceCorrelation = 0.0f;
    float referenceWidthPct = 0.0f;
    float referenceTonalDeviation = 0.0f;
    float abLevelMatchGainDb = 0.0f;
    bool inputTrimStageActive = false;
    bool toneStageActive = false;
    bool lowEndStageActive = false;
    bool dynamicsStageActive = false;
    bool widthStageActive = false;
    bool saturationStageActive = false;
    bool clipperStageActive = false;
    bool limiterStageActive = false;
    bool outputTrimStageActive = false;
    std::array<float, 7> actualCurve{};
    std::array<float, 7> targetCurve{};
  };

  Flow88MasterAudioProcessor();
  ~Flow88MasterAudioProcessor() override;

  void prepareToPlay(double sampleRate, int samplesPerBlock) override;
  void releaseResources() override;
  bool isBusesLayoutSupported(const BusesLayout &layouts) const override;
  void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override;

  juce::AudioProcessorEditor *createEditor() override;
  bool hasEditor() const override { return true; }

  const juce::String getName() const override { return JucePlugin_Name; }
  bool acceptsMidi() const override { return false; }
  bool producesMidi() const override { return false; }
  bool isMidiEffect() const override { return false; }
  double getTailLengthSeconds() const override { return 0.0; }

  int getNumPrograms() override { return 1; }
  int getCurrentProgram() override { return 0; }
  void setCurrentProgram(int) override {}
  const juce::String getProgramName(int) override { return "Flow88 Master Init"; }
  void changeProgramName(int, const juce::String &) override {}

  void getStateInformation(juce::MemoryBlock &destData) override;
  void setStateInformation(const void *data, int sizeInBytes) override;

  juce::AudioProcessorValueTreeState &getAPVTS() noexcept { return apvts_; }
  const flow88::state::FlowSessionState &getSessionState() const noexcept {
    return sessionState_;
  }

  DisplayState buildDisplayState(double animationSeconds) const;

  void setSubgenreIndex(int index);
  void setStyleIndex(int index);
  void setLoudnessPresetIndex(int index);
  void setTargetLufsExact(float value);
  void setOversamplingIndex(int index);
  void toggleAdvancedPanel();
  void switchABSlot(int slotIndex);
  void startAnalysisSimulation();
  void finishAnalysisSimulation();
  void loadReferenceTrack(const juce::File &file);
  void clearReferenceTrack();

private:
  class ReferenceAnalysisJob;

  juce::AudioProcessorValueTreeState apvts_;
  flow88::state::FlowSessionState sessionState_;
  flow88::state::ProfileLibrary profileLibrary_;
  flow88::dsp::FlowMasteringChain masteringChain_;
  flow88::dsp::FlowMeterSource meterSource_;
  juce::LinearSmoothedValue<float> inputGainSmoothed_;
  juce::LinearSmoothedValue<float> outputGainSmoothed_;
  juce::LinearSmoothedValue<float> abLevelMatchGainSmoothed_;

  std::atomic<float> *inputTrimParam_ = nullptr;
  std::atomic<float> *outputTrimParam_ = nullptr;
  std::atomic<float> *bypassParam_ = nullptr;
  std::atomic<float> compressorGainReductionDb_{0.0f};
  std::atomic<float> limiterGainReductionDb_{0.0f};
  std::atomic<float> targetDriveDb_{0.0f};
  std::atomic<uint32_t> stageFlags_{0};
  std::atomic<float> toneScale_{1.0f};
  std::atomic<float> punchScale_{1.0f};
  std::atomic<float> widthScale_{1.0f};
  std::atomic<float> lowEndFocusScale_{1.0f};
  std::atomic<float> attitudeScale_{1.0f};
  std::atomic<float> targetDriveBiasDb_{0.0f};
  std::atomic<float> lowEndAnchorHz_{140.0f};
  std::atomic<float> widthProtectHz_{155.0f};
  std::atomic<float> dynamicsThresholdBiasDb_{0.0f};
  std::atomic<float> limiterReleaseScale_{1.0f};
  std::atomic<float> harshnessAmount_{0.0f};
  std::atomic<float> abLevelMatchGainDb_{0.0f};
  std::atomic<bool> abLevelMatchReady_{false};
  std::array<std::atomic<float>, 2> abSlotComparableLufs_{
      std::atomic<float>{-100.0f}, std::atomic<float>{-100.0f}};

  struct ReferenceSnapshot {
    bool analyzing = false;
    bool ready = false;
    juce::String filePath;
    juce::String fileLabel;
    juce::String errorMessage;
    flow88::eval::AnalysisMetrics metrics;
  };

  mutable juce::CriticalSection referenceLock_;
  ReferenceSnapshot referenceSnapshot_;
  juce::ThreadPool referenceThreadPool_{1};
  std::atomic<int> referenceRequestId_{0};

  juce::String getSelectedSubgenreId() const;
  juce::String getSelectedStyleId() const;
  flow88::state::EffectiveProfile getCurrentProfile() const;
  void applySelectionProfile(const juce::String &subgenreId,
                             const juce::String &styleId);
  void refreshReferenceStatus();
  void cacheProfileTuning(const flow88::state::EffectiveProfile &profile);
  ReferenceSnapshot copyReferenceSnapshot() const;
  void beginReferenceAnalysis(const juce::File &file);
  void completeReferenceAnalysis(
      int requestId, const juce::File &file,
      const flow88::eval::AnalysisMetrics &metrics,
      const juce::String &errorMessage);
  void ensureSnapshotDefaults();
  static float targetForLoudnessPreset(int index);

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Flow88MasterAudioProcessor)
};
