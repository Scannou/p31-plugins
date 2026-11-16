//
//  MeterEditor.h
//  p31-meter
//

#pragma once

#include "MeterMeasurement.h"
#include "MeterModule.h"

#include <p31/plugin/Editor.h>

#include <memory>

// The meter's editor (MeterEditor.mm). Kept behind a factory so the plugin
// code stays plain C++.
std::unique_ptr<p31::Editor> makeMeterEditor(MeterModule& module, MeterMeasurement& measurement, uint32_t width,
                                             uint32_t height);
