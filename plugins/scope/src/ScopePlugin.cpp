//
//  ScopePlugin.cpp
//  p31-scope
//

#include "ScopePlugin.h"

namespace {
    const char* const kFeatures[] = {
        CLAP_PLUGIN_FEATURE_AUDIO_EFFECT,
        CLAP_PLUGIN_FEATURE_ANALYZER,
        CLAP_PLUGIN_FEATURE_MONO,
        CLAP_PLUGIN_FEATURE_STEREO,
        nullptr,
    };
}

const clap_plugin_descriptor ScopePlugin::descriptor{
    .clap_version = CLAP_VERSION_INIT,
    .id = P31_PLUGIN_ID,
    .name = P31_PLUGIN_NAME,
    .vendor = P31_VENDOR,
    .url = "",
    .manual_url = "",
    .support_url = "",
    .version = P31_VERSION,
    .description = "Vectorscope with tri-band phase correlation",
    .features = kFeatures,
};

bool p31_scope_init(const char*) { return true; }
void p31_scope_deinit() {}
const void* p31_scope_get_factory(const char* factoryId) {
    return p31::SinglePluginFactory<ScopePlugin>::get(factoryId);
}
