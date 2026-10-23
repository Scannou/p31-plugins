//
//  ScopePlugin.cpp
//  p31-scope
//

#include "ScopePlugin.h"

#include "ScopeEditor.h"

#include <p31/core/StateCodec.h>

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
    .url = P31_PROJECT_URL,
    .manual_url = "",
    .support_url = P31_PROJECT_URL "/issues",
    .version = P31_VERSION,
    .description = "Vectorscope with tri-band phase correlation",
    .features = kFeatures,
};

ScopePlugin::ScopePlugin(const clap_host* host) : PluginBase(&descriptor, host), _ring(2, kRingFrames) {}

void ScopePlugin::analyze(const float* const* channels, uint32_t channelCount, uint32_t frames) {
    _ring.write(channels, channelCount, frames);
}

void ScopePlugin::setSettings(const p31::scope::ScopeSettings& settings) {
    _settings = settings.sanitized();
    _settings.theme = p31::scope::themeMatching(_settings);
    settingsEdited();
}

std::string ScopePlugin::encodeSettings() const {
    return p31::encodeSettings(_settings);
}

void ScopePlugin::decodeSettings(std::string_view text) {
    p31::scope::ScopeSettings s;
    p31::decodeSettings(text, s);
    _settings = s.sanitized();
    // The stored index may come from the older theme list; the colors decide.
    _settings.theme = p31::scope::themeMatching(_settings);
}

std::unique_ptr<p31::Editor> ScopePlugin::createEditor(uint32_t width, uint32_t height) {
    return makeScopeEditor(*this, width, height);
}

p31::EditorSizeLimits ScopePlugin::editorSizeLimits() const {
    p31::EditorSizeLimits limits;
    limits.defaultWidth = 520;
    limits.defaultHeight = 415;
    return limits;
}

bool p31_scope_init(const char*) { return true; }
void p31_scope_deinit() {}
const void* p31_scope_get_factory(const char* factoryId) {
    return p31::SinglePluginFactory<ScopePlugin>::get(factoryId);
}
