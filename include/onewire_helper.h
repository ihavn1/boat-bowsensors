#pragma once

#include <cstdint>

namespace sensesp {
namespace onewire {
class DallasTemperatureSensors;
}  // namespace onewire
}  // namespace sensesp

// Add a one-wire temperature sensor + Linear calibration + SK output
// See implementation in src/onewire_helper.cpp
void add_onewire_temp(sensesp::onewire::DallasTemperatureSensors* dts,
                      unsigned int read_delay, const char* base_name,
                      const char* signal_k_path, const char* human_label,
                      int sensor_sort, int linear_sort, int sk_sort);
