# Flow88 Master Architecture

## Overview

Flow88 Master is structured around a small, opinionated mastering workflow:

1. Read incoming stereo audio safely in realtime
2. Measure loudness, peaks, spectral balance, and stereo behavior
3. Apply a broad musical mastering chain
4. Present profile-aware controls in a restrained one-screen UI
5. Support offline evaluation so preset changes can be validated instead of guessed

The project is split so DSP, state, UI, and validation can evolve without collapsing into one monolithic processor file.

## Major Subsystems

### `PluginProcessor`

`Source/PluginProcessor.*`

Responsibilities:

- Owns the `AudioProcessorValueTreeState`
- Owns session/UI state
- Owns the built-in profile library
- Applies profile selections and profile-aware DSP tuning
- Runs the realtime mastering chain
- Publishes analyzer and stage status data for the UI
- Manages A/B snapshot recall
- Orchestrates background reference-track analysis

Important design rule:

- audio-thread work stays deterministic and lock-free
- background file analysis is kept off the audio thread

### `flow88::state`

`Source/flow88/state/`

Responsibilities:

- Declares parameter IDs and choice lists
- Builds the APVTS parameter layout
- Defines built-in profile schema
- Loads profile JSON from embedded binary data
- Composes subgenre + style into an `EffectiveProfile`
- Stores non-automatable UI/session state
- Serializes plugin state cleanly

Key split:

- APVTS: automatable DSP-facing state
- `FlowSessionState`: UI/session/reference/A-B state

### `flow88::dsp`

`Source/flow88/dsp/`

Contains two primary pieces:

- `FlowMeterSource`
  - realtime analyzer bridge for:
    - input/output peaks
    - LUFS
    - true peak
    - stereo width/correlation
    - tonal balance curve
- `FlowMasteringChain`
  - broad mastering stages:
    - tone shaping
    - low-end anchoring
    - glue dynamics
    - stereo width
    - saturation
    - clipper
    - limiter

The mastering chain is intentionally broad-band and musical. It is not trying to replicate a full surgical mastering suite.

### `flow88::eval`

`Source/flow88/eval/`

Purpose:

- offline measurement for rendered files
- scoring of preset outcomes
- JSON / CSV reporting
- validation support for tuning and regression checks

Core types:

- `OfflineAnalysisEngine`
- `AnalysisMetrics`
- `EvaluationScores`
- `EvaluationReport`
- `PresetEvaluationOptions`

This layer is how Flow88 avoids tuning presets blindly.

### `flow88::ui`

`Source/flow88/ui/`

Responsibilities:

- theme constants
- custom look-and-feel
- reusable widgets
- cards, segmented controls, macro dials, meters, chips, tonal display

The UI is intentionally stable. Work here should generally polish and clarify, not redesign the plugin.

## Signal Flow

Current realtime order:

1. Input trim
2. Tone shaping
3. Low-end focus / stereo width shaping
4. Glue dynamics
5. Saturation
6. Soft clipper
7. True-peak-safe limiter
8. Output trim
9. Meter publication

Supporting rules:

- parameter changes are smoothed where needed
- nonlinear stages can run oversampled
- stage activity is exposed back to the diagnostics overlay

## Profile System

Profiles are embedded JSON, not external runtime dependencies.

Subgenre profiles define:

- human-facing summary
- default loudness target
- macro defaults
- advanced defaults
- analyzer targets
- DSP tuning bias

Style profiles define:

- summary suffix
- macro deltas
- advanced deltas
- DSP tuning deltas

Composition result:

- `EffectiveProfile`

That object is the bridge between preset intent and actual processor behavior.

## Reference Analysis

Reference import is file-based and background-driven.

Flow:

1. User selects a file in the editor
2. Processor stores the path in session state
3. A background thread-pool job analyzes the file
4. Metrics are stored into a thread-safe snapshot
5. UI reads the snapshot and shows restrained delta guidance

Reference mode currently informs:

- tonal target display
- loudness delta
- true peak delta
- stereo width delta
- correlation delta

Reference mode does not yet auto-match the live signal.

## Validation Strategy

The repo includes two complementary validation paths:

### Realtime validation

- plugin meters
- diagnostics overlay
- stage activity flags
- before/after style inspection in the UI

### Offline validation

- `Flow88MasterEval`
- unit tests around analyzers and preset processing
- JSON/CSV reports that can be compared over time

This is the core of the v2 direction: make preset changes measurable and reviewable.

## Build Targets

Main targets in `CMakeLists.txt`:

- `Flow88MasterAssets`
- `Flow88MasterCore`
- `Flow88MasterUI`
- `Flow88Master`
- `Flow88MasterValidation`
- `Flow88MasterEval`
- `Flow88MasterTests`

## Constraints To Preserve

- Windows-first desktop plugin
- VST3 only
- JUCE / CMake
- no external AI runtime
- no blocking on the audio thread
- no UI redesign unless explicitly requested
- defaults should stay conservative and premium
- kick/bass clarity and true-peak safety matter more than aggressive loudness

## Near-Term Priorities

- deepen preset validation using real render sets
- keep tightening loudness translation
- improve limiter maturity
- add true level-matched A/B
- continue polishing diagnostics and UI alignment in-host
