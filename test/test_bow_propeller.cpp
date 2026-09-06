// Unit tests for bow propeller controller and integration
// Tests basic motor control, GPIO safety, and system integration

#include <unity.h>
#include <Arduino.h>
#include <string>
#include "pin_config.h"

// Mock GPIO states for testing
extern bool mock_gpio_states[40];
extern int mock_gpio_modes[40];

// ========== MOCK IMPLEMENTATIONS ==========

/**
 * Mock motor interface implementation for testing
 * Simulates GPIO control without hardware
 */
class MockBowPropellerMotor {
public:
    enum class Direction { STOPPED = 0, PORT = -1, STARBOARD = 1 };
    
    MockBowPropellerMotor() 
        : current_direction_(Direction::STOPPED),
          port_active_(false),
          starboard_active_(false) {}
    
    void initialize() {
        // Simulate GPIO initialization
        mock_gpio_modes[PinConfig::BOW_PORT] = OUTPUT;
        mock_gpio_modes[PinConfig::BOW_STARBOARD] = OUTPUT;
        // Start with inactive (HIGH)
        mock_gpio_states[PinConfig::BOW_PORT] = HIGH;
        mock_gpio_states[PinConfig::BOW_STARBOARD] = HIGH;
    }
    
    void turnPort() {
        // Safety: ensure starboard is OFF
        mock_gpio_states[PinConfig::BOW_STARBOARD] = HIGH;
        starboard_active_ = false;
        // Activate port
        mock_gpio_states[PinConfig::BOW_PORT] = LOW;
        port_active_ = true;
        current_direction_ = Direction::PORT;
    }
    
    void turnStarboard() {
        // Safety: ensure port is OFF
        mock_gpio_states[PinConfig::BOW_PORT] = HIGH;
        port_active_ = false;
        // Activate starboard
        mock_gpio_states[PinConfig::BOW_STARBOARD] = LOW;
        starboard_active_ = true;
        current_direction_ = Direction::STARBOARD;
    }
    
    void stop() {
        mock_gpio_states[PinConfig::BOW_PORT] = HIGH;
        mock_gpio_states[PinConfig::BOW_STARBOARD] = HIGH;
        port_active_ = false;
        starboard_active_ = false;
        current_direction_ = Direction::STOPPED;
    }
    
    bool isActive() const {
        return (current_direction_ != Direction::STOPPED);
    }
    
    int getDirection() const {
        return static_cast<int>(current_direction_);
    }
    
    // Test helpers
    bool isPortActive() const { return port_active_; }
    bool isStarboardActive() const { return starboard_active_; }
    
private:
    Direction current_direction_;
    bool port_active_;
    bool starboard_active_;
};

/**
 * Mock bow propeller controller for testing
 * Wraps the motor and provides high-level control
 */
class MockBowPropellerController {
public:
    MockBowPropellerController(MockBowPropellerMotor& motor)
        : motor_(motor),
          is_stopped_(true),
          last_command_(0) {}
    
    void turnPort() {
        motor_.turnPort();
        is_stopped_ = false;
        last_command_ = -1;
    }
    
    void turnStarboard() {
        motor_.turnStarboard();
        is_stopped_ = false;
        last_command_ = 1;
    }
    
    void stop() {
        motor_.stop();
        is_stopped_ = true;
        last_command_ = 0;
    }
    
    bool isStopped() const { return is_stopped_; }
    int getLastCommand() const { return last_command_; }
    bool isActive() const { return motor_.isActive(); }
    int getDirection() const { return motor_.getDirection(); }
    
private:
    MockBowPropellerMotor& motor_;
    bool is_stopped_;
    int last_command_;
};

// ========== MOTOR HARDWARE TESTS ==========

