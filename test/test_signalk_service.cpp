// Unit tests for SignalKService
// Tests SignalK listener setup and output management

#include <unity.h>
#include <Arduino.h>
#include <stdexcept>
#include "pin_config.h"
#include "mock_sensesp.h"

// Mock implementations (normally from services)
class MockStateManager {
public:
    bool isEmergencyStopActive() const { return emergency_stop_active_; }
    bool areCommandsAllowed() const { return commands_allowed_; }
    
    void setPulseCount(int count) { pulse_count_ = count; }
    void setRodeLength(float length) { rode_length_ = length; }
    void setAutoModeEnabled(bool enabled) { auto_mode_enabled_ = enabled; }
    void setAutoModeTarget(float target) { auto_mode_target_ = target; }
    
    int getPulseCount() const { return pulse_count_; }
    float getRodeLength() const { return rode_length_; }
    
    int pulse_count_ = 0;
    float rode_length_ = 0.0f;
    bool emergency_stop_active_ = false;
    bool commands_allowed_ = true;
    bool auto_mode_enabled_ = false;
    float auto_mode_target_ = 0.0f;
};

class MockAnchorWinchController {
public:
    void moveUp() { is_active_ = true; direction_ = 1; }
    void moveDown() { is_active_ = true; direction_ = -1; }
    void stop() { is_active_ = false; direction_ = 0; }
    bool isActive() const { return is_active_; }
    
    bool is_active_ = false;
    int direction_ = 0;
};

class MockHomeSensor {
public:
    bool getHomeState() const { return at_home_; }
    bool at_home_ = true;
};

class MockAutomaticModeController {
public:
    void setEnabled(bool enabled) { enabled_ = enabled; }
    void setTargetLength(float length) { target_length_ = length; }
    bool isEnabled() const { return enabled_; }
    
    bool enabled_ = false;
    float target_length_ = 0.0f;
};

class MockEmergencyStopService {
public:
    void setActive(bool active, const char* source) { active_ = active; }
    bool active_ = false;
};

class MockPulseCounterService {
public:
    void start() { started_ = true; }
    bool started_ = false;
};

// SignalK service mock (simplified for testing)
class TestSignalKService {
public:
    TestSignalKService(MockStateManager& state_manager,
                      MockAnchorWinchController& winch_controller,
                      MockHomeSensor& home_sensor,
                      MockAutomaticModeController* auto_mode_controller,
                      MockEmergencyStopService* emergency_stop_service,
                      MockPulseCounterService* pulse_counter_service)
        : state_manager_(state_manager),
          winch_controller_(winch_controller),
          home_sensor_(home_sensor),
          auto_mode_controller_(auto_mode_controller),
          emergency_stop_service_(emergency_stop_service),
          pulse_counter_service_(pulse_counter_service) {}
    
    void initialize() {
        setupRodeLengthOutput();
        setupEmergencyStopBindings();
        setupManualControlBindings();
    }
    
    void setupRodeLengthOutput() {
        rode_output_ = new sensesp::SKOutputFloat("navigation.anchor.rodeLength", "");
        rode_output_->set_metadata(new sensesp::SKMetadata("m"));
    }
    
    void setupEmergencyStopBindings() {
        emergency_stop_status_value_ = new sensesp::ObservableValue<bool>();
        emergency_stop_status_value_->set(false);
    }
    
    void setupManualControlBindings() {
        manual_control_output_ = new sensesp::SKOutputString("navigation.anchor.windlass.state", "");
        manual_control_output_->set_input("stopped");
    }
    
    void updateRodeLength(float length) {
        if (rode_output_) {
            rode_output_->set_input(length);
        }
    }
    
    void triggerEmergencyStop(bool active) {
        if (emergency_stop_service_) {
            emergency_stop_service_->setActive(active, "test");
        }
        if (emergency_stop_status_value_) {
            emergency_stop_status_value_->set(active);
            emergency_stop_status_value_->notify();
        }
    }
    
    void triggerManualControl(const std::string& command) {
        if (command == "up") {
            winch_controller_.moveUp();
        } else if (command == "down") {
            winch_controller_.moveDown();
        } else {
            winch_controller_.stop();
        }
        if (manual_control_output_) {
            manual_control_output_->set_input(command == "up" || command == "down" ? command : "stopped");
        }
    }
    
    // Accessors for testing
    sensesp::SKOutputFloat* getRodeOutput() { return rode_output_; }
    sensesp::SKOutputString* getManualControlOutput() { return manual_control_output_; }
    sensesp::ObservableValue<bool>* getEmergencyStopStatus() { return emergency_stop_status_value_; }
    
private:
    MockStateManager& state_manager_;
    MockAnchorWinchController& winch_controller_;
    MockHomeSensor& home_sensor_;
    MockAutomaticModeController* auto_mode_controller_;
    MockEmergencyStopService* emergency_stop_service_;
    MockPulseCounterService* pulse_counter_service_;
    
