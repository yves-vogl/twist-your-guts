#include "dsp/SplitGap.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

// docs/design-brief.md's Topology/Split Low/Split High sections: splitHighHz
// must always stay above splitLowHz by at least a 1/3-octave musical gap,
// even though the two parameters' independent ranges overlap (splitLowHz up
// to 400 Hz, above splitHighHz's own 300 Hz floor). These tests exercise
// cryp::clampSplitHighHz() - the single pure function that enforces that
// clamp - directly at and around its boundary.
TEST_CASE ("clampSplitHighHz: leaves a requested value alone when it already clears the minimum gap", "[splitgap][dsp]")
{
    // 120 Hz -> minimum gap floor is 120 * 2^(1/3) ~= 151.2 Hz; 600 Hz clears
    // it by a wide margin, so the clamp must be a no-op.
    CHECK (cryp::clampSplitHighHz (120.0f, 600.0f) == Catch::Approx (600.0f));
}

TEST_CASE ("clampSplitHighHz: raises a too-close requested value up to exactly the minimum gap", "[splitgap][dsp]")
{
    constexpr float splitLowHz = 120.0f;
    const auto expectedFloorHz = splitLowHz * std::pow (2.0f, 1.0f / 3.0f);

    // Requesting splitHighHz *equal to* splitLowHz (a degenerate zero-width
    // Mid band if left unclamped) must be raised to the floor.
    CHECK (cryp::clampSplitHighHz (splitLowHz, splitLowHz) == Catch::Approx (expectedFloorHz));

    // Requesting splitHighHz *below* splitLowHz (an inverted Mid band) must
    // also be raised to the same floor, not just left at the requested
    // (inverted) value.
    CHECK (cryp::clampSplitHighHz (splitLowHz, 50.0f) == Catch::Approx (expectedFloorHz));
}

TEST_CASE ("clampSplitHighHz: tracks the floor as splitLowHz moves, across the full overlapping range", "[splitgap][dsp]")
{
    // splitLowHz's own range extends to 400 Hz, above splitHighHz's 300 Hz
    // floor - exercise every combination where the raw parameter values
    // alone would violate the minimum gap.
    const float splitLowValuesHz[] = { 60.0f, 120.0f, 200.0f, 300.0f, 400.0f };

    for (const auto splitLowHz : splitLowValuesHz)
    {
        const auto expectedFloorHz = splitLowHz * std::pow (2.0f, 1.0f / 3.0f);

        INFO ("splitLowHz = " << splitLowHz);
        // A requested splitHighHz pinned to splitLowHz's own value is always
        // a violation (zero gap) regardless of which splitLowHz value it is.
        CHECK (cryp::clampSplitHighHz (splitLowHz, splitLowHz) == Catch::Approx (expectedFloorHz));

        // A requested splitHighHz comfortably above the floor is always left
        // untouched.
        CHECK (cryp::clampSplitHighHz (splitLowHz, expectedFloorHz * 2.0f) == Catch::Approx (expectedFloorHz * 2.0f));
    }
}

TEST_CASE ("clampSplitHighHz: is a real-time-safe pure function (no allocation, deterministic)", "[splitgap][dsp]")
{
    // No explicit allocation-tracking here (this codebase verifies real-time
    // safety primarily by design/API, matching the rest of the suite) - this
    // asserts the determinism property that safety actually depends on:
    // repeated calls with the same inputs always produce the exact same
    // output, with no hidden state.
    for (int i = 0; i < 100; ++i)
        CHECK (cryp::clampSplitHighHz (150.0f, 700.0f) == Catch::Approx (700.0f));
}
