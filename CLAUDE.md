# Twist Your Guts — parallel bass processor (bass)

Per-repo working memory for Claude Code sessions on this plugin. Part of the **Metal up your ass** symphonic-metal plugin suite (`github.com/metal-up-your-ass`).

## What this is
Twist Your Guts is a Parallax-style **parallel bass processor** for metal: it LR4-splits the bass into low/high bands, parallel-compresses the lows, runs the highs through selectable distortion voicings, then sums back through a 4-band EQ and an IR cabinet loader. AU / VST3 / Standalone.

## Status (pre-1.0, v0.1.0)
M0 bootstrap and M1 "DSP completion & test coverage" are both done: the full v1.0 signal path is wired and tested (gate, LR4 crossover, parallel low-band compressor, three oversampled high-band voicings, post-sum 4-band EQ, IR loader, latency compensation). Not yet done: preset manager/versioned state (M2), custom GUI/metering/accessibility (M3), signing/notarization/v1.0.0 release (M4). Voicing character (drive-gain ranges, mid-filter hump/scoop settings) is engineering-tuned, not yet ear-tuned against reference material. IR loader has no bundled factory IRs and no GUI file browser yet (DSP engine is fully live; file-loading is a `loadImpulseResponse()` seam a future GUI/preset system will call). See GitHub **milestones/issues** for open work, `README.md` for the feature scope and signal-flow diagram, and `docs/manual.md` for the full parameter reference.

## DSP (v1.0 target — fully wired as of M1)
`Input Trim → Gate → LR4 split (60–1000 Hz) → [Low: parallel comp → makeup → mix → level] + [High: voicing (Gnaw/Wool/Razor, 4x oversampled) → drive → tone → blend → level] → delay-compensated sum → 4-band EQ → IR loader → optional safety clip → Output Trim`.
- Each stage lives in its own `src/dsp/*.{h,cpp}` class with a dedicated Catch2 test file: `Crossover` (LR4 split/sum, flat-sum tested — canonical crossover the suite's `triptych` multiband reuses), `NoiseGateStage`, `ParallelCompressor`, `Voicing` (oversampling + latency reporting), `BandEQ`, `IRLoader`.
- `src/dsp/RealtimeCoefficients.h`: shared helper for updating `juce::dsp::IIR` filter coefficients from the audio thread without heap allocation (`ArrayCoefficients` stack-only computation + in-place raw-storage write). Used by `BandEQ` and `Voicing`.
- Latency: only `Voicing`'s 4x oversampling adds sample latency; reported via `setLatencySamples`, low band delay-compensated to match. See `docs/architecture.md`'s "Latency compensation" section, including the `DryWetMixer` priming gotcha.
- Params via APVTS (`src/params/`).

## Build & test
```sh
export CPM_SOURCE_CACHE="$HOME/.cache/CPM"
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target Tests TwistYourGuts_Standalone --parallel 4
ctest --test-dir build --output-on-failure
```
Release/universal + pluginval + auval run in CI.

## Conventions & guardrails
- JUCE 8.0.14 via CPM · C++20 · AGPLv3 · Pamplejuce `SharedCode` · manufacturer `Yvsv`, plugin code `Tygt`, `com.yvesvogl.twistyourguts`.
- Real-time safety (no alloc/lock/IO/log on the audio thread; allocate in `prepareToPlay`; `reset()` clears state; `ScopedNoDenormals`; smoothed params).
- DryWetMixer gotcha: prime `setWetMixProportion` before `reset()` (see the suite's overture for the pattern).
- `main` protected — feature branch + PR, green CI required, Conventional Commits. New DSP needs tests (flat-sum/null, NaN/Inf, state round-trip).

## Roadmap
GitHub milestones (M1 DSP & tests · M2 presets/state · M3 GUI & a11y · M4 release/signing/v1.0.0) + issues, plus the README roadmap table.

## Suite context
This is the bass member of the suite; its LR4 crossover is the reference pattern reused by `triptych`. Sibling plugins: overture, tenebrae, nave, silentium, requiem, seraph, aureate, firmament, triptych, apotheosis.