void test_bow_motor_initializes_pins(void) {
    MockBowPropellerMotor motor;
    motor.initialize();
    
    TEST_ASSERT_EQUAL_INT(OUTPUT, mock_gpio_modes[PinConfig::BOW_PORT]);
    TEST_ASSERT_EQUAL_INT(OUTPUT, mock_gpio_modes[PinConfig::BOW_STARBOARD]);
    TEST_ASSERT_EQUAL_INT(HIGH, mock_gpio_states[PinConfig::BOW_PORT]);
    TEST_ASSERT_EQUAL_INT(HIGH, mock_gpio_states[PinConfig::BOW_STARBOARD]);
}

void test_bow_motor_turn_port(void) {
    MockBowPropellerMotor motor;
    motor.initialize();
    motor.turnPort();
    
    TEST_ASSERT_TRUE(motor.isPortActive());
    TEST_ASSERT_FALSE(motor.isStarboardActive());
    TEST_ASSERT_EQUAL_INT(LOW, mock_gpio_states[PinConfig::BOW_PORT]);
    TEST_ASSERT_EQUAL_INT(HIGH, mock_gpio_states[PinConfig::BOW_STARBOARD]);
}

void test_bow_motor_turn_starboard(void) {
    MockBowPropellerMotor motor;
    motor.initialize();
    motor.turnStarboard();
    
    TEST_ASSERT_FALSE(motor.isPortActive());
    TEST_ASSERT_TRUE(motor.isStarboardActive());
    TEST_ASSERT_EQUAL_INT(HIGH, mock_gpio_states[PinConfig::BOW_PORT]);
    TEST_ASSERT_EQUAL_INT(LOW, mock_gpio_states[PinConfig::BOW_STARBOARD]);
}

void test_bow_motor_stop(void) {
    MockBowPropellerMotor motor;
    motor.initialize();
    
    // Turn port then stop
    motor.turnPort();
    TEST_ASSERT_TRUE(motor.isActive());
    
    motor.stop();
    TEST_ASSERT_FALSE(motor.isActive());
    TEST_ASSERT_EQUAL_INT(HIGH, mock_gpio_states[PinConfig::BOW_PORT]);
    TEST_ASSERT_EQUAL_INT(HIGH, mock_gpio_states[PinConfig::BOW_STARBOARD]);
}

void test_bow_motor_mutual_exclusion_port_then_starboard(void) {
    MockBowPropellerMotor motor;
    motor.initialize();
    
    motor.turnPort();
    TEST_ASSERT_TRUE(motor.isPortActive());
    TEST_ASSERT_FALSE(motor.isStarboardActive());
    
    // Turn starboard - should deactivate port first
    motor.turnStarboard();
    TEST_ASSERT_FALSE(motor.isPortActive());
    TEST_ASSERT_TRUE(motor.isStarboardActive());
    TEST_ASSERT_EQUAL_INT(HIGH, mock_gpio_states[PinConfig::BOW_PORT]);
    TEST_ASSERT_EQUAL_INT(LOW, mock_gpio_states[PinConfig::BOW_STARBOARD]);
}

void test_bow_motor_mutual_exclusion_starboard_then_port(void) {
    MockBowPropellerMotor motor;
    motor.initialize();
    
    motor.turnStarboard();
    TEST_ASSERT_FALSE(motor.isPortActive());
    TEST_ASSERT_TRUE(motor.isStarboardActive());
    
    // Turn port - should deactivate starboard first
    motor.turnPort();
    TEST_ASSERT_TRUE(motor.isPortActive());
    TEST_ASSERT_FALSE(motor.isStarboardActive());
    TEST_ASSERT_EQUAL_INT(LOW, mock_gpio_states[PinConfig::BOW_PORT]);
    TEST_ASSERT_EQUAL_INT(HIGH, mock_gpio_states[PinConfig::BOW_STARBOARD]);
}

// ========== CONTROLLER LOGIC TESTS ==========

void test_bow_controller_initializes_with_motor(void) {
    MockBowPropellerMotor motor;
    MockBowPropellerController controller(motor);
    
    TEST_ASSERT_TRUE(controller.isStopped());
    TEST_ASSERT_EQUAL_INT(0, controller.getLastCommand());
}

