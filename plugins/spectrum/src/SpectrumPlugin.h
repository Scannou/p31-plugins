//
//  SpectrumPlugin.h
//  p31-spectrum
//

#pragma once

#include <p31/plugin/PluginBase.h>

class SpectrumPlugin final : public p31::PluginBase {
public:
    static const clap_plugin_descriptor descriptor;

    explicit SpectrumPlugin(const clap_host* host) : PluginBase(&descriptor, host) {}
};

// Entry functions exported by Entry.cpp as clap_entry.
bool p31_spectrum_init(const char* pluginPath);
void p31_spectrum_deinit();
const void* p31_spectrum_get_factory(const char* factoryId);
