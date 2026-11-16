//
//  SuitePlugin.h
//  p31-suite
//
//  P31 Suite: the spectrum, the scope and the spectrogram in one window.
//  The audio thread copies every block into one ring buffer; each panel's
//  editor reads it on the display refresh, as in the plugins of their own.
//  The state holds every panel's settings, shown or not, and how the panels
//  are arranged.
//

#pragma once

#include "ScopeModule.h"
#include "SpectrogramModule.h"
#include "SpectrumModule.h"

#include <p31/core/AudioRing.h>
#include <p31/plugin/PluginBase.h>
#include <p31/suite/SuiteLayout.h>

class SuitePlugin final : public p31::PluginBase {
public:
    static const clap_plugin_descriptor descriptor;

    explicit SuitePlugin(const clap_host* host);

    SpectrumModule& spectrum() { return _spectrum; }
    ScopeModule& scope() { return _scope; }
    SpectrogramModule& spectrogram() { return _spectrogram; }

    // Main thread. Which panels show, and where.
    const p31::suite::Layout& layout() const { return _layout; }
    // Main thread: the editor changed the arrangement.
    void setLayout(const p31::suite::Layout& layout);

    // Main thread. The arrangement new instances start with: the one saved
    // with saveDefaultLayout(), else the built-in one.
    p31::suite::Layout defaultLayout() const;
    bool hasDefaultLayout() const;
    bool saveDefaultLayout() const;
    void clearDefaultLayout() const;

private:
    void analyze(const float* const* channels, uint32_t channelCount, uint32_t frames) override;
    std::unique_ptr<p31::Editor> createEditor(uint32_t width, uint32_t height) override;
    bool hasEditor() const override { return true; }
    p31::EditorSizeLimits editorSizeLimits() const override;
    std::string encodeSettings() const override;
    void decodeSettings(std::string_view text) override;
    std::string defaultSettings() const override;

    p31::AudioRing _ring;
    SpectrumModule _spectrum;
    ScopeModule _scope;
    SpectrogramModule _spectrogram;
    p31::suite::Layout _layout;
};

// Entry functions exported by Entry.cpp as clap_entry.
bool p31_suite_init(const char* pluginPath);
void p31_suite_deinit();
const void* p31_suite_get_factory(const char* factoryId);
