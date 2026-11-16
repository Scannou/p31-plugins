//
//  AutoTopTests.cpp
//  p31-core-tests
//

#include <p31/core/AutoTop.h>

#include <doctest/doctest.h>

#include <cmath>
#include <limits>

using p31::AutoTop;

namespace {

constexpr double kFrame = 1.0 / 60.0;

AutoTop rider(float startTop = -8.0f) {
    AutoTop a;
    AutoTop::Settings s;
    s.headroomDb = 6.0f;
    s.minTopDb = -60.0f;
    s.maxTopDb = 0.0f;
    a.configure(s);
    a.reset(startTop);
    return a;
}

// `seconds` of a steady level, one display frame at a time.
float hold(AutoTop& a, float levelDb, double seconds) {
    float top = a.topDb();
    for (double t = 0.0; t < seconds; t += kFrame) top = a.process(levelDb, kFrame);
    return top;
}

} // namespace

TEST_CASE("AutoTop: starts at the display's own top") {
    AutoTop a = rider(-8.0f);
    CHECK(a.topDb() == -8.0f);
    // Nothing shown yet: it stays.
    CHECK(a.process(-std::numeric_limits<float>::infinity(), kFrame) == -8.0f);
    CHECK(a.process(-150.0f, kFrame) == -8.0f);
}

TEST_CASE("AutoTop: a louder level raises the top at once") {
    AutoTop a = rider(-30.0f);
    CHECK(a.process(-10.0f, kFrame) == doctest::Approx(-4.0f));
    // Never past the highest top.
    CHECK(a.process(3.0f, kFrame) == doctest::Approx(0.0f));
}

TEST_CASE("AutoTop: quiet material brings the top down gently") {
    AutoTop a = rider(-8.0f);
    // The first frame moves only part of the way.
    const float first = a.process(-40.0f, kFrame);
    CHECK(first < -8.0f);
    CHECK(first > -12.0f);
    // A second later it has settled: the level sits the headroom below.
    CHECK(hold(a, -40.0f, 1.0) == doctest::Approx(-34.0f).epsilon(0.01));
    // Never below the lowest top.
    CHECK(hold(a, -90.0f, 30.0) == doctest::Approx(-60.0f));
}

TEST_CASE("AutoTop: after a loud passage the envelope falls as the scope's") {
    AutoTop a = rider(-8.0f);
    a.process(-10.0f, kFrame);
    // 3 s time constant: 2.9 dB per second, once the top has caught up
    // with the envelope.
    const float early = hold(a, -40.0f, 2.0);
    const float later = hold(a, -40.0f, 1.0);
    CHECK(early - later == doctest::Approx(8.6859f / 3.0f).epsilon(0.02));
    CHECK(later > -10.0f - 3.0f * 8.6859f / 3.0f + 6.0f - 0.5f);
}

TEST_CASE("AutoTop: silence holds the top") {
    AutoTop a = rider(-8.0f);
    hold(a, -40.0f, 2.0);
    const float settled = a.topDb();
    CHECK(hold(a, -std::numeric_limits<float>::infinity(), 10.0) == doctest::Approx(settled).epsilon(0.001));
    CHECK(hold(a, -120.0f, 10.0) == doctest::Approx(settled).epsilon(0.001));
    // No time, no change.
    CHECK(a.process(-5.0f, 0.0) == doctest::Approx(settled).epsilon(0.001));
}

TEST_CASE("AutoTop: reset forgets the envelope") {
    AutoTop a = rider(-8.0f);
    a.process(-2.0f, kFrame);
    a.reset(-20.0f);
    CHECK(a.topDb() == -20.0f);
    CHECK(a.process(-50.0f, kFrame) < -20.0f);
}
