//
//  SpectrogramEngine.cpp
//  p31-core (ported from foo_jl_spectrogram_mac SpectrogramEngine.cpp)
//

#include "p31/spectrogram/SpectrogramEngine.h"

#include <algorithm>
#include <cmath>

namespace p31::spectrogram {

namespace {
    constexpr int kMinFft = 256;
    constexpr int kMaxFft = 65536;
    static_assert(kMinFft >= static_cast<int>(spectrum::kMinFftSize));
    static_assert(kMaxFft <= static_cast<int>(spectrum::kMaxFftSize));

    constexpr double kTwoPi = 6.283185307179586476925286766559;

    // Crossovers of the multi-resolution mode. Each blends over one octave.
    constexpr double kLowSplitHz  = 200.0;   // long -> base between 200 and 400 Hz
    constexpr double kHighSplitHz = 2000.0;  // base -> short between 2 and 4 kHz

    constexpr float kSilenceDb = -200.0f;

    // Reassigned mode: the standard spectrum shown underneath the lines, this
    // far down, so noise and transients keep their texture.
    constexpr float kUnderlayDb = 18.0f;

    // Reassigned lines are softened over two rows each side, keeping their
    // own level (a max, not an average): exp(-d^2 / 1.6) for d = 1, 2.
    constexpr double kSoften[3] = {1.0, 0.535, 0.082};

    // Bins quieter than this (dB) are not reassigned.
    constexpr float kReassignFloorDb = -150.0f;

    inline int clampPow2(int v) {
        int p = kMinFft;
        while (p < v && p < kMaxFft) p <<= 1;
        return p;
    }

