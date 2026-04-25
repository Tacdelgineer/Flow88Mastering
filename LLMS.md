# Flow88 Master LLM Context

This file is for coding agents and future automated contributors. Read it before making architectural or UI changes.

## Mission

Build and refine a Windows-only JUCE VST3 mastering plugin for electronic music that feels focused, premium, and trustworthy.

The project is not trying to be an all-purpose mastering workstation. Favor clarity, translation, and safe musical moves over feature breadth.

## Product Rules

- Target DAW workflow: Ableton insert on master or premaster bus
- Genres: House, Techno, Melodic, Trance, Progressive
- Styles: Clean, Club, Warm
- Dark-only UI
- One-screen primary layout should remain intact
- Processing must stay fully local, deterministic, and realtime safe
- Do not introduce external AI runtimes or cloud dependencies

## Engineering Rules

- Never block the audio thread
- Avoid allocations in `processBlock`
- Prefer broad musical DSP over surgical complexity
- Preserve kick and bass clarity
- Protect the low end from unsafe widening
- Respect safe true-peak ceiling behavior
- Defaults should sound conservative and premium, not hyped or crushed

## Current State

Implemented:

- realtime analyzers
- first audible mastering chain
- built-in profile composition
- reference-track import and background analysis
- diagnostics overlay
- offline evaluation/scoring CLI

Not fully finished:

- true level-matched A/B
- auto reference matching
- mature lookahead mastering limiter
- deep multiband / surgical processing

## Code Map

- `Source/PluginProcessor.*`
  - processor orchestration, profile application, reference jobs, display-state publishing
- `Source/PluginEditor.*`
  - editor shell, diagnostics overlay, advanced panel, reference chooser
- `Source/flow88/state/`
  - parameters, profile schema, session state, serialization
- `Source/flow88/dsp/`
  - analyzers and mastering chain
- `Source/flow88/eval/`
  - offline analysis and preset evaluation
- `Source/flow88/ui/`
  - theme and reusable widgets

## State Model

- APVTS holds automatable DSP-facing parameters
- `FlowSessionState` holds:
  - analysis state
  - suggested profile/style
  - assistant summary
  - active A/B slot and snapshots
  - advanced-panel state
  - reference path/status

If you add state, decide carefully whether it belongs in APVTS or session state.

## Profile Model

Profiles are composed from:

- one subgenre JSON
- one style JSON

Schema includes:

- macro defaults / deltas
- advanced defaults / deltas
- analysis targets
- DSP tuning biases

If you change macro behavior, update both:

- profile composition logic
- evaluation expectations and tests

## Validation Workflow

Use both:

1. Realtime validation
   - diagnostics overlay
   - live meter movement
   - stage activity flags
2. Offline validation
   - `Flow88MasterEval`
   - JSON/CSV reports
   - unit tests

Do not tune presets by ear alone if the repo already has a measurable way to compare outcomes.

## Build And Test

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -DJUCE_DIR="M:/ProjectsM/DeterministicMelody/JUCE"
cmake --build build --config Release --parallel 6
ctest --test-dir build -C Release --output-on-failure
```

Offline eval example:

```powershell
build\Flow88MasterEval_artefacts\Release\Flow88MasterEval.exe `
  --input path\to\render.wav `
  --output build\report.json `
  --subgenre house `
  --style clean `
  --mode preset
```

## When Editing

- Prefer extending the existing layout over redesigning it
- Keep wording restrained and credible
- Keep diagnostics honest: if a stage is heuristic or placeholder, say so
- Update tests when adding DSP behavior or profile schema fields
- Update `README.md` and `docs/architecture.md` when the repo's mental model changes

## Good Next Steps

- improve limiter maturity
- strengthen preset evaluation with more real-world renders
- add true level-matched A/B
- keep refining translation instead of adding unrelated features
