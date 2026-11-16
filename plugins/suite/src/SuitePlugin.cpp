//
//  SuitePlugin.cpp
//  p31-suite
//

#include "SuitePlugin.h"

#include "SuiteEditor.h"

#include <p31/core/PresetStore.h>
#include <p31/core/StateCodec.h>

#include <algorithm>

namespace {
    const char* const kFeatures[] = {
        CLAP_PLUGIN_FEATURE_AUDIO_EFFECT,
        CLAP_PLUGIN_FEATURE_ANALYZER,
        CLAP_PLUGIN_FEATURE_MONO,
        CLAP_PLUGIN_FEATURE_STEREO,
        nullptr,
    };

    // One history for every panel: as long as the longest any of them needs.
    constexpr size_t kRingFrames = std::max({kSpectrumRingFrames, kScopeRingFrames, kSpectrogramRingFrames});

    // The panels' settings share the state, each under its own key prefix.
    // The arrangement's keys ("layout.*") have none.
    constexpr const char* kSpectrumKeys = "spectrum.";
    constexpr const char* kScopeKeys = "scope.";
    constexpr const char* kSpectrogramKeys = "spectrogram.";

    // A module's settings as saved for new instances of its own plugin.
    template <class Module>
    std::string moduleDefault(const Module& module, const char* prefix) {
        return p31::withKeyPrefix(module.presets().loadDefault().value_or(std::string()), prefix);
    }
}

const clap_plugin_descriptor SuitePlugin::descriptor{
    .clap_version = CLAP_VERSION_INIT,
    .id = P31_PLUGIN_ID,
    .name = P31_PLUGIN_NAME,
    .vendor = P31_VENDOR,
    .url = P31_PROJECT_URL,
    .manual_url = "",
    .support_url = P31_PROJECT_URL "/issues",
    .version = P31_VERSION,
    .description = "Spectrum analyzer, vectorscope and spectrogram in one window",
    .features = kFeatures,
};

SuitePlugin::SuitePlugin(const clap_host* host)
    : PluginBase(&descriptor, host),
      _ring(2, kRingFrames),
      _spectrum(makeSpectrumModule(*this, _ring)),
      _scope(makeScopeModule(*this, _ring)),
      _spectrogram(makeSpectrogramModule(*this, _ring)) {}

void SuitePlugin::analyze(const float* const* channels, uint32_t channelCount, uint32_t frames) {
    _ring.write(channels, channelCount, frames);
}

void SuitePlugin::setLayout(const p31::suite::Layout& layout) {
    _layout = layout;
    settingsEdited();
}

p31::suite::Layout SuitePlugin::defaultLayout() const {
    p31::suite::Layout layout;
    layout.decode(PluginBase::defaultSettings());
    return layout;
}

bool SuitePlugin::hasDefaultLayout() const {
    return p31::PresetStore::forPlugin(name()).hasDefault();
}

bool SuitePlugin::saveDefaultLayout() const {
    return p31::PresetStore::forPlugin(name()).saveDefault(_layout.encode());
}

void SuitePlugin::clearDefaultLayout() const {
    p31::PresetStore::forPlugin(name()).clearDefault();
}

std::string SuitePlugin::encodeSettings() const {
    return _layout.encode() + p31::withKeyPrefix(_spectrum.encode(), kSpectrumKeys) +
           p31::withKeyPrefix(_scope.encode(), kScopeKeys) +
           p31::withKeyPrefix(_spectrogram.encode(), kSpectrogramKeys);
}

void SuitePlugin::decodeSettings(std::string_view text) {
    _layout = p31::suite::Layout();
    _layout.decode(text);
    _spectrum.decode(p31::withoutKeyPrefix(text, kSpectrumKeys));
    _scope.decode(p31::withoutKeyPrefix(text, kScopeKeys));
    _spectrogram.decode(p31::withoutKeyPrefix(text, kSpectrogramKeys));
}

// The arrangement saved for new instances of the suite, and every panel's
// settings as saved for new instances of its own plugin.
std::string SuitePlugin::defaultSettings() const {
    return PluginBase::defaultSettings() + moduleDefault(_spectrum, kSpectrumKeys) +
           moduleDefault(_scope, kScopeKeys) + moduleDefault(_spectrogram, kSpectrogramKeys);
}

std::unique_ptr<p31::Editor> SuitePlugin::createEditor(uint32_t width, uint32_t height) {
    return makeSuiteEditor(*this, width, height);
}

p31::EditorSizeLimits SuitePlugin::editorSizeLimits() const {
    p31::EditorSizeLimits limits;
    limits.defaultWidth = 1200;
    limits.defaultHeight = 700;
    limits.minWidth = 420;
    limits.minHeight = 240;
    return limits;
}

bool p31_suite_init(const char*) { return true; }
void p31_suite_deinit() {}
const void* p31_suite_get_factory(const char* factoryId) {
    return p31::SinglePluginFactory<SuitePlugin>::get(factoryId);
}
