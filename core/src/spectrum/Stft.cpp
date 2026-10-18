//
//  Stft.cpp
//  p31-core
//

#include "p31/spectrum/Stft.h"

#include <cmath>

namespace p31::spectrum {

namespace {
    constexpr double kTwoPi = 6.283185307179586476925286766559;
    // Upper bound on the time between spectra.
    constexpr double kMaxHopSeconds = 0.01;
}

void makeWindow(int type, float* out, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        const double x = kTwoPi * static_cast<double>(i) / static_cast<double>(n);
        double w;
        if (type == WindowBlackmanHarris) {
            w = 0.35875 - 0.48829 * std::cos(x) + 0.14128 * std::cos(2 * x) - 0.01168 * std::cos(3 * x);
        } else {
            w = 0.5 - 0.5 * std::cos(x);
        }
        out[i] = static_cast<float>(w);
    }
}

size_t Stft::defaultHop(size_t fftSize, double sampleRate) {
    size_t hop = fftSize / 4;
    if (sampleRate > 0) {
        hop = std::min(hop, static_cast<size_t>(std::lround(sampleRate * kMaxHopSeconds)));
    }
    return std::max<size_t>(hop, 1);
}

bool Stft::configure(const Config& config) {
    if (config.hop == 0) return false;
    auto fft = RealFft::create(config.fftSize, config.backend);
    if (!fft) return false;

    _config = config;
    _fft = std::move(fft);
    const size_t n = config.fftSize;
    _window.resize(n);
    makeWindow(config.window, _window.data(), n);
    double sum = 0.0;
    for (float w : _window) sum += w;
    // Amplitude of bin k is 2 |X[k]| / sum(window).
    const double ampScale = 2.0 / sum;
    _powerScale = static_cast<float>(ampScale * ampScale);

    _mid.assign(n, 0.0f);
    _side.assign(config.side ? n : 0, 0.0f);
    _frame.assign(n, 0.0f);
    _re.assign(n / 2 + 1, 0.0f);
    _im.assign(n / 2 + 1, 0.0f);
    _midPower.assign(n / 2, 0.0f);
    _sidePower.assign(config.side ? n / 2 : 0, 0.0f);
    reset();
    return true;
}

void Stft::reset() {
    std::fill(_mid.begin(), _mid.end(), 0.0f);
    std::fill(_side.begin(), _side.end(), 0.0f);
    _writeIndex = 0;
    _untilHop = _config.hop;
}

void Stft::append(const float* lr, size_t frames) {
    const size_t n = _mid.size();
    for (size_t i = 0; i < frames; ++i) {
        const float l = lr[2 * i], r = lr[2 * i + 1];
        _mid[_writeIndex] = 0.5f * (l + r);
        if (_config.side) _side[_writeIndex] = 0.5f * (l - r);
        if (++_writeIndex == n) _writeIndex = 0;
    }
}

void Stft::spectrumOf(const std::vector<float>& history, std::vector<float>& power) {
    // Oldest sample sits at the write index.
    const size_t n = history.size();
    const size_t tail = n - _writeIndex;
    for (size_t j = 0; j < tail; ++j) _frame[j] = history[_writeIndex + j] * _window[j];
    for (size_t j = 0; j < _writeIndex; ++j) _frame[tail + j] = history[j] * _window[tail + j];

    _fft->forward(_frame.data(), _re.data(), _im.data());
    power[0] = 0.0f;   // DC is never shown
    for (size_t k = 1; k < power.size(); ++k) {
        power[k] = (_re[k] * _re[k] + _im[k] * _im[k]) * _powerScale;
    }
}

void Stft::analyse() {
    spectrumOf(_mid, _midPower);
    if (_config.side) spectrumOf(_side, _sidePower);
}

} // namespace p31::spectrum
