//
//  SpectrogramClock.h
//  p31-core (ported from foo_jl_spectrogram_mac SpectrogramClock.h)
//
//  Decides how many columns each display frame renders. Columns sit on a
//  fixed time grid; a display clock (host time plus an offset that slowly
//  follows the stream time) walks along it. The stream time advances in
//  uneven steps and briefly reports none at all, so following it directly
//  makes the picture stutter.
//
//  In the plugin the stream time is the audio written so far, which grows by
//  one host buffer at a time. Columns need their audio to have arrived, so
//  they trail the display clock by a delay that follows the buffer length
//  (noteStreamStep).
//

#pragma once

#include <algorithm>
#include <cmath>

namespace p31::spectrogram {

class SpectrogramClock {
public:
    // A difference between the display clock and the stream time larger than
    // this (seek, restart, stall) starts over at the stream time instead of
    // replaying or skipping the gap.
    static constexpr double kMaxGap = 0.5;

    // Fraction of the display clock's error to the stream time corrected per frame.
    static constexpr double kSlew = 0.05;

    // Columns trail the display clock by at least this much, so their audio
    // is already there when the clock runs slightly ahead of the stream.
    static constexpr double kRenderDelay = 0.05;

    // The delay for a stream time that advances `step` seconds at a time. The
    // display clock settles half a step behind the newest audio; the stream
    // falls a whole step behind just before the next one arrives.
    static double renderDelayForStep(double step) {
        return std::clamp(0.55 * step + 0.015, kRenderDelay, 0.8 * kMaxGap);
    }

    // Each noted step lets the remembered one shrink by this factor, so the
    // delay comes back down after a single late buffer.
    static constexpr double kStepDecay = 0.98;

    // The stream time just advanced by `step` seconds in one go. The render
    // delay follows the largest recent step: a longer delay holds the picture
    // until the clock has caught up, a shorter one takes effect at once.
    void noteStreamStep(double step) {
        _step = std::max(_step * kStepDecay, std::min(step, kMaxGap));
        _delay = renderDelayForStep(_step);
    }
    double renderDelay() const { return _delay; }

    // Forget the clock (pause, stop, freeze, hidden). The next frame with a
    // stream time starts over there.
    void reset() { _valid = false; }

    // Number of columns due at host time `host`, `hop` seconds apart. Pass
    // haveStream = false when the stream gave no time this frame: the clock
    // then coasts on host time. Returns 0 while (re)starting.
    int due(double host, bool haveStream, double stream, double hop) {
        if (!_valid) {
            if (!haveStream) return 0;
            _offset = stream - host;
            _last = _target = stream - _delay;
            _valid = true;
            return 0;
        }
        if (haveStream) {
            const double error = stream - (host + _offset);
            if (std::fabs(error) > kMaxGap) {
                _valid = false;
                return 0;
            }
            _offset += error * kSlew;
        }
        const double target = _target = host + _offset - _delay;
        // Columns that could not be rendered for a while (their audio never
        // arrived) are skipped, or the grid would stay stuck behind the
        // stream's backlog.
        if (target - _last > kMaxGap) _last = target - hop;
        // The epsilon keeps a whole hop from rounding down to zero columns.
        const double n = std::floor((target - _last) / hop + 1e-9);
        return n > 0 ? static_cast<int>(n) : 0;
    }

    // Stream time of the next column on the grid.
    double next(double hop) const { return _last + hop; }

    // The column at next(hop) has been rendered.
    void advance(double hop) { _last += hop; }

    // How far the display clock has moved past the newest rendered column at
    // the last due() call, in columns (0..1). The view shifts the picture by
    // this fraction, so it moves the same distance every frame even when the
    // refresh rate is not a multiple of the column rate.
    double phase(double hop) const {
        const double p = (_target - _last) / hop;
        return p < 0.0 ? 0.0 : (p > 1.0 ? 1.0 : p);
    }

private:
    bool   _valid = false;
    double _step = 0.0;     // largest recent advance of the stream time
    double _delay = kRenderDelay;
    double _offset = 0.0;   // stream time minus host time, smoothed
    double _last = 0.0;     // stream time of the newest column
    double _target = 0.0;   // display clock at the last due() call, minus the render delay
};

} // namespace p31::spectrogram
