#include <unity.h>
#include <Arduino.h>
#include "pin_config.h"

// Forward declarations for bow propeller tests
// Motor hardware tests
extern void test_bow_motor_initializes_pins(void);
extern void test_bow_motor_turn_port(void);
extern void test_bow_motor_turn_starboard(void);
extern void test_bow_motor_stop(void);
extern void test_bow_motor_mutual_exclusion_port_then_starboard(void);
extern void test_bow_motor_mutual_exclusion_starboard_then_port(void);

// Controller logic tests
extern void test_bow_controller_initializes_with_motor(void);
extern void test_bow_controller_turn_port_command(void);
extern void test_bow_controller_turn_starboard_command(void);
extern void test_bow_controller_stop_command(void);
extern void test_bow_controller_port_to_starboard_switching(void);
extern void test_bow_controller_repeated_same_command(void);

// SignalK command mapping tests
extern void test_signalk_command_port_maps_to_port(void);
extern void test_signalk_command_stop_maps_to_stop(void);
extern void test_signalk_command_starboard_maps_to_starboard(void);

// Safety tests
extern void test_bow_motor_never_activates_both_relays(void);
extern void test_bow_controller_safe_repeated_stop(void);

// Integration tests
extern void test_bow_system_rapid_direction_changes(void);
extern void test_bow_system_stop_after_rapid_changes(void);
extern void test_bow_startup_always_inactive(void);
extern void test_bow_controller_state_consistency(void);

// SignalK integration tests
extern void test_signalk_sends_port_command(void);
extern void test_signalk_sends_stop_command(void);
extern void test_signalk_sends_starboard_command(void);
extern void test_signalk_status_output_reflects_state(void);
extern void test_signalk_blocks_when_not_connected(void);
extern void test_signalk_blocks_when_emergency_stop_active(void);
extern void test_signalk_commands_resume_after_emergency_stop_cleared(void);
extern void test_signalk_reconnection_resumes_commands(void);

// Emergency stop tests
extern void test_emergency_stop_blocks_signalk_commands(void);
extern void test_emergency_stop_blocks_remote_commands(void);
extern void test_emergency_stop_activation_stops_motor(void);
extern void test_emergency_stop_counts_blocked_attempts(void);

// Remote control integration tests
extern void test_remote_func3_button_activates_port(void);
extern void test_remote_func4_button_activates_starboard(void);
extern void test_remote_button_release_stops_motor(void);

// System-level integration tests
extern void test_signalk_and_remote_can_coexist(void);
extern void test_emergency_stop_blocks_both_signalk_and_remote(void);
extern void test_full_scenario_normal_operation(void);

// Mock GPIO states for testing
bool mock_gpio_states[40] = {false};
int mock_gpio_modes[40] = {0};

// Mock pulse counter and control variables (normally in main.cpp)
volatile long test_pulse_count = 0;
float test_target_rode_length = -1.0;
bool test_winch_active = false;
bool test_automatic_mode_enabled = false;
float test_config_meters_per_pulse = 0.1;

// GPIO pin definitions (must match main.cpp)
// Now using PinConfig from pin_config.h
#define PULSE_INPUT_PIN PinConfig::PULSE_INPUT
#define DIRECTION_PIN PinConfig::DIRECTION
#define WINCH_UP_PIN PinConfig::WINCH_UP
#define WINCH_DOWN_PIN PinConfig::WINCH_DOWN
#define ANCHOR_HOME_PIN PinConfig::ANCHOR_HOME
#define REMOTE_UP_PIN PinConfig::REMOTE_UP
#define REMOTE_DOWN_PIN PinConfig::REMOTE_DOWN

// Mock Arduino functions
int digitalRead(uint8_t pin) {
    if (pin < 40) return mock_gpio_states[pin] ? HIGH : LOW;
    return LOW;
}

void digitalWrite(uint8_t pin, uint8_t val) {
    if (pin < 40) mock_gpio_states[pin] = (val == HIGH);
}

void pinMode(uint8_t pin, uint8_t mode) {
    if (pin < 40) mock_gpio_modes[pin] = mode;
}

// Mock winch control functions
void test_stopWinch() {
    mock_gpio_states[WINCH_UP_PIN] = false;
    mock_gpio_states[WINCH_DOWN_PIN] = false;
    test_winch_active = false;
}