    inline double melOf(double hz) { return std::log(1.0 + hz / 700.0); }
    inline double hzOfMel(double mel) { return 700.0 * (std::exp(mel) - 1.0); }
}

void SpectrogramEngine::configure(const Settings& settings) {
    _settings = settings;
    if (_settings.minHz < 5.0) _settings.minHz = 5.0;
    if (_settings.maxHz < _settings.minHz + 100.0) _settings.maxHz = _settings.minHz + 100.0;
    if (_settings.rangeDb < 10.0f) _settings.rangeDb = 10.0f;
    _dirty = true;
}

void SpectrogramEngine::setRows(int rows) {
    if (rows < 0) rows = 0;
    if (rows == _rows) return;
    _rows = rows;
    _dirty = true;
}

void SpectrogramEngine::setSampleRate(double sampleRate) {
    if (sampleRate <= 0.0 || sampleRate == _sampleRate) return;
    _sampleRate = sampleRate;
    _dirty = true;
}

int SpectrogramEngine::windowLength() {
    if (_dirty) rebuild();
    return _length;
}

double SpectrogramEngine::fractionForHz(int scale, double minHz, double maxHz, double hz) {
    switch (scale) {
        case ScaleLinear:
            return (hz - minHz) / (maxHz - minHz);
        case ScaleMel:
            return (melOf(hz) - melOf(minHz)) / (melOf(maxHz) - melOf(minHz));
        default:
            return (std::log(hz) - std::log(minHz)) / (std::log(maxHz) - std::log(minHz));
    }
}

double SpectrogramEngine::hzForFraction(int scale, double minHz, double maxHz, double t) {
    switch (scale) {
        case ScaleLinear:
            return minHz + (maxHz - minHz) * t;
        case ScaleMel:
            return hzOfMel(melOf(minHz) + (melOf(maxHz) - melOf(minHz)) * t);
        default:
            return std::exp(std::log(minHz) + (std::log(maxHz) - std::log(minHz)) * t);
    }
}

SpectrogramEngine::Tap SpectrogramEngine::makeTap(Stage& stage, double f0, double f1, double fc) const {
    const double binHz = _sampleRate / stage.size;
    const int maxBin = stage.size / 2 - 1;
    const double b0 = f0 / binHz;
    const double b1 = f1 / binHz;

    Tap tap;
    if (b1 - b0 < 2.0) {
        // Narrower than the bins: sample at the row centre. Bin 0 is DC, so
        // rows below bin 1 hold its level instead of fading toward nothing.
        const double c = std::max(1.0, std::min(fc / binHz, static_cast<double>(maxBin)));
        tap.lo = std::min(static_cast<int>(c), maxBin - 1);
        tap.frac = static_cast<float>(c - tap.lo);
        tap.hi = -1;
        stage.binLo = std::min(stage.binLo, tap.lo);
        stage.binHi = std::max(stage.binHi, tap.lo + 1);
    } else {
        tap.lo = std::max(1, std::min(static_cast<int>(std::ceil(b0)), maxBin));
        tap.hi = std::max(tap.lo, std::min(static_cast<int>(std::floor(b1)), maxBin));
        stage.binLo = std::min(stage.binLo, tap.lo);
        stage.binHi = std::max(stage.binHi, tap.hi);
    }
    return tap;
}

void SpectrogramEngine::rebuild() {
    _dirty = false;

    const int rateScale = _sampleRate <= 64000.0 ? 1 : (_sampleRate <= 128000.0 ? 2 : 4);
    const int base = std::min(clampPow2(_settings.fftSize) * rateScale, kMaxFft);
    const int mode = _settings.mode;
    const bool multi = mode == ModeMultiRes;
    // Reassigned mode keeps the short treble window but needs no long one:
    // reassignment thins the bass lines instead.
    const bool split = mode != ModeStandard;
    const int sizes[3] = {
        multi ? std::min(base * 4, kMaxFft) : base,
        base,
        split ? std::max(base / 4, kMinFft) : base
    };
    _length = sizes[0];

    for (int s = 0; s < 3; ++s) {
        Stage& st = _stages[s];
        st.size = sizes[s];
        st.binLo = st.size;
        st.binHi = -1;
        st.db.assign(static_cast<size_t>(st.size / 2), kSilenceDb);

        st.offset = (_length - st.size) / 2;
        st.reassign = mode == ModeReassigned;
        st.window.resize(static_cast<size_t>(st.size));
        st.dwindow.resize(static_cast<size_t>(st.size));
        double sum = 0.0, sumSq = 0.0;
        const bool bh = _settings.window == WindowBlackmanHarris;
        const double dx = kTwoPi / st.size;   // d(x)/dn
        for (int n = 0; n < st.size; ++n) {
            const double x = dx * n;
            const double w = bh
                ? 0.35875 - 0.48829 * std::cos(x) + 0.14128 * std::cos(2 * x) - 0.01168 * std::cos(3 * x)
                : 0.5 - 0.5 * std::cos(x);
            const double dw = bh
                ? dx * (0.48829 * std::sin(x) - 2 * 0.14128 * std::sin(2 * x) + 3 * 0.01168 * std::sin(3 * x))
                : dx * 0.5 * std::sin(x);
            st.window[static_cast<size_t>(n)] = static_cast<float>(w);
            st.dwindow[static_cast<size_t>(n)] = static_cast<float>(dw);
            sum += w;
            sumSq += w * w;
        }
        // A sine of amplitude A peaks at A * sum(window) / 2 in the DFT.
        // Scaling the window by 2 / sum up front makes the squared magnitude
        // read directly in dBFS. The derivative window only enters as a
        // ratio to it, so it gets the same factor.
        const float norm = static_cast<float>(2.0 / sum);
        for (float& w : st.window) w *= norm;
        for (float& w : st.dwindow) w *= norm;
        // A tone's energy summed over its whole lobe, relative to its peak bin
        // (Parseval): reassignment gathers the lobe into one row.
        st.lobe = static_cast<float>(st.size * sumSq / (sum * sum));
    }

    const auto length = static_cast<size_t>(_length);
    const auto rows = static_cast<size_t>(_rows);
    _windowed.resize(length);
    _re.resize(length / 2 + 1);
    _im.resize(length / 2 + 1);
    _dRe.resize(length / 2 + 1);
    _dIm.resize(length / 2 + 1);
    _rowPower.assign(rows, 0.0);
    for (Stage& st : _stages) st.rowDb.assign(st.reassign ? rows : 0, kSilenceDb);

    const double nyquist = _sampleRate * 0.5;
    const int scale = _settings.scale;
    const double minHz = _settings.minHz, maxHz = _settings.maxHz;

    _rowTable.assign(rows, Row{});
    for (int r = 0; r < _rows; ++r) {
        const double t1 = 1.0 - static_cast<double>(r) / _rows;
        const double t0 = 1.0 - static_cast<double>(r + 1) / _rows;
        const double f0 = hzForFraction(scale, minHz, maxHz, t0);
        const double f1 = std::min(hzForFraction(scale, minHz, maxHz, t1), nyquist);
        const double fc = hzForFraction(scale, minHz, maxHz, 0.5 * (t0 + t1));

        Row& row = _rowTable[static_cast<size_t>(r)];
        if (fc >= nyquist) continue;  // nothing to show above Nyquist

        if (!split) {
            row.a = row.b = 1;
        } else if (fc < kLowSplitHz * 2.0 && multi) {
            row.a = 0; row.b = 1;
            row.mix = static_cast<float>(std::max(0.0, std::min(1.0, std::log2(fc / kLowSplitHz))));
        } else {
            row.a = 1; row.b = 2;
            row.mix = static_cast<float>(std::max(0.0, std::min(1.0, std::log2(fc / kHighSplitHz))));
        }
        // Outside a crossover only one stage is read.
        if (row.mix <= 0.0f) { row.b = row.a; }
        else if (row.mix >= 1.0f) { row.a = row.b; row.mix = 0.0f; }

        row.tilt = _settings.slopeDb * static_cast<float>(std::log2(fc / 1000.0));
        row.tapA = makeTap(_stages[row.a], f0, f1, fc);
        if (row.b != row.a) row.tapB = makeTap(_stages[row.b], f0, f1, fc);
    }

    // One transform per stage that some row reads.
    for (Stage& st : _stages) {
        if (st.binHi < st.binLo) {
            st.fft.reset();
        } else if (!st.fft || st.fft->size() != static_cast<size_t>(st.size)) {
            st.fft = spectrum::RealFft::create(static_cast<size_t>(st.size));
        }
    }
}

float SpectrogramEngine::readTap(const Stage& stage, const Tap& tap) const {
    const float* db = stage.db.data();
    if (tap.hi < 0) {
        return db[tap.lo] + (db[tap.lo + 1] - db[tap.lo]) * tap.frac;
    }
    float peak = db[tap.lo];
    for (int k = tap.lo + 1; k <= tap.hi; ++k) {
        if (db[k] > peak) peak = db[k];
    }
    return peak;
}

void SpectrogramEngine::renderColumn(const float* samples, float* out) {
    if (_dirty) rebuild();
    if (_rows <= 0) return;
    _windowedSource = samples;

    for (Stage& st : _stages) {
        if (st.binHi < st.binLo || !st.fft) continue;  // no row reads this stage

        const float* in = samples + st.offset;
        for (int n = 0; n < st.size; ++n) _windowed[static_cast<size_t>(n)] = in[n] * st.window[static_cast<size_t>(n)];
        st.fft->forward(_windowed.data(), _re.data(), _im.data());

        for (int k = st.binLo; k <= st.binHi; ++k) {
            const float re = _re[static_cast<size_t>(k)], im = _im[static_cast<size_t>(k)];
            st.db[static_cast<size_t>(k)] = 10.0f * std::log10(re * re + im * im + 1e-20f);
        }

        if (st.reassign) reassignStage(st);
    }

    const float floorDb = _settings.topDb - _settings.rangeDb;
    const float invRange = 1.0f / _settings.rangeDb;
    for (int r = 0; r < _rows; ++r) {
        const Row& row = _rowTable[static_cast<size_t>(r)];
        if (row.a < 0) { out[r] = 0.0f; continue; }

        auto level = [&](int stage, const Tap& tap) {
            const Stage& st = _stages[stage];
            const float standard = readTap(st, tap);
            return st.reassign ? std::max(st.rowDb[static_cast<size_t>(r)], standard - kUnderlayDb) : standard;
        };
        float db = level(row.a, row.tapA);
        if (row.b != row.a) {
            db += (level(row.b, row.tapB) - db) * row.mix;
        }
        const float v = (db + row.tilt - floorDb) * invRange;
        out[r] = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
    }
}

// Frequency reassignment for one stage. With X the spectrum through the window
// and Xd through its derivative, a component in bin k really sits at
//     k - Im(Xd * conj(X)) / |X|^2 * N / (2 pi)
// bins. Every bin's power goes to the row of that frequency, so a tone's whole
// lobe lands in one row: thin lines from a window short enough for sharp kicks.
void SpectrogramEngine::reassignStage(Stage& st) {
    const int half = st.size / 2;
    const float* in = _windowedSource + st.offset;
    for (int n = 0; n < st.size; ++n) _windowed[static_cast<size_t>(n)] = in[n] * st.dwindow[static_cast<size_t>(n)];
    st.fft->forward(_windowed.data(), _dRe.data(), _dIm.data());

    std::fill(_rowPower.begin(), _rowPower.end(), 0.0);
    const double binHz = _sampleRate / st.size;
    const double toBins = st.size / kTwoPi;
    const int scale = _settings.scale;
    const double minHz = _settings.minHz, maxHz = _settings.maxHz;
    const double floorPower = std::pow(10.0, kReassignFloorDb / 10.0);

    // Only rows this stage serves matter; bins a little outside their range
    // can still be reassigned into it.
    const int lo = std::max(1, st.binLo - 4);
    const int hi = std::min(half - 1, st.binHi + 4);
    for (int k = lo; k <= hi; ++k) {
        const double re = _re[static_cast<size_t>(k)], im = _im[static_cast<size_t>(k)];
        const double p = re * re + im * im;
        if (p < floorPower) continue;
        // Im(Xd * conj(X))
        const double cross = static_cast<double>(_dIm[static_cast<size_t>(k)]) * re -
                             static_cast<double>(_dRe[static_cast<size_t>(k)]) * im;
        const double hz = (k - cross / p * toBins) * binHz;
        if (hz < minHz || hz >= maxHz) continue;
        const double t = fractionForHz(scale, minHz, maxHz, hz);
        const int r = static_cast<int>((1.0 - t) * _rows);
        if (r >= 0 && r < _rows) _rowPower[static_cast<size_t>(r)] += p;
    }

    // Soften each line over its neighbours (keeping its level), and express
    // the gathered lobe energy as the tone's peak level.
    const double invLobe = 1.0 / st.lobe;
    for (int r = 0; r < _rows; ++r) {
        double m = _rowPower[static_cast<size_t>(r)];
        for (int d = 1; d <= 2; ++d) {
            if (r - d >= 0)    m = std::max(m, _rowPower[static_cast<size_t>(r - d)] * kSoften[d]);
            if (r + d < _rows) m = std::max(m, _rowPower[static_cast<size_t>(r + d)] * kSoften[d]);
        }
        st.rowDb[static_cast<size_t>(r)] = m > 0.0 ? static_cast<float>(10.0 * std::log10(m * invLobe)) : kSilenceDb;
    }
}

} // namespace p31::spectrogram
