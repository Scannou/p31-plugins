//
//  Module.h
//  p31-plugin-common
//
//  One analyzer as its editor sees it: the plugin it runs in (parameters,
//  source), the audio history it reads and its own settings. A plugin with
//  one analyzer has one module; P31 Suite has one per panel, all reading the
//  same audio.
//

#pragma once

#include "p31/plugin/PluginBase.h"

#include <p31/core/AudioRing.h>
#include <p31/core/PresetStore.h>
#include <p31/core/StateCodec.h>

#include <string>
#include <string_view>

namespace p31 {

template <class Settings>
class Module {
public:
    // Settings as the module keeps them: sanitized, with whatever the
    // analyzer derives from them.
    using Normalize = Settings (*)(const Settings&);

    // `name` is the name of the analyzer's own plugin ("P31 Spectrum"): its
    // presets are the same in every plugin that has the analyzer.
    Module(PluginBase& plugin, const AudioRing& ring, const char* name, Normalize normalize)
        : _plugin(plugin), _ring(ring), _name(name), _normalize(normalize), _settings(normalize(Settings())) {}

    PluginBase& plugin() const { return _plugin; }
    const AudioRing& ring() const { return _ring; }
    double sampleRate() const { return _plugin.sampleRate(); }
    const char* name() const { return _name; }
    PresetStore presets() const { return PresetStore::forPlugin(_name); }

    // Main thread.
    const Settings& settings() const { return _settings; }
    // Main thread: the editor changed the settings.
    void setSettings(const Settings& settings) {
        _settings = _normalize(settings);
        _plugin.settingsEdited();
    }

    // Main thread. Settings to and from text (StateCodec); decode starts
    // from the defaults.
    std::string encode() const { return encodeSettings(_settings); }
    void decode(std::string_view text) {
        Settings s;
        decodeSettings(text, s);
        _settings = _normalize(s);
    }

private:
    PluginBase& _plugin;
    const AudioRing& _ring;
    const char* _name;
    Normalize _normalize;
    Settings _settings;
};

} // namespace p31