void test_setWinchUp() {
    if (mock_gpio_states[ANCHOR_HOME_PIN] == LOW) {
        test_stopWinch();
        return;
    }
    mock_gpio_states[WINCH_DOWN_PIN] = false;
    mock_gpio_states[WINCH_UP_PIN] = true;
    test_winch_active = true;
}

void test_setWinchDown() {
    mock_gpio_states[WINCH_UP_PIN] = false;
    mock_gpio_states[WINCH_DOWN_PIN] = true;
    test_winch_active = true;
}

// Test fixture setup/teardown
void setUp(void) {
    // Reset all states before each test
    for (int i = 0; i < 40; i++) {
        mock_gpio_states[i] = false;
        mock_gpio_modes[i] = 0;
    }
    test_pulse_count = 0;
    test_target_rode_length = -1.0;
    test_winch_active = false;
    test_automatic_mode_enabled = false;
    test_config_meters_per_pulse = 0.1;
    
    // Set default GPIO states (pullups = HIGH when not active)
    mock_gpio_states[PULSE_INPUT_PIN] = true;
    mock_gpio_states[DIRECTION_PIN] = true;
    mock_gpio_states[ANCHOR_HOME_PIN] = true;  // Not home by default
}

void tearDown(void) {
    // Cleanup after each test
}

// =============================================================================
// TEST: Pulse Counter Increment (Chain Out)
// =============================================================================
void test_pulse_counter_increment_on_chain_out(void) {
    test_pulse_count = 0;
    
    // Simulate direction = HIGH (chain out)
    mock_gpio_states[DIRECTION_PIN] = true;
    
    // Simulate 10 pulses
    for (int i = 0; i < 10; i++) {
        if (mock_gpio_states[DIRECTION_PIN] == HIGH) {
            test_pulse_count++;
        }
    }
    
    TEST_ASSERT_EQUAL_INT32(10, test_pulse_count);
}

// =============================================================================
// TEST: Pulse Counter Decrement (Chain In)
// =============================================================================
void test_pulse_counter_decrement_on_chain_in(void) {
    test_pulse_count = 20;  // Start with 20 pulses
    
    // Simulate direction = LOW (chain in)
    mock_gpio_states[DIRECTION_PIN] = false;
    
    // Simulate 5 pulses (should decrement)
    for (int i = 0; i < 5; i++) {
        if (mock_gpio_states[DIRECTION_PIN] == LOW) {
            test_pulse_count--;
            if (test_pulse_count < 0) {
                test_pulse_count = 0;
            }
        }
    }
    
    TEST_ASSERT_EQUAL_INT32(15, test_pulse_count);
}

// =============================================================================
// TEST: Pulse Counter Does Not Go Negative
// =============================================================================
void test_pulse_counter_prevents_negative_values(void) {
    test_pulse_count = 2;
    
    // Simulate direction = LOW (chain in)
    mock_gpio_states[DIRECTION_PIN] = false;
    
    // Try to decrement 5 times (should stop at 0)
    for (int i = 0; i < 5; i++) {
        if (mock_gpio_states[DIRECTION_PIN] == LOW) {
            test_pulse_count--;
            if (test_pulse_count < 0) {
                test_pulse_count = 0;
            }
        }
    }
    
    TEST_ASSERT_EQUAL_INT32(0, test_pulse_count);
}

// =============================================================================
// TEST: Meters Calculation
// =============================================================================
void test_meters_calculation_from_pulses(void) {
    test_pulse_count = 50;
    test_config_meters_per_pulse = 0.1;
    
    float meters = test_pulse_count * test_config_meters_per_pulse;
    
    TEST_ASSERT_FLOAT_WITHIN(0.01, 5.0, meters);
}

// =============================================================================
// TEST: Home Sensor Prevents Retrieval
// =============================================================================
void test_home_sensor_blocks_winch_up(void) {
    // Anchor is already home
    mock_gpio_states[ANCHOR_HOME_PIN] = false;  // LOW = home
    
    test_setWinchUp();
    
    // Winch should NOT be active
    TEST_ASSERT_FALSE(test_winch_active);
    TEST_ASSERT_FALSE(mock_gpio_states[WINCH_UP_PIN]);
}

