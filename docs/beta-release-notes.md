# Flow88 Master v0.1.0-beta

Windows x64 VST3 beta. The release workflow requires a clean Release build,
Steinberg VST3 Validator, and processor plus loaded-VST3 audio acceptance checks
before it packages or publishes. See the ZIP's evidence directory for raw logs.

DAW compatibility is unverified: no Ableton Live or other DAW session has been
validated. Automated plugin-host checks are not DAW testing. macOS, Windows ARM64,
and Linux distribution compatibility are untested.

Known limitations: the limiter is an instantaneous gain limiter, not a mature
lookahead mastering limiter; the loudness target drives a heuristic gain stage,
not automatic normalization. A/B level matching is estimated. Oversampling mode
changes reset filters and alter reported latency; stop transport before changing
modes. Bypass preserves gain and the selected mode's delay, but seamless click-free
switching has not been established. Reference files require their original path
when reopening sessions. This is experimental software, not a flawless mastering
or broadcast compliance tool.
