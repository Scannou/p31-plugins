//
//  LoudnessTests.cpp
//  p31-core-tests (ported from foo_jl_levelmeter_mac)
//
//  K-weighting against the BS.1770 coefficient table, and the EBU Tech 3341
//  (loudness) and Tech 3342 (loudness range) minimum-requirement cases.
//

#include "../TestHarness.h"
#include <p31/meter/Loudness.h>

#include <cmath>
#include <vector>

using namespace p31::meter;

namespace {

const double kPi = 3.14159265358979323846;

struct Segment {
    double dbfs;      // sine peak level per channel
    double seconds;
};

// Stereo 1 kHz sine segments, both channels in phase, fed in 10 ms pieces
// like the player delivers them.
void feed(LoudnessMeter &m, double sr, const std::vector<Segment> &segments, double hz = 1000.0) {
    double phase = 0.0;
    std::vector<float> buf;
    for (const Segment &s : segments) {
        const double amp = std::pow(10.0, s.dbfs / 20.0);
        size_t remaining = (size_t)std::lround(s.seconds * sr);
        while (remaining > 0) {
            const size_t n = std::min(remaining, (size_t)(sr / 100.0) + 7);
            buf.resize(n * 2);
            for (size_t i = 0; i < n; ++i) {
                const float v = (float)(amp * std::sin(phase));
                buf[2 * i] = buf[2 * i + 1] = v;
                phase += 2.0 * kPi * hz / sr;
                if (phase > 2.0 * kPi) phase -= 2.0 * kPi;
            }
            m.process(buf.data(), n);
            remaining -= n;
        }
    }
}

double integratedOf(const std::vector<Segment> &segments, double sr = 48000.0) {
    LoudnessMeter m;
    m.configure(sr, 2);
    feed(m, sr, segments);
    return m.integrated();
}

double rangeOf(const std::vector<Segment> &segments, double sr = 48000.0) {
    LoudnessMeter m;
    m.configure(sr, 2);
    feed(m, sr, segments);
    return m.range();
}

void testCoefficients() {
    g_context = "K-weighting 48k";
    Biquad shelf, hp;
    kWeightingFilters(48000.0, shelf, hp);
    auto near = [](double a, double b) { return std::fabs(a - b) < 1e-7; };
    EXPECT(near(shelf.b0, 1.53512485958697), "shelf b0");
    EXPECT(near(shelf.b1, -2.69169618940638), "shelf b1");
    EXPECT(near(shelf.b2, 1.19839281085285), "shelf b2");
    EXPECT(near(shelf.a1, -1.69065929318241), "shelf a1");
    EXPECT(near(shelf.a2, 0.73248077421585), "shelf a2");
    EXPECT(near(hp.a1, -1.99004745483398), "high pass a1");
    EXPECT(near(hp.a2, 0.99007225036621), "high pass a2");
}

void testSteadySine() {
    for (double sr : { 44100.0, 48000.0, 96000.0 }) {
        g_context = "steady sine " + std::to_string((int)sr);
        LoudnessMeter m;
        m.configure(sr, 2);
        feed(m, sr, { { -23.0, 20.0 } });
        EXPECT(std::fabs(m.momentary() + 23.0) < 0.1, "momentary -23");
        EXPECT(std::fabs(m.shortTerm() + 23.0) < 0.1, "short-term -23");
        EXPECT(std::fabs(m.integrated() + 23.0) < 0.1, "integrated -23");
        EXPECT(m.range() < 0.1, "steady tone has no range");
        EXPECT(std::fabs(m.measuredSeconds() - 20.0) < 0.01, "measured time");
    }

    g_context = "level -33";
    EXPECT(std::fabs(integratedOf({ { -33.0, 20.0 } }) + 33.0) < 0.1, "integrated -33");

    g_context = "mono";
    // Mono counts once: the same tone in one channel is 3 dB quieter than
    // the stereo pair.
    LoudnessMeter m;
    m.configure(48000.0, 1);
    feed(m, 48000.0, { { -23.0, 10.0 } });
    EXPECT(std::fabs(m.integrated() + 26.01) < 0.1, "mono reads -26");
}

// EBU Tech 3341 cases 3-5: the gates must ignore the quiet parts.
void testGating() {
    g_context = "Tech 3341 case 3";
    EXPECT(std::fabs(integratedOf({ { -36, 10 }, { -23, 60 }, { -36, 10 } }) + 23.0) < 0.1, "relative gate");
    g_context = "Tech 3341 case 4";
    EXPECT(std::fabs(integratedOf({ { -72, 10 }, { -36, 10 }, { -23, 60 }, { -36, 10 }, { -72, 10 } }) + 23.0) < 0.1,
          "absolute + relative gate");
    g_context = "Tech 3341 case 5";
    EXPECT(std::fabs(integratedOf({ { -26, 20 }, { -20, 20.1 }, { -26, 20 } }) + 23.0) < 0.1, "mixed levels");

    g_context = "below absolute gate";
    EXPECT(std::isinf(integratedOf({ { -80, 5 } })), "silence-level audio has no integrated value");
}

// EBU Tech 3342 cases 1-4.
void testRange() {
    g_context = "Tech 3342 case 1";
    EXPECT(std::fabs(rangeOf({ { -20, 20 }, { -30, 20 } }) - 10.0) < 1.0, "LRA 10");
    g_context = "Tech 3342 case 2";
    EXPECT(std::fabs(rangeOf({ { -20, 20 }, { -15, 20 } }) - 5.0) < 1.0, "LRA 5");
    g_context = "Tech 3342 case 3";
    EXPECT(std::fabs(rangeOf({ { -40, 20 }, { -20, 20 } }) - 20.0) < 1.0, "LRA 20");
    g_context = "Tech 3342 case 4";
    EXPECT(std::fabs(rangeOf({ { -50, 20 }, { -35, 20 }, { -20, 20 }, { -35, 20 }, { -50, 20 } }) - 15.0) < 1.0, "LRA 15");
}

void testWindows() {
    g_context = "windows";
    LoudnessMeter m;
    m.configure(48000.0, 2);
    EXPECT(std::isinf(m.momentary()), "nothing before audio");
    EXPECT(std::isinf(m.integrated()), "no integrated before audio");

    // 200 ms of tone: the 400 ms window is half full, so -3 dB.
    feed(m, 48000.0, { { -23.0, 0.2 } });
    EXPECT(std::fabs(m.momentary() + 26.0) < 0.3, "half-filled window reads 3 dB low");
    EXPECT(std::isinf(m.integrated()), "no gating block before 400 ms");

    feed(m, 48000.0, { { -23.0, 4.0 } });
    EXPECT(std::fabs(m.momentary() + 23.0) < 0.1, "momentary settles");

    // Pausing lets the windows fall but keeps the integrated value.
    const double before = m.integrated();
    m.pushSilence(1.0);
    EXPECT(std::isinf(m.momentary()), "momentary falls to silence after a pause");
    EXPECT(m.shortTerm() < -24.0, "short-term falls during a pause");
    EXPECT_FLOAT_EQ(m.integrated(), before, "pause keeps the integrated value");

    // Resuming does not count blocks that straddle the pause.
    feed(m, 48000.0, { { -40.0, 0.3 } });
    EXPECT_FLOAT_EQ(m.integrated(), before, "no gating block until 400 ms of unbroken audio");

    m.reset();
    EXPECT(std::isinf(m.integrated()), "reset clears integrated");
    EXPECT(std::isinf(m.momentary()), "reset clears momentary");
    EXPECT_FLOAT_EQ(m.range(), 0.0, "reset clears range");
}

void testHistogram() {
    g_context = "histogram";
    LoudnessHistogram h;
    EXPECT(std::isinf(h.gatedLoudness(-10)), "empty");
    EXPECT_FLOAT_EQ(h.range(), 0.0, "empty range");
    // -23 LUFS energy
    const double e = std::pow(10.0, (-23.0 + 0.691) / 10.0);
    h.add(e);
    h.add(1e-12);   // far below the absolute gate
    EXPECT_EQ(h.count(), 1, "absolute gate drops quiet blocks");
    EXPECT(std::fabs(h.gatedLoudness(-10) + 23.0) < 0.01, "single block");
}

} // namespace

TEST_CASE("Meter loudness: coefficients") { testCoefficients(); }
TEST_CASE("Meter loudness: steady sine") { testSteadySine(); }
TEST_CASE("Meter loudness: gating") { testGating(); }
TEST_CASE("Meter loudness: range") { testRange(); }
TEST_CASE("Meter loudness: windows") { testWindows(); }
TEST_CASE("Meter loudness: histogram") { testHistogram(); }
