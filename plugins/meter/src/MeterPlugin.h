//
//  MeterPlugin.h
//  p31-meter
//
//  P31 Meter: the audio thread copies every block into a ring buffer; the
//  measurement reads it on the main thread, from a timer and from the
//  editor's display refresh, and the editor draws its readings.
//

#pragma once

#include "MeterMeasurement.h"
#include "MeterModule.h"

#include <p31/core/AudioRing.h>
#include <p31/plugin/PluginBase.h>

class MeterPlugin final : public p31::PluginBase {
public:
    static const clap_plugin_descriptor descriptor;

    explicit MeterPlugin(const clap_host* host);

private:
    void analyze(const float* const* channels, uint32_t channelCount, uint32_t frames) override;
    std::unique_ptr<p31::Editor> createEditor(uint32_t width, uint32_t height) override;
    bool hasEditor() const override { return true; }
    p31::EditorSizeLimits editorSizeLimits() const override;
    std::string encodeSettings() const override;
    void decodeSettings(std::string_view text) override;

    p31::AudioRing _ring;
    MeterModule _module;
    MeterMeasurement _measurement;
};

// Entry functions exported by Entry.cpp as clap_entry.
bool p31_meter_init(const char* pluginPath);
void p31_meter_deinit();
const void* p31_meter_get_factory(const char* factoryId);
