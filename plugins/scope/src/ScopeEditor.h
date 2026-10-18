//
//  ScopeEditor.h
//  p31-scope
//

#pragma once

#include <p31/plugin/Editor.h>

#include <memory>

class ScopePlugin;

// The scope's editor (ScopeEditor.mm). Kept behind a factory so the plugin
// code stays plain C++.
std::unique_ptr<p31::Editor> makeScopeEditor(ScopePlugin& plugin, uint32_t width, uint32_t height);
