#pragma once

#include "FlowTheme.h"

namespace flow88::ui {

class Card : public juce::Component {
public:
  void setEyebrow(const juce::String &text);
  void setTitle(const juce::String &text);
  void setSubtitle(const juce::String &text);
  void setAccent(juce::Colour colour);
  void setElevated(bool shouldElevate);

  juce::Rectangle<int> getContentBounds() const;
  juce::Rectangle<int> getHeaderBounds() const;

  void paint(juce::Graphics &) override;

private:
  struct HeaderMetrics {
    juce::Rectangle<int> headerBounds;
    juce::Rectangle<int> contentBounds;
    juce::Rectangle<int> eyebrowBounds;
    juce::Rectangle<int> titleBounds;
    juce::Rectangle<int> subtitleBounds;
    int reservedHeight = 0;
    bool hasHeader = false;
    bool hasSubtitle = false;
  };

  HeaderMetrics getHeaderMetrics() const;

  juce::String eyebrow_;
  juce::String title_;
  juce::String subtitle_;
  juce::Colour accent_ = theme::cyan;
  bool elevated_ = false;
};

class SegmentedControl : public juce::Component {
public:
  std::function<void(int)> onSelectionChanged;

  void setOptions(const juce::StringArray &options);
  void setSelectedIndex(int index);
  int getSelectedIndex() const noexcept { return selectedIndex_; }

  void resized() override;

private:
  std::vector<std::unique_ptr<juce::TextButton>> buttons_;
  int selectedIndex_ = 0;
};

class MacroDial : public juce::Component {
public:
  explicit MacroDial(const juce::String &label, juce::Colour accent = theme::cyan);

  juce::Slider &getSlider() noexcept { return slider_; }
  void setAccent(juce::Colour colour);

  void resized() override;
  void paint(juce::Graphics &) override;

private:
  juce::String label_;
  juce::Colour accent_;
  juce::Slider slider_;
};

class MeterBar : public juce::Component {
public:
  explicit MeterBar(const juce::String &label = {});

  void setLabel(const juce::String &label);
  void setValueText(const juce::String &valueText);
  void setNormalisedValue(float value);
  void setAccent(juce::Colour colour);
  void setBipolar(bool shouldBeBipolar);

  void paint(juce::Graphics &) override;

private:
  juce::String label_;
  juce::String valueText_;
  juce::Colour accent_ = theme::cyan;
  float normalisedValue_ = 0.0f;
  bool bipolar_ = false;
};

class TonalBalanceView : public juce::Component {
public:
  void setAccent(juce::Colour colour);
  void setCurves(const std::array<float, 7> &actual,
                 const std::array<float, 7> &target);

  void paint(juce::Graphics &) override;

private:
  juce::Colour accent_ = theme::cyan;
  std::array<float, 7> actual_{};
  std::array<float, 7> target_{};
};

class StatusChip : public juce::Component {
public:
  void setText(const juce::String &text);
  void setAccent(juce::Colour colour);
  void setActive(bool isActive);

  void paint(juce::Graphics &) override;

private:
  juce::String text_;
  juce::Colour accent_ = theme::cyan;
  bool active_ = true;
};

class AssistantSummaryChip : public juce::Component {
public:
  void setHeading(const juce::String &text);
  void setText(const juce::String &text);
  void setAccent(juce::Colour colour);

  void paint(juce::Graphics &) override;

private:
  juce::String heading_ = "ASSISTANT SUMMARY";
  juce::String text_;
  juce::Colour accent_ = theme::cyan;
};

} // namespace flow88::ui
