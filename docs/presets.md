# Factory presets

Nine factory presets ship with Crypta's v0.2.0 M2 preset system. All settings are starting
points designed against the research-derived v0.2.0 defaults (`docs/design-brief.md`'s Factory
Presets section), not exact renders against any reference material. `IR Enable` presets below
turn the IR loader stage on but do not load an actual impulse response - v0.2.0 does not yet
bundle factory cabinet IRs (same as v0.1.x; see `docs/manual.md`), so those presets currently
run the loader in its safe-by-default identity-passthrough state until a user loads their own IR.

| Preset | Category | Intent |
|---|---|---|
| **Default** | Init | The plain `ParameterLayout` defaults, loaded on a fresh instance - identical settings to "Glue & Grind" below, filed separately under the technical `Init`/"Default" name the preset system's default-resolution order looks for. |
| **Glue & Grind** | Bass | The shipped default character: fast/gentle low-band glue compression, moderate mid saturation, tight high-band fuzz. |
| **Sub Lock** | Bass | Maximum low-end control for a dense mix; the low band does almost all the work, mids/highs kept modest. |
| **Throat** | Bass | Emphasizes the mid band's documented "throatier" character; the mid band carries most of the grind. |
| **Fuzz Wall** | Bass | Maximum documented "fuzz" pull: Tight at its sourced floor (100 Hz), aggressive Wool voicing at high Drive. |
| **Cut Through** | Bass | Drop-tuned rhythm use case: both splits pushed up so more note body reaches the distorted bands. |
| **Definition Only** | Bass | Showcases the high band's harshness-control role: Tight pulled up, Drive kept moderate, EQ presence bump engaged. |
| **Clean Low, Loud Top** | Bass | Low band audibly present but mostly uncompressed (Mix pulled down), mid/high pushed harder. |
| **Cab-Colored Grind** | Bass | Demonstrates the v0.2.0-relocated IR loader coloring only the Mid+High path while the low end stays uncolored. |
