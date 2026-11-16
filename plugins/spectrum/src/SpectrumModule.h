//
//  SpectrumModule.h
//  p31-spectrum
//
//  The spectrum analyzer as a module: what P31 Spectrum and P31 Suite's spectrum
//  panel both run.
//

#pragma once

#include <p31/plugin/Module.h>
#include <p31/spectrum/SpectrumSettings.h>

using SpectrumModule = p31::Module<p31::spectrum::SpectrumSettings>;

// Audio history the editor needs. It reads at most ~100 ms per display
// frame; the rest is headroom for a stalled UI.
inline constexpr size_t kSpectrumRingFrames = 65536;

inline SpectrumModule makeSpectrumModule(p31::PluginBase& plugin, const p31::AudioRing& ring) {
    return SpectrumModule(plugin, ring, "P31 Spectrum", [](const p31::spectrum::SpectrumSettings& settings) {
        return settings.sanitized();
    });
}
