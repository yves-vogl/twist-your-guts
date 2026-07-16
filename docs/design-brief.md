# Crypta — Design Brief v2 (binding; supersedes v1's implicit spec)

Parallel bass processor: split the signal by frequency, keep the low end tight via fast
parallel-bus compression, run the upper bands through saturation, sum back through EQ and a
cabinet IR. Research-driven rewrite: every default below is sourced (see
`crypta-research-notes.md`) or explicitly reasoned where no source exists. **No brand or
person names in parameters, UI or marketing copy** — generic descriptors only ("Tight",
"Drive", "Split"); the manual/research notes may cite public sources (the named commercial
bass-multiband plugin this genre is built on, its designer's other published hardware) freely,
since those are the honestly-disclosed reference class, not implied endorsement — consistent
with the existing README's non-affiliation language, which this brief does not change.

## Why v1 falls short (the three core corrections)

1. **The band count is wrong for the category.** The reference class's own defining sentence
   is "bass, mids, and high frequencies are processed separately with distortion and
   compression to be mixed back together" — a **three**-way split (low/mid/high), with the mid
   and high bands doing functionally distinct jobs (mid: "throatier color... note
   articulation"; high: "presence... fuzz or tightness... tames harshness"). v1's two-band
   (low/high) split conflates those two distinct distortion characters into a single high band
   with three switchable voicings — a reasonable engineering simplification, but it is not what
   the category it names itself after actually does (research notes §3, §5).
2. **The low-band compressor's ballistics model the wrong sub-genre of parallel compression.**
   v1's manual explicitly calls the low band "New York style" and defaults to ratio 4:1, attack
   10 ms, release 120 ms — classic heavy drum-bus parallel-compression language. The reference
   class's own low-band compressor is fixed at **ratio 2.0, attack 3 ms, release 6 ms**
   (research notes §3) — a fast, gentle, "glue" bus compressor, categorically different from
   New York-style squashing (which typically runs 8–16:1). v1 isn't just using different
   numbers, it's modeling a different technique under the right-sounding wrong name.
3. **The pre-distortion high-pass is a single voicing's quirk instead of the high band's
   primary character control.** Only Razor gets a pre-clip HPF (fixed 200 Hz) in v1; the
   reference class treats a *variable* pre-distortion HPF ("pullable down to 100Hz") as the
   main "how much fuzz vs. tightness" knob for the *entire* upper-band signal chain, independent
   of which saturation voicing is selected (research notes §3, §5).

## Topology (restructured: 2-band → 3-band, cascaded LR4)

```
Input Trim → Gate (full-band, unchanged) →
  LR4 split #1 (Split Low, 60-400 Hz, default 120 Hz) →
    Low band  → [Parallel Comp: fast/gentle defaults] → Level
    Remainder → LR4 split #2 (Split High, 300-2000 Hz, default 600 Hz) →
                  Mid band  → Drive (staged saturation) → Level
                  High band → Tight (pre-drive HPF, band-wide) → Voicing → Drive → Tone → Blend → Level
                       │
        [Mid + High summed] → IR loader (cab sim) ── Low band rejoins AFTER the IR loader,
                                                        never passed through convolution
                       │
              Sum (Low + [Mid+High post-IR], delay-compensated)
                       │
                  4-band EQ (post-sum, unchanged parametric structure, re-anchored defaults)
                       │
              Safety Clip (optional) → Output Trim
```

- **Cascaded (not parallel) LR4** is the correct way to build an N-band flat-summing crossover
  — this is the exact pattern the suite's `triptych` multiband plugin already establishes and
  that v1's own `Crossover.h` comments flag as the reusable canonical crossover. v2 instantiates
  two `cryp::Crossover` objects in series: the first peels off Low, the second splits the
  remainder into Mid/High. The three-way sum still reconstructs the original signal within
  ±0.1 dB (same guarantee class as v1's existing 2-band test, extended to 3 bands).
- **Deliberate engineering choice not copied from the reference**: the reference class's own
  three bands are *not* flat-summing — its Low band has an independently adjustable low-pass
  and its High/Treble band has an independently adjustable high-pass with no coupling between
  them, so overlaps/gaps between bands are possible by design (it optimizes for tone-shaping
  flexibility, not phase-coherent reconstruction). Crypta keeps its existing, more rigorous
  flat-summing LR4 discipline instead of copying this looser behavior — the goal of this brief
  is to authentically match the reference class's *voicing and ballistics character*, not to
  reproduce an engineering shortcut that a rigorously-tested plugin doesn't need. This is called
  out explicitly so it isn't mistaken for an oversight.
- **Low band routed around the IR loader, mono-summed last** — matches the reference class's
  explicit "low band bypasses the cabsim... remains mono" architecture (research notes §3, §7
  gap 5). This is a structural relocation of the existing `IRLoader` stage: it now sits between
  the Mid+High sum and the final three-way sum, not after the full mix as in v1.
- Gate, Safety Clip, Output Trim, oversampling/latency-compensation architecture for the
  distortion bands: unchanged from v1. The high band's 4x oversampling and delay-compensation
  contract carries forward as-is (research found no reference-class-specific reason to change
  it — oversampling factor is an implementation detail invisible to the reference's own
  documentation).

## Module specifications (authentic behaviors, generically named)

### Split Low / Split High — crossover (was single Crossover Frequency)
- **`splitLowHz`** (was `Crossover Frequency`): 60–400 Hz, log taper, **default 120 Hz**
  (reasoned: centers inside the sourced 60–1000 Hz range's lower half, low enough to keep the
  fundamental of typical bass tunings — including drop tunings — inside the compressed-only Low
  band, consistent with the reference class's "low band never gets distorted" architecture;
  not a number taken directly from a source, flagged as reasoned).
- **`splitHighHz`** (NEW): 300–2000 Hz, log taper, **default 600 Hz** (reasoned: the reference
  class's Mid band is described as "fixed around 400Hz" as its distortion *character center*,
  not a band edge — since Crypta's Mid band needs an actual upper corner rather than a center
  frequency, 600 Hz is chosen as a plausible edge that keeps 400 Hz well inside the Mid band's
  passband rather than at its boundary; explicitly reasoned, not sourced to an exact crossover
  number, because the reference class does not publish one).
- Both remain real-time-safe coefficient recomputes, matching v1's existing `Crossover`
  contract; `splitHighHz` is clamped to always stay above `splitLowHz` by a minimum musical
  gap (reasoned safety margin, e.g. ≥ 1/3 octave) to avoid a degenerate near-zero-width Mid
  band.

### Low band: parallel compressor + level (ballistics re-sourced, structure unchanged)
- Keeps v1's fully parametric structure (threshold/ratio/attack/release/makeup/mix) — this is
  **more capable than the reference class**, which exposes only a single combined
  Compression knob with fixed ballistics. Crypta's adjustability is kept deliberately (a
  genuine improvement, not a gap), but the **defaults** are re-anchored to the sourced fixed
  values so that turning every knob to its default reproduces the reference character:
  - `lowCompRatio`: range unchanged 1…20, **default changes 4 → 2** (sourced exactly: "Ratio
    2.0").
  - `lowCompAttackMs`: range unchanged 0.1…100, **default changes 10 → 3 ms** (sourced exactly:
    "Attack 3ms").
  - `lowCompReleaseMs`: **range floor lowered from 10 ms to 5 ms** (breaking change, pre-1.0)
    so the sourced default is reachable; **default changes 120 → 6 ms** (sourced exactly:
    "Release 6ms"). This is a fast, gentle glue compressor, not a squash — document the
    manual's "New York style" framing as **retired**, replaced with "fast bus-style glue
    compression" language that matches what the reference class actually implements (research
    notes §4).
  - `lowCompThreshold`, `lowCompMakeup`, `lowCompMix`, `lowLevel`: ranges and defaults
    unchanged — no source contradicts v1's existing choices here, and the reference's single
    combined knob (0 to +10 dB "gain reduction and make up gain") doesn't cleanly decompose
    into separate threshold/makeup numbers to source against.

### Mid band (NEW): drive + level
- **`midDriveAmount`** (NEW, 0–100%, default 30%): staged saturation, structurally similar to
  v1's Wool voicing (cascaded tanh stages) since the reference class describes "multiple tube
  gain stages... designed for the Mid and Treble bands separately" (research notes §3, §7 gap
  7) — cascaded/staged saturation, not a single hard-clamp, is the directionally-sourced
  character for this band. Default 30% (reasoned: a mid-distorted band is new to Crypta and
  should be audibly present but not dominant out of the box, since it sits in the frequency
  range most likely to clash with the guitar wall it's meant to cut through, per the existing
  manual's own crossover-tuning tip about avoiding guitar-range competition).
- **`midLevel`** (NEW, −24…+12 dB, default 0 dB): matches the Low/High band's existing Level
  convention exactly.
- No filter/tone control on this band (matches the reference class exactly: "Mid Drive... Mid
  Level" only, no filter exposed) — deliberately minimal, not an oversight.
- Runs inside the same oversampled block as the High band's voicing stage (shares the
  oversampling instance for CPU efficiency; both bands' nonlinearities need the same
  anti-aliasing headroom).

### High band: Tight (pre-drive HPF, promoted) + Voicing + Drive + Tone + Blend + Level
- **`highTightHz`** (was Razor-only fixed 200 Hz internal constant, now a first-class,
  voicing-independent control): **20–500 Hz, log taper, default 100 Hz.** Sourced: the
  reference's high-band HPF is "pullable down to 100Hz" and is framed as the primary "fuzz or
  tightness" control for the whole band, plus a harshness-control role ("tames harshness...
  when it comes to high saturation drive sounds" — research notes §3, §5). 100 Hz is the
  sourced floor of the reference's own documented pull-down range, chosen as the default because
  it's the most fuzz-forward, most-quoted end of that range rather than an untested midpoint.
  Applies **before** the voicing stage for all three voicings (Gnaw/Wool/Razor), not just
  Razor — this retires Razor's old fixed-200 Hz internal pre-HPF entirely in favor of the new
  shared, adjustable control.
- **Voicing enum** (`Gnaw`/`Wool`/`Razor`): unchanged selectable set and persisted indices
  (hard backward-compatibility constraint, same convention as the rest of the suite). Character
  math (drive-gain ceilings, mid-hump/scoop filters) unchanged from v1 — no source in this pass
  gives exact per-voicing gain-stage numbers for the reference's own treble band, so v1's
  existing (already-flagged-as-engineering-default) voicing math is kept as-is rather than
  altered without a source.
- `highDrive`, `highTone`, `highBlend`, `highLevel`: ranges and defaults unchanged — no sourced
  reason to change any of these from v1.

### IR loader (cabinet simulation) — relocated, not respecified
- Structural move only (see Topology): now processes the **Mid+High post-sum signal only**,
  never the Low band. Enable/Mix parameters, convolution engine, and the "no IR loaded = bit-
  exact passthrough" guarantee all carry forward unchanged from v1. This matches the reference
  class's explicit "low band bypasses the cabsim, remains mono" architecture (research notes
  §3, §7 gap 5) — Crypta's low band already has no stereo-mono distinction to protect the same
  way (Crypta doesn't currently offer a stereo input mode toggle the way the reference does),
  so only the "no cab coloration on the low band" half of that reference behavior is adopted;
  the mono-forcing half is out of scope (no source pressure to add a feature Crypta doesn't
  otherwise have).

### Post-sum 4-band EQ — re-anchored default frequencies, structure unchanged
- Stays fully parametric (LowShelf/Peak/Peak/HighShelf, ±18 dB) — already a strictly more
  capable tool than the reference's fixed 6-band graphic EQ (100/250/500/1k/1.5k/5k Hz,
  ±12 dB), so no structural change. **Default corner frequencies re-anchored** to the sourced
  bass-tone-stack frequency set from the same design lineage as the named reference plugin
  (research notes §6), since v1's existing 100/500/2500/8000 Hz defaults were unsourced
  placeholders:
  - `eqLowShelfHz`: default changes 100 → **80 Hz** (sourced).
  - `eqPeak1Hz`: default changes 500 → **500 Hz** (already matched the sourced anchor —
    unchanged).
  - `eqPeak2Hz`: default changes 2500 → **2800 Hz** (sourced: the "presence/definition" high-
    mid anchor).
  - `eqHighShelfHz`: default changes 8000 → **5000 Hz** (sourced).
  - Gain/Q ranges and defaults (0 dB, unchanged Q defaults) stay as-is — EQ ships off/flat by
    default either way, so these are dormant-until-engaged anchors, not an audible v1→v2
    change unless a user (or a factory preset) turns the EQ on.

### Gate, Safety Clip, Bypass, Input/Output Trim — unchanged, explicitly validated
- No sourced reason to change any of these. Worth stating explicitly: the reference class's own
  gate is a single coarse knob with no separate ratio/attack/release — Crypta's four-parameter
  gate already exceeds it in capability, so this is a **confirmed-correct existing decision**,
  not an area silently left alone by omission.

## Factory Presets (for the M2 preset system — proposed, not yet implemented)

Generic descriptors only, no names/brands. Settings are starting points, not exact renders;
all reference the v2 3-band topology and re-sourced defaults above.

| Preset | Intent | Rough settings |
|---|---|---|
| **Glue & Grind** | The shipped default: fast/gentle low-band glue, moderate mid saturation, tight high-band fuzz. | Split Low 120 Hz · Split High 600 Hz · Low Comp Ratio 2:1 / Atk 3 ms / Rel 6 ms / Mix 100% · Mid Drive 30% · High Tight 100 Hz · Voicing Gnaw · High Drive 50% · Blend 100% |
| **Sub Lock** | Maximum low-end control for a dense mix; low band does almost all the work. | Split Low 90 Hz · Split High 500 Hz · Low Comp Ratio 3:1 / Atk 3 ms / Rel 6 ms / Makeup +2 dB · Mid Drive 15% · High Tight 150 Hz · Voicing Razor · High Drive 35% |
| **Throat** | Emphasizes the mid band's "throatier" documented character; the mid band carries most of the grind. | Split Low 100 Hz · Split High 800 Hz · Low Comp Ratio 2:1 · Mid Drive 65% · Mid Level +2 dB · High Tight 120 Hz · Voicing Wool · High Drive 30% |
| **Fuzz Wall** | Maximum documented "fuzz" pull: Tight at the sourced floor, aggressive high-band voicing. | Split Low 130 Hz · Split High 550 Hz · Low Comp Ratio 2:1 · Mid Drive 25% · High Tight 100 Hz · Voicing Wool · High Drive 85% · Blend 100% |
| **Cut Through** | Drop-tuned rhythm use case: crossover pushed up so more note body reaches the distorted bands (existing v1 manual tip, now spanning 2 splits). | Split Low 180 Hz · Split High 900 Hz · Low Comp Ratio 2:1 · Mid Drive 40% · High Tight 130 Hz · Voicing Razor · High Drive 55% |
| **Definition Only** | High band's harshness-control role showcased: Tight pulled up, Drive kept moderate, EQ presence bump engaged. | Split Low 110 Hz · Split High 650 Hz · Mid Drive 20% · High Tight 250 Hz · Voicing Razor · High Drive 30% · EQ Enable on, Peak2 2800 Hz +3 dB |
| **Clean Low, Loud Top** | Low band audibly present but untouched in character (Mix low), everything else pushed. | Split Low 120 Hz · Split High 600 Hz · Low Comp Mix 40% · Mid Drive 45% · High Tight 100 Hz · Voicing Gnaw · High Drive 75% |
| **Cab-Colored Grind** | Demonstrates the relocated IR loader coloring only the Mid+High path while the low end stays uncolored. | Split Low 120 Hz · Split High 600 Hz · Mid Drive 35% · High Tight 100 Hz · Voicing Wool · High Drive 60% · IR Enable on, Mix 70% |

## Guarantees & tests (Catch2; keep all still-valid v1 cases, extend for the 3-band rebuild)

1. **Three-way flat-sum:** Low + Mid + High reconstructs the input within ±0.1 dB across the
   band, for a swept combination of `splitLowHz`/`splitHighHz` settings (extends v1's existing
   2-band flat-sum test to the cascaded 3-band topology; the minimum-gap clamp between the two
   splits is exercised at its boundary).
2. **Low-band ballistics regression at the new defaults:** at `lowCompRatio=2`,
   `lowCompAttackMs=3`, `lowCompReleaseMs=6`, verify measured gain-reduction time-constants
   (10–90% attack/release settling) land within the compressor's own smoothing tolerance of the
   sourced 3 ms/6 ms figures — a category-specific measurable proof that the "glue, not squash"
   character is actually implemented, not just documented.
3. **Low band never reaches the IR loader:** with IR enabled and a non-trivial IR loaded,
   assert the Low band's output (isolated pre-sum) is bit-exact identical whether IR is on or
   off — the structural "low band bypasses cab-sim" guarantee, directly testable now that the
   IR loader has moved in the signal path.
4. **Mid band null/passthrough and monotonic-drive proofs:** `midDriveAmount = 0` is a
   passthrough within tolerance (mirrors existing per-stage null tests); harmonic energy
   increases monotonically with `midDriveAmount`, mirroring the existing Voicing monotonic-
   harmonic test pattern extended to the new Mid stage.
5. **Tight (highTightHz) is voicing-independent:** for a fixed Drive and a fixed low-frequency
   test tone, sweep `highTightHz` across its full 20–500 Hz range for *each* of the three
   voicings and assert low-frequency attenuation increases monotonically with `highTightHz` in
   all three cases (proof that Tight is no longer Razor-only, closing the gap identified in
   Why-v1-falls-short §3).
6. **EQ default-frequency anchors land correctly:** loading the default parameter state and
   reading back `eqLowShelfHz`/`eqPeak1Hz`/`eqPeak2Hz`/`eqHighShelfHz` matches the re-anchored
   80/500/2800/5000 Hz values exactly (regression guard against silently drifting back to the
   old unsourced defaults).
7. **State migration tolerance (v1 → v2, structural):** old (v1) saved state — with a single
   `crossoverFrequency` (no `splitLowHz`/`splitHighHz`), no mid-band parameters, and
   `lowCompRatio`/`Attack`/`Release` at v1's old defaults — loads without crashing; the old
   single crossover value maps to `splitHighHz` (best-effort: the v1 split point is closer in
   role to the reference's high-band edge than to the new low-band edge, since v1 never had a
   dedicated mid band), **clamped into the new 300–2000 Hz range on import — NOTE that v1's
   SHIPPED DEFAULT crossover is 250 Hz, i.e. below the new floor, so the single most common
   migration path (untouched v1 session) hits this clamp and must land exactly at 300 Hz; a
   dedicated test asserts this default-session path** — while `splitLowHz` falls back to its
   new v2 default; all new mid-band
   parameters fall back to their v2 defaults (never zero/garbage); old `lowCompRatio`/`Attack`/
   `Release` values, if explicitly set away from v1's old defaults by the user, are preserved
   as-is (only the *shipped default* changes, not a forced migration of a user's deliberate old
   setting) — documented as a lossy, best-effort migration per Versioning below.
8. **NaN/Inf robustness across the rebuilt topology:** sweep `splitLowHz`, `splitHighHz` (incl.
   their minimum-gap boundary), `midDriveAmount`, `highTightHz` to their extremes combined with
   extreme Drive/Level/EQ settings; confirm no NaN/Inf propagates (extends v1's existing
   robustness-sweep pattern to every new/changed control).
9. **Real-time-safety carry-forward:** no new allocation on the audio thread from the second
   crossover stage, the Mid band's saturation, or the relocated IR loader; existing
   `docs/architecture.md` latency-compensation contract and its tests remain green, updated
   only where the IR loader's new position changes which stage owns which compensation delay
   (the Low band's already-zero-latency compressor path needs no new compensation; only the
   Mid+High branch's existing oversampling latency is now also the point after which the IR
   loader's own zero-added-latency convolution runs).
10. **Preset round-trip:** every factory preset in the table above loads, all parameter values
    land within tolerance of their specified settings, and produces no NaN/Inf/silence on a
    standard test signal.

## Honesty & framing

- `crypta-research-notes.md` ships the sourced findings (quotes + URLs) — the topology and
  default changes in this brief are **research-derived from the named reference plugin's own
  official user manual, a third-party professional review, the same design lineage's hardware
  product manual, and general parallel-bus-compression community/vendor consensus — not
  measured against the reference plugin's actual audio output, its DSP source code, or any
  hardware unit by this project.** Say so in the manual, in the same place v1 already discloses
  that voicing character is "engineering defaults... not yet finalized by ear against reference
  material" — this brief narrows, but does not eliminate, that disclosure.
- The **single most load-bearing sourced number** in this brief — the low-band compressor's
  fixed ratio 2.0 / attack 3 ms / release 6 ms — comes from the reference plugin's own official
  PDF manual, verbatim and unambiguous ("Fixed settings: Attack 3ms - Release 6ms - Ratio 2.0"),
  the strongest-grade source used in this pass.
- Several new defaults (`splitLowHz` 120 Hz, `splitHighHz` 600 Hz, `midDriveAmount` 30%) are
  **reasoned engineering choices anchored to the sourced qualitative behavior** (a 3-band split
  exists; the mid band's character centers "around 400Hz"), **not numbers taken directly from a
  published crossover-frequency spec** — the reference class does not publish exact crossover
  corner frequencies, only a description of each band's own filter behavior. Each is called out
  individually above; do not represent them as measured/published reference values.
- The mid band's saturation math (cascaded/staged shaping, reusing the existing Wool voicing's
  two-stage tanh structure) is a **directional** finding — the reference's manual confirms
  "multiple tube gain stages" exist for Mid and Treble but gives no stage count, gain-per-stage,
  or transfer-function detail. v2 reuses v1's existing, already-implemented cascaded-stage math
  as the closest available building block, not because it's been confirmed to match the
  reference's actual circuit.
- Manual notes that the named commercial plugin and its designer's other published hardware are
  cited as documented public sources for the *technique and category conventions*, without
  implying endorsement, sponsorship, or affiliation — consistent with the existing README's
  non-affiliation language, unchanged by this brief.
- Out of scope for v2 (explicitly): a continuously-blendable pair of distortion characters
  (mirroring the same design lineage's hardware "Mod" blend control) replacing the discrete
  Gnaw/Wool/Razor selector — noted as a possible future direction in the research notes, not
  adopted here because it would break the frozen voicing-enum backward-compatibility
  constraint; a stereo input-mode toggle that would make the Low band's "stays mono" reference
  behavior meaningful for Crypta (Crypta has no stereo-mode distinction today); factory cabinet
  IRs and a GUI file browser (already tracked as a later-milestone item in v1's own roadmap,
  unaffected by this brief). Custom GUI remains M3 as in v1's roadmap; this brief is
  DSP/parameter-layer only.

## Versioning

Ships as **v0.2.0** (breaking parameter changes are acceptable pre-1.0, per suite convention;
the Voicing enum's persisted indices remain a hard-frozen exception carried forward from v1).
State migration = tolerant import: old single-crossover, 2-band v1 state loads without
crashing, maps its one crossover value to the new `splitHighHz` (best-effort, lossy — see
Guarantee 7), all new mid-band/Tight parameters fall back to v2 defaults, and unknown IDs in
either direction are ignored rather than fatal (same forward/backward tolerance pattern as the
rest of the suite). CHANGELOG documents the 2-band → 3-band restructuring, the low-band
compressor's ballistics re-source (and retirement of the "New York style" description), and the
IR loader's relocation as the headline v0.2.0 changes.
