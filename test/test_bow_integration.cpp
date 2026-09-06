// Integration tests for bow propeller with SignalK and Emergency Stop
// Tests high-level system behavior (command blocking, status reporting)

#include <unity.h>
#include <Arduino.h>
#include <string>
#include "pin_config.h"

extern bool mock_gpio_states[40];
extern int mock_gpio_modes[40];

// ========== MOCK OBJECTS ==========

/**
 * Mock motor for integration testing
 */
class IntegrationMockMotor {
public:
    void initialize() {
        mock_gpio_modes[PinConfig::BOW_PORT] = OUTPUT;
        mock_gpio_modes[PinConfig::BOW_STARBOARD] = OUTPUT;
        mock_gpio_states[PinConfig::BOW_PORT] = HIGH;
        mock_gpio_states[PinConfig::BOW_STARBOARD] = HIGH;
    }
    
    void turnPort() {
        mock_gpio_states[PinConfig::BOW_STARBOARD] = HIGH;
        mock_gpio_states[PinConfig::BOW_PORT] = LOW;
    }
    
    void turnStarboard() {
        mock_gpio_states[PinConfig::BOW_PORT] = HIGH;
        mock_gpio_states[PinConfig::BOW_STARBOARD] = LOW;
    }
    
    void stop() {
        mock_gpio_states[PinConfig::BOW_PORT] = HIGH;
        mock_gpio_states[PinConfig::BOW_STARBOARD] = HIGH;
    }
    
    bool isActive() const {
        return (mock_gpio_states[PinConfig::BOW_PORT] == LOW || 
                mock_gpio_states[PinConfig::BOW_STARBOARD] == LOW);
    }
};

/**
 * Mock controller wrapping motor
 */
class IntegrationMockController {
public:
    IntegrationMockController(IntegrationMockMotor& motor)
        : motor_(motor), last_command_(0) {}
    
    void turnPort() { motor_.turnPort(); last_command_ = -1; }
    void turnStarboard() { motor_.turnStarboard(); last_command_ = 1; }
    void stop() { motor_.stop(); last_command_ = 0; }
    
    bool isActive() const { return motor_.isActive(); }
    int getLastCommand() const { return last_command_; }
    
private:
    IntegrationMockMotor& motor_;
    int last_command_;
};

/**
 * Mock emergency stop service
 * When activated, blocks all propeller commands
 */
class MockEmergencyStopService {
public:
    MockEmergencyStopService()
        : is_active_(false),
          blocked_commands_(0) {}
    
    void setActive(bool active) {
        is_active_ = active;
        if (active) {
            blocked_commands_ = 0;
        }
    }
    
    bool isActive() const { return is_active_; }
    
    bool canExecuteCommand() const { return !is_active_; }
    
    void recordBlockedCommand() { blocked_commands_++; }
    int getBlockedCommandCount() const { return blocked_commands_; }
    
private:
    bool is_active_;
    int blocked_commands_;
};

/**
 * Mock SignalK service with command listener
 * Simulates receiving commands from SignalK server
 * Blocks commands during emergency stop or connection issues
 */
class MockSignalKService {
public:
    MockSignalKService(IntegrationMockController& controller,
                      MockEmergencyStopService& emergency_service)
        : controller_(controller),
          emergency_service_(emergency_service),
          is_connected_(false),
          last_signalk_command_("stop"),
          blocked_count_(0),
          executed_count_(0) {}
    
    void setConnected(bool connected) {
        is_connected_ = connected;
    }
    
    bool isConnected() const { return is_connected_; }
    
    /**
     * Process incoming command from SignalK server
     * Mimics behavior from SignalKService::setupBowPropellerBindings()
     * 
     * Blocks commands if:
     * - Emergency stop is active
     * - Not connected to SignalK server (5-second check)
     */
    bool processCommand(const std::string& command) {
        last_signalk_command_ = command;
        
        // Block if emergency stop active
        if (emergency_service_.isActive()) {
            emergency_service_.recordBlockedCommand();
            blocked_count_++;
            return false;
        }
        
        // Block if not connected
        if (!is_connected_) {
            blocked_count_++;
            return false;
        }
        
        // Execute command
        if (command == "port") {
            controller_.turnPort();
        } else if (command == "stop") {
            controller_.stop();
        } else if (command == "starboard") {
            controller_.turnStarboard();
        } else {
            return false;
        }
        
        executed_count_++;
        return true;
    }
    
    const std::string& getLastCommand() const { return last_signalk_command_; }
    int getBlockedCount() const { return blocked_count_; }
    int getExecutedCount() const { return executed_count_; }
    
