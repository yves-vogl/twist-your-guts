# Twist Your Guts — User Manual

*Split your bass. Compress the lows. Twist the guts out of the highs.*

## What it is

Twist Your Guts is a Parallax-style **parallel bass processor** built for metal production. It splits your bass signal into a low band and a high band with a 4th-order Linkwitz-Riley ("LR4") crossover, keeps the low band tight with a parallel compressor, and runs the high band through a choice of three distortion voicings before summing everything back together through a 4-band EQ and a cabinet-simulation IR loader.

### Where it sits in a symphonic-metal chain

Twist Your Guts is designed to be the **bass-specific voicing stage** in the "Metal up your ass" suite:

- Track order: **DI/amp sim → Twist Your Guts → bus compression/glue → mix bus**. It expects a reasonably clean, already-amp-sim'd or DI'd bass signal; it is not itself a full amp sim (no built-in preamp gain staging beyond the input trim and drive controls).
- The low band's parallel compressor is meant to keep the fundamental/sub content of the bass locked in place under a wall of distorted guitars, while the high band's voicing adds the upper-mid "grind" that lets the bass cut through a dense mix without competing for the same frequency range as the guitars.
- The crossover point (default 250 Hz) is deliberately tunable across the whole low-mid register (60 Hz–1000 Hz) so you can match the split to the song's tuning (drop-tunings push useful low-end content further up).
- The output stage's IR loader is meant for quick cabinet-style tone shaping without needing a separate cab-sim plugin later in the chain, though it can also be left off entirely if you're already running a dedicated cab sim elsewhere.

## Signal flow

```
Input Trim → Gate → LR4 Split (60–1000 Hz, default 250 Hz)
                      │
        ┌─────────────┴─────────────┐
        │                           │
     Low band                   High band
  Parallel Comp → Level   Voicing → Drive → Tone → Blend → Level
        │                           │
        └─────────────┬─────────────┘
                       │
              Sum (delay-compensated)
                       │
                  4-band EQ
                       │
                  IR loader
                       │
              Safety Clip (optional)
                       │
                     Output
```

The high band runs its distortion voicing oversampled (4x) to keep aliasing under control; the low band carries a matching compensation delay so both bands stay time-aligned at the sum. See [`docs/architecture.md`](architecture.md) for the full technical breakdown, including exactly how that latency compensation works.

## Parameter reference

Unless noted otherwise, all continuous parameters are smoothed to avoid zipper noise when automated.

### IO / Global

| Parameter | Range | Default | Unit | What it does |
|---|---|---|---|---|
| Input Gain | −24 … +24 | 0 | dB | Trims the signal before anything else in the chain. Use this to get a hot but not clipping signal into the gate/compressor/voicing stages - all of their thresholds are calibrated assuming a reasonably "line level" input. |
| Output Gain | −24 … +24 | 0 | dB | Final output trim, applied after everything else (including the safety clip). |
| Bypass | off/on | off | — | Forces a bit-exact passthrough of the input signal. Also exposed as the plugin's host-facing bypass parameter, so your DAW's own bypass button/automation lane works too. |
| Safety Clip | off/on | off | — | A soft (tanh) limiter on the very last stage before the output trim. Off by default; turn it on as a safety net against accidental hard-clipped overs, not as a tone-shaping tool - at typical playing levels it's inaudible, and it only starts rounding peaks once they approach 0 dBFS. |

### Noise Gate (full-band, before the crossover split)

Sits ahead of the crossover, so it gates the input signal as a whole rather than per band.

| Parameter | Range | Default | Unit | What it does |
|---|---|---|---|---|
| Gate Enable | off/on | **off** | — | Enables the gate. Off by default - most already-tracked bass DI/amp signals don't need one, and an incorrectly-set gate can chop off legitimate low-level playing (ghost notes, decays). |
| Gate Threshold | −80 … 0 | −60 | dB | Signal level below which the gate starts attenuating. |
| Gate Ratio | 1 … 20 | 10 | :1 | How aggressively the gate attenuates once below threshold. Higher = closer to a hard mute. |
| Gate Attack | 0.1 … 50 | 1 | ms | How fast the gate opens once the signal crosses back above threshold. |
| Gate Release | 5 … 500 | 100 | ms | How fast the gate closes once the signal drops below threshold. |

### Crossover

| Parameter | Range | Default | Unit | What it does |
|---|---|---|---|---|
| Crossover Frequency | 60 … 1000 | 250 | Hz | The LR4 split point between the low and high bands. Log-scaled control (equal knob travel per octave). Lower it to push more of the fundamental into the (typically cleaner, compressed) low band; raise it to give the distortion voicing more of the low-mid content to work with. |

### Low band: parallel compressor + level

The low band is compressed **in parallel** ("New York style"): the compressed signal is blended back with its own uncompressed self via Mix, rather than replacing it outright, which is what keeps the low end feeling tight and controlled without ever sounding squashed or lifeless.

| Parameter | Range | Default | Unit | What it does |
|---|---|---|---|---|
| Low Comp Threshold | −60 … 0 | −18 | dB | Level above which the low-band compressor engages. |
| Low Comp Ratio | 1 … 20 | 4 | :1 | Compression ratio above threshold. |
| Low Comp Attack | 0.1 … 100 | 10 | ms | How fast the compressor clamps down once above threshold. |
| Low Comp Release | 10 … 1000 | 120 | ms | How fast the compressor lets go once back under threshold. |
| Low Comp Makeup | −12 … +24 | 0 | dB | Gain applied to the compressed (wet) signal before it's blended back with the dry low band - use this to bring the compressed signal back up to match the dry level, so Mix behaves as a true "how much compression character" control rather than also changing overall loudness. |
| Low Comp Mix | 0 … 100 | 100 | % | Blend between the dry (uncompressed) and wet (compressed + makeup) low band. 0% = compressor has no audible effect; 100% = fully compressed. |
| Low Level | −24 … +12 | 0 | dB | Level trim on the low band, applied after compression and before the bands are summed back together. |

