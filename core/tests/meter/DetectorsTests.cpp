//
//  DetectorsTests.cpp
//  p31-core-tests (ported from foo_jl_levelmeter_mac)
//
//  RMS window, VU movement and quasi-peak rectifier.
//

#include "../TestHarness.h"
#include <p31/meter/Detectors.h>

#include <cmath>

using namespace p31::meter;

namespace {

const double kPi = 3.14159265358979323846;
const double kSR = 48000.0;

void testConversions() {
    g_context = "conversions";
    EXPECT_FLOAT_EQ(linearToDb(1.0), 0.0, "unity is 0 dB");
    EXPECT(std::fabs(linearToDb(0.5) + 6.0206) < 1e-3, "half is -6 dB");
    EXPECT_FLOAT_EQ(linearToDb(0.0), kFloorDb, "silence is the floor");
    EXPECT(std::fabs(dbToLinear(-20.0) - 0.1) < 1e-9, "-20 dB");
}

void testRms() {
    g_context = "rms sine";
    RmsWindow w;
    w.configure(kSR, 0.3);
    for (int i = 0; i < (int)(kSR * 0.5); ++i) w.push((float)std::sin(2.0 * kPi * 1000.0 * i / kSR));
    EXPECT(std::fabs(w.rms() - 0.70711) < 0.002, "sine RMS is 1/sqrt 2");

    g_context = "rms rise";
    RmsWindow r;
    r.configure(kSR, 0.3);
    for (int i = 0; i < (int)(kSR * 0.15); ++i) r.push(1.0f);
    EXPECT(std::fabs(r.meanSquare() - 0.5) < 0.01, "half-filled window counts the rest as silence");

    g_context = "rms drift";
    // Long runs must not drift (rebuild keeps the sum honest).
    RmsWindow d;
    d.configure(kSR, 0.05);
    for (int i = 0; i < (int)(kSR * 5); ++i) d.push(i % 2 ? 0.9f : -0.9f);
    for (int i = 0; i < (int)(kSR * 0.05); ++i) d.push(0.0f);
    EXPECT(d.meanSquare() < 1e-9, "returns to zero");

    g_context = "rms reconfigure";
    d.configure(kSR, 0.1);
    EXPECT_FLOAT_EQ(d.meanSquare(), 0.0, "new window starts empty");
}

// Step response of the movement on a steady sine.
void testVuRise() {
    g_context = "vu rise";
    VuBallistics vu;
    vu.configure(kSR);
    double t99 = -1, maxV = 0;
    for (int i = 0; i < (int)(kSR * 1.5); ++i) {
        vu.push((float)(std::sqrt(2.0) * std::sin(2.0 * kPi * 1000.0 * i / kSR)));   // RMS 1
        if (t99 < 0 && vu.value() >= 0.99) t99 = (i + 1) / kSR;
        maxV = std::max(maxV, vu.value());
    }
    EXPECT(t99 > 0.27 && t99 < 0.33, "99% in 300 ms +-10%");
    EXPECT(maxV > 1.005 && maxV < 1.02, "overshoot between 0.5% and 2%");
    EXPECT(std::fabs(vu.value() - 1.0) < 0.01, "settles on the sine's RMS");

    g_context = "vu stops";
    VuBallistics s;
    s.configure(kSR);
    s.setMaxValue(1.2);
    for (int i = 0; i < (int)kSR; ++i) s.push(10.0f);
    EXPECT_FLOAT_EQ(s.value(), 1.2, "pins at the end stop");
    // Released from the stop, the needle starts moving at once.
    for (int i = 0; i < (int)(kSR * 0.05); ++i) s.push(0.0f);
    EXPECT(s.value() < 1.15, "leaves the stop promptly");
    for (int i = 0; i < (int)(kSR * 2); ++i) s.push(0.0f);
    EXPECT(s.value() >= 0.0 && s.value() < 0.01, "falls to rest, never below zero");
}

void testPpm() {
    g_context = "ppm burst";
    for (NeedleBallistics b : { NeedleBallistics::PpmDin, NeedleBallistics::PpmBbc, NeedleBallistics::PpmNordic }) {
        const PpmTiming t = ppmTiming(b);
        QuasiPeak q;
        q.configure(kSR, t.attackSeconds, t.fallDbPerSecond);
        // A DC burst isolates the attack response from sine ripple.
        const double burst = b == NeedleBallistics::PpmNordic ? 0.005 : 0.010;
        for (int i = 0; i < (int)(kSR * burst); ++i) q.push(1.0f);
        // Undo the sine calibration: compare against the raw burst level.
        QuasiPeak steady;
        steady.configure(kSR, t.attackSeconds, t.fallDbPerSecond);
        for (int i = 0; i < (int)(kSR * 0.5); ++i) steady.push(1.0f);
        const double reading = linearToDb(q.value() / steady.value());
        const double want = b == NeedleBallistics::PpmBbc ? -2.5 : -1.0;
        EXPECT(std::fabs(reading - want) < 0.2, "burst response");

        // Then the fall rate.
        q.reset();
        for (int i = 0; i < (int)(kSR * 0.5); ++i) q.push(1.0f);
        const double start = linearToDb(q.value());
        for (int i = 0; i < (int)kSR; ++i) q.push(0.0f);
        EXPECT(std::fabs((start - linearToDb(q.value())) - t.fallDbPerSecond) < 0.1, "fall rate");
    }

    g_context = "ppm sine calibration";
    QuasiPeak q;
    const PpmTiming t = ppmTiming(NeedleBallistics::PpmDin);
    q.configure(kSR, t.attackSeconds, t.fallDbPerSecond);
    for (int i = 0; i < (int)(kSR * 0.5); ++i) q.push((float)(std::sqrt(2.0) * 0.1 * std::sin(2.0 * kPi * 1000.0 * i / kSR)));
    EXPECT(std::fabs(linearToDb(q.value()) + 20.0) < 0.3, "steady sine reads its RMS");
}

} // namespace

TEST_CASE("Meter detectors: conversions") { testConversions(); }
TEST_CASE("Meter detectors: rms") { testRms(); }
TEST_CASE("Meter detectors: vu rise") { testVuRise(); }
TEST_CASE("Meter detectors: ppm") { testPpm(); }
