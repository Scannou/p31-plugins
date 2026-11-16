//
//  SpectrumPlugin.cpp
//  p31-spectrum
//

#include "SpectrumPlugin.h"

#include "SpectrumEditor.h"

namespace {
    const char* const kFeatures[] = {
        CLAP_PLUGIN_FEATURE_AUDIO_EFFECT,
        CLAP_PLUGIN_FEATURE_ANALYZER,
        CLAP_PLUGIN_FEATURE_MONO,
        CLAP_PLUGIN_FEATURE_STEREO,
        nullptr,
    };
}

const clap_plugin_descriptor SpectrumPlugin::descriptor{
    .clap_version = CLAP_VERSION_INIT,
    .id = P31_PLUGIN_ID,
    .name = P31_PLUGIN_NAME,
    .vendor = P31_VENDOR,
    .url = P31_PROJECT_URL,
    .manual_url = "",
    .support_url = P31_PROJECT_URL "/issues",
    .version = P31_VERSION,
    .description = "Real-time spectrum analyzer",
    .features = kFeatures,
};

SpectrumPlugin::SpectrumPlugin(const clap_host* host)
    : PluginBase(&descriptor, host), _ring(2, kSpectrumRingFrames), _module(makeSpectrumModule(*this, _ring)) {}

void SpectrumPlugin::analyze(const float* const* channels, uint32_t channelCount, uint32_t frames) {
    _ring.write(channels, channelCount, frames);
}

std::string SpectrumPlugin::encodeSettings() const {
    return _module.encode();
}

void SpectrumPlugin::decodeSettings(std::string_view text) {
    _module.decode(text);
}

std::unique_ptr<p31::Editor> SpectrumPlugin::createEditor(uint32_t width, uint32_t height) {
    return makeSpectrumEditor(_module, width, height);
}

p31::EditorSizeLimits SpectrumPlugin::editorSizeLimits() const {
    p31::EditorSizeLimits limits;
    limits.defaultWidth = 960;
    limits.defaultHeight = 540;
    limits.minWidth = 200;
    limits.minHeight = 100;
    return limits;
}

bool p31_spectrum_init(const char*) { return true; }
void p31_spectrum_deinit() {}
const void* p31_spectrum_get_factory(const char* factoryId) {
    return p31::SinglePluginFactory<SpectrumPlugin>::get(factoryId);
}