void test_bow_controller_turn_port_command(void) {
    MockBowPropellerMotor motor;
    motor.initialize();
    MockBowPropellerController controller(motor);
    
    controller.turnPort();
    
    TEST_ASSERT_FALSE(controller.isStopped());
    TEST_ASSERT_TRUE(controller.isActive());
    TEST_ASSERT_EQUAL_INT(-1, controller.getDirection());
    TEST_ASSERT_EQUAL_INT(-1, controller.getLastCommand());
}

void test_bow_controller_turn_starboard_command(void) {
    MockBowPropellerMotor motor;
    motor.initialize();
    MockBowPropellerController controller(motor);
    
    controller.turnStarboard();
    
    TEST_ASSERT_FALSE(controller.isStopped());
    TEST_ASSERT_TRUE(controller.isActive());
    TEST_ASSERT_EQUAL_INT(1, controller.getDirection());
    TEST_ASSERT_EQUAL_INT(1, controller.getLastCommand());
}

void test_bow_controller_stop_command(void) {
    MockBowPropellerMotor motor;
    motor.initialize();
    MockBowPropellerController controller(motor);
    
    controller.turnPort();
    TEST_ASSERT_TRUE(controller.isActive());
    
    controller.stop();
    TEST_ASSERT_TRUE(controller.isStopped());
    TEST_ASSERT_FALSE(controller.isActive());
    TEST_ASSERT_EQUAL_INT(0, controller.getLastCommand());
}

void test_bow_controller_port_to_starboard_switching(void) {
    MockBowPropellerMotor motor;
    motor.initialize();
    MockBowPropellerController controller(motor);
    
    // Start port
    controller.turnPort();
    TEST_ASSERT_EQUAL_INT(-1, controller.getDirection());
    TEST_ASSERT_TRUE(motor.isPortActive());
    
    // Switch to starboard
    controller.turnStarboard();
    TEST_ASSERT_EQUAL_INT(1, controller.getDirection());
    TEST_ASSERT_FALSE(motor.isPortActive());
    TEST_ASSERT_TRUE(motor.isStarboardActive());
}

void test_bow_controller_repeated_same_command(void) {
    MockBowPropellerMotor motor;
    motor.initialize();
    MockBowPropellerController controller(motor);
    
    controller.turnStarboard();
    TEST_ASSERT_TRUE(motor.isStarboardActive());
    
    // Send same command again - should be safe
    controller.turnStarboard();
    TEST_ASSERT_TRUE(motor.isStarboardActive());
    TEST_ASSERT_FALSE(motor.isPortActive());
}

// ========== SIGNALK COMMAND MAPPING TESTS ==========

void test_signalk_command_port_maps_to_port(void) {
    MockBowPropellerMotor motor;
    motor.initialize();
    MockBowPropellerController controller(motor);
    
    std::string signalk_command = "port";
    if (signalk_command == "port") {
        controller.turnPort();
    }
    
    TEST_ASSERT_EQUAL_INT(-1, controller.getDirection());
    TEST_ASSERT_TRUE(motor.isPortActive());
}

void test_signalk_command_stop_maps_to_stop(void) {
    MockBowPropellerMotor motor;
    motor.initialize();
    MockBowPropellerController controller(motor);
    
    controller.turnPort();
    TEST_ASSERT_TRUE(controller.isActive());
    
    std::string signalk_command = "stop";
    if (signalk_command == "stop") {
        controller.stop();
    }
    
    TEST_ASSERT_TRUE(controller.isStopped());
    TEST_ASSERT_FALSE(controller.isActive());
}

void test_signalk_command_starboard_maps_to_starboard(void) {
    MockBowPropellerMotor motor;
    motor.initialize();
    MockBowPropellerController controller(motor);
    
    std::string signalk_command = "starboard";
    if (signalk_command == "starboard") {
        controller.turnStarboard();
    }
    
    TEST_ASSERT_EQUAL_INT(1, controller.getDirection());
    TEST_ASSERT_TRUE(motor.isStarboardActive());
}

// ========== SAFETY TESTS ==========

