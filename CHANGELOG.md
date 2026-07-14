# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog 1.1.0](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [0.1.0] - 2026-07-14

### Added

- Project bootstrap: README, license, contributing guide, architecture and build docs, ADRs, and CI workflow.
- Full v1.0 `AudioProcessorValueTreeState` parameter layout (frozen parameter IDs) covering IO/global, noise gate, crossover, low band, high band, EQ, and IR loader.
- LR4 (Linkwitz-Riley 4th order) crossover band-split (`src/dsp/Crossover`), flat-sum tested, with a latency-compensation framework/seam in the processor.
- **DSP completion (M1):** the full v1.0 signal path is wired and live:
  - Full-band input noise gate (`src/dsp/NoiseGateStage`), off by default.
  - Low-band parallel ("New York style") compressor with makeup gain and wet/dry mix (`src/dsp/ParallelCompressor`).
  - High-band distortion engine (`src/dsp/Voicing`) with three selectable voicings — **Gnaw** (op-amp hard clip), **Wool** (cascaded soft-clip fuzz with mid scoop), **Razor** (tight overdrive: pre-clip highpass, soft clip, mid hump) — each running its nonlinear shaping stage 4x oversampled (FIR half-band equiripple) to control aliasing, with drive, tone, and clean/distorted blend controls.
  - Post-sum 4-band EQ (`src/dsp/BandEQ`: LowShelf / Peak / Peak / HighShelf), off by default.
  - Cab-sim IR loader (`src/dsp/IRLoader`, `juce::dsp::Convolution`-based), off by default, safe-by-default (bit-exact passthrough with no IR loaded, at every session sample rate); `loadImpulseResponse()` is the DSP-side seam a future GUI/preset system will call to load user or factory IRs.
  - Latency compensation extended to cover the high band's oversampling latency: reported to the host via `setLatencySamples`, low band delay-compensated to match, high band's own clean/distorted `DryWetMixer` blend delay-compensated too.
  - `src/dsp/RealtimeCoefficients.h`: shared real-time-safe (zero-allocation) `juce::dsp::IIR` coefficient update helper, used by `BandEQ` and `Voicing`'s mid/tone filters.
- Broadened Catch2 test suite (issue #43): dedicated test files for every new DSP stage (`NoiseGateTests`, `ParallelCompressorTests`, `VoicingTests`, `BandEQTests`, `IRLoaderTests`), plus sample-rate sweeps (44.1–192 kHz), mono/stereo bus-configuration tests, extreme-parameter-automation and long-run NaN/Inf stability soak tests (`SampleRateAndRobustnessTests`). Existing gain-staging/latency/passthrough tests updated to account for the now-live (non-transparent-by-default) compressor and voicing stages.
- `docs/manual.md`: full user manual — what the plugin is, where it sits in a symphonic-metal chain, signal-flow description, complete parameter reference, and usage tips.

### Changed

- `docs/architecture.md`: signal-flow diagram and module map updated to match the new full signal path; new sections documenting the real-time-safe filter-coefficient pattern, the IR loader's safe-by-default behaviour, and the extended latency-compensation design (including the `DryWetMixer` priming gotcha).
- `README.md`: feature list, signal-flow diagram, and roadmap table updated to match the live DSP and the project's actual milestone scheme (M1 DSP completion & test coverage → M2 presets & state → M3 GUI & accessibility → M4 release).
