//
//  MeterPlugin.cpp
//  p31-meter
//

#include "MeterPlugin.h"

#include "MeterEditor.h"

namespace {
    const char* const kFeatures[] = {
        CLAP_PLUGIN_FEATURE_AUDIO_EFFECT,
        CLAP_PLUGIN_FEATURE_ANALYZER,
        CLAP_PLUGIN_FEATURE_MONO,
        CLAP_PLUGIN_FEATURE_STEREO,
        nullptr,
    };
}

const clap_plugin_descriptor MeterPlugin::descriptor{
    .clap_version = CLAP_VERSION_INIT,
    .id = P31_PLUGIN_ID,
    .name = P31_PLUGIN_NAME,
    .vendor = P31_VENDOR,
    .url = P31_PROJECT_URL,
    .manual_url = "",
    .support_url = P31_PROJECT_URL "/issues",
    .version = P31_VERSION,
    .description = "Peak, RMS and loudness meter with an analog VU look",
    .features = kFeatures,
};

MeterPlugin::MeterPlugin(const clap_host* host)
    : PluginBase(&descriptor, host),
      _ring(2, kMeterRingFrames),
      _module(makeMeterModule(*this, _ring)),
      _measurement(_module) {}

void MeterPlugin::analyze(const float* const* channels, uint32_t channelCount, uint32_t frames) {
    _ring.write(channels, channelCount, frames);
}

std::string MeterPlugin::encodeSettings() const {
    return _module.encode();
}

void MeterPlugin::decodeSettings(std::string_view text) {
    _module.decode(text);
    _measurement.settingsChanged();
}

std::unique_ptr<p31::Editor> MeterPlugin::createEditor(uint32_t width, uint32_t height) {
    return makeMeterEditor(_module, _measurement, width, height);
}

p31::EditorSizeLimits MeterPlugin::editorSizeLimits() const {
    p31::EditorSizeLimits limits;
    limits.defaultWidth = 300;
    limits.defaultHeight = 480;
    limits.minWidth = 140;
    limits.minHeight = 120;
    return limits;
}

bool p31_meter_init(const char*) { return true; }
void p31_meter_deinit() {}
const void* p31_meter_get_factory(const char* factoryId) {
    return p31::SinglePluginFactory<MeterPlugin>::get(factoryId);
}
