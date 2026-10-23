//
//  SpectrumAnalyzer.h
//  p31-core (ported from foo_jl_spectrum_mac SpectrumAnalyzer.h)
//
//  Turns a stereo stream into display bands: overlapping windowed FFT frames
//  (Stft), bin-to-band mapping (BandMap), then temporal smoothing, a slower
//  "shadow" layer and falling peak lines. Audio arrives through process(),
//  the display advances through tick(); all dynamics scale with real time,
//  so any block size and refresh rate look the same.
//
//  Not realtime safe: runs on the UI thread, fed from the plugin's ring.
//

#pragma once

#include "p31/spectrum/BandMap.h"
#include "p31/spectrum/Stft.h"

#include <deque>
#include <vector>

namespace p31::spectrum {

// How band levels are smoothed over time
enum SmoothingMode {
    SmoothingAttackDecay = 0,   // Fast attack, slower decay, applied to the display value
    SmoothingRms = 1,           // Exponential average of power per FFT bin (RTA-style)
};

class SpectrumAnalyzer {
public:
    struct Settings {
        int    barCount = 48;
        int    fftSize = 2048;          // power of 2
        int    window = WindowHann;
        int    minHz = 20;
        int    maxHz = 20000;
        int    smoothing = 60;          // attack/decay: 0-100, higher = smoother
        int    smoothingMode = SmoothingAttackDecay;
        int    rmsMs = 125;             // RMS: power averaging time constant (ms)
        bool   side = false;            // also analyse the side channel
        int    octaveFraction = 0;      // 0 = per-bin detail, N = 1/N octave smoothing
        float  slopeDbPerOct = 0.0f;    // level tilt around 1 kHz (0 = flat)
        int    freqScale = FreqScaleLog;
        float  topDb = 0.0f;            // level at height 1
        float  rangeDb = 80.0f;         // dB from height 1 down to 0

        // Fall dynamics, as rates per 1/60 s frame. Each layer falls slower
        // than the one below it.
        float  shadowFall = 0.012f;     // shadow linear fall per frame
        float  peakFall = 0.0025f;      // peak line linear fall per frame, after its hold
        float  peakHoldSec = 0.4f;      // how long a peak holds before falling
        bool   peakHold = true;         // keep the peak line; off: no peak history
    };

    static constexpr int kMinBarCount = 4;
    static constexpr int kMaxBarCount = 2048;
    // The peak history grows with the hold time.
    static constexpr float kMaxPeakHoldSec = 2.0f;

    SpectrumAnalyzer();

    // Apply new settings (allocates). Current levels are kept, resampled when
    // the bar count changes, so a settings tweak does not blank the display.
    void configure(const Settings& settings);
    const Settings& settings() const { return _settings; }

    // Sample rate of the frames passed to process(). A change rebuilds the
    // band table and restarts the FFT history.
    void setSampleRate(double sampleRate);
    double sampleRate() const { return _sampleRate; }

    // Analyse `frames` interleaved stereo frames, oldest first.
    void process(const float* lr, size_t frames);

    // Advance the display by `dt` seconds. Returns true when audio arrived
    // since the previous tick.
    bool tick(double dt);

    // Drop all levels and history (the editor was hidden).
    void reset();

    // Display values (0..1), one per bar.
    const std::vector<float>& bars() const { return _bars; }
    const std::vector<float>& shadow() const { return _shadow; }
    const std::vector<float>& peaks() const { return _peaks; }
    // Side channel per bar when analysed, else empty.
    const std::vector<float>& side() const { return _sideBars; }

    // Any bar, shadow or peak is still above zero.
    bool isActive() const { return _active; }

private:
    void configureStft();
    void rebuildBands();
    void onSpectrum(const float* midPower, const float* sidePower);
    void updateLayer(std::vector<float>& layer, std::vector<float>& target, std::vector<float>& power,
                     std::vector<float>& tickMax, bool live, float attackKeep, float decayKeep, double dt);
    void updatePeaks(double dtSec);
    void clearPeakHistory();

    Settings _settings;
    double _sampleRate = 0.0;
    Stft _stft;
    BandMap _bands;
    bool _bandsDirty = true;

    // Per bin: RMS mode's running power average; attack/decay mode's loudest
    // power since the last tick.
    std::vector<float> _binPow, _binPowSide;
    std::vector<float> _tickMax, _tickMaxSide;
    bool _fresh = false;      // a spectrum arrived since the last tick
    bool _live = false;       // audio arrived since the last tick
    float _hopRmsKeep = 0.0f; // RMS average decay per hop
    std::vector<float> _mag;  // scratch: per-bin magnitude

    // Attack/decay: the level each layer eases towards.
    std::vector<float> _target, _targetSide;

    std::vector<float> _bars;     // fast: smoothed level
    std::vector<float> _shadow;   // medium: falls slower than the bar
    std::vector<float> _peaks;    // slowest: holds, then falls at a steady rate
    std::vector<float> _sideBars;

    // Peak line history; see updatePeaks.
    std::deque<std::vector<float>> _peakRing;
    std::deque<double> _peakTimes;
    std::vector<std::vector<float>> _peakPool;
    std::vector<float> _peakQ;
    double _peakQTime = 0.0;
    double _clock = 0.0;

    bool _active = false;
};

} // namespace p31::spectrum