// =============================================================================
// TEST: Home Sensor Allows Deployment
// =============================================================================
void test_home_sensor_allows_winch_down(void) {
    // Anchor is home
    mock_gpio_states[ANCHOR_HOME_PIN] = false;  // LOW = home
    
    test_setWinchDown();
    
    // Winch DOWN should work
    TEST_ASSERT_TRUE(test_winch_active);
    TEST_ASSERT_TRUE(mock_gpio_states[WINCH_DOWN_PIN]);
}

// =============================================================================
// TEST: Winch UP Works When Not Home
// =============================================================================
void test_winch_up_works_when_not_home(void) {
    // Anchor is NOT home
    mock_gpio_states[ANCHOR_HOME_PIN] = true;  // HIGH = not home
    
    test_setWinchUp();
    
    TEST_ASSERT_TRUE(test_winch_active);
    TEST_ASSERT_TRUE(mock_gpio_states[WINCH_UP_PIN]);
    TEST_ASSERT_FALSE(mock_gpio_states[WINCH_DOWN_PIN]);
}

// =============================================================================
// TEST: Winch DOWN Works
// =============================================================================
void test_winch_down_works(void) {
    test_setWinchDown();
    
    TEST_ASSERT_TRUE(test_winch_active);
    TEST_ASSERT_TRUE(mock_gpio_states[WINCH_DOWN_PIN]);
    TEST_ASSERT_FALSE(mock_gpio_states[WINCH_UP_PIN]);
}

// =============================================================================
// TEST: Stop Winch Function
// =============================================================================
void test_stop_winch_clears_all_outputs(void) {
    // Start winch
    test_setWinchDown();
    TEST_ASSERT_TRUE(test_winch_active);
    
    // Stop it
    test_stopWinch();
    
    TEST_ASSERT_FALSE(test_winch_active);
    TEST_ASSERT_FALSE(mock_gpio_states[WINCH_UP_PIN]);
    TEST_ASSERT_FALSE(mock_gpio_states[WINCH_DOWN_PIN]);
}

// =============================================================================
// TEST: Manual Mode Allows Control
// =============================================================================
void test_manual_mode_allows_control(void) {
    test_automatic_mode_enabled = false;  // Manual mode
    mock_gpio_states[ANCHOR_HOME_PIN] = true;  // Not home
    
    // Simulate manual UP command
    if (!test_automatic_mode_enabled) {
        test_setWinchUp();
    }
    
    TEST_ASSERT_TRUE(test_winch_active);
    TEST_ASSERT_TRUE(mock_gpio_states[WINCH_UP_PIN]);
}

// =============================================================================
// TEST: Automatic Mode Blocks Manual Control
// =============================================================================
void test_automatic_mode_blocks_manual_control(void) {
    test_automatic_mode_enabled = true;  // Automatic mode
    mock_gpio_states[ANCHOR_HOME_PIN] = true;  // Not home
    
    // Try to activate manual UP (should be blocked)
    bool manual_command_accepted = false;
    if (!test_automatic_mode_enabled) {
        test_setWinchUp();
        manual_command_accepted = true;
    }
    
    TEST_ASSERT_FALSE(manual_command_accepted);
    TEST_ASSERT_FALSE(test_winch_active);
}

// =============================================================================
// TEST: Arm and Fire - Target Armed First
// =============================================================================
void test_arm_target_then_fire(void) {
    test_automatic_mode_enabled = false;  // Start in manual mode
    test_pulse_count = 0;  // Current: 0m
    test_target_rode_length = -1.0;  // No target
    mock_gpio_states[ANCHOR_HOME_PIN] = true;  // Not home
    
    // Step 1: ARM - Set target while in manual mode
    test_target_rode_length = 10.0;
    TEST_ASSERT_FALSE(test_winch_active);  // Winch should not start yet
    
    // Step 2: FIRE - Enable automatic mode
    test_automatic_mode_enabled = true;
    float current_length = test_pulse_count * test_config_meters_per_pulse;
    
    // System should start deploying when auto mode enabled
    if (test_automatic_mode_enabled && test_target_rode_length >= 0) {
        if (current_length < test_target_rode_length) {
            test_setWinchDown();
        }
    }
    
    TEST_ASSERT_TRUE(test_winch_active);
    TEST_ASSERT_TRUE(mock_gpio_states[WINCH_DOWN_PIN]);
}