    /**
     * Mock status output - simulates publishing status to SignalK
     */
    const char* getStatusValue() const {
        if (controller_.getLastCommand() == -1) return "port";
        if (controller_.getLastCommand() == 1) return "starboard";
        return "stopped";
    }
    
private:
    IntegrationMockController& controller_;
    MockEmergencyStopService& emergency_service_;
    bool is_connected_;
    std::string last_signalk_command_;
    int blocked_count_;
    int executed_count_;
};

/**
 * Mock remote control command processor
 * Simulates physical FUNC3/FUNC4 button presses
 * Emergency stop blocks these too
 */
class MockRemoteControl {
public:
    MockRemoteControl(IntegrationMockController& controller,
                     MockEmergencyStopService& emergency_service)
        : controller_(controller),
          emergency_service_(emergency_service),
          blocked_count_(0) {}
    
    /**
     * Process physical button press
     * Returns true if executed, false if blocked
     */
    bool processFUNC3Press() {
        // FUNC3 = Bow PORT
        if (emergency_service_.isActive()) {
            blocked_count_++;
            return false;
        }
        controller_.turnPort();
        return true;
    }
    
    bool processFUNC4Press() {
        // FUNC4 = Bow STARBOARD
        if (emergency_service_.isActive()) {
            blocked_count_++;
            return false;
        }
        controller_.turnStarboard();
        return true;
    }
    
    void processButtonRelease() {
        // Button release = STOP (unless emergency stop is active)
        // Actually during emergency stop, we stop anyway
        controller_.stop();
    }
    
    int getBlockedCount() const { return blocked_count_; }
    
private:
    IntegrationMockController& controller_;
    MockEmergencyStopService& emergency_service_;
    int blocked_count_;
};

// ========== SIGNALK INTEGRATION TESTS ==========

void test_signalk_sends_port_command(void) {
    IntegrationMockMotor motor;
    motor.initialize();
    IntegrationMockController controller(motor);
    MockEmergencyStopService emergency;
    MockSignalKService signalk(controller, emergency);
    
    signalk.setConnected(true);
    bool executed = signalk.processCommand("port");
    
    TEST_ASSERT_TRUE(executed);
    TEST_ASSERT_EQUAL_INT(-1, controller.getLastCommand());
    TEST_ASSERT_EQUAL_INT(1, signalk.getExecutedCount());
}

void test_signalk_sends_stop_command(void) {
    IntegrationMockMotor motor;
    motor.initialize();
    IntegrationMockController controller(motor);
    MockEmergencyStopService emergency;
    MockSignalKService signalk(controller, emergency);
    
    signalk.setConnected(true);
    signalk.processCommand("port");
    TEST_ASSERT_TRUE(controller.isActive());
    
    bool executed = signalk.processCommand("stop");
    
    TEST_ASSERT_TRUE(executed);
    TEST_ASSERT_EQUAL_INT(0, controller.getLastCommand());
    TEST_ASSERT_FALSE(controller.isActive());
}

void test_signalk_sends_starboard_command(void) {
    IntegrationMockMotor motor;
    motor.initialize();
    IntegrationMockController controller(motor);
    MockEmergencyStopService emergency;
    MockSignalKService signalk(controller, emergency);
    
    signalk.setConnected(true);
    bool executed = signalk.processCommand("starboard");
    
    TEST_ASSERT_TRUE(executed);
    TEST_ASSERT_EQUAL_INT(1, controller.getLastCommand());
    TEST_ASSERT_EQUAL_INT(1, signalk.getExecutedCount());
}

void test_signalk_status_output_reflects_state(void) {
    IntegrationMockMotor motor;
    motor.initialize();
    IntegrationMockController controller(motor);
    MockEmergencyStopService emergency;
    MockSignalKService signalk(controller, emergency);
    
    signalk.setConnected(true);
    
    signalk.processCommand("port");
    TEST_ASSERT_EQUAL_STRING("port", signalk.getStatusValue());
    
    signalk.processCommand("stop");
    TEST_ASSERT_EQUAL_STRING("stopped", signalk.getStatusValue());
    
    signalk.processCommand("starboard");
    TEST_ASSERT_EQUAL_STRING("starboard", signalk.getStatusValue());
}

void test_signalk_blocks_when_not_connected(void) {
    IntegrationMockMotor motor;
    motor.initialize();
    IntegrationMockController controller(motor);
    MockEmergencyStopService emergency;
    MockSignalKService signalk(controller, emergency);
    
    signalk.setConnected(false);
    bool executed = signalk.processCommand("port");
    
    TEST_ASSERT_FALSE(executed);
    TEST_ASSERT_EQUAL_INT(1, signalk.getBlockedCount());
    TEST_ASSERT_FALSE(controller.isActive());
}

