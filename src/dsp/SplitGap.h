#pragma once

#include <cmath>

// v0.2.0 3-band rebuild (docs/design-brief.md, "Split Low / Split High"
// section): the two cascaded LR4 crossovers' cutoffs are independently
// automatable parameters with overlapping ranges - splitLowHz extends up to
// 400 Hz, above splitHighHz's own 300 Hz floor - so without an explicit
// clamp, a user (or a preset, or automation) could push splitLowHz above
// splitHighHz and collapse the Mid band to a degenerate, near-zero-width (or
// inverted) passband. This header is the single, pure, real-time-safe
// function that enforces the brief's "clamped to always stay above
// splitLowHz by a minimum musical gap (reasoned safety margin, e.g. >= 1/3
// octave)" rule - used identically by CryptaAudioProcessor and by the test
// suite (SplitGapTests.cpp, ThreeBandFlatSumTests.cpp) so the exact boundary
// behaviour is directly testable rather than only implicit in the processor.
namespace cryp
{
    // Reasoned (not sourced - see docs/design-brief.md) minimum gap between
    // the two crossover points, in octaves. 1/3 octave is comfortably above
    // the transition-band width either LR4 crossover needs to sound like a
    // normal crossover rather than an abrupt notch/bump, while still leaving
    // most of each parameter's own musically-useful range unclamped.
    inline constexpr float minSplitGapOctaves = 1.0f / 3.0f;

    // Returns the splitHighHz value actually fed to the second (Mid/High)
    // crossover, given the raw (possibly too-close, possibly even inverted)
    // splitLowHz/requestedSplitHighHz parameter values. Pure and real-time
    // safe: no allocation, no state, just a floating-point floor.
    inline float clampSplitHighHz (float splitLowHz, float requestedSplitHighHz) noexcept
    {
        const auto minimumSplitHighHz = splitLowHz * std::pow (2.0f, minSplitGapOctaves);
        return requestedSplitHighHz > minimumSplitHighHz ? requestedSplitHighHz : minimumSplitHighHz;
    }
}