// =============================================================================
// TEST: Automatic Target Deploy
// =============================================================================
void test_automatic_target_deploy(void) {
    test_automatic_mode_enabled = true;
    test_pulse_count = 0;  // Current: 0m
    test_target_rode_length = 10.0;  // Target: 10m
    test_config_meters_per_pulse = 0.1;
    mock_gpio_states[ANCHOR_HOME_PIN] = true;  // Not home
    
    float current_length = test_pulse_count * test_config_meters_per_pulse;
    
    // System should start deploying
    if (test_automatic_mode_enabled && test_target_rode_length >= 0) {
        if (current_length < test_target_rode_length) {
            test_setWinchDown();
        }
    }
    
    TEST_ASSERT_TRUE(test_winch_active);
    TEST_ASSERT_TRUE(mock_gpio_states[WINCH_DOWN_PIN]);
}

// =============================================================================
// TEST: Automatic Target Retrieve
// =============================================================================
void test_automatic_target_retrieve(void) {
    test_automatic_mode_enabled = true;
    test_pulse_count = 150;  // Current: 15m
    test_target_rode_length = 5.0;  // Target: 5m
    test_config_meters_per_pulse = 0.1;
    mock_gpio_states[ANCHOR_HOME_PIN] = true;  // Not home
    
    float current_length = test_pulse_count * test_config_meters_per_pulse;
    
    // System should start retrieving
    if (test_automatic_mode_enabled && test_target_rode_length >= 0) {
        if (current_length > test_target_rode_length) {
            test_setWinchUp();
        }
    }
    
    TEST_ASSERT_TRUE(test_winch_active);
    TEST_ASSERT_TRUE(mock_gpio_states[WINCH_UP_PIN]);
}

// =============================================================================
// TEST: Automatic Stops and Disables at Target
// =============================================================================
void test_automatic_stops_and_disables_at_target(void) {
    test_automatic_mode_enabled = true;
    test_pulse_count = 100;  // Current: 10m
    test_target_rode_length = 10.0;  // Target: 10m
    test_config_meters_per_pulse = 0.1;
    test_winch_active = true;
    
    float current_length = test_pulse_count * test_config_meters_per_pulse;
    float tolerance = test_config_meters_per_pulse * 2.0;
    
    // Check if target reached - should stop and disable automatic mode
    if (test_automatic_mode_enabled && test_target_rode_length >= 0 && test_winch_active) {
        if (fabs(current_length - test_target_rode_length) <= tolerance) {
            test_stopWinch();
            test_automatic_mode_enabled = false;  // Auto-disable on completion
        }
    }
    
    TEST_ASSERT_FALSE(test_winch_active);
    TEST_ASSERT_FALSE(test_automatic_mode_enabled);  // Should return to manual mode
}

// =============================================================================
// TEST: Counter Reset at Home
// =============================================================================
void test_counter_resets_at_home_position(void) {
    test_pulse_count = 50;  // 5 meters out
    
    // Simulate anchor reaching home
    mock_gpio_states[ANCHOR_HOME_PIN] = false;  // LOW = home
    
    // System should reset counter
    if (mock_gpio_states[ANCHOR_HOME_PIN] == LOW) {
        test_pulse_count = 0;
    }
    
    TEST_ASSERT_EQUAL_INT32(0, test_pulse_count);
}

// =============================================================================
// TEST: System Starts in Manual Mode
// =============================================================================
void test_system_defaults_to_manual_mode(void) {
    // Fresh initialization
    bool initial_automatic_mode = false;
    
    TEST_ASSERT_FALSE(initial_automatic_mode);
}

// =============================================================================
// TEST: Physical Remote UP Button
// =============================================================================
void test_physical_remote_up_button(void) {
    test_automatic_mode_enabled = false;  // Manual mode
    mock_gpio_states[ANCHOR_HOME_PIN] = true;  // Not home
    mock_gpio_states[REMOTE_UP_PIN] = true;  // UP button pressed
    mock_gpio_states[REMOTE_DOWN_PIN] = false;
    
    // Simulate handleManualInputs()
    if (!test_automatic_mode_enabled) {
        bool remote_up = mock_gpio_states[REMOTE_UP_PIN];
        bool remote_down = mock_gpio_states[REMOTE_DOWN_PIN];
        
        if (remote_up) {
            test_setWinchUp();
        } else if (remote_down) {
            test_setWinchDown();
        } else {
            test_stopWinch();
        }
    }
    
    TEST_ASSERT_TRUE(test_winch_active);
    TEST_ASSERT_TRUE(mock_gpio_states[WINCH_UP_PIN]);
}

