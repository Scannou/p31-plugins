//
//  StftTests.cpp
//  p31-core-tests
//

#include <p31/spectrum/Stft.h>

#include <doctest/doctest.h>

#include <cmath>
#include <vector>

using namespace p31::spectrum;

namespace {

constexpr double kTwoPi = 6.283185307179586476925286766559;

// Interleaved stereo: `l` and `r` gains on one sine.
std::vector<float> stereoSine(size_t frames, double hz, double sr, float l, float r) {
    std::vector<float> v(frames * 2);
    for (size_t i = 0; i < frames; ++i) {
        const float s = static_cast<float>(std::sin(kTwoPi * hz * static_cast<double>(i) / sr));
        v[2 * i] = l * s;
        v[2 * i + 1] = r * s;
    }
    return v;
}

size_t loudestBin(const std::vector<float>& p) {
    size_t best = 0;
    for (size_t k = 1; k < p.size(); ++k) if (p[k] > p[best]) best = k;
    return best;
}

} // namespace

TEST_CASE("Windows are periodic and have the expected coherent gain") {
    for (int type : {WindowHann, WindowBlackmanHarris}) {
        const size_t n = 4096;
        std::vector<float> w(n);
        makeWindow(type, w.data(), n);
        double sum = 0.0;
        for (float x : w) sum += x;
        const double gain = type == WindowHann ? 0.5 : 0.35875;
        CHECK(sum / n == doctest::Approx(gain).epsilon(1e-5));
        // Periodic: symmetric around n/2, not around (n-1)/2.
        CHECK(w[1] == doctest::Approx(w[n - 1]).epsilon(1e-6));
        CHECK(w[n / 2] == doctest::Approx(type == WindowHann ? 1.0 : 1.0).epsilon(1e-5));
    }
}

TEST_CASE("defaultHop overlaps at least 75 % and caps at 10 ms") {
    CHECK(Stft::defaultHop(1024, 48000) == 256);
    CHECK(Stft::defaultHop(2048, 48000) == 480);
    CHECK(Stft::defaultHop(32768, 44100) == 441);
    CHECK(Stft::defaultHop(256, 0) == 64);
}

TEST_CASE("A full-scale bin-centred sine reads power 1 in its bin") {
    for (int window : {WindowHann, WindowBlackmanHarris}) {
        Stft stft;
        Stft::Config c;
        c.fftSize = 2048;
        c.window = window;
        c.hop = 2048;
        REQUIRE(stft.configure(c));
        const double sr = 48000.0, bin = 64;   // 1500 Hz
        const auto lr = stereoSine(2048, bin * sr / 2048, sr, 1.0f, 1.0f);
        int calls = 0;
        stft.feed(lr.data(), 2048, [&](const float* mid, const float* side) {
            ++calls;
            CHECK(side == nullptr);
            std::vector<float> p(mid, mid + stft.binCount());
            CHECK(loudestBin(p) == 64);
            CHECK(p[64] == doctest::Approx(1.0).epsilon(1e-3));
            CHECK(p[0] == 0.0f);
            CHECK(p[64 + 10] < 1e-6);   // leakage dies off within a few bins
        });
        CHECK(calls == 1);
    }
}

TEST_CASE("Mid and side split the stereo image") {
    Stft stft;
    Stft::Config c;
    c.fftSize = 1024;
    c.hop = 1024;
    c.side = true;
    REQUIRE(stft.configure(c));
    const double sr = 48000.0, hz = 32 * sr / 1024;
    const auto lr = stereoSine(1024, hz, sr, 1.0f, -1.0f);   // pure side
    stft.feed(lr.data(), 1024, [&](const float* mid, const float* side) {
        REQUIRE(side != nullptr);
        CHECK(mid[32] < 1e-9);
        CHECK(side[32] == doctest::Approx(1.0).epsilon(1e-3));
    });
}

TEST_CASE("Block size does not change the spectra") {
    Stft::Config c;
    c.fftSize = 1024;
    c.hop = 300;   // not a divisor of any block size below
    const auto lr = stereoSine(10000, 997.0, 48000.0, 0.7f, 0.3f);

    const auto run = [&](size_t block) {
        Stft stft;
        REQUIRE(stft.configure(c));
        std::vector<std::vector<float>> out;
        for (size_t at = 0; at < 10000; at += block) {
            const size_t n = std::min(block, size_t{10000} - at);
            stft.feed(lr.data() + 2 * at, n, [&](const float* mid, const float*) {
                out.emplace_back(mid, mid + stft.binCount());
            });
        }
        return out;
    };
    const auto whole = run(10000);
    CHECK(whole.size() == 10000 / 300);
    for (size_t block : {size_t{1}, size_t{64}, size_t{512}, size_t{4096}}) {
        CAPTURE(block);
        CHECK(run(block) == whole);
    }
}

TEST_CASE("reset forgets the history") {
    Stft stft;
    Stft::Config c;
    c.fftSize = 512;
    c.hop = 512;
    REQUIRE(stft.configure(c));
    const auto loud = stereoSine(256, 3000.0, 48000.0, 1.0f, 1.0f);
    stft.feed(loud.data(), 256, [](const float*, const float*) {});
    stft.reset();
    const std::vector<float> silence(512 * 2, 0.0f);
    stft.feed(silence.data(), 512, [&](const float* mid, const float*) {
        for (size_t k = 0; k < stft.binCount(); ++k) CHECK(mid[k] == 0.0f);
    });
}

TEST_CASE("configure rejects bad sizes and keeps the old setup") {
    Stft stft;
    Stft::Config c;
    c.fftSize = 1024;
    REQUIRE(stft.configure(c));
    Stft::Config bad = c;
    bad.fftSize = 1000;
    CHECK_FALSE(stft.configure(bad));
    bad = c;
    bad.hop = 0;
    CHECK_FALSE(stft.configure(bad));
    CHECK(stft.config().fftSize == 1024);
    CHECK(stft.binCount() == 512);
}
