//
//  ScopeModule.h
//  p31-scope
//
//  The scope as a module: what P31 Scope and P31 Suite's scope
//  panel both run.
//

#pragma once

#include <p31/plugin/Module.h>
#include <p31/scope/ScopeSettings.h>

using ScopeModule = p31::Module<p31::scope::ScopeSettings>;

// Audio history the editor needs. It reads at most ~50 ms per display
// frame; the rest is headroom for a stalled UI.
inline constexpr size_t kScopeRingFrames = 65536;

inline ScopeModule makeScopeModule(p31::PluginBase& plugin, const p31::AudioRing& ring) {
    return ScopeModule(plugin, ring, "P31 Scope", [](const p31::scope::ScopeSettings& settings) {
        p31::scope::ScopeSettings s = settings.sanitized();
        // A stored index may come from the older theme list; the colors decide.
        s.theme = p31::scope::themeMatching(s);
        return s;
    });
}
