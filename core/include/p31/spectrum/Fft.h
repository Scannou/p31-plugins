//
//  Fft.h
//  p31-core
//
//  Real-input forward FFT behind one interface. vDSP (Accelerate) is the
//  default on Apple platforms; pffft is the portable fallback and the
//  reference the vDSP backend is tested against.
//

#pragma once

#include <cstddef>
#include <memory>

namespace p31::spectrum {

constexpr size_t kMinFftSize = 256;
constexpr size_t kMaxFftSize = 32768;

enum class FftBackend {
    Default,   // vDSP where available, else pffft
    Vdsp,
    Pffft,
};

class RealFft {
public:
    virtual ~RealFft() = default;

    // Power of two in [kMinFftSize, kMaxFftSize].
    static bool isValidSize(size_t n);

    // Allocates; nullptr for an invalid size or a backend this platform
    // does not have.
    static std::unique_ptr<RealFft> create(size_t n, FftBackend backend = FftBackend::Default);

    size_t size() const { return _size; }
    virtual const char* name() const = 0;

    // Unscaled DFT of size() real samples, X[k] = sum x[j] e^(-2 pi i k j / N)
    // for k = 0 .. N/2: `re` and `im` each receive N/2 + 1 values (the
    // imaginary parts of DC and Nyquist are zero). No allocation; `input` is
    // left untouched and needs no particular alignment.
    virtual void forward(const float* input, float* re, float* im) = 0;

protected:
    explicit RealFft(size_t n) : _size(n) {}

private:
    size_t _size;
};

} // namespace p31::spectrum
