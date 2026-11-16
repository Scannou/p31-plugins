//
//  ScopeEditor.h
//  p31-scope
//

#pragma once

#include "ScopeModule.h"

#include <p31/plugin/Editor.h>

#include <memory>

// The scope's editor (ScopeEditor.mm). Kept behind a factory so the plugin
// code stays plain C++.
std::unique_ptr<p31::Editor> makeScopeEditor(ScopeModule& module, uint32_t width, uint32_t height);
