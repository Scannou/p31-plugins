//
//  Fft.cpp
//  p31-core
//

#include "p31/spectrum/Fft.h"

#include <pffft.h>

#include <cstring>

#if defined(__APPLE__)
#include <Accelerate/Accelerate.h>
#endif

namespace p31::spectrum {

namespace {

#if defined(__APPLE__)
class VdspFft final : public RealFft {
public:
    explicit VdspFft(size_t n) : RealFft(n) {
        while ((size_t{1} << _log2) < n) ++_log2;
        _setup = vDSP_create_fftsetup(_log2, kFFTRadix2);
    }
    ~VdspFft() override { vDSP_destroy_fftsetup(_setup); }

    bool ok() const { return _setup != nullptr; }
    const char* name() const override { return "vDSP"; }

    void forward(const float* input, float* re, float* im) override {
        const size_t half = size() / 2;
        // The caller's arrays serve as the split-complex buffer.
        DSPSplitComplex split{re, im};
        vDSP_ctoz(reinterpret_cast<const DSPComplex*>(input), 2, &split, 1, half);
        vDSP_fft_zrip(_setup, &split, 1, _log2, kFFTDirection_Forward);
        // zrip packs Nyquist into im[0] and returns twice the DFT.
        re[half] = im[0];
        im[0] = 0.0f;
        im[half] = 0.0f;
        const float scale = 0.5f;
        vDSP_vsmul(re, 1, &scale, re, 1, half + 1);
        vDSP_vsmul(im, 1, &scale, im, 1, half + 1);
    }

private:
    vDSP_Length _log2 = 0;
    FFTSetup _setup = nullptr;
};
#endif

class Pffft final : public RealFft {
public:
    explicit Pffft(size_t n) : RealFft(n) {
        _setup = pffft_new_setup(static_cast<int>(n), PFFFT_REAL);
        _buffer = static_cast<float*>(pffft_aligned_malloc(n * sizeof(float)));
        _work = static_cast<float*>(pffft_aligned_malloc(n * sizeof(float)));
    }
    ~Pffft() override {
        pffft_aligned_free(_work);
        pffft_aligned_free(_buffer);
        if (_setup) pffft_destroy_setup(_setup);
    }

    bool ok() const { return _setup && _buffer && _work; }
    const char* name() const override { return "pffft"; }

    void forward(const float* input, float* re, float* im) override {
        const size_t n = size(), half = n / 2;
        // pffft wants SIMD-aligned buffers; the input may be anywhere.
        std::memcpy(_buffer, input, n * sizeof(float));
        pffft_transform_ordered(_setup, _buffer, _buffer, _work, PFFFT_FORWARD);
        // Ordered output: DC and Nyquist in the first pair, then
        // interleaved re/im for k = 1 .. N/2 - 1.
        re[0] = _buffer[0];
        im[0] = 0.0f;
        re[half] = _buffer[1];
        im[half] = 0.0f;
        for (size_t k = 1; k < half; ++k) {
            re[k] = _buffer[2 * k];
            im[k] = _buffer[2 * k + 1];
        }
    }

private:
    PFFFT_Setup* _setup = nullptr;
    float* _buffer = nullptr;
    float* _work = nullptr;
};

template <class T>
std::unique_ptr<RealFft> make(size_t n) {
    auto fft = std::make_unique<T>(n);
    if (!fft->ok()) return nullptr;
    return fft;
}

} // namespace

bool RealFft::isValidSize(size_t n) {
    return n >= kMinFftSize && n <= kMaxFftSize && (n & (n - 1)) == 0;
}

std::unique_ptr<RealFft> RealFft::create(size_t n, FftBackend backend) {
    if (!isValidSize(n)) return nullptr;
    switch (backend) {
        case FftBackend::Pffft:
            return make<Pffft>(n);
        case FftBackend::Vdsp:
#if defined(__APPLE__)
            return make<VdspFft>(n);
#else
            return nullptr;
#endif
        case FftBackend::Default:
        default:
#if defined(__APPLE__)
            return make<VdspFft>(n);
#else
            return make<Pffft>(n);
#endif
    }
}

} // namespace p31::spectrum