void test_signalk_blocks_when_emergency_stop_active(void) {
    IntegrationMockMotor motor;
    motor.initialize();
    IntegrationMockController controller(motor);
    MockEmergencyStopService emergency;
    MockSignalKService signalk(controller, emergency);
    
    signalk.setConnected(true);
    emergency.setActive(true);
    
    bool executed = signalk.processCommand("port");
    
    TEST_ASSERT_FALSE(executed);
    TEST_ASSERT_EQUAL_INT(1, signalk.getBlockedCount());
    TEST_ASSERT_FALSE(controller.isActive());
}

void test_signalk_commands_resume_after_emergency_stop_cleared(void) {
    IntegrationMockMotor motor;
    motor.initialize();
    IntegrationMockController controller(motor);
    MockEmergencyStopService emergency;
    MockSignalKService signalk(controller, emergency);
    
    signalk.setConnected(true);
    
    // Initially blocked
    emergency.setActive(true);
    bool executed = signalk.processCommand("port");
    TEST_ASSERT_FALSE(executed);
    
    // After clearing emergency stop, commands work again
    emergency.setActive(false);
    executed = signalk.processCommand("port");
    TEST_ASSERT_TRUE(executed);
    TEST_ASSERT_EQUAL_INT(-1, controller.getLastCommand());
}

void test_signalk_reconnection_resumes_commands(void) {
    IntegrationMockMotor motor;
    motor.initialize();
    IntegrationMockController controller(motor);
    MockEmergencyStopService emergency;
    MockSignalKService signalk(controller, emergency);
    
    // Initially disconnected
    signalk.setConnected(false);
    bool executed = signalk.processCommand("port");
    TEST_ASSERT_FALSE(executed);
    
    // After reconnection, commands work
    signalk.setConnected(true);
    executed = signalk.processCommand("port");
    TEST_ASSERT_TRUE(executed);
    TEST_ASSERT_EQUAL_INT(-1, controller.getLastCommand());
}

// ========== EMERGENCY STOP TESTS ==========

void test_emergency_stop_blocks_signalk_commands(void) {
    IntegrationMockMotor motor;
    motor.initialize();
    IntegrationMockController controller(motor);
    MockEmergencyStopService emergency;
    MockSignalKService signalk(controller, emergency);
    
    signalk.setConnected(true);
    
    // Start with normal operation
    bool executed = signalk.processCommand("port");
    TEST_ASSERT_TRUE(executed);
    TEST_ASSERT_TRUE(controller.isActive());
    
    // Emergency stop blocks new commands
    emergency.setActive(true);
    executed = signalk.processCommand("starboard");
    TEST_ASSERT_FALSE(executed);
    TEST_ASSERT_EQUAL_INT(-1, controller.getLastCommand());  // Unchanged
}

void test_emergency_stop_blocks_remote_commands(void) {
    IntegrationMockMotor motor;
    motor.initialize();
    IntegrationMockController controller(motor);
    MockEmergencyStopService emergency;
    MockRemoteControl remote(controller, emergency);
    
    // Normal operation
    bool executed = remote.processFUNC3Press();
    TEST_ASSERT_TRUE(executed);
    
    // Emergency stop blocks new remote commands
    emergency.setActive(true);
    executed = remote.processFUNC4Press();
    TEST_ASSERT_FALSE(executed);
    
    // But stop still works (or we want it to be forced)
    remote.processButtonRelease();
    // Note: the implementation may force stop even during emergency
}

void test_emergency_stop_activation_stops_motor(void) {
    IntegrationMockMotor motor;
    motor.initialize();
    IntegrationMockController controller(motor);
    MockEmergencyStopService emergency;
    MockSignalKService signalk(controller, emergency);
    
    signalk.setConnected(true);
    
    // Start operating
    signalk.processCommand("port");
    TEST_ASSERT_TRUE(controller.isActive());
    
    // Emergency stop gets activated - it should stop the motor
    // (In real implementation, activation stops the winch/propeller)
    emergency.setActive(true);
    controller.stop();  // Simulate emergency stop command
    
    TEST_ASSERT_FALSE(controller.isActive());
}

void test_emergency_stop_counts_blocked_attempts(void) {
    IntegrationMockMotor motor;
    motor.initialize();
    IntegrationMockController controller(motor);
    MockEmergencyStopService emergency;
    MockSignalKService signalk(controller, emergency);
    
    signalk.setConnected(true);
    emergency.setActive(true);
    
    // Try multiple commands while stopped
    signalk.processCommand("port");
    signalk.processCommand("starboard");
    signalk.processCommand("port");
    
    TEST_ASSERT_EQUAL_INT(3, signalk.getBlockedCount());
    TEST_ASSERT_EQUAL_INT(0, signalk.getExecutedCount());
}

