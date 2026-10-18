//
//  PluginBase.h
//  p31-plugin-common
//
//  CLAP plugin shell shared by P31 Spectrum and P31 Scope: one main audio
//  port pair (mono or stereo), audio passed through untouched with zero
//  latency, a versioned state blob and a resizable editor. Subclasses add
//  analysis and the editor's contents.
//

#pragma once

#include "p31/plugin/Editor.h"

#include <clap/clap.h>

#include <atomic>
#include <cstdint>
#include <cstring>
#include <memory>

namespace p31 {

class PluginBase {
public:
    PluginBase(const clap_plugin_descriptor* desc, const clap_host* host);
    virtual ~PluginBase();
    PluginBase(const PluginBase&) = delete;
    PluginBase& operator=(const PluginBase&) = delete;

    const clap_plugin* clapPlugin() const { return &_plugin; }

protected:
    // Audio thread. Called once per block with the input channels, before
    // the audio is passed through. Must not allocate, lock or log.
    virtual void analyze(const float* const* channels, uint32_t channelCount, uint32_t frames) {
        (void)channels; (void)channelCount; (void)frames;
    }
    // Main thread, while deactivated.
    virtual bool onActivate(double sampleRate, uint32_t minFrames, uint32_t maxFrames) {
        (void)sampleRate; (void)minFrames; (void)maxFrames;
        return true;
    }
    virtual void onDeactivate() {}
    // Extensions beyond the shared ones. Return nullptr if unknown.
    virtual const void* extension(const char* id) { (void)id; return nullptr; }

    // Main thread. The editor shown while the host opens the plugin window,
    // created at `width` x `height` points; nullptr means no GUI at all.
    virtual std::unique_ptr<Editor> createEditor(uint32_t width, uint32_t height) {
        (void)width; (void)height;
        return nullptr;
    }
    virtual bool hasEditor() const { return false; }
    virtual EditorSizeLimits editorSizeLimits() const { return {}; }

    const clap_host* host() const { return _host; }
    // Sample rate of the current (or last) activation; 0 before the first.
    // Safe from any thread.
    double sampleRate() const { return _sampleRate.load(std::memory_order_relaxed); }

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

    // gui
    bool guiCreate(const char* api, bool isFloating);
    void guiDestroy();
    bool guiSetSize(uint32_t width, uint32_t height);
    void guiClampSize(uint32_t& width, uint32_t& height) const;

    static PluginBase* self(const clap_plugin* plugin);

    clap_plugin _plugin{};
    const clap_host* _host;
    bool _active = false;
    uint32_t _inChannels = 2;
    uint32_t _outChannels = 2;
    std::atomic<double> _sampleRate{0.0};
    // clap-wrapper's standalone app plays the output on the speakers while
    // recording the input: passing audio through would feed back.
    bool _muteOutput = false;

    std::unique_ptr<Editor> _editor;
    uint32_t _editorWidth = 0;    // 0 until the first editor
    uint32_t _editorHeight = 0;

    static const clap_plugin_audio_ports s_audioPorts;
    static const clap_plugin_configurable_audio_ports s_configurablePorts;
    static const clap_plugin_latency s_latency;
    static const clap_plugin_state s_state;
    static const clap_plugin_gui s_gui;
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
