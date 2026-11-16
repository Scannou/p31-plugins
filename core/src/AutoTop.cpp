//
//  AutoTop.cpp
//  p31-core
//

#include "p31/core/AutoTop.h"

#include <algorithm>
#include <cmath>

namespace p31 {

namespace {
    // Time constant of the top's way down, as the scope's gain on its way up.
    constexpr double kEaseSeconds = 0.15;
    // dB per second an amplitude envelope with a 1 s time constant falls.
    constexpr double kDbPerTimeConstant = 8.685889638065035;   // 20 / ln 10
}

void AutoTop::configure(const Settings& settings) {
    _settings = settings;
    _settings.maxTopDb = std::max(_settings.maxTopDb, _settings.minTopDb);
    _settings.headroomDb = std::max(_settings.headroomDb, 0.0f);
    _settings.releaseSeconds = std::max(_settings.releaseSeconds, 0.01);
    _top = std::clamp(_top, _settings.minTopDb, _settings.maxTopDb);
}

void AutoTop::reset(float topDb) {
    _env = -1e9f;
    _top = std::clamp(topDb, _settings.minTopDb, _settings.maxTopDb);
}

float AutoTop::process(float loudestDb, double seconds) {
    if (!(seconds > 0.0)) return _top;
    if (std::isfinite(loudestDb) && loudestDb > _settings.silenceDb) {
        const auto fall = static_cast<float>(seconds * kDbPerTimeConstant / _settings.releaseSeconds);
        _env = std::max(loudestDb, _env - fall);
    }
    if (_env <= _settings.silenceDb) return _top;

    const float wanted = std::clamp(_env + _settings.headroomDb, _settings.minTopDb, _settings.maxTopDb);
    // Up at once, so the level that raised the envelope already fits; down
    // gently.
    if (wanted > _top) _top = wanted;
    else _top += (wanted - _top) * static_cast<float>(1.0 - std::exp(-seconds / kEaseSeconds));
    return _top;
}

} // namespace p31
