//
//  MeterMeasurement.cpp
//  p31-meter
//

#include "MeterMeasurement.h"

#include <p31/core/AudioRing.h>

#include <dispatch/dispatch.h>

#include <algorithm>
#include <chrono>
#include <mutex>
#include <vector>

namespace {
    using Clock = std::chrono::steady_clock;

    // The timer's period. An open editor updates more often.
    constexpr uint64_t kTimerNanoseconds = 50'000'000;
    // Longest stretch taken in one update (2.7 s at 48 kHz). After a longer
    // stall the measurement resumes at "now" instead of replaying the gap.
    constexpr size_t kMaxFramesPerUpdate = 131072;
    // Longest time one update moves the ballistics on by.
    constexpr double kMaxStep = 0.25;
    // A host hands audio over a buffer at a time, so an update often finds
    // nothing new. Only a gap longer than this (as in P31 Spectrogram) is a
    // stop: from then on the levels fall as if fed silence.
    constexpr double kStallSeconds = 0.6;
    // After this long without audio everything has fallen back.
    constexpr double kSettleSeconds = 4.0;
}

struct MeterMeasurement::State {
    explicit State(MeterModule& m) : module(m), reader(m.ring()), lr(kMaxFramesPerUpdate * 2) {}

    std::mutex mutex;
    bool alive = true;       // false once the measurement is gone: the module is too
    bool running = true;
    MeterModule& module;
    p31::AudioRingReader reader;
    p31::meter::MeterEngine engine;
    std::vector<float> lr;   // interleaved L/R read by one update
    Clock::time_point lastUpdate{};
    bool updated = false;
    double sinceAudio = kSettleSeconds;
    dispatch_source_t timer = nullptr;

    // With the mutex held.
    void update() {
        const Clock::time_point now = Clock::now();
        const double elapsed = updated ? std::chrono::duration<double>(now - lastUpdate).count() : 0.0;
        const double dt = std::clamp(elapsed, 0.0, kMaxStep);
        lastUpdate = now;
        updated = true;

        const double sampleRate = module.sampleRate();
        const size_t frames = sampleRate > 0 ? reader.readNew(lr.data(), kMaxFramesPerUpdate).frames : 0;
        if (frames > 0) {
            const float gain = module.plugin().analysisGain();
            if (gain != 1.0f) {
                for (size_t i = 0; i < frames * 2; ++i) lr[i] *= gain;
            }
            engine.process(lr.data(), frames, sampleRate, static_cast<int>(module.plugin().analysisChannels()));
            sinceAudio = 0.0;
        } else {
            sinceAudio += elapsed;
            if (sinceAudio > kStallSeconds && sinceAudio < kSettleSeconds) engine.idle(dt);
        }
        engine.advance(dt);
    }

    static void tick(void* context) {
        auto* s = static_cast<State*>(context);
        const std::lock_guard<std::mutex> lock(s->mutex);
        if (s->alive && s->running) s->update();
    }

    // The timer is cancelled: nothing refers to the state any more.
    static void finish(void* context) {
        auto* s = static_cast<State*>(context);
        dispatch_release(s->timer);
        delete s;
    }
};

MeterMeasurement::MeterMeasurement(MeterModule& module) : _state(new State(module)) {
    _state->engine.configure(p31::meter::engineSettings(module.settings()));
    _state->timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, dispatch_get_main_queue());
    if (!_state->timer) return;
    dispatch_set_context(_state->timer, _state);
    dispatch_source_set_event_handler_f(_state->timer, &State::tick);
    dispatch_source_set_cancel_handler_f(_state->timer, &State::finish);
    dispatch_source_set_timer(_state->timer, dispatch_time(DISPATCH_TIME_NOW, static_cast<int64_t>(kTimerNanoseconds)),
                              kTimerNanoseconds, kTimerNanoseconds / 5);
    dispatch_resume(_state->timer);
}

MeterMeasurement::~MeterMeasurement() {
    {
        const std::lock_guard<std::mutex> lock(_state->mutex);
        _state->alive = false;
    }
    // A tick may be on its way: the state goes once the timer has let go.
    if (_state->timer) dispatch_source_cancel(_state->timer);
    else delete _state;
}

void MeterMeasurement::setRunning(bool running) {
    const std::lock_guard<std::mutex> lock(_state->mutex);
    if (running == _state->running) return;
    _state->running = running;
    if (running) {
        // Continue at "now".
        _state->reader.skipToNow();
        _state->updated = false;
    }
}

void MeterMeasurement::settingsChanged() {
    const std::lock_guard<std::mutex> lock(_state->mutex);
    _state->engine.configure(p31::meter::engineSettings(_state->module.settings()));
}

void MeterMeasurement::update() {
    const std::lock_guard<std::mutex> lock(_state->mutex);
    if (_state->running) _state->update();
}

p31::meter::Snapshot MeterMeasurement::snapshot() const {
    const std::lock_guard<std::mutex> lock(_state->mutex);
    return _state->engine.snapshot();
}

double MeterMeasurement::quietSeconds() const {
    const std::lock_guard<std::mutex> lock(_state->mutex);
    return _state->sinceAudio;
}

void MeterMeasurement::resetPeaks() {
    const std::lock_guard<std::mutex> lock(_state->mutex);
    _state->engine.resetPeaks();
}

void MeterMeasurement::resetLoudness() {
    const std::lock_guard<std::mutex> lock(_state->mutex);
    _state->engine.resetLoudness();
}
