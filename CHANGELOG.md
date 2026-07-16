# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog 1.1.0](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.2.0] - 2026-07-16

### Changed (headline: 2-band → 3-band topology rebuild)

- **Restructured the signal path from a 2-band (low/high) split to a genuine 3-band (low/mid/high) split**, matching the reference class's own defining architecture ("bass, mids, and high frequencies are processed separately" - see `docs/design-brief.md`/`docs/research-notes.md` for the full sourcing). Two cascaded 4th-order Linkwitz-Riley crossovers replace v0.1.x's single split: `Split Low` (60–400 Hz, default 120 Hz, was `Crossover Frequency`) peels off the Low band; `Split High` (300–2000 Hz, default 600 Hz, NEW) further splits the remainder into Mid and High. `Split High` is always clamped at least 1/3 octave above `Split Low` (`src/dsp/SplitGap.h`) to prevent a degenerate near-zero-width Mid band.
- **Added a new Mid band** (`src/dsp/MidBand.h/.cpp`): staged/cascaded drive-only saturation (0-100%, default 30%) plus an independent output Level, no filter/tone/blend - matching the reference class's own Mid band exactly. Runs its own 4x-oversampled shaping stage.
- **Promoted the High band's pre-drive highpass ("Tight") from a Razor-only fixed 200 Hz internal constant to a first-class, voicing-independent control** (20-500 Hz, default 100 Hz), applied ahead of all three voicings (Gnaw/Wool/Razor) - closing the gap identified in the design brief's "Why v1 falls short" analysis.
- **Re-sourced the low-band parallel compressor's ballistics defaults** to the reference class's own fixed, documented "glue" bus-compressor values: ratio 4:1 → **2:1**, attack 10 ms → **3 ms**, release 120 ms → **6 ms** (release range floor lowered from 10 ms to 5 ms, a breaking pre-1.0 change, so the sourced default is reachable). Retires the v0.1.x manual's "New York style" framing, which characterized the wrong sub-genre of parallel compression for what the reference class actually implements - see `docs/research-notes.md` §3-4.
- **Relocated the IR loader (cab-sim convolution)** to process only the Mid+High post-sum signal, never the Low band - matching the reference class's "low band bypasses the cabsim" architecture. Structurally enforced (not just conventional) and directly tested (`tests/LowBandIsolationTests.cpp`): the Low band's own isolated output is bit-exact identical whether the IR loader is on or off, or which IR is loaded.
- **Re-anchored the post-sum 4-band EQ's default corner frequencies** to a sourced bass-tone-stack frequency set from the same design lineage as the reference class: Low Shelf 100 → **80 Hz**, Peak 2 2500 → **2800 Hz**, High Shelf 8000 → **5000 Hz** (Peak 1's existing 500 Hz default already matched the sourced anchor). Dormant-until-engaged (EQ ships off by default), so not an audible v0.1.x → v0.2.0 change unless a user or preset turns the EQ on.
- **Added a phase-alignment allpass filter for the Low band** (`src/dsp/PhaseAlignFilter.h`), required to make the new cascaded (not parallel) crossover topology actually flat-sum - discovered and fixed during this rebuild: a naive cascade of two independent LR4 crossovers does *not* flat-sum on its own (deviations up to −10 dB were measured at close `Split Low`/`Split High` ratios before this fix). The fix is proven algebraically, not just empirically - see `docs/architecture.md`'s "Cascaded 3-band flat-sum and phase alignment" section for the full derivation. This is a genuine engineering necessity this rebuild surfaced, not something anticipated by the original design brief.
- **Lossy state migration for v0.1.x sessions**: the old single `Crossover Frequency` (`crossoverFreq`) parameter is migrated to the new `Split High` parameter on load, clamped into its 300–2000 Hz range. v0.1.x's shipped default (250 Hz) sits below that floor, so the single most common migration path - an untouched v0.1.x session - lands exactly at the 300 Hz floor (dedicated regression test in `tests/StateMigrationTests.cpp`). `Split Low` and every new Mid-band/Tight parameter fall back to their v0.2.0 defaults; any low-band compressor values a user had explicitly changed away from v0.1.x's old defaults are preserved as-is.
- Docs rewritten to match: `docs/manual.md`, `docs/architecture.md`, `README.md` (signal-flow diagrams, parameter tables, feature list). `docs/design-brief.md` and `docs/research-notes.md` added (the binding brief and its sourcing for this rebuild).

