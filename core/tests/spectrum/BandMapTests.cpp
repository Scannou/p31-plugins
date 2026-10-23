//
//  BandMapTests.cpp
//  p31-core-tests
//

#include <p31/spectrum/BandMap.h>

#include <doctest/doctest.h>

#include <cmath>
#include <vector>

using namespace p31::spectrum;

namespace {

BandMap::Config config(int scale, int bands = 64) {
    BandMap::Config c;
    c.bands = bands;
    c.fftSize = 4096;
    c.sampleRate = 48000.0;
    c.minHz = 20.0;
    c.maxHz = 20000.0;
    c.scale = scale;
    return c;
}

// Index of the band whose range holds `hz`.
int bandOf(const BandMap& map, double hz) {
    const double hzPerBin = map.config().sampleRate / static_cast<double>(map.config().fftSize);
    const int bin = static_cast<int>(std::lround(hz / hzPerBin));
    for (int i = 0; i < map.bandCount(); ++i) {
        if (bin >= map.lowBin(i) && bin <= map.highBin(i)) return i;
    }
    return -1;
}

} // namespace

TEST_CASE("Scale positions round-trip") {
    for (int scale : {FreqScaleLog, FreqScaleLinear, FreqScaleSoftLog}) {
        for (double hz : {20.0, 50.0, 440.0, 1000.0, 15000.0}) {
            CHECK(freqScaleHz(scale, freqScalePos(scale, hz)) == doctest::Approx(hz).epsilon(1e-9));
        }
    }
}

TEST_CASE("Soft log gives the bass less room than log, more than linear") {
    const auto fractionBelow100 = [](int scale) {
        const double lo = freqScalePos(scale, 20), hi = freqScalePos(scale, 20000);
        return (freqScalePos(scale, 100) - lo) / (hi - lo);
    };
    const double log = fractionBelow100(FreqScaleLog);
    const double soft = fractionBelow100(FreqScaleSoftLog);
    const double lin = fractionBelow100(FreqScaleLinear);
    CHECK(log == doctest::Approx(0.233).epsilon(0.01));
    CHECK(soft < log * 0.6);
    CHECK(soft > lin * 5);
}

TEST_CASE("magnitudeToDisplay maps the dB window to 0..1") {
    CHECK(magnitudeToDisplay(1.0f, 0.0f, 90.0f) == doctest::Approx(1.0));
    CHECK(magnitudeToDisplay(0.1f, 0.0f, 90.0f) == doctest::Approx(70.0 / 90.0));
    CHECK(magnitudeToDisplay(0.1f, -20.0f, 60.0f) == doctest::Approx(1.0));
    CHECK(magnitudeToDisplay(2.0f, 0.0f, 90.0f) == 1.0f);
    CHECK(magnitudeToDisplay(0.0f, 0.0f, 90.0f) == 0.0f);
    CHECK(magnitudeToDisplay(1e-6f, 0.0f, 90.0f) == 0.0f);
}

TEST_CASE("Bands cover the range in order on every scale") {
    for (int scale : {FreqScaleLog, FreqScaleLinear, FreqScaleSoftLog}) {
        CAPTURE(scale);
        BandMap map;
        map.build(config(scale, 200));
        REQUIRE(map.bandCount() == 200);
        for (int i = 0; i < map.bandCount(); ++i) {
            CHECK(map.lowBin(i) >= 1);   // never DC
            CHECK(map.lowBin(i) <= map.highBin(i));
            CHECK(map.highBin(i) < static_cast<int>(map.binCount()));
            if (i > 0) {
                CHECK(map.lowBin(i) >= map.lowBin(i - 1));
                CHECK(map.centreHz(i) > map.centreHz(i - 1));
            }
        }
        CHECK(map.centreHz(0) > 20.0);
        CHECK(map.centreHz(199) < 20000.0);
    }
}

TEST_CASE("Log bands split octaves evenly") {
    BandMap map;
    map.build(config(FreqScaleLog, 30));   // 20 Hz - 20 kHz is ~10 octaves: 3 bands each
    CHECK(map.centreHz(3) / map.centreHz(0) == doctest::Approx(std::pow(1000.0, 0.1)).epsilon(1e-9));
}

