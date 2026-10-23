//
//  SpectrumAnalyzer.cpp
//  p31-core (ported from foo_jl_spectrum_mac SpectrumAnalyzer.cpp)
//

#include "p31/spectrum/SpectrumAnalyzer.h"

#include <algorithm>
#include <cmath>

namespace p31::spectrum {

namespace {
    // Nearest-neighbour resample along the frequency axis (bars share the same
    // fractional layout at any count). Empty input yields zeros.
    template <typename T>
    void resampleTo(std::vector<T>& v, size_t n) {
        const size_t old = v.size();
        if (old == n) return;
        if (old == 0) { v.assign(n, T{}); return; }
        std::vector<T> out(n);
        for (size_t i = 0; i < n; ++i) {
            out[i] = v[std::min(old - 1, static_cast<size_t>((static_cast<double>(i) + 0.5) * old / n))];
        }
        v.swap(out);
    }

    int nextPow2(int v) {
        int p = 1;
        while (p < v) p <<= 1;
        return p;
    }

    // Peak history keeps at most one entry per 1/60 s; faster frames merge.
    constexpr double kPeakHistoryStep = 1.0 / 60.0;
    // Attack/decay levels this low with nothing to rise to snap to zero.
    constexpr float kSilentLevel = 0.0015f;
}

SpectrumAnalyzer::SpectrumAnalyzer() {
    configure(Settings{});
}

void SpectrumAnalyzer::configure(const Settings& settings) {
    const Settings old = _settings;
    const bool first = _stft.binCount() == 0;
    _settings = settings;

    Settings& s = _settings;
    s.barCount = std::clamp(s.barCount, kMinBarCount, kMaxBarCount);
    s.fftSize = nextPow2(std::clamp(s.fftSize, static_cast<int>(kMinFftSize), static_cast<int>(kMaxFftSize)));
    if (s.window != WindowBlackmanHarris) s.window = WindowHann;
    s.minHz = std::max(s.minHz, 10);
    s.maxHz = std::max(s.maxHz, s.minHz + 100);
    s.smoothing = std::clamp(s.smoothing, 0, 100);
    if (s.smoothingMode != SmoothingRms) s.smoothingMode = SmoothingAttackDecay;
    s.rmsMs = std::max(s.rmsMs, 0);
    s.octaveFraction = std::max(s.octaveFraction, 0);
    s.shadowFall = std::max(s.shadowFall, 0.0005f);
    s.peakFall = std::max(s.peakFall, 0.0001f);
    s.peakHoldSec = std::clamp(s.peakHoldSec, 0.0f, kMaxPeakHoldSec);
    s.rangeDb = std::max(s.rangeDb, 10.0f);

    if (first || s.fftSize != old.fftSize || s.window != old.window || s.side != old.side) {
        configureStft();
    } else if (s.rmsMs != old.rmsMs) {
        const double hopSec = static_cast<double>(_stft.config().hop) / (_sampleRate > 0 ? _sampleRate : 48000.0);
        _hopRmsKeep = s.rmsMs > 0 ? static_cast<float>(std::exp(-hopSec * 1000.0 / s.rmsMs)) : 0.0f;
    }

    const size_t n = static_cast<size_t>(s.barCount);
    resampleTo(_bars, n);
    resampleTo(_shadow, n);
    resampleTo(_peaks, n);
    resampleTo(_target, n);
    resampleTo(_peakQ, n);
    for (auto& entry : _peakRing) resampleTo(entry, n);
    _peakPool.clear();
    if (s.side) {
        resampleTo(_sideBars, n);
        resampleTo(_targetSide, n);
    } else {
        _sideBars.clear();
        _targetSide.clear();
    }
    if (s.smoothingMode != old.smoothingMode) {
        std::fill(_binPow.begin(), _binPow.end(), 0.0f);
        std::fill(_binPowSide.begin(), _binPowSide.end(), 0.0f);
    }
    _bandsDirty = true;
}

void SpectrumAnalyzer::setSampleRate(double sampleRate) {
    if (sampleRate <= 0 || sampleRate == _sampleRate) return;
    _sampleRate = sampleRate;
    configureStft();
    _bandsDirty = true;
}

void SpectrumAnalyzer::configureStft() {
    const double sr = _sampleRate > 0 ? _sampleRate : 48000.0;
    Stft::Config c;
    c.fftSize = static_cast<size_t>(_settings.fftSize);
    c.window = _settings.window;
    c.hop = Stft::defaultHop(c.fftSize, sr);
    c.side = _settings.side;
    _stft.configure(c);

    const size_t bins = _stft.binCount();
    _binPow.assign(bins, 0.0f);
    _tickMax.assign(bins, 0.0f);
    _binPowSide.assign(c.side ? bins : 0, 0.0f);
    _tickMaxSide.assign(c.side ? bins : 0, 0.0f);
    _mag.assign(bins, 0.0f);
    _fresh = false;

    const double hopSec = static_cast<double>(_stft.config().hop) / sr;
    _hopRmsKeep = _settings.rmsMs > 0 ? static_cast<float>(std::exp(-hopSec * 1000.0 / _settings.rmsMs)) : 0.0f;
}

void SpectrumAnalyzer::rebuildBands() {
    BandMap::Config c;
    c.bands = _settings.barCount;
    c.fftSize = _stft.config().fftSize;
    c.sampleRate = _sampleRate > 0 ? _sampleRate : 48000.0;
    c.minHz = _settings.minHz;
    c.maxHz = _settings.maxHz;
    c.scale = _settings.freqScale;
    c.octaveFraction = _settings.octaveFraction;
    c.slopeDbPerOct = _settings.slopeDbPerOct;
    _bands.build(c);
    _bandsDirty = false;
}

void SpectrumAnalyzer::process(const float* lr, size_t frames) {
    if (frames == 0) return;
    _live = true;
    _stft.feed(lr, frames, [this](const float* mid, const float* side) { onSpectrum(mid, side); });
}

void SpectrumAnalyzer::onSpectrum(const float* midPower, const float* sidePower) {
    _fresh = true;
    const size_t bins = _binPow.size();
    const auto accumulate = [&](const float* p, std::vector<float>& avg, std::vector<float>& max) {
        if (_settings.smoothingMode == SmoothingRms) {
            // Average power per bin over time, so transients decay at one
            // rate everywhere. Per hop, so the time constant is exact.
            const float keep = _hopRmsKeep;
            for (size_t b = 0; b < bins; ++b) avg[b] = avg[b] * keep + p[b] * (1.0f - keep);
        } else {
            // The loudest moment since the last display frame.
            for (size_t b = 0; b < bins; ++b) max[b] = std::max(max[b], p[b]);
        }
    };
    accumulate(midPower, _binPow, _tickMax);
    if (sidePower && _binPowSide.size() == bins) accumulate(sidePower, _binPowSide, _tickMaxSide);
}

void SpectrumAnalyzer::updateLayer(std::vector<float>& layer, std::vector<float>& target, std::vector<float>& power,
                                   std::vector<float>& tickMax, bool live, float attackKeep, float decayKeep,
                                   double dt) {
    const int bars = static_cast<int>(layer.size());
    const size_t bins = power.size();
    if (bars != _bands.bandCount() || bins != _bands.binCount() || _mag.size() != bins) return;
    const float topDb = _settings.topDb, rangeDb = _settings.rangeDb;

    if (_settings.smoothingMode == SmoothingRms) {
        // The bars read the averaged spectrum directly. With no audio coming
        // in it runs down at its own rate.
        if (!live && _settings.rmsMs > 0) {
            const float keep = static_cast<float>(std::exp(-dt * 1000.0 / _settings.rmsMs));
            for (float& p : power) p *= keep;
        } else if (!live) {
            std::fill(power.begin(), power.end(), 0.0f);
        }
        for (size_t b = 0; b < bins; ++b) _mag[b] = std::sqrt(power[b]);
        _bands.read(_mag.data(), topDb, rangeDb, layer.data());
        return;
    }

    if (live && _fresh) {
        for (size_t b = 0; b < bins; ++b) _mag[b] = std::sqrt(tickMax[b]);
        std::fill(tickMax.begin(), tickMax.end(), 0.0f);
        _bands.read(_mag.data(), topDb, rangeDb, target.data());
    } else if (!live) {
        std::fill(target.begin(), target.end(), 0.0f);
    }
    // Between spectra (a fast display) the bars keep easing towards the
    // latest one.
    for (int i = 0; i < bars; ++i) {
        const float prev = layer[i];
        const float keep = target[i] > prev ? attackKeep : decayKeep;
        layer[i] = prev * keep + target[i] * (1.0f - keep);
        if (target[i] <= 0.0f && layer[i] < kSilentLevel) layer[i] = 0.0f;
    }
}

// The peak line at time t is the highest of every past level L(s), each held
// for the hold time and then falling at the peak fall rate:
//     peak(t) = max over s <= t of  L(s) - rate * max(0, t - s - hold)
//             = max( highest L within [t - hold, t],  Q(t - hold) )
// where Q is the plain linear-fall envelope (no hold) of the levels older than
// the hold window. A per-bar "hold timer" that restarts only on a new maximum
// forgets a slightly lower level that arrives during the hold, so neighbouring
// bars end up seconds apart in their fall and the line shows notches; this
// form keeps every level and stays as smooth across frequency as the bars.
void SpectrumAnalyzer::updatePeaks(double dtSec) {
    _clock += dtSec;
    const size_t n = _bars.size();
    const double hold = _settings.peakHoldSec;
    const double fallPerSec = _settings.peakFall * 60.0;

    // An entry keeps the time of its first frame. Moving it to the latest
    // merged frame would let a fast display merge into one entry forever,
    // which never ages out of the hold window and pins the peak line.
    if (!_peakTimes.empty() && _clock - _peakTimes.back() < kPeakHistoryStep) {
        std::vector<float>& last = _peakRing.back();
        for (size_t i = 0; i < n; ++i) last[i] = std::max(last[i], _bars[i]);
    } else {
        std::vector<float> entry;
        if (!_peakPool.empty()) {
            entry.swap(_peakPool.back());
            _peakPool.pop_back();
        }
        entry.assign(_bars.begin(), _bars.end());
        _peakRing.push_back(std::move(entry));
        _peakTimes.push_back(_clock);
    }

    // Levels whose hold has run out fold into the envelope Q.
    if (_peakQ.size() != n) _peakQ.assign(n, 0.0f);
    while (!_peakTimes.empty() && _peakTimes.front() <= _clock - hold) {
        const double s = _peakTimes.front();
        const float fall = static_cast<float>(std::max(0.0, s - _peakQTime) * fallPerSec);
        const std::vector<float>& v = _peakRing.front();
        for (size_t i = 0; i < n; ++i) _peakQ[i] = std::max(_peakQ[i] - fall, v[i]);
        _peakQTime = s;
        _peakPool.push_back(std::move(_peakRing.front()));
        _peakRing.pop_front();
        _peakTimes.pop_front();
    }

    // Entry by entry across all bars rather than bar by bar across the ring:
    // contiguous inner loops the compiler vectorises, over up to bars x
    // entries-in-the-hold-window values per frame.
    const float qFall = static_cast<float>(std::max(0.0, (_clock - hold) - _peakQTime) * fallPerSec);
    for (size_t i = 0; i < n; ++i) _peaks[i] = _peakQ[i] - qFall;
    for (const std::vector<float>& v : _peakRing) {
        for (size_t i = 0; i < n; ++i) _peaks[i] = std::max(_peaks[i], v[i]);
    }
    for (size_t i = 0; i < n; ++i) _peaks[i] = std::max(_peaks[i], 0.0f);
}

void SpectrumAnalyzer::clearPeakHistory() {
    for (auto& entry : _peakRing) _peakPool.push_back(std::move(entry));
    _peakRing.clear();
    _peakTimes.clear();
    std::fill(_peakQ.begin(), _peakQ.end(), 0.0f);
    _peakQTime = _clock;
}

bool SpectrumAnalyzer::tick(double dt) {
    // All rates are defined per 1/60 s frame; `frames` rescales them to the
    // actual step so motion looks the same at any refresh rate. Clamped so a
    // stall does not turn into one huge step.
    const float frames = static_cast<float>(std::clamp(dt * 60.0, 0.25, 4.0));
    const double step = frames / 60.0;

    // Fraction of the previous value kept per frame: bars rise quickly and
    // fall smoothly.
    const float s = static_cast<float>(_settings.smoothing) / 100.0f;
    const float decayKeep = std::pow(0.60f + 0.39f * s, frames);    // ~0.60 .. 0.99 per frame
    const float attackKeep = std::pow(0.10f + 0.50f * s, frames);   // ~0.10 .. 0.60 per frame

    const bool live = _live;
    // No audio, and everything has fallen off the display: skip the per-frame
    // work until audio returns. The averaged spectrum is below the display by
    // now, so it restarts from silence.
    if (!live && !_active) {
        std::fill(_binPow.begin(), _binPow.end(), 0.0f);
        std::fill(_binPowSide.begin(), _binPowSide.end(), 0.0f);
        std::fill(_tickMax.begin(), _tickMax.end(), 0.0f);
        std::fill(_tickMaxSide.begin(), _tickMaxSide.end(), 0.0f);
        clearPeakHistory();
        _fresh = false;
        return false;
    }

    if (_bandsDirty) rebuildBands();
    updateLayer(_bars, _target, _binPow, _tickMax, live, attackKeep, decayKeep, step);
    if (_settings.side) updateLayer(_sideBars, _targetSide, _binPowSide, _tickMaxSide, live, attackKeep, decayKeep, step);
    _fresh = false;
    _live = false;

    // Three timescales, each rising instantly to the bar and falling slower
    // than the layer beneath it: bar (fast) < shadow (medium) < peak (slowest).
    const float shadowFall = _settings.shadowFall * frames;
    const size_t bars = _bars.size();
    for (size_t i = 0; i < bars; ++i) {
        const float b = _bars[i];
        if (b >= _shadow[i]) {
            _shadow[i] = b;
        } else {
            _shadow[i] = std::max(_shadow[i] - shadowFall, b);
        }
    }
    if (_settings.peakHold) {
        updatePeaks(step);
    } else {
        // Not drawn: keep nothing that would hold the display awake.
        _clock += step;
        clearPeakHistory();
        std::fill(_peaks.begin(), _peaks.end(), 0.0f);
    }

    bool active = false;
    for (size_t i = 0; i < bars && !active; ++i) active = _bars[i] > 0.0f || _shadow[i] > 0.0f || _peaks[i] > 0.0f;
    for (size_t i = 0; i < _sideBars.size() && !active; ++i) active = _sideBars[i] > 0.0f;
    _active = active;
    return live;
}

void SpectrumAnalyzer::reset() {
    _stft.reset();
    std::fill(_bars.begin(), _bars.end(), 0.0f);
    std::fill(_shadow.begin(), _shadow.end(), 0.0f);
    std::fill(_peaks.begin(), _peaks.end(), 0.0f);
    std::fill(_target.begin(), _target.end(), 0.0f);
    std::fill(_sideBars.begin(), _sideBars.end(), 0.0f);
    std::fill(_targetSide.begin(), _targetSide.end(), 0.0f);
    std::fill(_binPow.begin(), _binPow.end(), 0.0f);
    std::fill(_binPowSide.begin(), _binPowSide.end(), 0.0f);
    std::fill(_tickMax.begin(), _tickMax.end(), 0.0f);
    std::fill(_tickMaxSide.begin(), _tickMaxSide.end(), 0.0f);
    clearPeakHistory();
    _fresh = false;
    _live = false;
    _active = false;
}

} // namespace p31::spectrum
