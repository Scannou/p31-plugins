//
//  SpectrumEditor.h
//  p31-spectrum
//

#pragma once

#include <p31/plugin/Editor.h>

#include <memory>

class SpectrumPlugin;

// The spectrum's editor (SpectrumEditor.mm). Kept behind a factory so the
// plugin code stays plain C++.
std::unique_ptr<p31::Editor> makeSpectrumEditor(const SpectrumPlugin& plugin, uint32_t width, uint32_t height);