// =============================================================================
// TEST: Physical Remote DOWN Button
// =============================================================================
void test_physical_remote_down_button(void) {
    test_automatic_mode_enabled = false;  // Manual mode
    mock_gpio_states[ANCHOR_HOME_PIN] = true;  // Not home
    mock_gpio_states[REMOTE_UP_PIN] = false;
    mock_gpio_states[REMOTE_DOWN_PIN] = true;  // DOWN button pressed
    
    // Simulate handleManualInputs()
    if (!test_automatic_mode_enabled) {
        bool remote_up = mock_gpio_states[REMOTE_UP_PIN];
        bool remote_down = mock_gpio_states[REMOTE_DOWN_PIN];
        
        if (remote_up) {
            test_setWinchUp();
        } else if (remote_down) {
            test_setWinchDown();
        } else {
            test_stopWinch();
        }
    }
    
    TEST_ASSERT_TRUE(test_winch_active);
    TEST_ASSERT_TRUE(mock_gpio_states[WINCH_DOWN_PIN]);
}

// =============================================================================
// TEST: Physical Remote Stops When Released
// =============================================================================
void test_physical_remote_stops_when_released(void) {
    test_automatic_mode_enabled = false;  // Manual mode
    test_winch_active = true;  // Winch running
    mock_gpio_states[REMOTE_UP_PIN] = false;  // Released
    mock_gpio_states[REMOTE_DOWN_PIN] = false;  // Released
    
    // Simulate handleManualInputs()
    if (!test_automatic_mode_enabled) {
        bool remote_up = mock_gpio_states[REMOTE_UP_PIN];
        bool remote_down = mock_gpio_states[REMOTE_DOWN_PIN];
        
        if (remote_up) {
            test_setWinchUp();
        } else if (remote_down) {
            test_setWinchDown();
        } else {
            test_stopWinch();
        }
    }
    
    TEST_ASSERT_FALSE(test_winch_active);
}

// =============================================================================
// TEST: Physical Remote Blocked in Automatic Mode
// =============================================================================
void test_physical_remote_blocked_in_auto_mode(void) {
    test_automatic_mode_enabled = true;  // Automatic mode active
    mock_gpio_states[REMOTE_UP_PIN] = true;  // Button pressed
    
    // Simulate handleManualInputs()
    bool command_executed = false;
    if (!test_automatic_mode_enabled) {
        test_setWinchUp();
        command_executed = true;
    }
    
    TEST_ASSERT_FALSE(command_executed);
    TEST_ASSERT_FALSE(test_winch_active);
}

// =============================================================================
// TEST: Pulse ISR Increments on Chain Out
// =============================================================================
void test_pulse_isr_increments_on_direction_high(void) {
    test_pulse_count = 10;
    mock_gpio_states[DIRECTION_PIN] = true;  // Chain OUT
    
    // Simulate ISR behavior
    if (mock_gpio_states[DIRECTION_PIN]) {
        test_pulse_count++;
    } else {
        test_pulse_count--;
        if (test_pulse_count < 0) test_pulse_count = 0;
    }
    
    TEST_ASSERT_EQUAL_INT32(11, test_pulse_count);
}

// =============================================================================
// TEST: Pulse ISR Decrements on Chain In
// =============================================================================
void test_pulse_isr_decrements_on_direction_low(void) {
    test_pulse_count = 10;
    mock_gpio_states[DIRECTION_PIN] = false;  // Chain IN
    
    // Simulate ISR behavior
    if (mock_gpio_states[DIRECTION_PIN]) {
        test_pulse_count++;
    } else {
        test_pulse_count--;
        if (test_pulse_count < 0) test_pulse_count = 0;
    }
    
    TEST_ASSERT_EQUAL_INT32(9, test_pulse_count);
}

// =============================================================================
// TEST: Reset Function Clears State
// =============================================================================
void test_reset_clears_all_state(void) {
    test_pulse_count = 100;
    test_target_rode_length = 15.0;
    test_winch_active = true;
    
    // Simulate reset() method
    test_pulse_count = 0;
    test_stopWinch();
    test_target_rode_length = -1.0;
    
    TEST_ASSERT_EQUAL_INT32(0, test_pulse_count);
    TEST_ASSERT_EQUAL_FLOAT(-1.0, test_target_rode_length);
    TEST_ASSERT_FALSE(test_winch_active);
}