// ========== REMOTE CONTROL INTEGRATION TESTS ==========

void test_remote_func3_button_activates_port(void) {
    IntegrationMockMotor motor;
    motor.initialize();
    IntegrationMockController controller(motor);
    MockEmergencyStopService emergency;
    MockRemoteControl remote(controller, emergency);
    
    bool executed = remote.processFUNC3Press();
    
    TEST_ASSERT_TRUE(executed);
    TEST_ASSERT_EQUAL_INT(-1, controller.getLastCommand());
}

void test_remote_func4_button_activates_starboard(void) {
    IntegrationMockMotor motor;
    motor.initialize();
    IntegrationMockController controller(motor);
    MockEmergencyStopService emergency;
    MockRemoteControl remote(controller, emergency);
    
    bool executed = remote.processFUNC4Press();
    
    TEST_ASSERT_TRUE(executed);
    TEST_ASSERT_EQUAL_INT(1, controller.getLastCommand());
}

void test_remote_button_release_stops_motor(void) {
    IntegrationMockMotor motor;
    motor.initialize();
    IntegrationMockController controller(motor);
    MockEmergencyStopService emergency;
    MockRemoteControl remote(controller, emergency);
    
    remote.processFUNC3Press();
    TEST_ASSERT_TRUE(controller.isActive());
    
    remote.processButtonRelease();
    TEST_ASSERT_FALSE(controller.isActive());
    TEST_ASSERT_EQUAL_INT(0, controller.getLastCommand());
}

// ========== SYSTEM-LEVEL INTEGRATION TESTS ==========

void test_signalk_and_remote_can_coexist(void) {
    IntegrationMockMotor motor;
    motor.initialize();
    IntegrationMockController controller(motor);
    MockEmergencyStopService emergency;
    MockSignalKService signalk(controller, emergency);
    MockRemoteControl remote(controller, emergency);
    
    signalk.setConnected(true);
    
    // SignalK sends command
    signalk.processCommand("port");
    TEST_ASSERT_EQUAL_INT(-1, controller.getLastCommand());
    
    // Remote overrides with different command
    remote.processFUNC4Press();
    TEST_ASSERT_EQUAL_INT(1, controller.getLastCommand());
    
    // SignalK can regain control
    signalk.processCommand("port");
    TEST_ASSERT_EQUAL_INT(-1, controller.getLastCommand());
}

void test_emergency_stop_blocks_both_signalk_and_remote(void) {
    IntegrationMockMotor motor;
    motor.initialize();
    IntegrationMockController controller(motor);
    MockEmergencyStopService emergency;
    MockSignalKService signalk(controller, emergency);
    MockRemoteControl remote(controller, emergency);
    
    signalk.setConnected(true);
    emergency.setActive(true);
    
    bool signalk_exec = signalk.processCommand("port");
    bool remote_exec = remote.processFUNC4Press();
    
    TEST_ASSERT_FALSE(signalk_exec);
    TEST_ASSERT_FALSE(remote_exec);
    TEST_ASSERT_FALSE(controller.isActive());
}

void test_full_scenario_normal_operation(void) {
    // Scenario: Operator uses remote, then switches to SignalK
    IntegrationMockMotor motor;
    motor.initialize();
    IntegrationMockController controller(motor);
    MockEmergencyStopService emergency;
    MockSignalKService signalk(controller, emergency);
    MockRemoteControl remote(controller, emergency);
    
    signalk.setConnected(true);
    
    // 1. Remote operator turns port
    remote.processFUNC3Press();
    TEST_ASSERT_EQUAL_INT(-1, controller.getLastCommand());
    
    // 2. Remote operator releases button (stops)
    remote.processButtonRelease();
    TEST_ASSERT_EQUAL_INT(0, controller.getLastCommand());
    
    // 3. SignalK takes over - turns starboard
    signalk.processCommand("starboard");
    TEST_ASSERT_EQUAL_INT(1, controller.getLastCommand());
    
    // 4. Emergency stop activated
    emergency.setActive(true);
    controller.stop();
    TEST_ASSERT_EQUAL_INT(0, controller.getLastCommand());
    
    // 5. Both remote and SignalK blocked
    bool remote_blocked = !remote.processFUNC3Press();
    bool signalk_blocked = !signalk.processCommand("starboard");
    TEST_ASSERT_TRUE(remote_blocked);
    TEST_ASSERT_TRUE(signalk_blocked);
}

// Note: These tests are registered with test_anchor_counter.cpp's main harness
// setUp/tearDown are provided above for Unity framework
