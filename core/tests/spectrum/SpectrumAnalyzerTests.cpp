//
//  SpectrumAnalyzerTests.cpp
//  p31-core-tests
//

#include <p31/spectrum/SpectrumAnalyzer.h>
#include <p31/spectrum/SpectrumSettings.h>

#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <vector>

using namespace p31::spectrum;

namespace {

constexpr double kTwoPi = 6.283185307179586476925286766559;
constexpr double kRate = 48000.0;

// Streams a stereo sine into the analyzer in real time steps.
struct Driver {
    explicit Driver(SpectrumAnalyzer& a) : analyzer(a) {}

    SpectrumAnalyzer& analyzer;
    double hz = 1000.0;
    float amplitude = 1.0f;
    double phase = 0.0;
    std::vector<float> buffer;

    // Feed `seconds` of signal in display frames of `dt`, ticking after each.
    void run(double seconds, double dt = 1.0 / 60.0, bool audio = true) {
        const int ticks = static_cast<int>(std::lround(seconds / dt));
        for (int t = 0; t < ticks; ++t) {
            if (audio) {
                const size_t frames = static_cast<size_t>(std::lround(dt * kRate));
                buffer.resize(frames * 2);
                for (size_t i = 0; i < frames; ++i) {
                    const float s = amplitude * static_cast<float>(std::sin(phase));
                    phase += kTwoPi * hz / kRate;
                    buffer[2 * i] = buffer[2 * i + 1] = s;
                }
                analyzer.process(buffer.data(), frames);
            }
            analyzer.tick(dt);
        }
    }
};

SpectrumAnalyzer::Settings baseSettings(int mode) {
    SpectrumAnalyzer::Settings s;
    s.barCount = 64;
    s.fftSize = 4096;
    s.freqScale = FreqScaleLog;
    s.minHz = 20;
    s.maxHz = 20000;
    s.smoothingMode = mode;
    s.topDb = 0.0f;
    s.rangeDb = 90.0f;
    return s;
}

float maxOf(const std::vector<float>& v) { return *std::max_element(v.begin(), v.end()); }

} // namespace

TEST_CASE("A steady sine settles at its level in both smoothing modes") {
    for (int mode : {SmoothingAttackDecay, SmoothingRms}) {
        CAPTURE(mode);
        SpectrumAnalyzer a;
        a.configure(baseSettings(mode));
        a.setSampleRate(kRate);
        Driver d{a};
        d.amplitude = 0.5f;   // -6 dBFS
        d.run(2.0);
        // Within the window's scalloping loss (Hann: up to 1.4 dB).
        const float expected = static_cast<float>((90.0 - 6.02) / 90.0);
        CHECK(maxOf(a.bars()) <= expected + 0.002f);
        CHECK(maxOf(a.bars()) >= expected - 1.5f / 90.0f);
        CHECK(a.isActive());
    }
}

TEST_CASE("Bars fall to silence after the audio stops") {
    for (bool hostKeepsCalling : {false, true}) {
        for (int mode : {SmoothingAttackDecay, SmoothingRms}) {
            CAPTURE(mode);
            CAPTURE(hostKeepsCalling);
            SpectrumAnalyzer::Settings s = baseSettings(mode);
            s.peakHoldSec = 0.5f;
            s.peakFall = static_cast<float>(30.0 / 60.0 / 90.0);   // 30 dB/s
            SpectrumAnalyzer a;
            a.configure(s);
            a.setSampleRate(kRate);
            Driver d{a};
            d.run(1.0);
            REQUIRE(maxOf(a.bars()) > 0.8f);
            // A stopped DAW either keeps sending silence or stops calling.
            d.amplitude = 0.0f;
            d.run(5.0, 1.0 / 60.0, hostKeepsCalling);
            CHECK(maxOf(a.bars()) == 0.0f);
            CHECK(maxOf(a.peaks()) == 0.0f);
            CHECK(maxOf(a.shadow()) == 0.0f);
            CHECK_FALSE(a.isActive());
        }
    }
}

