//
//  SpectrogramModule.h
//  p31-spectrogram
//
//  The spectrogram as a module: what P31 Spectrogram and P31 Suite's spectrogram
//  panel both run.
//

#pragma once

#include <p31/plugin/Module.h>
#include <p31/spectrogram/SpectrogramSettings.h>

using SpectrogramModule = p31::Module<p31::spectrogram::SpectrogramSettings>;

// Audio history the editor needs. Each column reads a whole analysis window
// (up to 65536 frames) that ends a little behind the newest audio; the rest
// is headroom for long host buffers and a late display frame.
inline constexpr size_t kSpectrogramRingFrames = 262144;

inline SpectrogramModule makeSpectrogramModule(p31::PluginBase& plugin, const p31::AudioRing& ring) {
    return SpectrogramModule(plugin, ring, "P31 Spectrogram", [](const p31::spectrogram::SpectrogramSettings& settings) {
        return settings.sanitized();
    });
}
