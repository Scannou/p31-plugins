//
//  MeterScaleTests.cpp
//  p31-core-tests (ported from foo_jl_levelmeter_mac)
//
//  Bar scale mapping and tick placement, VU face geometry.
//

#include "../TestHarness.h"
#include <p31/meter/MeterScale.h>

#include <cmath>

using namespace p31::meter;

namespace {

void testPositions() {
    for (ScaleSpacing sp : { ScaleSpacing::Linear, ScaleSpacing::Extended }) {
        g_context = sp == ScaleSpacing::Linear ? "linear" : "extended";
        BarScale s(-60, sp);
        EXPECT_FLOAT_EQ(s.position(0.0), 1.0, "0 dBFS at the top");
        EXPECT_FLOAT_EQ(s.position(3.0), 1.0, "overs clamp to the top");
        EXPECT_FLOAT_EQ(s.position(-60.0), 0.0, "floor at the bottom");
        EXPECT_FLOAT_EQ(s.position(kFloorDb), 0.0, "silence at the bottom");
        double last = -1;
        bool rising = true;
        for (int db = -60; db <= 0; ++db) {
            const double p = s.position(db);
            if (p <= last) rising = false;
            last = p;
        }
        EXPECT(rising, "position rises with level");
    }
    g_context = "deep extended";
    // Below -70 the scale keeps rising instead of flattening at the floor.
    BarScale deep(-120, ScaleSpacing::Extended);
    EXPECT(deep.position(-100) > 0.0, "-100 above the floor");
    EXPECT(deep.position(-80) > deep.position(-100), "-80 above -100");
    EXPECT(deep.position(-70) > deep.position(-80), "-70 above -80");

    g_context = "linear midpoint";
    EXPECT_FLOAT_EQ(BarScale(-60, ScaleSpacing::Linear).position(-30), 0.5, "-30 halfway");
    g_context = "extended top";
    // Extended spacing gives the top 20 dB more than a third of the bar.
    EXPECT(BarScale(-60, ScaleSpacing::Extended).position(-20) < 0.6, "top 20 dB get over 40%");
}

void checkTicks(const BarScale &s, double length, double labelPx, const char *what) {
    g_context = what;
    const auto ticks = s.ticks(length, labelPx);
    EXPECT(!ticks.empty(), "has ticks");
    if (ticks.empty()) return;
    EXPECT(ticks.front().major, "top tick labelled");
    EXPECT(std::fabs(ticks.front().pos - 1.0) < 1e-9, "top tick at 0 dBFS");
    double lastMajorPx = -1e9;
    bool spaced = true, inRange = true;
    for (const auto &t : ticks) {
        if (t.db < s.floorDb() - 1e-9 || t.db > 1e-9) inRange = false;
        if (!t.major) continue;
        const double px = (1.0 - t.pos) * length;
        if (px - lastMajorPx < labelPx - 1e-9) spaced = false;
        lastMajorPx = px;
    }
    EXPECT(spaced, "labels never closer than labelPx");
    EXPECT(inRange, "ticks inside the scale");
}

void testTicks() {
    checkTicks(BarScale(-60, ScaleSpacing::Extended), 600, 12, "extended tall");
    checkTicks(BarScale(-60, ScaleSpacing::Extended), 120, 12, "extended short");
    checkTicks(BarScale(-90, ScaleSpacing::Linear), 300, 12, "linear -90");
    checkTicks(BarScale(-120, ScaleSpacing::Extended), 400, 12, "extended -120");
    checkTicks(BarScale(-80, ScaleSpacing::Linear), 250, 12, "linear -80");
    checkTicks(BarScale(-24, ScaleSpacing::Linear), 700, 26, "linear wide");
    checkTicks(BarScale(-60, ScaleSpacing::Extended, -20.0), 400, 12, "K-20");

    g_context = "round labels";
    // Labels come from a 1-2-5 ladder: every label is a multiple of its step.
    const auto ticks = BarScale(-60, ScaleSpacing::Extended).ticks(300, 12);
    bool round = true;
    for (const auto &t : ticks) {
        if (t.major && t.label % 2 != 0 && t.label % 5 != 0 && t.label != 0) round = false;
    }
    EXPECT(round, "labels are multiples of 2 or 5");

    g_context = "K labels";
    const auto k = BarScale(-60, ScaleSpacing::Linear, -14.0).ticks(400, 12);
    EXPECT_EQ(k.front().label, 14, "K-14 tops out at +14");
    bool hasZero = false;
    for (const auto &t : k) if (t.major && t.label == 0) hasZero = std::fabs(t.db + 14.0) < 1e-9;
    EXPECT(hasZero, "K-14 0 sits at -14 dBFS");

    g_context = "degenerate";
    EXPECT(BarScale(-60, ScaleSpacing::Linear).ticks(0, 12).empty(), "no length, no ticks");
}

void testVuFace() {
    g_context = "vu face";
    EXPECT_FLOAT_EQ(vu_face::fraction(3.0), 1.0, "+3 VU at the end of the sweep");
    EXPECT(std::fabs(vu_face::fraction(0.0) - 0.7079) < 1e-3, "0 VU at 71% (amplitude scale)");
    EXPECT(std::fabs(vu_face::fraction(-20.0) - 0.0708) < 1e-3, "-20 VU near the rest");
    EXPECT_FLOAT_EQ(vu_face::fraction(kFloorDb), 0.0, "silence rests at 0");
    EXPECT(std::fabs(vu_face::percentFraction(100) - vu_face::fraction(0.0)) < 1e-9, "100% = 0 VU");
    EXPECT_FLOAT_EQ(vu_face::angle(3.0), vu_face::kSweepDegrees / 2.0, "angle at +3");
    EXPECT_FLOAT_EQ(vu_face::angle(kFloorDb), -vu_face::kSweepDegrees / 2.0, "angle at rest");
    EXPECT(vu_face::majorMarks().front() == -20 && vu_face::majorMarks().back() == 3, "marks span -20..+3");
}

} // namespace

TEST_CASE("Meter scale: positions") { testPositions(); }
TEST_CASE("Meter scale: ticks") { testTicks(); }
TEST_CASE("Meter scale: vu face") { testVuFace(); }
