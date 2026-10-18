//
//  Stft.h
//  p31-core
//
//  Short-time spectrum of a stereo stream: keeps the last fftSize samples of
//  mid (L+R)/2 and, optionally, side (L-R)/2, and every `hop` new samples
//  windows them and produces a power spectrum. Overlapping frames mean every
//  sample is analysed whatever the block size or display rate.
//
//  Power is normalised to amplitude squared: a full-scale sine centred on a
//  bin reads 1.0 there. Bin k covers k * sampleRate / fftSize; the arrays
//  hold bins 0 .. fftSize/2 - 1, with DC forced to zero.
//

#pragma once

#include "p31/spectrum/Fft.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <vector>

namespace p31::spectrum {

enum WindowType {
    WindowHann = 0,             // Good all-round choice; -31 dB first sidelobe
    WindowBlackmanHarris = 1,   // 4-term, -92 dB sidelobes: clean floor, wider peaks
};

// Periodic (DFT-even) window of `n` values into `out`.
void makeWindow(int type, float* out, size_t n);

class Stft {
public:
    struct Config {
        size_t fftSize = 2048;
        int window = WindowHann;
        size_t hop = 512;
        bool side = false;   // also analyse the side signal
        FftBackend backend = FftBackend::Default;
    };

    // Hop for a frame size and sample rate: at least 75 % overlap, and at
    // least 100 spectra per second so large frames still move smoothly.
    static size_t defaultHop(size_t fftSize, double sampleRate);

    // Allocates. Returns false (and keeps the previous setup) for an invalid
    // configuration. Clears the history.
    bool configure(const Config& config);
    const Config& config() const { return _config; }
    // 0 until configured.
    size_t binCount() const { return _fft ? _config.fftSize / 2 : 0; }

    // Forget the history: the next frames start from silence.
    void reset();

    // Append `frames` interleaved stereo frames (oldest first). After every
    // hop calls onSpectrum(midPower, sidePower), each binCount() values;
    // sidePower is null unless the side is analysed. No allocation.
    template <class OnSpectrum>
    void feed(const float* lr, size_t frames, OnSpectrum&& onSpectrum) {
        if (!_fft) return;
        while (frames > 0) {
            const size_t take = std::min(frames, _untilHop);
            append(lr, take);
            lr += 2 * take;
            frames -= take;
            _untilHop -= take;
            if (_untilHop == 0) {
                _untilHop = _config.hop;
                analyse();
                onSpectrum(static_cast<const float*>(_midPower.data()),
                           _config.side ? static_cast<const float*>(_sidePower.data()) : nullptr);
            }
        }
    }

private:
    void append(const float* lr, size_t frames);
    void analyse();
    void spectrumOf(const std::vector<float>& history, std::vector<float>& power);

    Config _config;
    std::unique_ptr<RealFft> _fft;
    std::vector<float> _window;
    float _powerScale = 0.0f;
    std::vector<float> _mid, _side;   // circular histories, fftSize each
    size_t _writeIndex = 0;
    size_t _untilHop = 0;
    std::vector<float> _frame;        // windowed, unrolled history
    std::vector<float> _re, _im;      // fftSize/2 + 1
    std::vector<float> _midPower, _sidePower;
};

} // namespace p31::spectrum
