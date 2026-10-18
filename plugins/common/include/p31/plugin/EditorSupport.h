//
//  EditorSupport.h
//  p31-plugin-common
//
//  Pieces both editors share: settings rows for the parameters and the
//  appearance, the freeze toggle, and the Presets menu's source. Header-only
//  Objective-C++, included by the editors' .mm files (the AppKit types it
//  uses are compiled into each plugin with its own class names).
//

#pragma once

#include "p31/plugin/PluginBase.h"

#include <p31/core/Appearance.h>
#include <p31/core/PresetStore.h>
#include <p31/core/StateCodec.h>
#include <p31/render/Form.h>
#include <p31/render/Menus.h>

#include <cmath>
#include <cstdio>
#include <string>

namespace p31::editor {

inline void toggleFreeze(PluginBase& plugin) {
    plugin.beginParamEdit(PluginBase::kParamFreeze);
    plugin.setParamFromEditor(PluginBase::kParamFreeze, plugin.frozen() ? 0.0 : 1.0);
    plugin.endParamEdit(PluginBase::kParamFreeze);
}

inline ui::Row inputGainRow(PluginBase& plugin) {
    PluginBase* p = &plugin;
    ui::Row r = ui::slider(
        "Input gain", PluginBase::kGainMinDb, PluginBase::kGainMaxDb,
        [p] { return p->paramValue(PluginBase::kParamGain); },
        [p](double v) { p->setParamFromEditor(PluginBase::kParamGain, v); },
        [](double v) {
            char s[32];
            std::snprintf(s, sizeof(s), "%+.1f dB", v);
            return std::string(s);
        },
        "Boosts or cuts the signal before it is analysed. The audio passing through is never changed.");
    r.step = 0.5;
    r.begin = [p] { p->beginParamEdit(PluginBase::kParamGain); };
    r.end = [p] { p->endParamEdit(PluginBase::kParamGain); };
    return r;
}

inline ui::Row freezeRow(PluginBase& plugin) {
    PluginBase* p = &plugin;
    ui::Row r = ui::checkbox(
        "Freeze display", [p] { return p->frozen(); },
        [p](bool v) { p->setParamFromEditor(PluginBase::kParamFreeze, v ? 1.0 : 0.0); },
        "Hold the picture still. Also available as an automatable parameter.");
    r.begin = [p] { p->beginParamEdit(PluginBase::kParamFreeze); };
    r.end = [p] { p->endParamEdit(PluginBase::kParamFreeze); };
    return r;
}

inline ui::Row appearanceRow(int& appearance) {
    int* a = &appearance;
    return ui::popup(
        "Appearance", {{"Follow system", AppearanceAuto}, {"Light", AppearanceLight}, {"Dark", AppearanceDark}},
        [a] { return *a; }, [a](int v) { *a = v; }, "Light or dark colors for this instance");
}

// Theme popup items from a theme table with a `name` per entry.
template <class Themes>
std::vector<std::pair<std::string, int>> themeChoices(const Themes& themes) {
    std::vector<std::pair<std::string, int>> out;
    int i = 0;
    for (const auto& t : themes) out.emplace_back(t.name, i++);
    return out;
}

// The same, listed in `order` (theme indices).
template <class Themes, class Order>
std::vector<std::pair<std::string, int>> themeChoices(const Themes& themes, const Order& order) {
    std::vector<std::pair<std::string, int>> out;
    for (int i : order) out.emplace_back(themes[i].name, i);
    return out;
}

// Gain and freeze as last seen, to notice host automation once per frame.
struct ParamWatch {
    double gain = 0.0;
    bool frozen = false;

    // True when either changed since the last call.
    bool changed(const PluginBase& plugin) {
        const double g = plugin.paramValue(PluginBase::kParamGain);
        const bool f = plugin.frozen();
        const bool c = g != gain || f != frozen;
        gain = g;
        frozen = f;
        return c;
    }
};

// Presets for a plugin whose settings type `S` has visitFields. Applying a
// preset keeps the instance's appearance: that is a viewing preference, not
// part of a sound or look someone shares.
template <class S, class Apply>
ui::PresetSource presetSource(PluginBase& plugin, std::vector<std::pair<std::string, S>> factory, Apply apply) {
    ui::PresetSource src{{}, PresetStore::forPlugin(plugin.name()), {}, {}};
    for (const auto& [name, settings] : factory) src.factory.emplace_back(name, encodeSettings(settings));
    PluginBase* p = &plugin;
    src.currentText = [p] { return p->settingsText(); };
    src.apply = [apply](const std::string& text) {
        S s;
        decodeSettings(text, s);
        apply(s);
    };
    return src;
}

} // namespace p31::editor