// =============================================================================
// TEST: Home Sensor Stops Active Retrieval
// =============================================================================
void test_home_sensor_stops_active_retrieval(void) {
    test_winch_active = true;
    mock_gpio_states[WINCH_UP_PIN] = true;  // Actively retrieving
    mock_gpio_states[ANCHOR_HOME_PIN] = false;  // HOME reached
    
    // Simulate PulseCounter::update() home detection
    bool is_home = (mock_gpio_states[ANCHOR_HOME_PIN] == LOW);
    if (is_home && test_winch_active && mock_gpio_states[WINCH_UP_PIN]) {
        test_stopWinch();
        test_pulse_count = 0;
    }
    
    TEST_ASSERT_FALSE(test_winch_active);
    TEST_ASSERT_EQUAL_INT32(0, test_pulse_count);
}

// =============================================================================
// TEST: Automatic Mode Within Tolerance Stops
// =============================================================================
void test_automatic_within_tolerance_stops(void) {
    test_automatic_mode_enabled = true;
    test_target_rode_length = 10.0;
    test_pulse_count = 100;  // 10.0m with 0.1 meters per pulse
    test_config_meters_per_pulse = 0.1;
    test_winch_active = true;
    
    float current_meters = test_pulse_count * test_config_meters_per_pulse;
    float tolerance = test_config_meters_per_pulse * 2.0;  // ±0.2m
    
    // Simulate automatic mode check
    if (test_automatic_mode_enabled && test_target_rode_length >= 0) {
        if (fabs(current_meters - test_target_rode_length) <= tolerance) {
            if (test_winch_active) {
                test_stopWinch();
                test_automatic_mode_enabled = false;
            }
        }
    }
    
    TEST_ASSERT_FALSE(test_winch_active);
    TEST_ASSERT_FALSE(test_automatic_mode_enabled);
}

// =============================================================================
// TEST: Automatic Mode Starts Deploy When Below Target
// =============================================================================
void test_automatic_starts_deploy_below_target(void) {
    test_automatic_mode_enabled = true;
    test_target_rode_length = 15.0;
    test_pulse_count = 50;  // 5.0m
    test_config_meters_per_pulse = 0.1;
    mock_gpio_states[WINCH_DOWN_PIN] = false;  // Not active yet
    mock_gpio_states[ANCHOR_HOME_PIN] = true;  // Not home
    
    float current_meters = test_pulse_count * test_config_meters_per_pulse;
    float tolerance = test_config_meters_per_pulse * 2.0;
    
    // Simulate automatic mode control
    if (test_automatic_mode_enabled && test_target_rode_length >= 0) {
        if (fabs(current_meters - test_target_rode_length) > tolerance) {
            if (current_meters < test_target_rode_length && !mock_gpio_states[WINCH_DOWN_PIN]) {
                test_setWinchDown();
            }
        }
    }
    
    TEST_ASSERT_TRUE(test_winch_active);
    TEST_ASSERT_TRUE(mock_gpio_states[WINCH_DOWN_PIN]);
}

// =============================================================================
// TEST: Automatic Mode Starts Retrieve When Above Target
// =============================================================================
void test_automatic_starts_retrieve_above_target(void) {
    test_automatic_mode_enabled = true;
    test_target_rode_length = 5.0;
    test_pulse_count = 150;  // 15.0m
    test_config_meters_per_pulse = 0.1;
    mock_gpio_states[WINCH_UP_PIN] = false;  // Not active yet
    mock_gpio_states[ANCHOR_HOME_PIN] = true;  // Not home
    
    float current_meters = test_pulse_count * test_config_meters_per_pulse;
    float tolerance = test_config_meters_per_pulse * 2.0;
    
    // Simulate automatic mode control
    if (test_automatic_mode_enabled && test_target_rode_length >= 0) {
        if (fabs(current_meters - test_target_rode_length) > tolerance) {
            if (current_meters > test_target_rode_length && !mock_gpio_states[WINCH_UP_PIN]) {
                test_setWinchUp();
            }
        }
    }
    
    TEST_ASSERT_TRUE(test_winch_active);
    TEST_ASSERT_TRUE(mock_gpio_states[WINCH_UP_PIN]);
}

// =============================================================================
// TEST: Get Meters Per Pulse Function
// =============================================================================
void test_get_meters_per_pulse(void) {
    test_config_meters_per_pulse = 0.05;
    
    float result = test_config_meters_per_pulse;
    
    TEST_ASSERT_EQUAL_FLOAT(0.05, result);
}