TEST_CASE("RMS averaging decays at its time constant") {
    SpectrumAnalyzer::Settings s = baseSettings(SmoothingRms);
    s.rmsMs = 200;
    SpectrumAnalyzer a;
    a.configure(s);
    a.setSampleRate(kRate);
    Driver d{a};
    d.run(2.0);
    const float before = maxOf(a.bars());
    d.amplitude = 0.0f;
    d.run(0.5);
    // Power falls by e^(-t/tau): 10*log10(e) * 0.5 / 0.2 = 10.9 dB, plus up
    // to one FFT frame (85 ms) of the tone still in the window.
    const float fallDb = (before - maxOf(a.bars())) * 90.0f;
    CHECK(fallDb > 6.0f);
    CHECK(fallDb < 11.5f);
}

TEST_CASE("Motion is the same at 60 and 120 Hz refresh") {
    for (int mode : {SmoothingAttackDecay, SmoothingRms}) {
        CAPTURE(mode);
        std::vector<float> levels[2];
        for (int r = 0; r < 2; ++r) {
            SpectrumAnalyzer a;
            a.configure(baseSettings(mode));
            a.setSampleRate(kRate);
            Driver d{a};
            const double dt = r == 0 ? 1.0 / 60.0 : 1.0 / 120.0;
            d.run(1.0, dt);
            d.amplitude = 0.0f;
            d.run(0.25, dt);
            levels[r] = a.bars();
        }
        for (size_t i = 0; i < levels[0].size(); ++i) CHECK(levels[0][i] == doctest::Approx(levels[1][i]).epsilon(0.03));
    }
}

TEST_CASE("Peaks hold, then fall at the set rate") {
    SpectrumAnalyzer::Settings s = baseSettings(SmoothingAttackDecay);
    s.smoothing = 0;
    s.peakHoldSec = 0.5f;
    s.peakFall = static_cast<float>(12.0 / 60.0 / 90.0);   // 12 dB/s
    SpectrumAnalyzer a;
    a.configure(s);
    a.setSampleRate(kRate);
    Driver d{a};
    d.run(1.0);
    const float top = maxOf(a.peaks());
    d.amplitude = 0.0f;
    d.run(0.4);
    CHECK(maxOf(a.peaks()) == doctest::Approx(top).epsilon(1e-4));   // still held
    d.run(1.1);
    // Held 0.5 s after the last loud frame, then 1 s at 12 dB/s.
    CHECK((top - maxOf(a.peaks())) * 90.0f == doctest::Approx(12.0).epsilon(0.1));
}

TEST_CASE("Side layer tracks the side signal only") {
    SpectrumAnalyzer::Settings s = baseSettings(SmoothingRms);
    s.side = true;
    SpectrumAnalyzer a;
    a.configure(s);
    a.setSampleRate(kRate);
    std::vector<float> lr(800 * 2);
    double phase = 0.0;
    for (int t = 0; t < 120; ++t) {
        for (size_t i = 0; i < 800; ++i) {
            const float v = 0.5f * static_cast<float>(std::sin(phase));
            phase += kTwoPi * 1000.0 / kRate;
            lr[2 * i] = v;
            lr[2 * i + 1] = -v;   // all side, no mid
        }
        a.process(lr.data(), 800);
        a.tick(1.0 / 60.0);
    }
    REQUIRE(a.side().size() == a.bars().size());
    CHECK(maxOf(a.side()) > 0.85f);
    CHECK(maxOf(a.bars()) == 0.0f);
}

TEST_CASE("Reconfiguring keeps levels, resampled to the new count") {
    SpectrumAnalyzer a;
    a.configure(baseSettings(SmoothingRms));
    a.setSampleRate(kRate);
    Driver d{a};
    d.run(1.0);
    SpectrumAnalyzer::Settings s = baseSettings(SmoothingRms);
    s.barCount = 200;
    a.configure(s);
    CHECK(a.bars().size() == 200);
    CHECK(a.peaks().size() == 200);
    CHECK(maxOf(a.peaks()) > 0.8f);
    d.run(0.1);
    CHECK(maxOf(a.bars()) > 0.8f);
}

