#pragma once

#include "StateManager.h"
#include "PulseCounterService.h"
#include "EmergencyStopService.h"
#include "winch_controller.h"
#include "bow_propeller_controller.h"
#include "home_sensor.h"
#include "automatic_mode_controller.h"
#include "sensesp/signalk/signalk_output.h"
#include "sensesp/system/observablevalue.h"

using namespace sensesp;

/**
 * @file SignalKService.h
 * @brief Manages all SignalK listeners and outputs for the anchor chain counter
 * 
 * Encapsulates the creation and management of:
 * - All SignalK value listeners (remote commands)
 * - All SignalK outputs (status updates)
 * - Connection state monitoring
 */
class SignalKService {
public:
    /**
     * @brief Construct the SignalK service
     * @param state_manager Reference to central state manager
     * @param winch_controller Reference to winch controller
     * @param home_sensor Reference to home sensor
     * @param auto_mode_controller Pointer to automatic mode controller
     * @param emergency_stop_service Pointer to emergency stop service
     * @param pulse_counter_service Pointer to pulse counter service
     * @param bow_propeller_controller Pointer to bow propeller controller (can be nullptr)
     */
    SignalKService(StateManager& state_manager,
                   AnchorWinchController& winch_controller,
                   HomeSensor& home_sensor,
                   AutomaticModeController* auto_mode_controller,
                   EmergencyStopService* emergency_stop_service,
                   PulseCounterService* pulse_counter_service,
                   BowPropellerController* bow_propeller_controller = nullptr);

    /**
     * @brief Initialize all SignalK listeners and outputs
     * Must be called during setup() after all hardware has been initialized
     */
    void initialize();

    /**
     * @brief Update rode length output periodically
     * Called from main's periodic task (every 1000ms)
     */
    void updateRodeLength();

    /**
     * @brief Start connection monitoring
     * Must be called after SensESP app initialization
     */
    void startConnectionMonitoring();

    /**
     * @brief Get the emergency stop status value (for manual updates)
     */
    ObservableValue<bool>* getEmergencyStopStatus() { return emergency_stop_status_value_; }

private:
    // ========== Dependencies ==========
    StateManager& state_manager_;
    AnchorWinchController& winch_controller_;
    HomeSensor& home_sensor_;
    AutomaticModeController* auto_mode_controller_;
    EmergencyStopService* emergency_stop_service_;
    PulseCounterService* pulse_counter_service_;
    BowPropellerController* bow_propeller_controller_;

    // ========== SignalK Outputs ==========
    SKOutputFloat* rode_output_ = nullptr;
    SKOutputBool* reset_output_ = nullptr;
    ObservableValue<bool>* emergency_stop_status_value_ = nullptr;
    SKOutputRawJson* emergency_stop_notification_output_ = nullptr;
    SKOutputString* manual_control_output_ = nullptr;
    SKOutputBool* auto_mode_output_ = nullptr;
    SKOutputFloat* target_output_ = nullptr;
    SKOutputBool* home_command_output_ = nullptr;
    SKOutputString* bow_propeller_command_output_ = nullptr;
    SKOutputString* bow_propeller_status_output_ = nullptr;

    // ========== Connection Monitoring ==========
    unsigned long connection_stable_time_ = 0;

    // ========== Helper Methods ==========
    void setupRodeLengthOutput();
    void setupEmergencyStopBindings();
    void updateEmergencyStopOutputs(bool active);
    void setupManualControlBindings();
    void setupAutoModeBindings();
    void setupHomeCommandBindings();
    void setupBowPropellerBindings();
};
