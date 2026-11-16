//
//  SpectrumPlugin.h
//  p31-spectrum
//
//  P31 Spectrum: the audio thread copies every block into a ring buffer; the
//  editor reads it on the display refresh and runs the FFT pipeline there,
//  as the fb2k component did with the visualisation stream.
//

#pragma once

#include "SpectrumModule.h"

#include <p31/core/AudioRing.h>
#include <p31/plugin/PluginBase.h>

class SpectrumPlugin final : public p31::PluginBase {
public:
    static const clap_plugin_descriptor descriptor;

    explicit SpectrumPlugin(const clap_host* host);

private:
    void analyze(const float* const* channels, uint32_t channelCount, uint32_t frames) override;
    std::unique_ptr<p31::Editor> createEditor(uint32_t width, uint32_t height) override;
    bool hasEditor() const override { return true; }
    p31::EditorSizeLimits editorSizeLimits() const override;
    std::string encodeSettings() const override;
    void decodeSettings(std::string_view text) override;

    p31::AudioRing _ring;
    SpectrumModule _module;
};

// Entry functions exported by Entry.cpp as clap_entry.
bool p31_spectrum_init(const char* pluginPath);
void p31_spectrum_deinit();
const void* p31_spectrum_get_factory(const char* factoryId);