    sensesp::SKOutputFloat* rode_output_ = nullptr;
    sensesp::SKOutputString* manual_control_output_ = nullptr;
    sensesp::ObservableValue<bool>* emergency_stop_status_value_ = nullptr;
};

// ========== TESTS ==========

void test_signalk_service_initialization() {
    MockStateManager state_mgr;
    MockAnchorWinchController winch;
    MockHomeSensor home;
    MockAutomaticModeController auto_mode;
    MockEmergencyStopService emergency;
    MockPulseCounterService pulse;
    
    TestSignalKService service(state_mgr, winch, home, &auto_mode, &emergency, &pulse);
    service.initialize();
    
    TEST_ASSERT_NOT_NULL(service.getRodeOutput());
    TEST_ASSERT_NOT_NULL(service.getManualControlOutput());
    TEST_ASSERT_NOT_NULL(service.getEmergencyStopStatus());
    TEST_ASSERT_EQUAL_STRING("navigation.anchor.rodeLength", service.getRodeOutput()->sk_path_);
    TEST_ASSERT_EQUAL_STRING("navigation.anchor.windlass.state", service.getManualControlOutput()->sk_path_);
}

void test_rode_length_output() {
    MockStateManager state_mgr;
    MockAnchorWinchController winch;
    MockHomeSensor home;
    MockAutomaticModeController auto_mode;
    MockEmergencyStopService emergency;
    MockPulseCounterService pulse;
    
    TestSignalKService service(state_mgr, winch, home, &auto_mode, &emergency, &pulse);
    service.initialize();
    
    // Update rode length
    service.updateRodeLength(42.5f);
    
    TEST_ASSERT_EQUAL_FLOAT(42.5f, service.getRodeOutput()->get_input());
}

void test_emergency_stop_status() {
    MockStateManager state_mgr;
    MockAnchorWinchController winch;
    MockHomeSensor home;
    MockAutomaticModeController auto_mode;
    MockEmergencyStopService emergency;
    MockPulseCounterService pulse;
    
    TestSignalKService service(state_mgr, winch, home, &auto_mode, &emergency, &pulse);
    service.initialize();
    
    // Trigger emergency stop
    service.triggerEmergencyStop(true);
    
    TEST_ASSERT_TRUE(service.getEmergencyStopStatus()->get());
    TEST_ASSERT_TRUE(emergency.active_);
}

void test_manual_control_up() {
    MockStateManager state_mgr;
    MockAnchorWinchController winch;
    MockHomeSensor home;
    MockAutomaticModeController auto_mode;
    MockEmergencyStopService emergency;
    MockPulseCounterService pulse;
    
    TestSignalKService service(state_mgr, winch, home, &auto_mode, &emergency, &pulse);
    service.initialize();
    
    service.triggerManualControl("up");
    
    TEST_ASSERT_TRUE(winch.isActive());
    TEST_ASSERT_EQUAL_INT(1, winch.direction_);
    TEST_ASSERT_EQUAL_STRING("up", service.getManualControlOutput()->get_input().c_str());
}

void test_manual_control_down() {
    MockStateManager state_mgr;
    MockAnchorWinchController winch;
    MockHomeSensor home;
    MockAutomaticModeController auto_mode;
    MockEmergencyStopService emergency;
    MockPulseCounterService pulse;
    
    TestSignalKService service(state_mgr, winch, home, &auto_mode, &emergency, &pulse);
    service.initialize();
    
    service.triggerManualControl("down");
    
    TEST_ASSERT_TRUE(winch.isActive());
    TEST_ASSERT_EQUAL_INT(-1, winch.direction_);
    TEST_ASSERT_EQUAL_STRING("down", service.getManualControlOutput()->get_input().c_str());
}

void test_manual_control_stop() {
    MockStateManager state_mgr;
    MockAnchorWinchController winch;
    MockHomeSensor home;
    MockAutomaticModeController auto_mode;
    MockEmergencyStopService emergency;
    MockPulseCounterService pulse;
    
    TestSignalKService service(state_mgr, winch, home, &auto_mode, &emergency, &pulse);
    service.initialize();
    
    service.triggerManualControl("up");
    TEST_ASSERT_TRUE(winch.isActive());
    
    service.triggerManualControl("stop");
    
    TEST_ASSERT_FALSE(winch.isActive());
    TEST_ASSERT_EQUAL_INT(0, winch.direction_);
    TEST_ASSERT_EQUAL_STRING("stopped", service.getManualControlOutput()->get_input().c_str());
}