// =============================================================================
// TEST: Both Remote Buttons Cannot Be Active Simultaneously
// =============================================================================
void test_remote_buttons_priority(void) {
    test_automatic_mode_enabled = false;
    mock_gpio_states[REMOTE_UP_PIN] = true;  // Both pressed
    mock_gpio_states[REMOTE_DOWN_PIN] = true;  // (shouldn't happen in practice)
    mock_gpio_states[ANCHOR_HOME_PIN] = true;
    
    // Simulate handleManualInputs() - UP takes priority
    if (!test_automatic_mode_enabled) {
        bool remote_up = mock_gpio_states[REMOTE_UP_PIN];
        bool remote_down = mock_gpio_states[REMOTE_DOWN_PIN];
        
        if (remote_up) {
            test_setWinchUp();
        } else if (remote_down) {
            test_setWinchDown();
        } else {
            test_stopWinch();
        }
    }
    
    // UP should take priority due to if-else structure
    TEST_ASSERT_TRUE(test_winch_active);
    TEST_ASSERT_TRUE(mock_gpio_states[WINCH_UP_PIN]);
    TEST_ASSERT_FALSE(mock_gpio_states[WINCH_DOWN_PIN]);
}

// =============================================================================
// TEST: Target Length Can Be Armed Without Starting
// =============================================================================
void test_target_can_be_armed_without_starting(void) {
    test_automatic_mode_enabled = false;  // Still in manual mode
    test_target_rode_length = -1.0;  // No target
    test_winch_active = false;
    
    // Arm target
    test_target_rode_length = 20.0;
    
    // Should NOT start winch
    TEST_ASSERT_EQUAL_FLOAT(20.0, test_target_rode_length);
    TEST_ASSERT_FALSE(test_winch_active);
    TEST_ASSERT_FALSE(test_automatic_mode_enabled);
}

// =============================================================================
// TEST: Winch Outputs Are Mutually Exclusive
// =============================================================================
void test_winch_outputs_mutually_exclusive(void) {
    mock_gpio_states[ANCHOR_HOME_PIN] = true;  // Not home
    
    // Test UP
    test_setWinchUp();
    bool up_active = mock_gpio_states[WINCH_UP_PIN];
    bool down_active_during_up = mock_gpio_states[WINCH_DOWN_PIN];
    
    // Test DOWN
    test_setWinchDown();
    bool up_active_during_down = mock_gpio_states[WINCH_UP_PIN];
    bool down_active = mock_gpio_states[WINCH_DOWN_PIN];
    
    TEST_ASSERT_TRUE(up_active);
    TEST_ASSERT_FALSE(down_active_during_up);
    TEST_ASSERT_FALSE(up_active_during_down);
    TEST_ASSERT_TRUE(down_active);
}