### Added (M2 preset system)

- Suite-wide M2 preset system (`.scaffold/specs/preset-system-m2.md`), ported from `basilica-audio/nave`'s pilot implementation: `src/presets/PresetManager.{h,cpp}` (factory + user presets, save/save-as/delete/rename, default resolution, import/export of single presets and zip banks, dirty-state tracking) and `src/presets/PresetBar.{h,cpp}` (a horizontal preset strip docked at the top of the editor).
- Nine factory presets (`presets/factory/*.json`, category `Init`/`Bass`) - see `docs/presets.md` for what each demonstrates: **Default**, **Glue & Grind**, **Sub Lock**, **Throat**, **Fuzz Wall**, **Cut Through**, **Definition Only**, **Clean Low, Loud Top**, **Cab-Colored Grind**.
- German localisation of the preset UI frame (`resources/i18n/de.txt`), auto-selected via `SystemStats::getUserLanguage()`. Core/DSP terminology (parameter names, units) is never translated.
- User presets stored at `~/Library/Audio/Presets/Yves Vogl/Crypta/` (macOS) / `%APPDATA%\Yves Vogl\Crypta\Presets\` (Windows).

### Fixed

- Discovered and fixed during the topology rebuild: a naive cascade of two independent LR4 crossovers does not flat-sum (see the phase-alignment entry above) - this was never an issue in v0.1.x's single-crossover topology, so it is not a regression from any prior release, but a new correctness requirement introduced by the 3-band rebuild itself.

## [0.1.1] - 2026-07-16

### Changed

- Renamed plugin from Twist Your Guts to Crypta (new plugin code `Cryp`, new bundle id `com.yvesvogl.crypta`). Old identity: plugin code `Tygt`, bundle id `com.yvesvogl.twistyourguts` — DAWs treat this as a new plugin; v0.1.0-era sessions will need to be re-pointed at the new plugin identity. Part of the suite's move to Basilica Audio naming (the crypt: the basilica's low-end foundation).
- `.github/workflows/release.yml` reconciled with the suite-wide release template: org-level Apple signing secrets (`APPLE_CERT_P12`, `APPLE_CERT_PASSWORD`, `APPLE_API_KEY_P8`, `APPLE_API_KEY_ID`, `APPLE_API_ISSUER_ID`) instead of the prior per-repo secret set, `find`-based artefact discovery, tag-only (`v*`) trigger with no `workflow_dispatch` dry-run path.
- Removed `docs/releasing.md` and `docs/adr/0006-macos-signing-notarization.md`, which documented the prior per-repo signing pipeline; the org-level signing setup is now documented centrally at `.scaffold/SIGNING-SETUP.md`, matching sibling suite repos (none of which carry a per-repo releasing runbook or signing ADR).
- Reworded the stale "symphonic-metal" suite framing in `CLAUDE.md` and `docs/manual.md` to "heavy-music", matching the suite bible.
- `juce_add_plugin` now sets `ICON_BIG` from `docs/assets/icon.png`, so the plugin bundle carries its own icon instead of JUCE's default.

### Fixed

- `CryptaAudioProcessor` now overrides `reset()`, flushing every per-stage DSP class's own state (LR4 crossover filter memory, gate/compressor envelopes, the high-band voicing's oversampling/mid/tone filter state, the low-band latency-compensation delay line, EQ biquad history, IR convolution engine) on a host transport stop/loop/rewind, instead of leaving stale state ringing into whatever plays next.
- `Voicing::setVoicing()` now resets `midFilter`'s state on every voicing change, matching `preHighPass`'s existing Razor-switch handling, so a coefficient jump between voicings (e.g. Wool's -6dB scoop to Razor's +5dB hump) no longer rings a stale-state transient.
- `getTailLengthSeconds()` now reports `IRLoader`'s actually-loaded impulse response length instead of a hardcoded `0.0`, so hosts making bounce/freeze/render-tail decisions don't truncate a loaded cab IR's convolution tail.
- Added regression coverage for `processBlock()`'s chunking path when a host block exceeds `prepareToPlay()`'s `samplesPerBlock` (no functional bug found - the existing chunking logic was already correct, just untested).

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
