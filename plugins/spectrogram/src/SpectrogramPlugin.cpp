//
//  SpectrogramPlugin.cpp
//  p31-spectrogram
//

#include "SpectrogramPlugin.h"

#include "SpectrogramEditor.h"

#include <p31/core/StateCodec.h>

namespace {
    const char* const kFeatures[] = {
        CLAP_PLUGIN_FEATURE_AUDIO_EFFECT,
        CLAP_PLUGIN_FEATURE_ANALYZER,
        CLAP_PLUGIN_FEATURE_MONO,
        CLAP_PLUGIN_FEATURE_STEREO,
        nullptr,
    };

    // Audio history kept for the editor. Each column reads a whole analysis
    // window (up to 65536 frames) that ends a little behind the newest audio;
    // the rest is headroom for long host buffers and a late display frame.
    constexpr size_t kRingFrames = 262144;
}

const clap_plugin_descriptor SpectrogramPlugin::descriptor{
    .clap_version = CLAP_VERSION_INIT,
    .id = P31_PLUGIN_ID,
    .name = P31_PLUGIN_NAME,
    .vendor = P31_VENDOR,
    .url = P31_PROJECT_URL,
    .manual_url = "",
    .support_url = P31_PROJECT_URL "/issues",
    .version = P31_VERSION,
    .description = "Scrolling spectrogram",
    .features = kFeatures,
};

SpectrogramPlugin::SpectrogramPlugin(const clap_host* host) : PluginBase(&descriptor, host), _ring(2, kRingFrames) {}

void SpectrogramPlugin::analyze(const float* const* channels, uint32_t channelCount, uint32_t frames) {
    _ring.write(channels, channelCount, frames);
}

void SpectrogramPlugin::setSettings(const p31::spectrogram::SpectrogramSettings& settings) {
    _settings = settings.sanitized();
    settingsEdited();
}

std::string SpectrogramPlugin::encodeSettings() const {
    return p31::encodeSettings(_settings);
}

void SpectrogramPlugin::decodeSettings(std::string_view text) {
    p31::spectrogram::SpectrogramSettings s;
    p31::decodeSettings(text, s);
    _settings = s.sanitized();
}

std::unique_ptr<p31::Editor> SpectrogramPlugin::createEditor(uint32_t width, uint32_t height) {
    return makeSpectrogramEditor(*this, width, height);
}

p31::EditorSizeLimits SpectrogramPlugin::editorSizeLimits() const {
    p31::EditorSizeLimits limits;
    limits.defaultWidth = 960;
    limits.defaultHeight = 540;
    limits.minWidth = 200;
    limits.minHeight = 100;
    return limits;
}

bool p31_spectrogram_init(const char*) { return true; }
void p31_spectrogram_deinit() {}
const void* p31_spectrogram_get_factory(const char* factoryId) {
    return p31::SinglePluginFactory<SpectrogramPlugin>::get(factoryId);
}