TEST_CASE("Sample-rate change moves the tone to the right band") {
    SpectrumAnalyzer a;
    a.configure(baseSettings(SmoothingRms));
    a.setSampleRate(44100.0);
    a.setSampleRate(kRate);
    Driver d{a};
    d.hz = 5000.0;
    d.run(1.0);
    const auto& bars = a.bars();
    const auto loudest = std::max_element(bars.begin(), bars.end()) - bars.begin();
    // Band centre on a 20 Hz - 20 kHz log axis with 64 bands.
    const double centre = 20.0 * std::pow(1000.0, (loudest + 0.5) / 64.0);
    CHECK(std::fabs(std::log2(centre / 5000.0)) < 10.0 / 64.0);
}

TEST_CASE("Ticks before any sample rate is known show silence") {
    // An editor can open before the host activates the plugin.
    SpectrumAnalyzer a;
    a.configure(baseSettings(SmoothingRms));
    for (int i = 0; i < 10; ++i) CHECK_FALSE(a.tick(1.0 / 60.0));
    CHECK(a.bars().size() == 64);
    CHECK(maxOf(a.bars()) == 0.0f);
    SpectrumAnalyzer b;
    b.tick(1.0 / 60.0);
    CHECK(maxOf(b.bars()) == 0.0f);
}

TEST_CASE("reset clears everything") {
    SpectrumAnalyzer a;
    a.configure(baseSettings(SmoothingAttackDecay));
    a.setSampleRate(kRate);
    Driver d{a};
    d.run(0.5);
    a.reset();
    CHECK(maxOf(a.bars()) == 0.0f);
    CHECK(maxOf(a.peaks()) == 0.0f);
    CHECK_FALSE(a.isActive());
}

TEST_CASE("Settings map to analyzer rates as in fb2k") {
    SpectrumSettings u;
    CHECK(u.viewSizedBars());   // curve mode by default
    SpectrumAnalyzer::Settings s = analyzerSettings(u, 321);
    CHECK(s.barCount == 321);
    CHECK(s.smoothingMode == SmoothingRms);
    CHECK(s.freqScale == FreqScaleSoftLog);
    CHECK(s.peakFall == doctest::Approx(12.0 / 60.0 / 90.0));
    CHECK(s.peakHoldSec == doctest::Approx(0.92));
    CHECK(s.slopeDbPerOct == doctest::Approx(2.0));   // our default; fb2k's was flat
    // RMS: the shadow falls slower than the RMS curve can.
    const double curveFall = 10.0 * std::log10(std::exp(1.0)) / 0.27 / 60.0 / 90.0;
    CHECK(s.shadowFall < curveFall);

    u.drawMode = DrawModeBars;
    u.barCount = 48;
    CHECK_FALSE(u.viewSizedBars());
    CHECK(analyzerSettings(u, 321).barCount == 48);
}

TEST_CASE("sanitized pulls stored values back into range") {
    SpectrumSettings u;
    u.fftSize = 3000;
    u.rangeDb = 75;
    u.freqScale = 9;
    u.curveOctaveFraction = 5;
    u.barCount = 100000;
    u.peakFallDbPerSec = 0;
    const SpectrumSettings s = u.sanitized();
    CHECK(s.fftSize == 2048);
    CHECK(s.rangeDb == 90);
    CHECK(s.freqScale == FreqScaleSoftLog);
    CHECK(s.curveOctaveFraction == 24);   // the default
    CHECK(s.barCount == kMaxBarCount);
    CHECK(s.peakFallDbPerSec == kMinPeakFallDbPerSec);

    SpectrumSettings t;
    applyTheme(t, 3);
    CHECK(t.theme == 3);
    CHECK(t.dark.bar == kThemes[3].dark.bar);
    applyTheme(t, 0);
    CHECK(t.theme == 3);
}
