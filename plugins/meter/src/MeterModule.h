//
//  MeterModule.h
//  p31-meter
//
//  The level meter as a module: what P31 Meter and P31 Suite's meter panel
//  both run.
//

#pragma once

#include <p31/meter/MeterSettings.h>
#include <p31/plugin/Module.h>

using MeterModule = p31::Module<p31::meter::MeterSettings>;

// Audio history the measurement needs. It reads every frame once, from a
// timer; the length covers a main thread that is busy for a few seconds, so
// the loudness measurement has no gaps.
inline constexpr size_t kMeterRingFrames = 262144;

inline MeterModule makeMeterModule(p31::PluginBase& plugin, const p31::AudioRing& ring) {
    return MeterModule(plugin, ring, "P31 Meter", [](const p31::meter::MeterSettings& settings) {
        p31::meter::MeterSettings s = settings.sanitized();
        // The colors decide which theme is shown, not the stored index.
        s.theme = p31::meter::themeMatching(s);
        return s;
    });
}
