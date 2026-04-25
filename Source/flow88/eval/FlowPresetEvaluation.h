#pragma once

#include "FlowOfflineAnalysis.h"
#include <limits>

namespace flow88::eval {

struct PresetEvaluationOptions {
  juce::String subgenreId = "house";
  juce::String styleId = "clean";
  int blockSize = 1024;
  bool applyPreset = false;
  float exactTargetLufs = std::numeric_limits<float>::quiet_NaN();
};

bool evaluateFile(const juce::File &file, const state::ProfileLibrary &profileLibrary,
                  const PresetEvaluationOptions &options,
                  EvaluationReport &report, juce::String &errorMessage);

} // namespace flow88::eval
