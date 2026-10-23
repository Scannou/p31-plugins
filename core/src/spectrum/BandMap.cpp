//
//  BandMap.cpp
//  p31-core (ported from foo_jl_spectrum_mac SpectrumAnalyzer.cpp)
//

#include "p31/spectrum/BandMap.h"

#include <algorithm>
#include <cmath>

namespace p31::spectrum {

double freqScalePos(int scale, double hz) {
    switch (scale) {
        case FreqScaleLinear:  return hz;
        case FreqScaleSoftLog: return std::log10(1.0 + hz / kSoftLogCornerHz);
        default:               return std::log10(hz);
    }
}

double freqScaleHz(int scale, double pos) {
    switch (scale) {
        case FreqScaleLinear:  return pos;
        case FreqScaleSoftLog: return kSoftLogCornerHz * (std::pow(10.0, pos) - 1.0);
        default:               return std::pow(10.0, pos);
    }
}

float magnitudeToDisplay(float magnitude, float topDb, float rangeDb) {
    if (!(magnitude > 1e-7f)) return 0.0f;   // also NaN
    const float db = 20.0f * std::log10(magnitude);
    return std::clamp((db - (topDb - rangeDb)) / rangeDb, 0.0f, 1.0f);
}

double BandMap::centreHz(int band) const {
    const double t = (static_cast<double>(band) + 0.5) / static_cast<double>(bandCount());
    return freqScaleHz(_config.scale, _posMin + (_posMax - _posMin) * t);
}

void BandMap::build(const Config& config) {
    _config = config;
    const int bands = std::max(1, config.bands);
    const int binCount = static_cast<int>(config.fftSize / 2);
    const double sr = config.sampleRate > 0 ? config.sampleRate : 48000.0;
    const double nyquist = sr * 0.5;
    const double hzPerBin = sr / static_cast<double>(config.fftSize);

    _lo.assign(bands, 0);
    _hi.assign(bands, 0);
    _centre.assign(bands, -1.0f);
    _gain.assign(bands, 1.0f);
    _avgFrom.assign(bands, -1.0);
    _avgTo.assign(bands, -1.0);
    _cumPow.assign(static_cast<size_t>(binCount) + 1, 0.0);
    const double octaveHalf = config.octaveFraction > 0 ? std::pow(2.0, 0.5 / config.octaveFraction) : 0.0;

    const double minHz = std::min(config.minHz, nyquist - 1);
    const double maxHz = std::min(config.maxHz, nyquist);
    _posMin = freqScalePos(config.scale, minHz);
    _posMax = freqScalePos(config.scale, maxHz);
    const auto hzAt = [&](double t) { return freqScaleHz(config.scale, _posMin + (_posMax - _posMin) * t); };

    for (int i = 0; i < bands; ++i) {
        const double f0 = hzAt(static_cast<double>(i) / bands);
        const double f1 = hzAt(static_cast<double>(i + 1) / bands);
        const double fc = hzAt((static_cast<double>(i) + 0.5) / bands);

        // The bins whose centre (b * hzPerBin) lies in [f0, f1).
        int lo = static_cast<int>(std::ceil(f0 / hzPerBin));
        int hi = static_cast<int>(std::ceil(f1 / hzPerBin)) - 1;
        // Every band covers at least one bin and stays in range. The DC skip
        // comes before hi is raised to lo: a band entirely below bin 1 would
        // otherwise read DC (always zero).
        if (lo < 1) lo = 1;
        if (hi < lo) hi = lo;
        if (hi >= binCount) hi = binCount - 1;
        if (lo > hi) lo = hi;
        _lo[i] = lo;
        _hi[i] = hi;

        // At the low end several bands fall inside one bin and would all read
        // the same value, drawing flat steps. Sample those at their centre,
        // interpolated between neighbouring bins; below bin 1 hold at bin 1
        // so the lowest bands read flat instead of dropping towards DC.
        if (f1 - f0 < hzPerBin) _centre[i] = static_cast<float>(std::max(1.0, fc / hzPerBin));

        // Octave smoothing window around the centre, in bin units. Where it
        // is narrower than one bin the band is read the plain way.
        if (octaveHalf > 0.0) {
            double x0 = fc / octaveHalf / hzPerBin, x1 = fc * octaveHalf / hzPerBin;
            if (x1 - x0 >= 1.0) {
                x0 = std::max(x0, 0.5);   // skip the DC bin
                x1 = std::min(x1, binCount - 0.5);
                if (x1 > x0) {
                    _avgFrom[i] = x0;
                    _avgTo[i] = x1;
                }
            }
        }

        if (config.slopeDbPerOct != 0.0f) {
            _gain[i] = static_cast<float>(std::pow(10.0, config.slopeDbPerOct * std::log2(fc / 1000.0) / 20.0));
        }
    }
}

float BandMap::plainLevel(int i, const float* mag, float topDb, float rangeDb) const {
    const int bins = static_cast<int>(binCount());
    const float gain = _gain[i];
    // Narrower than a bin: interpolated at the centre, in dB (so the curve
    // does not sag towards the quieter bin), along a monotone cubic through
    // the neighbouring bins. Straight segments would show the coarse bin
    // spacing of the bass as corners; the cubic rounds them without ever
    // overshooting a bin's level (Steffen's tangents).
    const float centre = _centre[i];
    if (centre >= 1.0f && centre < static_cast<float>(bins - 1)) {
        const int b0 = static_cast<int>(centre);
        const float t = centre - static_cast<float>(b0);
        const auto at = [&](int b) { return magnitudeToDisplay(mag[b] * gain, topDb, rangeDb); };
        const float d0 = at(b0), d1 = at(b0 + 1);
        const float dPrev = b0 > 1 ? at(b0 - 1) : d0;          // bin 0 is DC
        const float dNext = b0 + 2 < bins ? at(b0 + 2) : d1;
        // Steffen's tangent from the slopes either side (unit spacing).
        const auto tangent = [](float s0, float s1) {
            if (s0 * s1 <= 0.0f) return 0.0f;                   // extremum or flat
            const float limit = std::min({std::fabs(s0), std::fabs(s1), 0.25f * std::fabs(s0 + s1)});
            return (s0 > 0.0f ? 2.0f : -2.0f) * limit;
        };
        const float m0 = tangent(d0 - dPrev, d1 - d0);
        const float m1 = tangent(d1 - d0, dNext - d1);
        const float t2 = t * t, t3 = t2 * t;
        return (2 * t3 - 3 * t2 + 1) * d0 + (t3 - 2 * t2 + t) * m0 + (-2 * t3 + 3 * t2) * d1 + (t3 - t2) * m1;
    }
    // Otherwise the loudest of its bins (reads punchier than the average).
    float m = 0.0f;
    for (int b = _lo[i]; b <= _hi[i]; ++b) m = std::max(m, mag[b]);
    return magnitudeToDisplay(m * gain, topDb, rangeDb);
}

float BandMap::level(int i, const float* mag, float topDb, float rangeDb) const {
    if (_avgFrom[i] < 0.0) return plainLevel(i, mag, topDb, rangeDb);

    // Octave smoothing: mean power over the window, counting the bins at its
    // edges in proportion to their overlap, so the level slides smoothly from
    // one band to the next. Bin b spans b - 0.5 .. b + 0.5.
    const int bins = static_cast<int>(binCount());
    const auto cumAt = [&](double x) {
        const double e = x + 0.5;
        const int k = std::clamp(static_cast<int>(e), 0, bins - 1);
        const double p = static_cast<double>(mag[k]) * mag[k];
        return _cumPow[k] + (e - k) * p;
    };
    const double x0 = _avgFrom[i], x1 = _avgTo[i];
    const double meanPow = (cumAt(x1) - cumAt(x0)) / (x1 - x0);
    const float smoothed =
        magnitudeToDisplay(static_cast<float>(std::sqrt(std::max(0.0, meanPow))) * _gain[i], topDb, rangeDb);
    // Windows of one to two bins blend in from the plain reading, so there is
    // no kink where smoothing takes over at the low end.
    const float w = static_cast<float>(std::min(1.0, x1 - x0 - 1.0));
    return w >= 1.0f ? smoothed : plainLevel(i, mag, topDb, rangeDb) * (1.0f - w) + smoothed * w;
}

void BandMap::read(const float* mag, float topDb, float rangeDb, float* out) {
    if (_config.octaveFraction > 0) {
        const size_t bins = binCount();
        _cumPow[0] = 0.0;
        for (size_t b = 0; b < bins; ++b) _cumPow[b + 1] = _cumPow[b] + static_cast<double>(mag[b]) * mag[b];
    }
    const int bands = bandCount();
    for (int i = 0; i < bands; ++i) out[i] = level(i, mag, topDb, rangeDb);
}

} // namespace p31::spectrum