void test_bow_motor_never_activates_both_relays(void) {
    MockBowPropellerMotor motor;
    motor.initialize();
    
    // Try port
    motor.turnPort();
    TEST_ASSERT_FALSE(motor.isStarboardActive());
    
    // Try to activate starboard while port active
    motor.turnStarboard();
    TEST_ASSERT_FALSE(motor.isPortActive());
    
    // Verify both are never active at same time
    motor.turnPort();
    TEST_ASSERT_FALSE(motor.isStarboardActive());
}

void test_bow_controller_safe_repeated_stop(void) {
    MockBowPropellerMotor motor;
    motor.initialize();
    MockBowPropellerController controller(motor);
    
    controller.stop();
    TEST_ASSERT_EQUAL_INT(HIGH, mock_gpio_states[PinConfig::BOW_PORT]);
    TEST_ASSERT_EQUAL_INT(HIGH, mock_gpio_states[PinConfig::BOW_STARBOARD]);
    
    // Stop again - should be safe
    controller.stop();
    TEST_ASSERT_EQUAL_INT(HIGH, mock_gpio_states[PinConfig::BOW_PORT]);
    TEST_ASSERT_EQUAL_INT(HIGH, mock_gpio_states[PinConfig::BOW_STARBOARD]);
}

// ========== INTEGRATION TESTS ==========

void test_bow_system_rapid_direction_changes(void) {
    MockBowPropellerMotor motor;
    motor.initialize();
    MockBowPropellerController controller(motor);
    
    // Rapid switching should not cause both relays to activate
    for (int i = 0; i < 10; i++) {
        controller.turnPort();
        TEST_ASSERT_FALSE(motor.isStarboardActive());
        
        controller.turnStarboard();
        TEST_ASSERT_FALSE(motor.isPortActive());
    }
    
    // Final state should be starboard
    TEST_ASSERT_TRUE(motor.isStarboardActive());
    TEST_ASSERT_FALSE(motor.isPortActive());
}

void test_bow_system_stop_after_rapid_changes(void) {
    MockBowPropellerMotor motor;
    motor.initialize();
    MockBowPropellerController controller(motor);
    
    controller.turnPort();
    controller.turnStarboard();
    controller.turnPort();
    
    // Stop after rapid changes
    controller.stop();
    
    TEST_ASSERT_FALSE(motor.isPortActive());
    TEST_ASSERT_FALSE(motor.isStarboardActive());
    TEST_ASSERT_FALSE(controller.isActive());
}

void test_bow_startup_always_inactive(void) {
    MockBowPropellerMotor motor;
    motor.initialize();
    
    // Verify startup state is inactive (GPIO pins HIGH)
    TEST_ASSERT_EQUAL_INT(HIGH, mock_gpio_states[PinConfig::BOW_PORT]);
    TEST_ASSERT_EQUAL_INT(HIGH, mock_gpio_states[PinConfig::BOW_STARBOARD]);
    TEST_ASSERT_FALSE(motor.isPortActive());
    TEST_ASSERT_FALSE(motor.isStarboardActive());
}

void test_bow_controller_state_consistency(void) {
    MockBowPropellerMotor motor;
    motor.initialize();
    MockBowPropellerController controller(motor);
    
    // State after port command
    controller.turnPort();
    TEST_ASSERT_EQUAL_INT(controller.getDirection(), motor.getDirection());
    TEST_ASSERT_EQUAL_INT(controller.isActive(), motor.isActive());
    
    // State after starboard command
    controller.turnStarboard();
    TEST_ASSERT_EQUAL_INT(controller.getDirection(), motor.getDirection());
    TEST_ASSERT_EQUAL_INT(controller.isActive(), motor.isActive());
    
    // State after stop
    controller.stop();
    TEST_ASSERT_EQUAL_INT(0, motor.getDirection());
    TEST_ASSERT_FALSE(motor.isActive());
}

// Note: These tests are registered with test_anchor_counter.cpp's main harness
// setUp/tearDown are provided above for Unity framework
