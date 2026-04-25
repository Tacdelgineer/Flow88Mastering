#include "flow88/eval/FlowPresetEvaluation.h"
#include "flow88/state/FlowProfiles.h"
#include <iostream>

namespace {

juce::String buildUsage() {
  return "Flow88MasterEval --input <file.wav> [--input <file2.wav> ...]\n"
         "                 [--dir <folder>] --output <report.json|report.csv>\n"
         "                 [--format json|csv] [--subgenre house]\n"
         "                 [--style clean] [--mode raw|preset]\n"
         "                 [--target -9.0] [--block-size 1024]\n";
}

bool isAudioExtension(const juce::File &file) {
  const auto extension = file.getFileExtension().toLowerCase();
  return extension == ".wav" || extension == ".aif" || extension == ".aiff" ||
         extension == ".flac";
}

} // namespace

int main(int argc, char *argv[]) {
  juce::Array<juce::File> inputFiles;
  juce::File inputDirectory;
  juce::File outputFile;
  juce::String format = "json";
  flow88::eval::PresetEvaluationOptions options;

  for (int index = 1; index < argc; ++index) {
    const juce::String argument(argv[index]);
    const auto nextValue = [&]() -> juce::String {
      if (index + 1 < argc)
        return juce::String(argv[++index]);
      return {};
    };

    if (argument == "--input") {
      inputFiles.add(juce::File(nextValue()));
    } else if (argument == "--dir") {
      inputDirectory = juce::File(nextValue());
    } else if (argument == "--output") {
      outputFile = juce::File(nextValue());
    } else if (argument == "--format") {
      format = nextValue().toLowerCase();
    } else if (argument == "--subgenre") {
      options.subgenreId = nextValue();
    } else if (argument == "--style") {
      options.styleId = nextValue();
    } else if (argument == "--mode") {
      options.applyPreset = nextValue().equalsIgnoreCase("preset");
    } else if (argument == "--target") {
      options.exactTargetLufs = nextValue().getFloatValue();
    } else if (argument == "--block-size") {
      options.blockSize = juce::jmax(256, nextValue().getIntValue());
    } else if (argument == "--help" || argument == "-h") {
      std::cout << buildUsage();
      return 0;
    } else {
      std::cerr << "Unknown argument: " << argument << "\n" << buildUsage();
      return 1;
    }
  }

  if (inputDirectory.isDirectory()) {
    juce::Array<juce::File> directoryFiles;
    inputDirectory.findChildFiles(directoryFiles, juce::File::findFiles, false);
    for (const auto &file : directoryFiles)
      if (isAudioExtension(file))
        inputFiles.addIfNotAlreadyThere(file);
  }

  if (inputFiles.isEmpty() || outputFile.getFullPathName().isEmpty()) {
    std::cerr << "Missing input or output.\n" << buildUsage();
    return 1;
  }

  flow88::state::ProfileLibrary profileLibrary;
  std::vector<flow88::eval::EvaluationReport> reports;

  for (const auto &file : inputFiles) {
    flow88::eval::EvaluationReport report;
    juce::String errorMessage;
    if (!flow88::eval::evaluateFile(file, profileLibrary, options, report,
                                    errorMessage)) {
      std::cerr << "Failed to evaluate " << file.getFullPathName() << ": "
                << errorMessage << "\n";
      return 1;
    }

    reports.push_back(std::move(report));
  }

  const auto reportText = format.equalsIgnoreCase("csv")
                              ? flow88::eval::reportToCsv(reports)
                              : flow88::eval::reportToJson(reports);

  if (!outputFile.replaceWithText(reportText)) {
    std::cerr << "Failed to write report to " << outputFile.getFullPathName()
              << "\n";
    return 1;
  }

  std::cout << "Wrote " << reports.size() << " evaluation report(s) to "
            << outputFile.getFullPathName() << "\n";
  return 0;
}
