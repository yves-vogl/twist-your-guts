# Crypta — Deep-Dive Research Notes (v2 brief prep)

Scope: Crypta is a "Parallax-style" parallel bass processor — LR4 band-split, parallel-compress
the lows, distort the highs, sum back through EQ + cab IR. This file collects sourced findings
on the reference class before the v2 brief proposes changes. Research-derived, not
hardware-measured; see the Honesty section of the brief for the disclosure this feeds.

## 1. v1 as-built (ground truth, from local repo read)

Source: `/Users/yves/Development/Audio/twist-your-guts` @ `main` (README.md, docs/manual.md,
src/dsp/*.{h,cpp}), read 2026-07-16.

- **Topology**: Input Trim → Gate (full-band, off by default) → **2-band** LR4 split
  (60–1000 Hz, default 250 Hz, log taper) → [Low: `juce::dsp::Compressor` parallel comp
  (thresh −60..0 dB def −18, ratio 1..20 def 4, attack 0.1..100 ms def 10, release 10..1000 ms
  def 120, makeup −12..+24 dB def 0, mix 0..100% def 100) → Level] + [High: voicing select
  (Gnaw hard-clip / Wool cascaded asymmetric soft-clip / Razor mild tanh + pre-HPF + mid-hump),
  4x oversampled, Drive 0..100%, Tone 0..100% (post-shaper 1st-order LPF sweep 700 Hz–15 kHz),
  Blend 0..100% def 100, Level] → delay-compensated sum → 4-band EQ (LowShelf/Peak/Peak/
  HighShelf, off by default, ±18 dB) → convolution IR loader (off by default, no factory IRs
  yet) → optional tanh safety clip → Output Trim.
- Compressor is plain `juce::dsp::Compressor<float>` (feed-forward VCA, no lookahead, 0
  latency) wrapped in a `DryWetMixer` for the parallel blend — architecturally sound, but
  ballistics/ratio defaults are generic ("reasonable" numbers, not sourced).
- Voicing engineering constants (`Voicing.cpp`): Gnaw max drive gain 40x hard-clamped ±1;
  Wool two-stage tanh (12x/6x max gain, 0.15 asymmetry bias); Razor tanh (8x max gain) + 200 Hz
  pre-HPF (Q 0.7071) + 900 Hz mid-hump (+5 dB, Q 1.0); Wool has a 500 Hz mid-scoop (−6 dB, Q
  0.9); Gnaw has a nominal flat "mid filter" at 1 kHz/0 dB/Q 0.7 (present in code but inert at
  0 dB gain). All three share one oversampled shaping call with a single Tone LPF (700 Hz–15
  kHz) after the mid filter. Code comments explicitly flag these as engineering defaults, "not
  yet finalized by ear against reference material" (docs/manual.md line 108, src/dsp/
  Voicing.cpp comment).
- Docs already frame Crypta as "Parallax-style" and explicitly note non-affiliation with
  "Neural DSP or the makers of any Parallax-branded product" (README.md).
- Test suite (Catch2): per-stage null/passthrough tests, flat-sum crossover tests (±0.1 dB),
  monotonic-harmonic-energy-with-drive test for voicings, NaN/Inf robustness sweeps, state
  round-trip. No test currently asserts anything about *frequency-dependent* clipping behavior,
  ballistics-vs-reference-class accuracy, or EQ-frequency placement rationale — all category-
  specific "does this sound like the reference class" guarantees are absent, only generic DSP
  hygiene is covered.

## 2. Reference class definition

Crypta's own docs point at "Parallax-branded" plugins, i.e. **Neural DSP Parallax** (2020,
designed by Doug Castro — founder of Darkglass Electronics — and Francisco Cresp of Neural
DSP) as the primary named reference. Darkglass's own hardware distortion pedals (the same
designer's other work, e.g. Alpha•Omega / B7K family) are the second reference: they define
the "blend a clean low end with a distorted top end" mechanism in the analog-pedal world that
Parallax explicitly digitizes into a multiband plugin. A third, generic reference is the
"New York style" parallel-bus-compression convention (dbx 160-class hardware, drum/bass bus
compression folklore) that both v1's own manual and Parallax's low-band compressor draw on.

## 3. Neural DSP Parallax — primary reference, sourced numbers

Source: official **Parallax User's Guide v2.0.0** PDF, fetched and read in full (17 pages),
`https://downloads.neuraldsp.com/file/parallax-installers/Parallax-v2.0.0.pdf`.

- **"Parallax is a multi-band distortion for bass. This plugin is meant to bring the user a
  ready tool, which is based on a studio technique used by audio engineers and producers to
  craft their bass tone. Bass, mids, and high frequencies are processed separately with
  distortion and compression to be mixed back together."** (p.8) — this is the direct
  definition of the category Crypta claims to be part of.
- **Band topology is 3-way, not 2-way**: Low / Mid / Treble, each independently enabled. This
  is a structural difference from Crypta's 2-band (Low/High) split. "Bass, mids, and high
  frequencies are processed separately" (p.8) — the whole premise of the category, per the
  designer, is a *three*-way split, not two.
  - Low band: **Low Pass filter** (variable — "for perfect control over the bottom end
    response") + Compression + Level. **Bypasses the cab-sim entirely and stays mono even in
    stereo input mode** ("The low band signal passes straight to the graphic equalizer
    bypassing the cabsim, and it remains mono while in stereo input mode." p.9).
  - Mid band: **Mid Drive** (tube-gain-stage saturation) + Mid Level only — no filter exposed
    on this band in the UI; its center is fixed. Per third-party review (Guitar Interactive
    Magazine, see §5): "The mid band, as described, is fixed around 400hz."
  - High/Treble band: **High Pass filter** (variable, "allows dialing the perfect amount of
    fuzz or tightness") + High Drive + High Level. Per the same review, the HPF is "pullable
    down to 100Hz."
  - Framing: "Dialing a high gain sound with presence, definition, and clarity requires
    removing a certain amount of low-end from the spectrum to be distorted." (p.9) — i.e. the
    HPF-before-distortion move (which Crypta's Razor voicing already does, but only for one of
    three voicings, and only as a fixed 200 Hz corner) is presented as *the* generic technique
    for the whole high-distortion band, not a single voicing's quirk.
- **Low-band compressor — exact fixed ballistics, the single most load-bearing sourced number
  in this research pass**: "COMPRESSION KNOB: Drag and move it to set the amount of gain
  reduction and make up gain from 0dB to +10dB. **Fixed settings: Attack 3ms - Release 6ms -
  Ratio 2.0.**" (p.9, verbatim, bold in original manual). This is dramatically different from
  Crypta v1's low-comp defaults (ratio 4:1, attack 10 ms, release 120 ms — a much slower,
  harder-hitting compressor). Parallax's reference-class low-band compressor is a **fast,
  low-ratio "glue" bus compressor** — a single combined Compression knob sets *only* gain-
  reduction depth/makeup, not ratio or ballistics, because the ballistics are deliberately
  fixed at values that behave more like parallel-bus glue than a dynamics tool. This matches
  general "bus compression" folklore (dbx 160-class hardware bass-bus compression commonly runs
  4–6 dB of GR at fast attack/release for "evening out low frequencies... more controlled and
  punchy" — TalkBass community sourcing, §4) but Parallax's own numbers are far faster and
  gentler-ratio than that generic folklore, confirming a deliberate "glue, not squash" design
  choice specific to this category.
- **Distortion is described as tube-modeled saturation ("multiple tube gain stages"), not
  simple hard-clip/tanh math**: "Individual multiple tube gain stages for Mid and Treble."
  (p.2, feature list) "The Mid Drive has enough dynamic range to go from mild saturation to
  blistering high gain, all without losing definition and articulation. Multiple tube gain
  stages were designed for the Mid and Treble bands separately." (p.9) This is presented as a
  cascaded-gain-stage character (closer to Crypta's Wool voicing, which already cascades two
  stages) rather than Gnaw's single hard-clamp, suggesting the reference class leans toward
  cascaded/staged saturation as the norm, not the exception.
- **EQ section — fixed 6-band graphic EQ, exact frequencies**: "Low Shelf: 100Hz, 250Hz,
  500Hz, 1.0kHz, 1.5kHz, 5.0kHz [likely OCR of a final band labeled High Shelf], boost/cut
  −12dB to +12dB." (p.10) These are graphic-EQ fixed bands (not parametric peak/shelf with
  variable frequency/Q, unlike Crypta's fully parametric 4-band EQ) — a deliberately simpler,
  "just enough to fix a mix problem" EQ, not a sound-design tool. Crypta's EQ (4-band, fully
  parametric, ±18 dB) is already more capable than the reference's graphic EQ; no functional
  gap here, but the fixed reference frequencies are useful anchors for factory-preset EQ
  moves.
- **Gate**: single "Gate Knob: Attenuates the input signal below the threshold" — no separate
  ratio/attack/release exposed at all (simpler than Crypta's 4-parameter gate). Confirms a gate
  is a secondary, coarse safety feature in this category, not a sound-design control — Crypta's
  richer gate is not a gap, it's already more capable.
- **Cab sim**: 6 movable virtual mic positions/distances, low band explicitly bypasses it.
  Confirms the "low band skips the cab/IR stage" pattern as a category convention, which Crypta
  v1 does *not* currently implement (Crypta's IR loader sits post-sum, after both bands are
  already combined) — flagged as a gap.
- **Input/Output gain framing**: "Input will affect how much signal the plugin will feed in.
  This will affect the amount of distortion range of the gain knobs..." (p.12) — i.e. input
  trim is explicitly documented as *part of* the distortion-amount control, not just gain
  staging. Matches Crypta's existing Input Gain framing in its own manual ("all of their
  thresholds are calibrated assuming a reasonably 'line level' input") — no gap here.

## 4. Parallel ("New York") bus compression — general technique background

Source: WebSearch aggregation across `dbxpro.com`, `talkbass.com`, `gearspace.com` forum
threads on New York/parallel bus compression (accessed 2026-07-16; no single canonical primary
source URL, treat as corroborated community/vendor consensus, not a single citable manual).

- "New York compression is also called 'Parallel Compression'... you need a much higher ratio
  (often starting at 10:1, which is really more like limiting), with fast attack times set as
  fast as possible while still allowing the leading edge of the transient through... Typical
  settings include 8–16 ratio with weird attack and release settings, but for safety, keep fast
  attack and long release." — general drum/bus New York-compression folklore.
- Bass-specific dbx 160 guidance (TalkBass community consensus): "use a ratio of 5:1 and set
  the threshold so you're getting about 4–6 dB of gain reduction, as the DBX 160's fast
  response evens out low frequencies and makes the bass feel more controlled and punchy."
- **Reading against Parallax's own numbers (§3)**: Parallax's fixed low-band compressor
  (ratio 2.0, attack 3 ms, release 6 ms) is *gentler in ratio* and *dramatically faster in
  release* than either the generic New York-drum convention (8–16:1) or the bass-specific dbx
  160 folklore (5:1). This confirms Parallax is not doing "New York style" heavy parallel
  compression in the classic drum-bus sense at all — it is doing something closer to a fast
  "glue"/optical-style bus compressor blended in parallel, calibrated for a *few dB* of
  continuous gentle control rather than big, audible pumping. Crypta v1's manual explicitly
  invokes "New York style" framing (`docs/manual.md` line 78) and a heavier 4:1/10 ms/120 ms
  default — this is the single largest characterization mismatch found in this research pass:
  v1's own naming ("New York style") points at the wrong sub-genre of parallel compression for
  what its named reference (Parallax) actually does.

## 5. Third-party review commentary (workflow lore)

Source: Guitar Interactive Magazine review of Neural DSP Parallax,
`https://guitarinteractivemagazine.com/review/neural-dsp-parallax/` (accessed 2026-07-16).

- "The mid band, as described, is fixed around 400hz." — corroborates the mid band's fixed
  center per §3.
- On the low band: distortion "can make anything sound stodgy and incoherent" by compressing
  transients — i.e. the reviewer frames the low band's job as compression-only (never
  distortion) precisely because low-frequency distortion reads as mud, reinforcing why the
  reference class keeps the low band's processing to compression/level only, never saturation.
- On the high/treble band: the HPF-before-distortion move "tames harshness... when it comes to
  high saturation drive sounds" — i.e. the pre-clip HPF is framed as *harshness control*, not
  (only) a tightness/mud-control move. This nuances Crypta's existing Razor-only 200 Hz pre-HPF:
  the reference class treats a pre-distortion HPF as a universal high-band hygiene move across
  *all* voicings, not a single voicing's identity trait.
- Workflow: "crafting low-end punch, applying band specific compression to tighten it up,
  careful midrange treatment, and using the built-in EQ for final shaping" — a mid-first, then
  EQ-to-finish order, consistent with (not contradicting) Crypta's existing manual tip to "set
  the low band first, then dial in the high band."

## 6. Darkglass Alpha•Omega — same designer's hardware lineage, tone-stack anchors

Source: `https://www.darkglass.com/pages/alpha-omega-manual` (accessed 2026-07-16). Doug
Castro (Darkglass founder) co-designed Parallax, so Darkglass's own hardware tone-stack
frequency choices are a reasonable secondary anchor for EQ-band placement, though this is
*not* the same product and should not be over-indexed.

- "Blend: Mixes the clean and processed signals. The clean signal remains at unity gain while
  the volume of the overdriven signal is set by the Level knob, allowing for fine mix tuning."
  — the canonical "clean stays at unity, wet gets its own level, blend crossfades" pattern;
  matches Crypta's existing High Blend + High Level pairing structurally already.
- "Mod: Selects or mixes between the two distinct distortion circuits: Alpha is punchy, tight
  with a lot of definition, whereas Omega is simply brutal and raw." — a *continuously
  blendable* pair of distortion characters (not a hard 3-way switch), a UX pattern Crypta's
  discrete Gnaw/Wool/Razor selector does not currently offer; noted as a possible future
  direction, out of scope for this v2 pass (kept as a discrete selector per the honesty/scope
  section below).
- Fixed EQ-band anchors: **Bass ±12 dB @ 80 Hz, Mid ±12 dB @ 500 Hz, Treble ±12 dB @ 5 kHz,
  Bite (high-mid presence boost) @ 2.8 kHz, Growl = shelving bass boost + increased low-end
  saturation.** These four frequencies (80 Hz / 500 Hz / 2.8 kHz / 5 kHz) are a well-
  corroborated, purpose-built "bass tone stack" frequency set from the same design lineage as
  Parallax, useful as sourced anchors for Crypta's post-sum EQ factory-preset defaults (Crypta
  already has a fully parametric 4-band EQ with generic corners of 100/500/2500/8000 Hz — close
  to, but not exactly anchored on, these sourced bass-specific corners).

## 7. Gaps identified — v1 vs. reference class (feeds directly into the brief's §1)

1. **Compressor ballistics/ratio mismatch (largest, most sourced gap).** v1: ratio 4:1, attack
   10 ms, release 120 ms, manual explicitly says "New York style." Reference (Parallax, exact
   fixed values): ratio 2.0, attack 3 ms, release 6 ms — much faster, gentler-ratio "glue" bus
   compression, not classic New York drum-style parallel squashing. v1's own naming points at
   the wrong technique for what its stated reference actually implements.
2. **2-band split vs. reference's 3-band split.** The reference class's defining move —
   "bass, mids, and high frequencies are processed separately" — is a 3-way split with a fixed-
   ish mid band around 400 Hz doing tube-style saturation, distinct from the treble band's
   harshness-controlled saturation. v1's 2-band (Low/High) split conflates what the reference
   class treats as two functionally distinct distorted bands (mid = "throatier color, note
   articulation"; high = "presence, fuzz/tightness, harshness control").
3. **Pre-distortion HPF is voicing-specific in v1, band-wide in the reference.** Only Razor
   gets a pre-clip HPF (fixed 200 Hz) in v1; the reference class treats a variable pre-
   distortion HPF as the primary "how much fuzz vs. tightness" control for the *entire* high
   band, available regardless of which saturation character is selected.
4. **Low band never gets distortion/saturation in the reference class, confirmed by both the
   manual's architecture (low band bypasses cab-sim, stays mono) and third-party commentary
   (low-frequency distortion reads as "stodgy and incoherent").** v1 already respects this
   (low band is compression-only) — not a gap, a confirmed-correct existing decision worth
   stating explicitly as validated rather than silently unchanged.
5. **IR/cab-sim routing gap.** Reference class explicitly routes the low band around the cab
   sim entirely (mono, bypasses convolution). v1's IR loader sits post-sum, processing the
   combined low+high signal — the low band's punch/mono character is not protected from cab-sim
   coloration/stereo-izing the way the reference class protects it.
6. **EQ frequency anchors are generic, not sourced.** v1's 4-band EQ defaults (100/500/2500/
   8000 Hz) are plausible but arbitrary; the same-designer hardware lineage (Darkglass) offers
   sourced bass-tone-stack anchors (80/500/2.8k/5k Hz) that are a better-grounded starting
   point for factory presets, even though v1's fully parametric EQ already exceeds the
   reference's fixed 6-band graphic EQ in capability (not a functional gap, a defaults gap).
7. **Voicing character is cascaded/staged saturation in the reference, single-stage in two of
   v1's three voicings.** Only Wool cascades two shaping stages; the reference's "multiple tube
   gain stages... designed for the Mid and Treble bands separately" language suggests staged
   saturation is the category norm, not Wool's special case. (Flagged as a directional finding,
   not a numerically sourced one — the manual doesn't give stage counts or gain-per-stage
   numbers, so this cannot be over-specified in the brief.)

## Sources (deduplicated)

- Neural DSP, *Parallax User's Guide v2.0.0* (PDF, 17pp), designer credits + full parameter
  reference + fixed compressor ballistics: https://downloads.neuraldsp.com/file/parallax-installers/Parallax-v2.0.0.pdf
- Guitar Interactive Magazine, Neural DSP Parallax review (workflow/mid-band-fixed-400Hz
  commentary): https://guitarinteractivemagazine.com/review/neural-dsp-parallax/
- Darkglass Electronics, Alpha•Omega manual (tone-stack frequency anchors, Blend/Mod pattern):
  https://www.darkglass.com/pages/alpha-omega-manual
- WebSearch aggregation, "New York compression" / dbx 160 bass bus-compression convention
  (dbxpro.com, talkbass.com, gearspace.com; no single canonical URL — community/vendor
  consensus): queried via WebSearch 2026-07-16, no single primary page fetched.
- Local repo ground truth: `/Users/yves/Development/Audio/twist-your-guts` `README.md`,
  `docs/manual.md`, `src/dsp/{ParallelCompressor,Voicing,Crossover,NoiseGateStage}.{h,cpp}`,
  `tests/*.cpp` — read directly, `main` branch, 2026-07-16.
