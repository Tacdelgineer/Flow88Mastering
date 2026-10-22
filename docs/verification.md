# Windows VST3 beta verification

Release decision: **blocked**. No release or tag has been created. Essential
limiter / true-peak stress checks fail. Windows CI is still being evaluated;
this document will record the completed run before the investigation closes.

## Build investigation

All four Windows runs on April 25, 2026 failed:

| Run | Commit |
| --- | --- |
| [24919402750](https://github.com/Tacdelgineer/Flow88Mastering/actions/runs/24919402750) | `3596b3a` |
| [24919554700](https://github.com/Tacdelgineer/Flow88Mastering/actions/runs/24919554700) | `5fce19f` |
| [24919609118](https://github.com/Tacdelgineer/Flow88Mastering/actions/runs/24919609118) | `e1ac112` |
| [24919620338](https://github.com/Tacdelgineer/Flow88Mastering/actions/runs/24919620338) | `a29ff1d` |

Their logs have expired (HTTP 410). Retained annotations report only exit code 1
and a Node 20 deprecation warning; they do not establish the exact failed command.
See [retained annotations](verification/april-25-runs.json).

The original tree declares JUCE in `.gitmodules` but has no JUCE gitlink. A fresh
source extraction reproduces `JUCE not found` at configuration; see the
[reproduction log](verification/baseline-missing-juce.log). The fix commits the
JUCE 8.0.12 gitlink at `29396c22c93392d6738e021b83196283d6e4d850`.

A new [Windows run](https://github.com/Tacdelgineer/Flow88Mastering/actions/runs/39719076364)
also demonstrated runner drift: `windows-latest` selected Windows Server 2025
with Visual Studio 2026, and CMake could not find the requested VS 2022 generator.
The repaired workflow uses `windows-2022`, VS 2022, x64, and Release. It checks out
the exact PR head, pins action revisions and Steinberg SDK 3.8.0 build 66, saves
logs/JUnit evidence, and prevents packaging/publication after failed checks.

## Focused processor repairs

- Current APVTS state is authoritative on reopen; stale active A/B snapshots no
  longer overwrite the latest knob/automation values. Saving updates that snapshot.
- Bypass omits input/output trims and A/B level correction, exposes the native
  bypass parameter to hosts, and preserves the reported dry delay.
- Integer oversampling latency is compensated to one fixed delay across 1x, 2x,
  4x and bypass. Latency notifications occur during preparation, outside processing.
- Host buffers larger than the JUCE prepare hint are split using preallocated DSP
  storage. The VST3 wrapper still requires hosts to honor its negotiated maximum.
- Static libraries use position-independent code so a Linux test VST3 can link.
  No Linux distribution compatibility claim is made.

## Local evidence

Ubuntu x64, GCC 13.3.0, CMake 3.28.3, Ninja, JUCE 8.0.12, Release. Local source and
loaded-bundle tests are automated host tests, not DAW validation.

The build and existing test suite pass. Steinberg Validator 3.8.0 build 66 reports
47 tests passed, zero failed. State, lifecycle/editor open-close, DSP controls,
bypass and fixed latency tests pass. The final corrected matrix is being rerun.
The matrix spans 44.1/48/88.2/96/192 kHz, blocks 1/17/64/257/512/1024, and all
three oversampling modes (90 combinations), plus zero and large variable blocks.

During test development, hosted parameter access was corrected to use normalized
host parameters. Low-end controls are tested using bass rather than a treble-only
signal. An invalid VST3 test call of 2049 samples after negotiating a maximum of
64 was traced to JUCE wrapper scratch-buffer overflow before the processor ran;
the corrected VST3 case negotiates 2049 while the direct processor still tests
blocks above its hint. These test errors are distinct from the limiter failures.

## Release-blocking audio defect

The limiter acts before the oversampling down-filter and does not bound final
reconstructed output peaks. The existing fractional-dB safety margin is
insufficient for strongly driven high-frequency material. Disabling oversampling
also leaves intersample peaks uncontrolled. Changing delay reporting does not
resolve this existing limiter design limitation.

The real processor and loaded VST3 both fail clipping/extreme-value and independent
true-peak reconstruction checks. At all-maximum controls, 48 kHz / 64 samples,
the 2x case reaches approximately +3.60 dBFS sample peak. At a -1 dBTP ceiling,
+12 dB input/output trims, full attitude/clipper and high-frequency stereo stress,
the independent 16x, 64-tap Hann-windowed sinc reconstruction reaches about
+5.64 dBTP in 1x mode, +2.8 dBTP in 2x, and +1.85 dBTP in 4x. The measurement is
an engineering stress check, not a certified BS.1770 compliance measurement.

The failing checks remain enabled. No larger arbitrary margin, reduced test
level, skip, or clipping patch has been used to obtain a pass. A final-output
true-peak limiter needs a separately designed and validated repair before beta
publication. This investigation preserves the existing mastering stages.

## Compatibility and remaining limits

No Ableton Live installation was available on the connected Linux machine; no
Windows desktop device was connected. No DAW was tested. Ableton, all other DAWs,
macOS, Windows ARM64 and Linux distribution compatibility are unverified. CI uses
Windows Server; Windows 10/11 desktop installation and listening tests remain
unverified. Oversampling and bypass switching are not established as click-free.
Reference imports, long sessions, hostile state inputs and exhaustive parameter
automation are not comprehensively covered by these acceptance checks.

## Reproduce on Windows

```powershell
git clone --recurse-submodules https://github.com/Tacdelgineer/Flow88Mastering.git
cd Flow88Mastering
git checkout fix/windows-vst3-beta
git submodule update --init --recursive
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DBUILD_TESTING=ON
cmake --build build --config Release --parallel 4
ctest --test-dir build -C Release --output-on-failure
```

See `.github/workflows/windows-build.yml` for the pinned Steinberg validator build
and invocation. Packaging runs only after every check passes. The tag-triggered
release workflow reuses the same gate and publishes only a prerelease ZIP with
the complete Windows x64 bundle, install instructions and verification evidence.
