//
//  Entry.cpp
//  p31-scope
//
//  Compiled into every format target (CLAP, VST3, AUv2, standalone) by
//  clap-wrapper; the plugin itself lives in the p31-scope-impl static library.
//

#include <clap/clap.h>

// Defined in ScopePlugin.cpp. Declared here rather than included so this file
// stays plain C++17 like the wrapper code it is compiled with.
bool p31_scope_init(const char* pluginPath);
void p31_scope_deinit();
const void* p31_scope_get_factory(const char* factoryId);

extern "C" {
CLAP_EXPORT const clap_plugin_entry clap_entry{
    CLAP_VERSION_INIT,
    p31_scope_init,
    p31_scope_deinit,
    p31_scope_get_factory,
};
}
