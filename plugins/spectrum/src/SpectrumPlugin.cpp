//
//  SpectrumPlugin.cpp
//  p31-spectrum
//

#include "SpectrumPlugin.h"

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
    .url = "",
    .manual_url = "",
    .support_url = "",
    .version = P31_VERSION,
    .description = "Real-time spectrum analyzer",
    .features = kFeatures,
};

bool p31_spectrum_init(const char*) { return true; }
void p31_spectrum_deinit() {}
const void* p31_spectrum_get_factory(const char* factoryId) {
    return p31::SinglePluginFactory<SpectrumPlugin>::get(factoryId);
}
