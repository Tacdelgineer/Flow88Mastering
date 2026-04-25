#include "FlowWidgets.h"

namespace flow88::ui {

namespace {

constexpr int kCardPaddingX = 18;
constexpr int kCardPaddingY = 16;
constexpr int kCardEyebrowHeight = 12;
constexpr int kCardTitleHeight = 24;
constexpr int kCardSubtitleHeight = 16;
constexpr int kCardHeaderGap = 4;
constexpr int kCardTitleOnlyHeight = kCardTitleHeight;
constexpr int kCardHeaderTitleOnlyHeight =
    kCardEyebrowHeight + kCardHeaderGap + kCardTitleHeight;
constexpr int kCardHeaderWithSubtitleHeight =
    kCardEyebrowHeight + kCardHeaderGap + kCardTitleHeight + kCardHeaderGap +
    kCardSubtitleHeight;

} // namespace

void Card::setEyebrow(const juce::String &text) {
  eyebrow_ = text;
  repaint();
}

void Card::setTitle(const juce::String &text) {
  title_ = text;
  repaint();
}

void Card::setSubtitle(const juce::String &text) {
  subtitle_ = text;
  repaint();
}

void Card::setAccent(juce::Colour colour) {
  accent_ = colour;
  repaint();
}

void Card::setElevated(bool shouldElevate) {
  elevated_ = shouldElevate;
  repaint();
}

Card::HeaderMetrics Card::getHeaderMetrics() const {
  HeaderMetrics metrics;
  auto innerBounds = getLocalBounds().reduced(kCardPaddingX, kCardPaddingY);
  metrics.contentBounds = innerBounds;

  metrics.hasHeader =
      !(eyebrow_.isEmpty() && title_.isEmpty() && subtitle_.isEmpty());
  if (!metrics.hasHeader)
    return metrics;

  const auto hasEyebrow = eyebrow_.isNotEmpty();
  metrics.hasSubtitle = subtitle_.isNotEmpty();
  if (!hasEyebrow && !metrics.hasSubtitle)
    metrics.reservedHeight = kCardTitleOnlyHeight;
  else if (metrics.hasSubtitle)
    metrics.reservedHeight = kCardHeaderWithSubtitleHeight;
  else
    metrics.reservedHeight = kCardHeaderTitleOnlyHeight;
  metrics.headerBounds = innerBounds.removeFromTop(metrics.reservedHeight);
  metrics.contentBounds = innerBounds;

  auto header = metrics.headerBounds;
  if (hasEyebrow) {
    metrics.eyebrowBounds = header.removeFromTop(kCardEyebrowHeight);
    header.removeFromTop(kCardHeaderGap);
  }
  metrics.titleBounds = header.removeFromTop(kCardTitleHeight);
  if (metrics.hasSubtitle) {
    header.removeFromTop(kCardHeaderGap);
    metrics.subtitleBounds = header.removeFromTop(kCardSubtitleHeight);
  }

  return metrics;
}

juce::Rectangle<int> Card::getContentBounds() const {
  return getHeaderMetrics().contentBounds;
}

juce::Rectangle<int> Card::getHeaderBounds() const {
  return getHeaderMetrics().headerBounds;
}

void Card::paint(juce::Graphics &g) {
  const auto bounds = getLocalBounds();
  g.setColour(theme::cardFill(elevated_));
  g.fillRoundedRectangle(bounds.toFloat(), theme::radiusLarge);
  g.setColour(theme::stroke.withAlpha(0.95f));
  g.drawRoundedRectangle(bounds.toFloat().reduced(0.5f), theme::radiusLarge, 1.0f);

  const auto metrics = getHeaderMetrics();
  if (metrics.hasHeader) {
    if (eyebrow_.isNotEmpty()) {
      g.setColour(theme::textDim);
      g.setFont(theme::eyebrowFont(11.0f));
      g.drawText(eyebrow_, metrics.eyebrowBounds,
                 juce::Justification::centredLeft, false);
    }

    g.setColour(theme::textMain);
    g.setFont(theme::titleFont(metrics.hasSubtitle ? 25.0f : 23.0f));
    g.drawFittedText(title_, metrics.titleBounds, juce::Justification::centredLeft,
                     1);

    if (metrics.hasSubtitle) {
      g.setColour(theme::textDim);
      g.setFont(theme::labelFont(14.0f));
      g.drawFittedText(subtitle_, metrics.subtitleBounds,
                       juce::Justification::centredLeft, 1);
    }

    g.setColour(accent_.withAlpha(0.28f));
    g.fillRoundedRectangle(
        juce::Rectangle<float>(static_cast<float>(bounds.getX() + kCardPaddingX),
                               static_cast<float>(bounds.getY() + kCardPaddingY),
                               32.0f, 2.0f),
        1.0f);
  }
}

void SegmentedControl::setOptions(const juce::StringArray &options) {
  buttons_.clear();
  for (int index = 0; index < options.size(); ++index) {
    auto button = std::make_unique<juce::TextButton>(options[index]);
    button->setClickingTogglesState(false);
    button->onClick = [this, index]() {
      setSelectedIndex(index);
      if (onSelectionChanged)
        onSelectionChanged(index);
    };
    addAndMakeVisible(*button);
    buttons_.push_back(std::move(button));
  }
  setSelectedIndex(selectedIndex_);
  resized();
}

void SegmentedControl::setSelectedIndex(int index) {
  selectedIndex_ = juce::jlimit(0, juce::jmax(0, static_cast<int>(buttons_.size()) - 1),
                                index);
  for (int i = 0; i < static_cast<int>(buttons_.size()); ++i)
    buttons_[static_cast<size_t>(i)]->setToggleState(i == selectedIndex_,
                                                     juce::dontSendNotification);
}

void SegmentedControl::resized() {
  auto bounds = getLocalBounds();
  if (buttons_.empty())
    return;

  const auto width = bounds.getWidth() / static_cast<int>(buttons_.size());
  for (auto &button : buttons_)
    button->setBounds(bounds.removeFromLeft(width).reduced(2));
}

MacroDial::MacroDial(const juce::String &label, juce::Colour accent)
    : label_(label), accent_(accent) {
  slider_.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
  slider_.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
  slider_.setColour(juce::Slider::rotarySliderFillColourId, accent_);
  slider_.setColour(juce::Slider::rotarySliderOutlineColourId, theme::stroke);
  slider_.setColour(juce::Slider::thumbColourId, theme::textMain);
  addAndMakeVisible(slider_);
}

void MacroDial::setAccent(juce::Colour colour) {
  accent_ = colour;
  slider_.setColour(juce::Slider::rotarySliderFillColourId, accent_);
  repaint();
}

void MacroDial::resized() {
  auto bounds = getLocalBounds().reduced(10, 6);
  bounds.removeFromTop(16);
  bounds.removeFromBottom(18);

  const auto sliderSize =
      juce::jmax(54, juce::jmin(bounds.getWidth(), bounds.getHeight()) - 14);
  slider_.setBounds(juce::Rectangle<int>(sliderSize, sliderSize)
                        .withCentre(bounds.getCentre()));
}

void MacroDial::paint(juce::Graphics &g) {
  auto bounds = getLocalBounds().reduced(8, 4);
  g.setColour(theme::textDim);
  g.setFont(theme::eyebrowFont(10.0f));
  g.drawFittedText(label_.toUpperCase(), bounds.removeFromTop(14),
                   juce::Justification::centred, 1);

  g.setColour(theme::textMain);
  g.setFont(theme::titleFont(12.0f));
  g.drawFittedText(slider_.getTextFromValue(slider_.getValue()),
                   getLocalBounds().reduced(8, 0).removeFromBottom(16),
                   juce::Justification::centred, 1);
}

MeterBar::MeterBar(const juce::String &label) : label_(label) {}

void MeterBar::setLabel(const juce::String &label) {
  label_ = label;
  repaint();
}

void MeterBar::setValueText(const juce::String &valueText) {
  valueText_ = valueText;
  repaint();
}

void MeterBar::setNormalisedValue(float value) {
  normalisedValue_ = juce::jlimit(0.0f, 1.0f, value);
  repaint();
}

void MeterBar::setAccent(juce::Colour colour) {
  accent_ = colour;
  repaint();
}

void MeterBar::setBipolar(bool shouldBeBipolar) {
  bipolar_ = shouldBeBipolar;
  repaint();
}

void MeterBar::paint(juce::Graphics &g) {
  auto bounds = getLocalBounds().reduced(4);
  auto top = bounds.removeFromTop(26);
  auto labelBounds = top.removeFromLeft(top.getWidth() / 2).translated(0, 1);
  auto valueBounds = top.translated(0, 1);

  g.setColour(theme::textDim);
  g.setFont(theme::eyebrowFont(10.0f));
  g.drawFittedText(label_.toUpperCase(), labelBounds, juce::Justification::centredLeft,
                   1);
  g.setColour(theme::textMain);
  g.setFont(theme::labelFont(16.0f));
  g.drawFittedText(valueText_, valueBounds, juce::Justification::centredRight, 1);

  auto track = bounds.withHeight(10).withY(bounds.getCentreY() - 5);
  g.setColour(theme::meterTrack);
  g.fillRoundedRectangle(track.toFloat(), 5.0f);

  if (bipolar_) {
    g.setColour(theme::stroke.brighter(0.1f));
    g.drawVerticalLine(track.getCentreX(), static_cast<float>(track.getY()),
                       static_cast<float>(track.getBottom()));

    const auto halfWidth = track.getWidth() / 2.0f;
    const auto signedValue = normalisedValue_ * 2.0f - 1.0f;
    if (signedValue >= 0.0f) {
      auto fill = track;
      fill.setX(track.getCentreX());
      fill.setWidth(static_cast<int>(halfWidth * signedValue));
      g.setColour(accent_);
      g.fillRoundedRectangle(fill.toFloat(), 5.0f);
    } else {
      auto fill = track;
      fill.setX(track.getCentreX() - static_cast<int>(halfWidth * -signedValue));
      fill.setWidth(static_cast<int>(halfWidth * -signedValue));
      g.setColour(accent_);
      g.fillRoundedRectangle(fill.toFloat(), 5.0f);
    }
    return;
  }

  auto fill = track;
  fill.setWidth(static_cast<int>(track.getWidth() * normalisedValue_));
  g.setColour(accent_);
  g.fillRoundedRectangle(fill.toFloat(), 5.0f);
}

void TonalBalanceView::setAccent(juce::Colour colour) {
  accent_ = colour;
  repaint();
}

void TonalBalanceView::setCurves(const std::array<float, 7> &actual,
                                 const std::array<float, 7> &target) {
  actual_ = actual;
  target_ = target;
  repaint();
}

void TonalBalanceView::paint(juce::Graphics &g) {
  auto bounds = getLocalBounds().reduced(14, 10).toFloat();

  g.setColour(theme::stroke.withAlpha(0.75f));
  for (int i = 1; i < 4; ++i) {
    const auto x = bounds.getX() + bounds.getWidth() * (static_cast<float>(i) / 4.0f);
    g.drawVerticalLine(static_cast<int>(x), bounds.getY(), bounds.getBottom());
  }
  g.drawHorizontalLine(static_cast<int>(bounds.getBottom() - 1), bounds.getX(),
                       bounds.getRight());

  auto buildPath = [&bounds](const std::array<float, 7> &values) {
    juce::Path path;
    for (size_t i = 0; i < values.size(); ++i) {
      const auto x = bounds.getX() +
                     bounds.getWidth() *
                         (static_cast<float>(i) / static_cast<float>(values.size() - 1));
      const auto y = bounds.getCentreY() - values[i] * 90.0f;
      if (i == 0)
        path.startNewSubPath(x, y);
      else
        path.lineTo(x, y);
    }
    return path;
  };

  auto targetPath = buildPath(target_);
  auto actualPath = buildPath(actual_);

  g.setColour(theme::textDim.withAlpha(0.35f));
  g.strokePath(targetPath, juce::PathStrokeType(12.0f, juce::PathStrokeType::curved,
                                                juce::PathStrokeType::rounded));
  g.setColour(accent_.withAlpha(0.9f));
  g.strokePath(actualPath, juce::PathStrokeType(2.2f, juce::PathStrokeType::curved,
                                                juce::PathStrokeType::rounded));
}

void StatusChip::setText(const juce::String &text) {
  text_ = text;
  repaint();
}

void StatusChip::setAccent(juce::Colour colour) {
  accent_ = colour;
  repaint();
}

void StatusChip::setActive(bool isActive) {
  active_ = isActive;
  repaint();
}

void StatusChip::paint(juce::Graphics &g) {
  auto bounds = getLocalBounds().toFloat();
  g.setColour(active_ ? accent_.withAlpha(0.16f) : theme::panel2);
  g.fillRoundedRectangle(bounds, theme::radiusSmall);
  g.setColour(active_ ? accent_.withAlpha(0.65f) : theme::stroke);
  g.drawRoundedRectangle(bounds.reduced(0.5f), theme::radiusSmall, 1.0f);
  g.setColour(active_ ? theme::textMain : theme::textDim);
  g.setFont(theme::labelFont(14.0f));
  g.drawFittedText(text_, getLocalBounds().reduced(10, 2), juce::Justification::centred,
                   1);
}

void AssistantSummaryChip::setHeading(const juce::String &text) {
  heading_ = text;
  repaint();
}

void AssistantSummaryChip::setText(const juce::String &text) {
  text_ = text;
  repaint();
}

void AssistantSummaryChip::setAccent(juce::Colour colour) {
  accent_ = colour;
  repaint();
}

void AssistantSummaryChip::paint(juce::Graphics &g) {
  auto bounds = getLocalBounds().toFloat();
  g.setColour(theme::panel2.brighter(0.02f));
  g.fillRoundedRectangle(bounds, theme::radiusMedium);
  g.setColour(accent_.withAlpha(0.25f));
  g.drawRoundedRectangle(bounds.reduced(0.5f), theme::radiusMedium, 1.0f);
  g.setColour(theme::textDim);
  g.setFont(theme::eyebrowFont(10.0f));
  g.drawFittedText(heading_, getLocalBounds().removeFromTop(16).reduced(9, 0),
                   juce::Justification::centredLeft, 1);
  g.setColour(theme::textMain);
  g.setFont(theme::labelFont(13.0f));
  g.drawFittedText(text_, getLocalBounds().reduced(9, 8).withTrimmedTop(10),
                   juce::Justification::topLeft, 4);
}

} // namespace flow88::ui