TEST_CASE("A tone reads loudest in the band holding its frequency") {
    for (int scale : {FreqScaleLog, FreqScaleLinear, FreqScaleSoftLog}) {
        CAPTURE(scale);
        BandMap map;
        map.build(config(scale, 64));
        const double hz = 3000.0;
        const size_t bin = static_cast<size_t>(std::lround(hz / (48000.0 / 4096)));
        std::vector<float> mag(map.binCount(), 0.0f);
        mag[bin] = 0.5f;   // -6 dBFS
        std::vector<float> out(64);
        map.read(mag.data(), 0.0f, 90.0f, out.data());
        const int band = bandOf(map, hz);
        REQUIRE(band >= 0);
        CHECK(out[band] == doctest::Approx((90.0 - 6.0206) / 90.0).epsilon(1e-4));
        for (int i = 0; i < 64; ++i) {
            if (map.lowBin(i) > static_cast<int>(bin) || map.highBin(i) < static_cast<int>(bin)) {
                if (map.centreBin(i) < 0) CHECK(out[i] == 0.0f);
            }
        }
    }
}

TEST_CASE("Bands narrower than a bin interpolate instead of stepping") {
    BandMap map;
    BandMap::Config c = config(FreqScaleLog, 400);
    c.fftSize = 1024;   // 47 Hz bins: the low bands are much narrower
    map.build(c);
    int narrow = 0;
    for (int i = 0; i < map.bandCount(); ++i) narrow += map.centreBin(i) >= 0 ? 1 : 0;
    CHECK(narrow > 50);

    // A falling slope across bins reads as a smooth falling run of bands.
    std::vector<float> mag(map.binCount());
    for (size_t b = 0; b < mag.size(); ++b) mag[b] = 1.0f / static_cast<float>(b + 1);
    std::vector<float> out(400);
    map.read(mag.data(), 0.0f, 90.0f, out.data());
    // Bands below bin 1 all hold at bin 1 (flat by design); count above them.
    int first = 0;
    while (map.centreBin(first) >= 0 && map.centreBin(first) <= 1.0f) ++first;
    CHECK(first > 0);
    int flatRuns = 0;
    for (int i = first + 1; i < narrow; ++i) {
        CHECK(out[i] <= out[i - 1] + 1e-6f);
        if (out[i] == out[i - 1]) ++flatRuns;
    }
    CHECK(flatRuns < 5);
}

TEST_CASE("Octave smoothing averages power around the band") {
    BandMap map;
    BandMap::Config c = config(FreqScaleLog, 64);
    c.octaveFraction = 3;
    map.build(c);
    std::vector<float> flat(map.binCount(), 0.1f);
    std::vector<float> out(64);
    map.read(flat.data(), 0.0f, 90.0f, out.data());
    // A flat spectrum stays flat (-20 dB) wherever smoothing applies.
    for (int i = 32; i < 64; ++i) CHECK(out[i] == doctest::Approx(70.0 / 90.0).epsilon(1e-4));

    // One loud bin spreads over the neighbouring bands, at a lower level.
    std::vector<float> tone(map.binCount(), 0.0f);
    const size_t bin = 256;   // 3 kHz
    tone[bin] = 1.0f;
    map.read(tone.data(), 0.0f, 90.0f, out.data());
    const int band = bandOf(map, 3000.0);
    CHECK(out[band] > 0.0f);
    CHECK(out[band] < 1.0f);
    CHECK(out[band + 1] > 0.0f);
    CHECK(out[band - 1] > 0.0f);
}

TEST_CASE("Slope tilts levels around 1 kHz") {
    BandMap map;
    BandMap::Config c = config(FreqScaleLog, 30);
    c.slopeDbPerOct = 3.0f;
    map.build(c);
    std::vector<float> flat(map.binCount(), 0.1f);
    std::vector<float> out(30);
    map.read(flat.data(), 0.0f, 90.0f, out.data());
    // One octave above the centre band reads 3 dB louder than it.
    const double ratioDb = 3.0 * std::log2(map.centreHz(18) / map.centreHz(15));
    CHECK((out[18] - out[15]) * 90.0 == doctest::Approx(ratioDb).epsilon(0.02));
}

