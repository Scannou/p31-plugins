//
//  PluginBase.h
//  p31-plugin-common
//
//  CLAP plugin shell shared by P31 Spectrum and P31 Scope: one main audio
//  port pair (mono or stereo), audio passed through untouched with zero
//  latency, and a versioned state blob. Subclasses add analysis and UI.
//

#pragma once

#include <clap/clap.h>

#include <cstdint>
#include <cstring>

namespace p31 {

class PluginBase {
public:
    PluginBase(const clap_plugin_descriptor* desc, const clap_host* host);
    virtual ~PluginBase() = default;
    PluginBase(const PluginBase&) = delete;
    PluginBase& operator=(const PluginBase&) = delete;

    const clap_plugin* clapPlugin() const { return &_plugin; }

protected:
    // Audio thread. Called once per block, after the audio has been passed
    // through, with the input channels. Must not allocate, lock or log.
    virtual void analyze(const float* const* channels, uint32_t channelCount, uint32_t frames) {
        (void)channels; (void)channelCount; (void)frames;
    }
    // Main thread, while deactivated.
    virtual bool onActivate(double sampleRate, uint32_t minFrames, uint32_t maxFrames) {
        (void)sampleRate; (void)minFrames; (void)maxFrames;
        return true;
    }
    virtual void onDeactivate() {}
    // Extensions beyond the shared ones (e.g. gui). Return nullptr if unknown.
    virtual const void* extension(const char* id) { (void)id; return nullptr; }

    const clap_host* host() const { return _host; }

private:
    // clap_plugin
    bool init();
    void destroy();
    bool activate(double sampleRate, uint32_t minFrames, uint32_t maxFrames);
    void deactivate();
    clap_process_status process(const clap_process* process);
    const void* getExtension(const char* id);

    // audio-ports / configurable-audio-ports
    uint32_t portCount(bool isInput) const;
    bool portInfo(uint32_t index, bool isInput, clap_audio_port_info* info) const;
    bool canApplyConfiguration(const clap_audio_port_configuration_request* requests, uint32_t count,
                               uint32_t& inChannels, uint32_t& outChannels) const;

    // state
    bool saveState(const clap_ostream* stream);
    bool loadState(const clap_istream* stream);

    static PluginBase* self(const clap_plugin* plugin);

    clap_plugin _plugin{};
    const clap_host* _host;
    bool _active = false;
    uint32_t _inChannels = 2;
    uint32_t _outChannels = 2;

    static const clap_plugin_audio_ports s_audioPorts;
    static const clap_plugin_configurable_audio_ports s_configurablePorts;
    static const clap_plugin_latency s_latency;
    static const clap_plugin_state s_state;
};

// Factory for a bundle that contains exactly one plugin type.
template <typename Plugin>
struct SinglePluginFactory {
    static const void* get(const char* factoryId) {
        static const clap_plugin_factory factory{
            [](const clap_plugin_factory*) -> uint32_t { return 1; },
            [](const clap_plugin_factory*, uint32_t index) -> const clap_plugin_descriptor* {
                return index == 0 ? &Plugin::descriptor : nullptr;
            },
            [](const clap_plugin_factory*, const clap_host* host, const char* pluginId) -> const clap_plugin* {
                if (!clap_version_is_compatible(host->clap_version)) return nullptr;
                if (std::strcmp(pluginId, Plugin::descriptor.id) != 0) return nullptr;
                return (new Plugin(host))->clapPlugin();
            },
        };
        return std::strcmp(factoryId, CLAP_PLUGIN_FACTORY_ID) == 0 ? &factory : nullptr;
    }
};

} // namespace p31
