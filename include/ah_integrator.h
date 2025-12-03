#pragma once

#include "sensesp/transforms/transform.h"
#include "sensesp_base_app.h"

namespace sensesp {

// Interval configuration (can be overridden at compile time)
// Integration interval in milliseconds (default 1 Hz)
#ifndef AH_INTEGRATION_INTERVAL_MS
#define AH_INTEGRATION_INTERVAL_MS 1000
#endif

// Persist-check interval in milliseconds (default 0.2 Hz -> every 5 seconds)
#ifndef AH_PERSIST_CHECK_INTERVAL_MS
#define AH_PERSIST_CHECK_INTERVAL_MS 5000
#endif


// Integrates current (A) over time to produce Amp-hours (Ah).
// Runs internal integration at a configurable interval (default 1 Hz via AH_INTEGRATION_INTERVAL_MS).
// Exposes Ah to consumers at their own polling rate (e.g., Signal K output).
class AmpHourIntegrator : public FloatTransform {
 public:
  // config_path is unused for now but kept for consistency with other transforms
  // battery_capacity_ah: capacity in Ah, used to clamp Ah between 0 and capacity
  explicit AmpHourIntegrator(const String& config_path = "", float initial_ah = 0.0f, 
                             float battery_capacity_ah = 0.0f);

  void set(const float& new_value) override;

  double get_ah() const { return ah_output_; }
  
  // Set the current Ah value (e.g., from Signal K reset command)
  // Clamped between 0 and battery_capacity_ah
  void set_ah(double ah);
  
  // Get/set efficiency for charging (current > 0), range 0-100%
  float get_charge_efficiency() const { return charge_efficiency_; }
  void set_charge_efficiency(float pct);
  
  // Get/set efficiency for discharging (current < 0), range 0-100%
  float get_discharge_efficiency() const { return discharge_efficiency_; }
  void set_discharge_efficiency(float pct);
  
  // Get/set marked (nameplate) capacity in Ah - the rated capacity
  float get_marked_capacity_ah() const { return marked_capacity_ah_; }
  void set_marked_capacity_ah(float capacity_ah);

  // Get/set current capacity in Ah - actual usable capacity (may degrade)
  float get_current_capacity_ah() const { return battery_capacity_ah_; }
  void set_current_capacity_ah(float capacity_ah);

 private:
  void integrate();  // Called by internal timer (interval set by AH_INTEGRATION_INTERVAL_MS)
  unsigned long last_update_ms_ = 0;
  double current_a_ = 0.0;  // Most recent current reading (A) - double for precision
  double ah_output_ = 0.0;  // Accumulated Ah value - double for precision
  float charge_efficiency_ = 100.0f;    // Efficiency % when charging (current > 0)
  float discharge_efficiency_ = 100.0f; // Efficiency % when discharging (current < 0)
  float marked_capacity_ah_ = 0.0f;     // Marked/nameplate capacity in Ah
  float battery_capacity_ah_ = 0.0f;    // Current capacity in Ah (used for clamping)
  // Config path used to create unique NVS keys for persistence
  String config_path_;
  // Ah persistence helpers
  bool ah_dirty_ = false;                    // Whether Ah has changed since last persisted
  unsigned long last_ah_persist_ms_ = 0;     // Timestamp of last Ah persist
  double last_persisted_ah_ = 0.0;           // Last persisted Ah value - double for precision
  unsigned long ah_persist_interval_ms_ = 600000; // Default persist interval (10 minutes)
  double ah_persist_delta_ = 0.5;            // Minimum Ah delta to trigger persist - double for precision
  void maybe_persist_ah();                   // Called periodically to flush Ah to NVS
};

}  // namespace sensesp
