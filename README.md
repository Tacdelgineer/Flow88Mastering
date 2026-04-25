# Flow88 Mastering

**A calm, premium, and trustworthy mastering plugin tuned for electronic music.**

![Platform](https://img.shields.io/badge/Platform-Windows-0078D6?style=for-the-badge&logo=windows)
![Plugin](https://img.shields.io/badge/Plugin-VST3-8A2BE2?style=for-the-badge)
![Built%20With](https://img.shields.io/badge/Built%20With-JUCE%20%2B%20C%2B%2B-orange?style=for-the-badge)
![Status](https://img.shields.io/badge/Status-Public%20Release-brightgreen?style=for-the-badge)
![AI%20Assisted](https://img.shields.io/badge/Made%20With-AI%20Assisted-ff69b4?style=for-the-badge)
![Local%20First](https://img.shields.io/badge/Local%20First-Yes-black?style=for-the-badge)

![Flow88 Mastering Screenshot](media/screenshot.png)

![Flow88 Mastering Demo](media/demo.gif)

## What it is
Flow88 Master is a Windows-only VST3 mastering plugin designed for the modern electronic music producer. Built around an Ableton-style insert workflow, it provides a unified, single-screen mastering chain targeted specifically at House, Techno, Melodic, Trance, and Progressive material. It combines real-time analyzer-driven metering with broad, musical DSP controls—allowing you to dial in your master bus with confidence.

## Why I built it
I built Flow88 because mastering shouldn't feel like navigating a spaceship control panel. I wanted a tool that feels calm, premium, and trustworthy, focusing on broad musical moves and safe defaults rather than overloaded features. v2 is centered on analysis quality, preset trust, translation, and refinement, giving you exactly what you need to finalize your tracks without the guesswork.

## Features
* **Analyzer-Driven Metering**: Real-time feedback for input/output peak, LUFS, true peak, stereo width/correlation, and tonal balance.
* **Unified Mastering Chain**: Integrated tone shaping, low-end focus, glue dynamics, width control, saturation, clipper, and limiter.
* **Genre-Aware Profiles**: Built-in subgenre and style profiles tuned for electronic music, giving you immediate starting points.
* **Reference Track Import**: Load your favorite reference tracks for restrained, advisory delta guidance.
* **Level-Matched A/B**: Instant, level-matched snapshot recall for unbiased comparison.
* **Premium Interface**: A calm, dark, single-screen UI shell with an optional engineering overlay for deep validation.

## Non-programmer Install Guide
Installing Flow88 Mastering is easy! You do not need to know how to code.

1. Download the latest release from the [Releases page](#).
2. Unzip the downloaded folder.
3. Double-click the included `install.bat` file.
   - *This script simply copies the plugin into your standard Windows VST3 folder (`C:\Program Files\Common Files\VST3`).*
4. Open your DAW (like Ableton Live, FL Studio, etc.) and rescan your VST3 plugins.
5. Load Flow88 Master onto your master bus and enjoy!

## Developer Build Instructions
If you want to compile Flow88 Master from the source code, follow these steps:

### Requirements
- Windows 10/11
- CMake 3.22+
- Visual Studio 2022 (with "Desktop development with C++" workload)
- JUCE 8.0.12 (You can place it in a local `JUCE/` folder or set the `JUCE_DIR` environment variable).

### Building
You can use the included `build.bat` script for a hassle-free build:
1. Open a terminal or command prompt.
2. Run `build.bat`.

Alternatively, build manually via CMake:
```powershell
cmake -S . -B build -G "Visual Studio 17 2022"
cmake --build build --config Release --parallel
```
The compiled plugin will be located at `build/Flow88Master_artefacts/Release/VST3/Flow88 Master.vst3`.

## YouTube Showcase
Check out the YouTube Showcase (link coming soon) for a full walkthrough of Flow88 Mastering in action, where I demonstrate the workflow, presets, and how to get the most out of the plugin.

## Known Limitations
* The limiter is highly capable but not yet a final long-lookahead mastering limiter.
* Reference guidance is advisory and manual, not fully automatic.
* Windows only (no macOS / AU support).

## Roadmap
* Automatic reference matching.
* A fully mature, dedicated mastering-grade lookahead limiter.
* Expanded preset validation with larger curated render sets.

## License
MIT License
