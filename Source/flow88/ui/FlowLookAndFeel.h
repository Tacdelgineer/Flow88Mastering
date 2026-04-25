#pragma once

#include "FlowTheme.h"

namespace flow88::ui {

class FlowLookAndFeel : public juce::LookAndFeel_V4 {
public:
  FlowLookAndFeel();

  void drawButtonBackground(juce::Graphics &, juce::Button &,
                            const juce::Colour &backgroundColour,
                            bool shouldDrawButtonAsHighlighted,
                            bool shouldDrawButtonAsDown) override;
  void drawButtonText(juce::Graphics &, juce::TextButton &,
                      bool shouldDrawButtonAsHighlighted,
                      bool shouldDrawButtonAsDown) override;
  void drawToggleButton(juce::Graphics &, juce::ToggleButton &,
                        bool shouldDrawButtonAsHighlighted,
                        bool shouldDrawButtonAsDown) override;
  void drawRotarySlider(juce::Graphics &, int x, int y, int width, int height,
                        float sliderPosProportional, float rotaryStartAngle,
                        float rotaryEndAngle, juce::Slider &) override;
  void drawLinearSlider(juce::Graphics &, int x, int y, int width, int height,
                        float sliderPos, float minSliderPos, float maxSliderPos,
                        const juce::Slider::SliderStyle, juce::Slider &) override;
};

} // namespace flow88::ui

