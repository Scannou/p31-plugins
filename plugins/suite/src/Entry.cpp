//
//  Entry.cpp
//  p31-suite
//
//  Compiled into every format target (CLAP, VST3, AUv2, standalone) by
//  clap-wrapper; the plugin itself lives in the p31-suite-impl static library.
//

#include <clap/clap.h>

// Defined in SuitePlugin.cpp. Declared here rather than included so this file
// stays plain C++17 like the wrapper code it is compiled with.
bool p31_suite_init(const char* pluginPath);
void p31_suite_deinit();
const void* p31_suite_get_factory(const char* factoryId);

extern "C" {
CLAP_EXPORT const clap_plugin_entry clap_entry{
    CLAP_VERSION_INIT,
    p31_suite_init,
    p31_suite_deinit,
    p31_suite_get_factory,
};
}
