//
//  SpectrogramPlugin.cpp
//  p31-spectrogram
//

#include "SpectrogramPlugin.h"

#include "SpectrogramEditor.h"

namespace {
    const char* const kFeatures[] = {
        CLAP_PLUGIN_FEATURE_AUDIO_EFFECT,
        CLAP_PLUGIN_FEATURE_ANALYZER,
        CLAP_PLUGIN_FEATURE_MONO,
        CLAP_PLUGIN_FEATURE_STEREO,
        nullptr,
    };
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

SpectrogramPlugin::SpectrogramPlugin(const clap_host* host)
    : PluginBase(&descriptor, host), _ring(2, kSpectrogramRingFrames), _module(makeSpectrogramModule(*this, _ring)) {}

void SpectrogramPlugin::analyze(const float* const* channels, uint32_t channelCount, uint32_t frames) {
    _ring.write(channels, channelCount, frames);
}

std::string SpectrogramPlugin::encodeSettings() const {
    return _module.encode();
}

void SpectrogramPlugin::decodeSettings(std::string_view text) {
    _module.decode(text);
}

std::unique_ptr<p31::Editor> SpectrogramPlugin::createEditor(uint32_t width, uint32_t height) {
    return makeSpectrogramEditor(_module, width, height);
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
