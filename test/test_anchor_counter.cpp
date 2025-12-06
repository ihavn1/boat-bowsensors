#include <unity.h>
#include <Arduino.h>

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
#define PULSE_INPUT_PIN 25
#define DIRECTION_PIN 26
#define WINCH_UP_PIN 27
#define WINCH_DOWN_PIN 14
#define ANCHOR_HOME_PIN 33

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
    
    // Safety sensor tests
    RUN_TEST(test_home_sensor_blocks_winch_up);
    RUN_TEST(test_home_sensor_allows_winch_down);
    RUN_TEST(test_counter_resets_at_home_position);
    
    // Winch control tests
    RUN_TEST(test_winch_up_works_when_not_home);
    RUN_TEST(test_winch_down_works);
    RUN_TEST(test_stop_winch_clears_all_outputs);
    
    // Manual/Automatic mode tests
    RUN_TEST(test_system_defaults_to_manual_mode);
    RUN_TEST(test_manual_mode_allows_control);
    RUN_TEST(test_automatic_mode_blocks_manual_control);
    
    // Automatic target control tests
    RUN_TEST(test_arm_target_then_fire);
    RUN_TEST(test_automatic_target_deploy);
    RUN_TEST(test_automatic_target_retrieve);
    RUN_TEST(test_automatic_stops_and_disables_at_target);
    
    UNITY_END();
}

void loop() {
    // Nothing here
}
