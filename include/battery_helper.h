// Helper declarations for battery INA226 setup
#pragma once

#include "INA226.h"

void setupBatteryINA(INA226 &ina, unsigned int read_interval, float shunt_resistance,
                     float current_LSB_mA, const char *voltage_path,
                     const char *current_path, const char* power_path, const char* ah_path,
                     const char* soc_path, float battery_capacity_ah, float initial_ah, const char* chip_name);