TEST_CASE("Range above Nyquist is clamped") {
    BandMap map;
    BandMap::Config c = config(FreqScaleLog, 64);
    c.sampleRate = 22050.0;
    map.build(c);
    for (int i = 0; i < map.bandCount(); ++i) CHECK(map.highBin(i) < static_cast<int>(map.binCount()));
    CHECK(map.centreHz(63) < 11025.0);
}

TEST_CASE("Sub-bin interpolation is smooth and never overshoots its bins") {
    BandMap map;
    BandMap::Config c = config(FreqScaleLog, 600);
    c.fftSize = 2048;   // 23 Hz bins: the bass bands read between bins
    map.build(c);
    std::vector<float> mag(map.binCount());
    for (size_t b = 0; b < mag.size(); ++b) mag[b] = 0.05f + 0.04f * static_cast<float>(std::sin(0.9 * b));
    std::vector<float> out(600);
    map.read(mag.data(), 0.0f, 90.0f, out.data());

    int checked = 0;
    for (int i = 0; i < map.bandCount(); ++i) {
        const float centre = map.centreBin(i);
        if (centre <= 1.0f || centre >= static_cast<float>(map.binCount() - 2)) continue;
        const int b0 = static_cast<int>(centre);
        const float d0 = magnitudeToDisplay(mag[b0], 0.0f, 90.0f);
        const float d1 = magnitudeToDisplay(mag[b0 + 1], 0.0f, 90.0f);
        CHECK(out[i] >= std::min(d0, d1) - 1e-6f);
        CHECK(out[i] <= std::max(d0, d1) + 1e-6f);
        ++checked;
    }
    CHECK(checked > 50);

    // Smooth: at a single loud bin the curve levels off into a rounded top.
    // Straight segments would meet there in a corner, keeping their full
    // slope right up to the bin.
    std::fill(mag.begin(), mag.end(), 0.01f);
    mag[5] = 0.1f;   // 117 Hz, 20 dB above its neighbours
    map.read(mag.data(), 0.0f, 90.0f, out.data());
    int below = -1;
    for (int i = 0; i + 1 < map.bandCount(); ++i) {
        if (map.centreBin(i) < 5.0f && map.centreBin(i + 1) >= 5.0f) below = i;
    }
    REQUIRE(below >= 0);
    const float binSlope = magnitudeToDisplay(0.1f, 0.0f, 90.0f) - magnitudeToDisplay(0.01f, 0.0f, 90.0f);
    const float slope = (out[below + 1] - out[below]) / (map.centreBin(below + 1) - map.centreBin(below));
    CHECK(std::fabs(slope) < 0.25f * binSlope);
}

TEST_CASE("NaN magnitudes read as the floor") {
    CHECK(magnitudeToDisplay(std::nanf(""), 0.0f, 90.0f) == 0.0f);
}

TEST_CASE("Wide bands read only the bins whose centre lies inside them") {
    for (int scale : {FreqScaleLog, FreqScaleLinear, FreqScaleSoftLog}) {
        CAPTURE(scale);
        BandMap map;
        map.build(config(scale, 32));
        const double hzPerBin = 48000.0 / 4096;
        const double lo = freqScalePos(scale, 20.0), hi = freqScalePos(scale, 20000.0);
        for (int i = 0; i < map.bandCount(); ++i) {
            if (map.centreBin(i) >= 0) continue;   // narrower than a bin: interpolated
            const double f0 = freqScaleHz(scale, lo + (hi - lo) * i / 32.0);
            const double f1 = freqScaleHz(scale, lo + (hi - lo) * (i + 1) / 32.0);
            CHECK(map.lowBin(i) * hzPerBin >= f0 - 1e-9);
            CHECK(map.highBin(i) * hzPerBin < f1);
        }
    }
}
