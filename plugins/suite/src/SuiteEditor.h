//
//  SuiteEditor.h
//  p31-suite
//

#pragma once

#include <p31/plugin/Editor.h>

#include <memory>

class SuitePlugin;

// The suite's editor (SuiteEditor.mm). Kept behind a factory so the plugin
// code stays plain C++.
std::unique_ptr<p31::Editor> makeSuiteEditor(SuitePlugin& plugin, uint32_t width, uint32_t height);