### High band: voicing, drive, tone, blend, level

Three selectable distortion voicings, each oversampled (4x) to keep the nonlinear shaping stage's aliasing out of the audible band.

| Parameter | Range | Default | Unit | What it does |
|---|---|---|---|---|
| High Voicing | Gnaw / Wool / Razor | Gnaw | — | Selects the distortion character. See below. |
| High Drive | 0 … 100 | 50 | % | How hard the signal is pushed into the selected voicing's nonlinearity. |
| High Tone | 0 … 100 | 50 | % | Post-shaper tone control: a low-pass sweeping from dark (0%) to bright (100%), tucking away or opening up fizz/harshness from the distortion stage. |
| High Blend | 0 … 100 | 100 | % | Blend between the clean (pre-voicing) and fully distorted high band. 0% = clean high band (voicing has no audible effect); 100% = fully distorted. |
| High Level | −24 … +12 | 0 | dB | Level trim on the high band, applied after voicing/blend and before the bands are summed back together. |

**Voicings:**

- **Gnaw** — an op-amp-style hard clip. Symmetric, unforgiving, the most aggressive of the three; pushes hard into a square-ish waveform at high drive. Good for a raw, buzzy attack.
- **Wool** — cascaded soft-clip fuzz with a mid scoop and a touch of asymmetry for a grittier, more fuzz-pedal-like harmonic character. Good for a woolier, less "digital" grind that still cuts.
- **Razor** — a tighter overdrive: the signal is high-passed before the (comparatively mild) clipper, and a mid-hump filter afterwards keeps the low end from ever getting mushy. Good for definition and pick/finger attack without piling on low-end mud.

*Starting points, not final voicing:* the drive-gain ranges and mid-filter hump/scoop settings for all three voicings are engineering defaults, tuned for musical usefulness and mathematically bounded (no runaway output at any drive setting), not yet finalized by ear against reference material. Expect these to be refined in a future release.

### Post-sum 4-band EQ

Applied after the low/high bands are summed back together. Off by default; when off, the EQ stage is skipped entirely (guaranteed transparent, not just set to unity gain).

| Parameter | Range | Default | Unit | What it does |
|---|---|---|---|---|
| EQ Enable | off/on | off | — | Enables the EQ stage. |
| EQ Low Shelf Frequency | 40 … 400 | 100 | Hz | Low shelf corner frequency. |
| EQ Low Shelf Gain | −18 … +18 | 0 | dB | Low shelf boost/cut. |
| EQ Peak 1 Frequency | 100 … 2000 | 500 | Hz | First parametric peak band's centre frequency. |
| EQ Peak 1 Gain | −18 … +18 | 0 | dB | First peak band's boost/cut. |
| EQ Peak 1 Q | 0.2 … 5.0 | 0.7 | — | First peak band's bandwidth (higher = narrower). |
| EQ Peak 2 Frequency | 500 … 8000 | 2500 | Hz | Second parametric peak band's centre frequency. |
| EQ Peak 2 Gain | −18 … +18 | 0 | dB | Second peak band's boost/cut. |
| EQ Peak 2 Q | 0.2 … 5.0 | 0.7 | — | Second peak band's bandwidth. |
| EQ High Shelf Frequency | 2000 … 16000 | 8000 | Hz | High shelf corner frequency. |
| EQ High Shelf Gain | −18 … +18 | 0 | dB | High shelf boost/cut. |

### IR loader (cabinet simulation)

A convolution-based cab-sim stage on the very end of the chain, before the safety clip. Off by default. With no impulse response loaded, this stage is a guaranteed bit-exact passthrough at every session sample rate, so turning it on before loading an IR never changes your sound.

| Parameter | Range | Default | Unit | What it does |
|---|---|---|---|---|
| IR Enable | off/on | off | — | Enables the IR loader stage. |
| IR Mix | 0 … 100 | 100 | % | Blend between the dry (pre-convolution) and fully convolved signal. |

*Loading impulse responses:* v0.1.0 does not yet ship an in-plugin file browser or factory cabinet IRs (both are on the roadmap for a later milestone alongside the custom GUI). The IR-loading DSP engine itself is fully implemented and real-time safe; a GUI file picker will be wired up to it once the custom interface lands.

## Tips

- **Start with the low band tight, then dial in the high band's grind.** Set Low Comp Mix and Makeup first so the fundamental feels locked in, *then* pick a voicing and drive amount - it's much easier to judge how much distortion character you actually need once the low end already feels solid.
- **The crossover point is a tone decision, not just a technical one.** Pushing it up (toward 400–600 Hz) moves more of the note's body into the distorted high band, which can help a bass cut through a dense guitar wall at the cost of low-end weight; pulling it down (toward 100–150 Hz) keeps more of the note clean/compressed and reserves the distortion for genuinely high harmonic content.
- **High Blend is your "how much" knob, High Drive is your "how hard" knob.** If a voicing feels too extreme, try lowering Blend before lowering Drive - you'll often keep more of the character that way, just at a lower overall intensity, rather than flattening the nonlinearity itself.
- **Razor plus a mild EQ high-shelf cut** is a good starting point if a mix feels harsh/fizzy - Razor's pre-clip highpass already keeps the low end tight, so a touch of top-end EQ taming after the fact is usually enough rather than reaching for less drive.
- **Leave the safety clip off during tracking/mixing**, and only reach for it as insurance against unexpected automation or a hot input on a specific pass - it's a safety net, not part of the intended tone-shaping signal path.
