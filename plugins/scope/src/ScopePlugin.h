//
//  ScopePlugin.h
//  p31-scope
//

#pragma once

#include <p31/plugin/PluginBase.h>

class ScopePlugin final : public p31::PluginBase {
public:
    static const clap_plugin_descriptor descriptor;

    explicit ScopePlugin(const clap_host* host) : PluginBase(&descriptor, host) {}
};

// Entry functions exported by Entry.cpp as clap_entry.
bool p31_scope_init(const char* pluginPath);
void p31_scope_deinit();
const void* p31_scope_get_factory(const char* factoryId);
