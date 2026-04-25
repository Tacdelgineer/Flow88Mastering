#pragma once

#include "flow88/FlowJuceIncludes.h"

namespace flow88::ui::theme {

inline const juce::Colour bg = juce::Colour::fromString("ff071018");
inline const juce::Colour panel = juce::Colour::fromString("ff0d1722");
inline const juce::Colour panel2 = juce::Colour::fromString("ff13202d");
inline const juce::Colour cyan = juce::Colour::fromString("ff4fd6ff");
inline const juce::Colour teal = juce::Colour::fromString("ff16b8c9");
inline const juce::Colour blue = juce::Colour::fromString("ff2c7dff");
inline const juce::Colour violet = juce::Colour::fromString("ff8b5cff");
inline const juce::Colour amber = juce::Colour::fromString("ffc98a52");
inline const juce::Colour textMain = juce::Colour::fromString("ffeaf4ff");
inline const juce::Colour textDim = juce::Colour::fromString("ff8fa4b8");
inline const juce::Colour stroke = juce::Colour::fromString("ff223344");
inline const juce::Colour meterTrack = juce::Colour::fromString("ff21303f");
inline const juce::Colour focusGlow = cyan.withAlpha(0.18f);

inline constexpr float radiusLarge = 24.0f;
inline constexpr float radiusMedium = 18.0f;
inline constexpr float radiusSmall = 12.0f;
inline constexpr int gap = 16;
inline constexpr int gapSmall = 10;
inline constexpr int headerHeight = 72;
inline constexpr int meterDockHeight = 126;

inline juce::Font titleFont(float size) {
  return juce::Font(juce::FontOptions(size, juce::Font::bold));
}

inline juce::Font labelFont(float size) {
  return juce::Font(juce::FontOptions(size, juce::Font::plain));
}

inline juce::Font eyebrowFont(float size) {
  auto font = juce::Font(juce::FontOptions(size, juce::Font::bold));
  font.setExtraKerningFactor(0.14f);
  return font;
}

inline juce::Colour cardFill(bool elevated = false) {
  return elevated ? panel2 : panel;
}

inline void fillPanelBackground(juce::Graphics &g, juce::Rectangle<int> bounds) {
  juce::ColourGradient gradient(panel.brighter(0.05f), 0.0f, 0.0f,
                                bg.darker(0.25f), 0.0f,
                                static_cast<float>(bounds.getBottom()), false);
  g.setGradientFill(gradient);
  g.fillRoundedRectangle(bounds.toFloat(), radiusLarge);
  g.setColour(stroke.withAlpha(0.9f));
  g.drawRoundedRectangle(bounds.toFloat().reduced(0.5f), radiusLarge, 1.0f);
}

} // namespace flow88::ui::theme
