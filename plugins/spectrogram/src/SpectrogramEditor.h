//
//  SpectrogramEditor.h
//  p31-spectrogram
//

#pragma once

#include "SpectrogramModule.h"

#include <p31/plugin/Editor.h>

#include <memory>

// The spectrogram's editor (SpectrogramEditor.mm). Kept behind a factory so
// the plugin code stays plain C++.
std::unique_ptr<p31::Editor> makeSpectrogramEditor(SpectrogramModule& module, uint32_t width, uint32_t height);
