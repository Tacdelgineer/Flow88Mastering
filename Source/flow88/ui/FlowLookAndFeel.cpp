#include "FlowLookAndFeel.h"

namespace flow88::ui {

FlowLookAndFeel::FlowLookAndFeel() {
  setColour(juce::TextButton::buttonColourId, theme::panel2);
  setColour(juce::TextButton::buttonOnColourId, theme::cyan);
  setColour(juce::TextButton::textColourOffId, theme::textMain);
  setColour(juce::TextButton::textColourOnId, theme::bg);
  setColour(juce::Slider::rotarySliderOutlineColourId, theme::stroke);
  setColour(juce::Slider::rotarySliderFillColourId, theme::cyan);
  setColour(juce::Slider::thumbColourId, theme::textMain);
  setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
  setColour(juce::Slider::textBoxBackgroundColourId, theme::panel2);
  setColour(juce::Slider::textBoxTextColourId, theme::textMain);
}

void FlowLookAndFeel::drawButtonBackground(
    juce::Graphics &g, juce::Button &button, const juce::Colour &backgroundColour,
    bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) {
  auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
  auto fill = backgroundColour;

  if (button.getToggleState())
    fill = theme::cyan.withAlpha(0.88f);
  else if (shouldDrawButtonAsDown)
    fill = fill.brighter(0.08f);
  else if (shouldDrawButtonAsHighlighted)
    fill = fill.brighter(0.04f);

  g.setColour(fill);
  g.fillRoundedRectangle(bounds, theme::radiusSmall);

  if (!button.getToggleState()) {
    g.setColour(theme::stroke.withAlpha(0.95f));
    g.drawRoundedRectangle(bounds, theme::radiusSmall, 1.0f);
  }
}

void FlowLookAndFeel::drawButtonText(juce::Graphics &g, juce::TextButton &button,
                                     bool, bool) {
  g.setColour(button.findColour(button.getToggleState()
                                    ? juce::TextButton::textColourOnId
                                    : juce::TextButton::textColourOffId));
  g.setFont(theme::labelFont(16.0f));
  g.drawText(button.getButtonText(), button.getLocalBounds(),
             juce::Justification::centred, true);
}

void FlowLookAndFeel::drawToggleButton(juce::Graphics &g, juce::ToggleButton &button,
                                       bool shouldDrawButtonAsHighlighted,
                                       bool shouldDrawButtonAsDown) {
  auto bounds = button.getLocalBounds().toFloat();
  auto pill = bounds.removeFromLeft(58.0f).reduced(2.0f, 8.0f);
  auto isOn = button.getToggleState();

  g.setColour(isOn ? theme::cyan.withAlpha(0.88f) : theme::panel2);
  g.fillRoundedRectangle(pill, pill.getHeight() * 0.5f);
  g.setColour(theme::stroke);
  g.drawRoundedRectangle(pill, pill.getHeight() * 0.5f, 1.0f);

  auto thumb = pill.reduced(4.0f);
  thumb.setWidth(thumb.getHeight());
  thumb.setX(isOn ? pill.getRight() - thumb.getWidth() - 4.0f : pill.getX() + 4.0f);
  g.setColour(isOn ? theme::bg : theme::textDim);
  g.fillEllipse(thumb);

  g.setColour(theme::textMain.withAlpha(shouldDrawButtonAsDown ? 0.95f : 1.0f));
  g.setFont(theme::labelFont(15.0f));
  g.drawText(button.getButtonText(), button.getLocalBounds().withTrimmedLeft(68), 
             juce::Justification::centredLeft, true);
}

void FlowLookAndFeel::drawRotarySlider(juce::Graphics &g, int x, int y, int width,
                                       int height, float sliderPosProportional,
                                       float rotaryStartAngle,
                                       float rotaryEndAngle,
                                       juce::Slider &slider) {
  auto bounds =
      juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y),
                             static_cast<float>(width), static_cast<float>(height))
          .reduced(10.0f);
  const auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
  const auto centre = bounds.getCentre();
  const auto ringBounds = bounds.withSizeKeepingCentre(radius * 2.0f, radius * 2.0f);
  const auto endAngle =
      rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

  g.setColour(theme::panel2.brighter(0.04f));
  g.fillEllipse(bounds);

  juce::Path baseArc;
  baseArc.addCentredArc(centre.x, centre.y, radius, radius, 0.0f, rotaryStartAngle,
                        rotaryEndAngle, true);
  g.setColour(theme::stroke.withAlpha(0.9f));
  g.strokePath(baseArc, juce::PathStrokeType(4.0f, juce::PathStrokeType::curved,
                                             juce::PathStrokeType::rounded));

  juce::Path valueArc;
  valueArc.addCentredArc(centre.x, centre.y, radius, radius, 0.0f, rotaryStartAngle,
                         endAngle, true);
  g.setColour(slider.findColour(juce::Slider::rotarySliderFillColourId)
                  .withAlpha(slider.isMouseOverOrDragging() ? 0.95f : 0.8f));
  g.strokePath(valueArc, juce::PathStrokeType(4.4f, juce::PathStrokeType::curved,
                                              juce::PathStrokeType::rounded));

  const auto thumbPoint =
      juce::Point<float>(centre.x + std::cos(endAngle - juce::MathConstants<float>::halfPi) *
                                        (radius - 3.0f),
                         centre.y + std::sin(endAngle - juce::MathConstants<float>::halfPi) *
                                        (radius - 3.0f));
  g.setColour(theme::textMain.withAlpha(0.95f));
  g.fillEllipse(thumbPoint.x - 3.0f, thumbPoint.y - 3.0f, 6.0f, 6.0f);

  g.setColour(theme::stroke.withAlpha(0.8f));
  g.drawEllipse(ringBounds, 1.0f);
}

void FlowLookAndFeel::drawLinearSlider(juce::Graphics &g, int x, int y, int width,
                                       int height, float sliderPos, float,
                                       float, const juce::Slider::SliderStyle style,
                                       juce::Slider &slider) {
  auto bounds =
      juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y),
                             static_cast<float>(width), static_cast<float>(height))
          .reduced(4.0f);

  if (style == juce::Slider::LinearHorizontal ||
      style == juce::Slider::LinearBar ||
      style == juce::Slider::LinearBarVertical) {
    const auto track = bounds.withHeight(6.0f).withCentre(bounds.getCentre());
    g.setColour(theme::meterTrack);
    g.fillRoundedRectangle(track, 3.0f);

    auto fill = track;
    fill.setRight(sliderPos);
    g.setColour(slider.findColour(juce::Slider::rotarySliderFillColourId));
    g.fillRoundedRectangle(fill, 3.0f);
    return;
  }

  auto track = bounds.withWidth(10.0f).withCentre(bounds.getCentre());
  g.setColour(theme::meterTrack);
  g.fillRoundedRectangle(track, 5.0f);

  auto fill = track;
  fill.setY(sliderPos);
  fill.setHeight(track.getBottom() - sliderPos);
  g.setColour(slider.findColour(juce::Slider::rotarySliderFillColourId));
  g.fillRoundedRectangle(fill, 5.0f);
}

} // namespace flow88::ui