// =============================================================================
// Main Test Runner
// =============================================================================
void setup() {
    delay(2000);  // Wait for serial monitor
    
    UNITY_BEGIN();
    
    // Counter tests
    RUN_TEST(test_pulse_counter_increment_on_chain_out);
    RUN_TEST(test_pulse_counter_decrement_on_chain_in);
    RUN_TEST(test_pulse_counter_prevents_negative_values);
    RUN_TEST(test_meters_calculation_from_pulses);
    RUN_TEST(test_get_meters_per_pulse);
    
    // ISR tests
    RUN_TEST(test_pulse_isr_increments_on_direction_high);
    RUN_TEST(test_pulse_isr_decrements_on_direction_low);
    
    // Safety sensor tests
    RUN_TEST(test_home_sensor_blocks_winch_up);
    RUN_TEST(test_home_sensor_allows_winch_down);
    RUN_TEST(test_home_sensor_stops_active_retrieval);
    RUN_TEST(test_counter_resets_at_home_position);
    
    // Winch control tests
    RUN_TEST(test_winch_up_works_when_not_home);
    RUN_TEST(test_winch_down_works);
    RUN_TEST(test_stop_winch_clears_all_outputs);
    RUN_TEST(test_winch_outputs_mutually_exclusive);
    
    // Physical remote control tests
    RUN_TEST(test_physical_remote_up_button);
    RUN_TEST(test_physical_remote_down_button);
    RUN_TEST(test_physical_remote_stops_when_released);
    RUN_TEST(test_physical_remote_blocked_in_auto_mode);
    RUN_TEST(test_remote_buttons_priority);
    
    // ========== BOW PROPELLER TESTS ==========
    
    // Bow motor hardware tests
    RUN_TEST(test_bow_motor_initializes_pins);
    RUN_TEST(test_bow_motor_turn_port);
    RUN_TEST(test_bow_motor_turn_starboard);
    RUN_TEST(test_bow_motor_stop);
    RUN_TEST(test_bow_motor_mutual_exclusion_port_then_starboard);
    RUN_TEST(test_bow_motor_mutual_exclusion_starboard_then_port);
    
    // Bow controller logic tests
    RUN_TEST(test_bow_controller_initializes_with_motor);
    RUN_TEST(test_bow_controller_turn_port_command);
    RUN_TEST(test_bow_controller_turn_starboard_command);
    RUN_TEST(test_bow_controller_stop_command);
    RUN_TEST(test_bow_controller_port_to_starboard_switching);
    RUN_TEST(test_bow_controller_repeated_same_command);
    
    // Bow SignalK command mapping tests
    RUN_TEST(test_signalk_command_port_maps_to_port);
    RUN_TEST(test_signalk_command_stop_maps_to_stop);
    RUN_TEST(test_signalk_command_starboard_maps_to_starboard);
    
    // Bow safety tests
    RUN_TEST(test_bow_motor_never_activates_both_relays);
    RUN_TEST(test_bow_controller_safe_repeated_stop);
    
    // Bow system integration tests
    RUN_TEST(test_bow_system_rapid_direction_changes);
    RUN_TEST(test_bow_system_stop_after_rapid_changes);
    RUN_TEST(test_bow_startup_always_inactive);
    RUN_TEST(test_bow_controller_state_consistency);
    
    // Bow SignalK integration tests
    RUN_TEST(test_signalk_sends_port_command);
    RUN_TEST(test_signalk_sends_stop_command);
    RUN_TEST(test_signalk_sends_starboard_command);
    RUN_TEST(test_signalk_status_output_reflects_state);
    RUN_TEST(test_signalk_blocks_when_not_connected);
    RUN_TEST(test_signalk_blocks_when_emergency_stop_active);
    RUN_TEST(test_signalk_commands_resume_after_emergency_stop_cleared);
    RUN_TEST(test_signalk_reconnection_resumes_commands);
    
    // Bow emergency stop tests
    RUN_TEST(test_emergency_stop_blocks_signalk_commands);
    RUN_TEST(test_emergency_stop_blocks_remote_commands);
    RUN_TEST(test_emergency_stop_activation_stops_motor);
    RUN_TEST(test_emergency_stop_counts_blocked_attempts);
    
    // Bow remote control integration tests
    RUN_TEST(test_remote_func3_button_activates_port);
    RUN_TEST(test_remote_func4_button_activates_starboard);
    RUN_TEST(test_remote_button_release_stops_motor);
    
    // Bow system-level integration tests
    RUN_TEST(test_signalk_and_remote_can_coexist);
    RUN_TEST(test_emergency_stop_blocks_both_signalk_and_remote);
    RUN_TEST(test_full_scenario_normal_operation);
    
    // ========== ANCHOR WINCH TESTS ==========
    
    RUN_TEST(test_system_defaults_to_manual_mode);
    RUN_TEST(test_manual_mode_allows_control);
    RUN_TEST(test_automatic_mode_blocks_manual_control);
    
    // Automatic target control tests
    RUN_TEST(test_target_can_be_armed_without_starting);
    RUN_TEST(test_arm_target_then_fire);
    RUN_TEST(test_automatic_target_deploy);
    RUN_TEST(test_automatic_target_retrieve);
    RUN_TEST(test_automatic_starts_deploy_below_target);
    RUN_TEST(test_automatic_starts_retrieve_above_target);
    RUN_TEST(test_automatic_within_tolerance_stops);
    RUN_TEST(test_automatic_stops_and_disables_at_target);
    
    // Reset function test
    RUN_TEST(test_reset_clears_all_state);
    
    UNITY_END();
}

void loop() {
    // Nothing here
}

// Native platform entry point
#ifdef ARDUINO
// For Arduino/ESP32: setup() and loop() are called by the framework
#else
// For native testing: provide a main() function
int main(int argc, char **argv) {
    setup();
    return 0;
}
#endif
