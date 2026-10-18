//
//  ScopePlugin.cpp
//  p31-scope
//

#include "ScopePlugin.h"

#include "ScopeEditor.h"

namespace {
    const char* const kFeatures[] = {
        CLAP_PLUGIN_FEATURE_AUDIO_EFFECT,
        CLAP_PLUGIN_FEATURE_ANALYZER,
        CLAP_PLUGIN_FEATURE_MONO,
        CLAP_PLUGIN_FEATURE_STEREO,
        nullptr,
    };

    // Audio history kept for the editor. The editor reads at most ~50 ms per
    // display frame; the rest is headroom for a stalled UI.
    constexpr size_t kRingFrames = 65536;
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

ScopePlugin::ScopePlugin(const clap_host* host) : PluginBase(&descriptor, host), _ring(2, kRingFrames) {}

void ScopePlugin::analyze(const float* const* channels, uint32_t channelCount, uint32_t frames) {
    _ring.write(channels, channelCount, frames);
}

std::unique_ptr<p31::Editor> ScopePlugin::createEditor(uint32_t width, uint32_t height) {
    return makeScopeEditor(*this, width, height);
}

p31::EditorSizeLimits ScopePlugin::editorSizeLimits() const {
    p31::EditorSizeLimits limits;
    limits.defaultWidth = 420;
    limits.defaultHeight = 460;
    return limits;
}

bool p31_scope_init(const char*) { return true; }
void p31_scope_deinit() {}
const void* p31_scope_get_factory(const char* factoryId) {
    return p31::SinglePluginFactory<ScopePlugin>::get(factoryId);
}
