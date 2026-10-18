//
//  FftTests.cpp
//  p31-core-tests
//

#include <p31/spectrum/Fft.h>

#include <doctest/doctest.h>

#include <cmath>
#include <random>
#include <vector>

using namespace p31::spectrum;

namespace {

constexpr double kTwoPi = 6.283185307179586476925286766559;

std::vector<float> noise(size_t n, unsigned seed) {
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    std::vector<float> v(n);
    for (float& x : v) x = dist(rng);
    return v;
}

// Reference DFT in double precision.
void naiveDft(const std::vector<float>& x, std::vector<double>& re, std::vector<double>& im) {
    const size_t n = x.size();
    re.assign(n / 2 + 1, 0.0);
    im.assign(n / 2 + 1, 0.0);
    for (size_t k = 0; k <= n / 2; ++k) {
        for (size_t j = 0; j < n; ++j) {
            const double a = kTwoPi * static_cast<double>((k * j) % n) / static_cast<double>(n);
            re[k] += x[j] * std::cos(a);
            im[k] -= x[j] * std::sin(a);
        }
    }
}

std::vector<FftBackend> backends() {
#if defined(__APPLE__)
    return {FftBackend::Vdsp, FftBackend::Pffft};
#else
    return {FftBackend::Pffft};
#endif
}

} // namespace

TEST_CASE("RealFft accepts only power-of-two sizes in range") {
    CHECK(RealFft::isValidSize(256));
    CHECK(RealFft::isValidSize(32768));
    CHECK_FALSE(RealFft::isValidSize(128));
    CHECK_FALSE(RealFft::isValidSize(65536));
    CHECK_FALSE(RealFft::isValidSize(3000));
    CHECK(RealFft::create(1000) == nullptr);
    REQUIRE(RealFft::create(1024) != nullptr);
    CHECK(RealFft::create(1024)->size() == 1024);
}

TEST_CASE("Every backend matches a reference DFT") {
    for (FftBackend backend : backends()) {
        for (size_t n : {size_t{256}, size_t{1024}}) {
            auto fft = RealFft::create(n, backend);
            REQUIRE(fft != nullptr);
            CAPTURE(fft->name());
            CAPTURE(n);
            const std::vector<float> x = noise(n, 11);
            std::vector<double> refRe, refIm;
            naiveDft(x, refRe, refIm);

            std::vector<float> re(n / 2 + 1, -99.0f), im(n / 2 + 1, -99.0f);
            fft->forward(x.data(), re.data(), im.data());
            double worst = 0.0;
            for (size_t k = 0; k <= n / 2; ++k) {
                worst = std::max(worst, std::fabs(re[k] - refRe[k]));
                worst = std::max(worst, std::fabs(im[k] - refIm[k]));
            }
            // Bins of unit noise reach ~sqrt(n); float error stays far below.
            CHECK(worst < 1e-3 * std::sqrt(static_cast<double>(n)));
            CHECK(im[0] == 0.0f);
            CHECK(im[n / 2] == 0.0f);
        }
    }
}

TEST_CASE("vDSP and pffft agree at every size") {
#if defined(__APPLE__)
    for (size_t n = kMinFftSize; n <= kMaxFftSize; n *= 2) {
        auto a = RealFft::create(n, FftBackend::Vdsp);
        auto b = RealFft::create(n, FftBackend::Pffft);
        REQUIRE(a != nullptr);
        REQUIRE(b != nullptr);
        CAPTURE(n);
        const std::vector<float> x = noise(n, static_cast<unsigned>(n));
        std::vector<float> ar(n / 2 + 1), ai(n / 2 + 1), br(n / 2 + 1), bi(n / 2 + 1);
        a->forward(x.data(), ar.data(), ai.data());
        b->forward(x.data(), br.data(), bi.data());
        double worst = 0.0;
        for (size_t k = 0; k <= n / 2; ++k) {
            worst = std::max(worst, static_cast<double>(std::fabs(ar[k] - br[k])));
            worst = std::max(worst, static_cast<double>(std::fabs(ai[k] - bi[k])));
        }
        CHECK(worst < 1e-3 * std::sqrt(static_cast<double>(n)));
    }
#endif
}

TEST_CASE("A bin-centred cosine lands in its bin with amplitude N/2") {
    for (FftBackend backend : backends()) {
        const size_t n = 2048, bin = 100;
        auto fft = RealFft::create(n, backend);
        REQUIRE(fft != nullptr);
        CAPTURE(fft->name());
        std::vector<float> x(n);
        for (size_t j = 0; j < n; ++j) x[j] = static_cast<float>(std::cos(kTwoPi * bin * j / n));
        std::vector<float> re(n / 2 + 1), im(n / 2 + 1);
        fft->forward(x.data(), re.data(), im.data());
        CHECK(re[bin] == doctest::Approx(n / 2.0).epsilon(1e-4));
        CHECK(std::fabs(re[bin + 1]) < 0.05);
        CHECK(std::fabs(re[0]) < 0.05);
    }
}

TEST_CASE("forward leaves its input untouched, unaligned input works") {
    for (FftBackend backend : backends()) {
        const size_t n = 512;
        auto fft = RealFft::create(n, backend);
        REQUIRE(fft != nullptr);
        std::vector<float> storage = noise(n + 1, 3);
        const std::vector<float> copy = storage;
        std::vector<float> re1(n / 2 + 1), im1(n / 2 + 1), re2(n / 2 + 1), im2(n / 2 + 1);
        fft->forward(storage.data() + 1, re1.data(), im1.data());   // 4-byte offset
        CHECK(storage == copy);
        std::vector<float> aligned(storage.begin() + 1, storage.end());
        fft->forward(aligned.data(), re2.data(), im2.data());
        CHECK(re1 == re2);
        CHECK(im1 == im2);
    }
}
