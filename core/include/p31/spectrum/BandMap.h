//
//  BandMap.h
//  p31-core (ported from foo_jl_spectrum_mac SpectrumAnalyzer / SpectrumConfig)
//
//  Maps FFT bins onto display bands along a frequency axis: logarithmic,
//  compact-bass logarithmic or linear. Bands narrower than a bin are read
//  between bins, wide ones take their loudest bin, and optional 1/N-octave
//  smoothing averages power over a window around each band centre.
//

#pragma once

#include <cstddef>
#include <vector>

namespace p31::spectrum {

// Frequency axis mapping
enum FreqScale {
    FreqScaleLog = 0,       // Logarithmic (musically natural)
    FreqScaleLinear = 1,    // Linear
    FreqScaleSoftLog = 2,   // Compact-bass log, log10(1 + f/50): log highs, near-linear lows
};

// Corner of the soft-log scale: below it the axis runs close to linear, so the
// bottom octaves take far less width than on a pure log axis.
constexpr double kSoftLogCornerHz = 50.0;

// Axis position of a frequency in scale units. A frequency's fraction across
// the plot is (pos(f) - pos(min)) / (pos(max) - pos(min)); the band table and
// the view's gridlines both go through this so they stay aligned.
double freqScalePos(int scale, double hz);
// Inverse of freqScalePos.
double freqScaleHz(int scale, double pos);

// A magnitude (a full-scale sine reads 1.0) as a position in the dB window
// [topDb - rangeDb, topDb], 0..1.
float magnitudeToDisplay(float magnitude, float topDb, float rangeDb);

class BandMap {
public:
    struct Config {
        int bands = 48;
        size_t fftSize = 2048;
        double sampleRate = 48000.0;
        double minHz = 20.0;
        double maxHz = 20000.0;
        int scale = FreqScaleLog;
        int octaveFraction = 0;       // 0 = per-bin detail, N = average power over 1/N octave
        float slopeDbPerOct = 0.0f;   // level tilt around 1 kHz
    };

    // Allocates.
    void build(const Config& config);
    const Config& config() const { return _config; }
    int bandCount() const { return static_cast<int>(_lo.size()); }
    size_t binCount() const { return _config.fftSize / 2; }

    // Display level (0..1) of every band from per-bin magnitudes (`mag`,
    // binCount() values) into `out` (bandCount() values). No allocation.
    void read(const float* mag, float topDb, float rangeDb, float* out);

    // Band layout, for tests and labels.
    int lowBin(int band) const { return _lo[band]; }
    int highBin(int band) const { return _hi[band]; }
    // Fractional bin a narrow band is read at, or -1 when it spans bins.
    float centreBin(int band) const { return _centre[band]; }
    // Centre frequency of a band on the chosen scale.
    double centreHz(int band) const;

private:
    float level(int band, const float* mag, float topDb, float rangeDb) const;
    float plainLevel(int band, const float* mag, float topDb, float rangeDb) const;

    Config _config;
    double _posMin = 0.0, _posMax = 1.0;
    std::vector<int> _lo, _hi;               // inclusive bin range per band
    std::vector<float> _centre;              // see centreBin()
    std::vector<float> _gain;                // linear gain from the slope
    std::vector<double> _avgFrom, _avgTo;    // octave window in bin units, or -1
    std::vector<double> _cumPow;             // scratch: running power sum per bin
};

} // namespace p31::spectrum
