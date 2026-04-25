#pragma once

#include "PluginProcessor.h"
#include "flow88/ui/FlowLookAndFeel.h"
#include "flow88/ui/FlowWidgets.h"

class Flow88MasterAudioProcessorEditor : public juce::AudioProcessorEditor,
                                         private juce::Timer {
public:
  explicit Flow88MasterAudioProcessorEditor(Flow88MasterAudioProcessor &);
  ~Flow88MasterAudioProcessorEditor() override;

  void paint(juce::Graphics &) override;
  void resized() override;

private:
  using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
  using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

  Flow88MasterAudioProcessor &processor_;
  flow88::ui::FlowLookAndFeel lookAndFeel_;

  flow88::ui::Card assistantCard_;
  flow88::ui::Card tonalCard_;
  flow88::ui::Card macroCard_;
  flow88::ui::Card meterDockCard_;
  flow88::ui::Card advancedCard_;
  flow88::ui::Card diagnosticsCard_;

  flow88::ui::StatusChip currentProfileChip_;
  flow88::ui::SegmentedControl abControl_;
  flow88::ui::StatusChip abMatchChip_;
  juce::ToggleButton bypassToggle_{"Bypass"};
  juce::TextButton diagnosticsButton_{"Inspect"};
  juce::TextButton settingsButton_{"Settings"};

  juce::TextButton analyzeButton_{"Analyze Playback"};
  flow88::ui::StatusChip analysisChip_;
  flow88::ui::StatusChip targetChip_;
  flow88::ui::StatusChip referenceChip_;
  flow88::ui::SegmentedControl styleControl_;
  flow88::ui::SegmentedControl loudnessControl_;

  std::vector<std::unique_ptr<juce::TextButton>> subgenreButtons_;
  std::vector<int> subgenreButtonSourceIndices_;

  flow88::ui::TonalBalanceView tonalView_;
  flow88::ui::MacroDial toneDial_{"Tone", flow88::ui::theme::cyan};
  flow88::ui::MacroDial punchDial_{"Punch", flow88::ui::theme::cyan};
  flow88::ui::MacroDial widthDial_{"Width", flow88::ui::theme::violet};
  flow88::ui::MacroDial lowEndDial_{"Low-End Focus", flow88::ui::theme::teal};
  flow88::ui::MacroDial targetDial_{"Target LUFS", flow88::ui::theme::cyan};
  flow88::ui::MacroDial attitudeDial_{"Attitude", flow88::ui::theme::amber};

  flow88::ui::MeterBar integratedMeter_{"Integrated / Short-Term"};
  flow88::ui::MeterBar truePeakMeter_{"True Peak"};
  flow88::ui::MeterBar gainReductionMeter_{"Gain Reduction"};
  flow88::ui::MeterBar correlationMeter_{"Correlation"};
  flow88::ui::MeterBar ioMeter_{"In / Out"};

  juce::TextButton closeAdvancedButton_{"Done"};
  juce::TextButton loadReferenceButton_{"Load Reference"};
  juce::TextButton clearReferenceButton_{"Clear"};
  juce::Label exactTargetLabel_;
  juce::TextEditor diagnosticsText_;
  juce::Slider inputTrimSlider_;
  juce::Slider outputTrimSlider_;
  juce::Slider limiterCeilingSlider_;
  flow88::ui::SegmentedControl oversamplingControl_;
  juce::Label inputTrimCaption_;
  juce::Label outputTrimCaption_;
  juce::Label limiterCaption_;
  juce::Label oversamplingCaption_;

  std::unique_ptr<SliderAttachment> toneAttachment_;
  std::unique_ptr<SliderAttachment> punchAttachment_;
  std::unique_ptr<SliderAttachment> widthAttachment_;
  std::unique_ptr<SliderAttachment> lowEndAttachment_;
  std::unique_ptr<SliderAttachment> targetAttachment_;
  std::unique_ptr<SliderAttachment> attitudeAttachment_;
  std::unique_ptr<ButtonAttachment> bypassAttachment_;
  std::unique_ptr<SliderAttachment> inputTrimAttachment_;
  std::unique_ptr<SliderAttachment> outputTrimAttachment_;
  std::unique_ptr<SliderAttachment> limiterAttachment_;

  bool diagnosticsVisible_ = false;
  bool exactTargetSync_ = false;
  bool analysisRunning_ = false;
  uint32_t analysisStartedMs_ = 0;
  std::unique_ptr<juce::FileChooser> referenceChooser_;

  void timerCallback() override;
  void refreshFromProcessor();
  void toggleDiagnosticsOverlay();
  void handleExactTargetCommit();
  void launchReferenceChooser();
  void syncSubgenreButtons(int selectedIndex);
  void configureAdvancedSlider(juce::Slider &slider, juce::Colour accent);

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Flow88MasterAudioProcessorEditor)
};
