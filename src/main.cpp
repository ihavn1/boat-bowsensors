#include <memory>
#include "sensesp_app_builder.h"
#include "sensesp/signalk/signalk_output.h"
#include "sensesp/ui/config_item.h"
#include "sensesp/ui/ui_controls.h"
#include "sensesp/sensors/sensor.h"
#include "sensesp/signalk/signalk_value_listener.h"
#include "sensesp/transforms/lambda_transform.h"
#include "sensesp/system/valueconsumer.h"

using namespace sensesp;

// GPIO Pin Definitions
#define PULSE_INPUT_PIN 25  // Pulse input from chain counter sensor
#define DIRECTION_PIN 26    // Direction sensing (HIGH = chain out, LOW = chain in)
#define WINCH_UP_PIN 27     // Winch control - UP (retrieve chain)
#define WINCH_DOWN_PIN 14   // Winch control - DOWN (deploy chain)
#define ANCHOR_HOME_PIN 33  // Anchor home position sensor (LOW when home)

// Global Variables
volatile long pulse_count = 0;              // Bidirectional pulse counter
float target_rode_length = -1.0;            // Target length in meters (-1 = no target)
bool winch_active = false;                  // Winch operation state
bool automatic_mode_enabled = false;        // Automatic control mode (false = manual mode)
float config_meters_per_pulse = 0.1;        // Configurable conversion factor
SKOutputFloat* auto_mode_output_ptr = nullptr;  // For updating auto mode status

// Interrupt Service Routine - Pulse Counter with Direction Sensing
void IRAM_ATTR pulseISR() {
    delayMicroseconds(10);  // Allow direction signal to stabilize
    
    if (digitalRead(DIRECTION_PIN)) {
        pulse_count++;  // Chain out
    } else {
        pulse_count--;  // Chain in
        if (pulse_count < 0) {
            pulse_count = 0;  // Prevent negative values
        }
    }
}

// Winch Control Functions
void stopWinch() {
    digitalWrite(WINCH_UP_PIN, LOW);
    digitalWrite(WINCH_DOWN_PIN, LOW);
    winch_active = false;
    debugD("Winch stopped");
}

void setWinchUp() {
    if (digitalRead(ANCHOR_HOME_PIN) == LOW) {
        debugD("Anchor already home - cannot retrieve further");
        stopWinch();
        return;
    }
    digitalWrite(WINCH_DOWN_PIN, LOW);
    digitalWrite(WINCH_UP_PIN, HIGH);
    winch_active = true;
    debugD("Winch UP activated");
}

void setWinchDown() {
    digitalWrite(WINCH_UP_PIN, LOW);
    digitalWrite(WINCH_DOWN_PIN, HIGH);
    winch_active = true;
    debugD("Winch DOWN activated");
}

// Custom sensor class to read pulse count and convert to meters
class PulseCounter : public FloatSensor {
private:
    uint read_delay_;

public:
    PulseCounter(uint read_delay = 100, String config_path = "")
        : FloatSensor(config_path), read_delay_(read_delay) {
        // Set up repeating task to read pulse count
        event_loop()->onRepeat(read_delay_, [this]() { this->update(); });
    }

    void update() {
        float meters = pulse_count * config_meters_per_pulse;
        this->emit(meters);
        
        // Periodic debug output
        static unsigned long last_debug = 0;
        if (millis() - last_debug > 5000) {
            bool dir_state = digitalRead(DIRECTION_PIN);
            debugD("Pulse count: %ld, Direction: %s, Chain: %.2f m", 
                   pulse_count, dir_state ? "OUT" : "IN", meters);
            last_debug = millis();
        }
        
        // Home position detection and auto-reset
        static bool was_home = false;
        bool is_home = (digitalRead(ANCHOR_HOME_PIN) == LOW);
        
        if (is_home) {
            if (winch_active && digitalRead(WINCH_UP_PIN) == HIGH) {
                stopWinch();
                debugD("Anchor home reached - stopped and reset");
            }
            if (!was_home) {
                pulse_count = 0;
                debugD("Anchor at home - counter reset");
            }
        }
        was_home = is_home;
        
        // Automatic mode control logic
        if (automatic_mode_enabled && target_rode_length >= 0) {
            float tolerance = config_meters_per_pulse * 2.0;
            
            if (fabs(meters - target_rode_length) <= tolerance) {
                if (winch_active) {
                    stopWinch();
                    automatic_mode_enabled = false;
                    if (auto_mode_output_ptr) {
                        auto_mode_output_ptr->set_input(0.0);
                    }
                    debugD("Target %.2f m reached - automatic mode disabled", meters);
                }
            } else if (meters < target_rode_length && digitalRead(WINCH_DOWN_PIN) == LOW) {
                setWinchDown();
            } else if (meters > target_rode_length && digitalRead(WINCH_UP_PIN) == LOW) {
                setWinchUp();
            }
        }
    }

    // Method to reset the counter
    void reset() {
        pulse_count = 0;
        stopWinch();  // Stop winch when resetting
        target_rode_length = -1.0;  // Clear target
        debugD("Pulse counter reset to 0");
    }

    float get_meters_per_pulse() const {
        return config_meters_per_pulse;
    }
};

void setup()
{
    // put your setup code here, to run once:
    SetupLogging();

    // Create the global SensESPApp() object
    SensESPAppBuilder builder;
    sensesp_app = builder
                      .set_hostname("bow-sensors")
                      ->get_app();

    // Configure the pulse input pin and direction pin
    pinMode(PULSE_INPUT_PIN, INPUT_PULLUP);
    pinMode(DIRECTION_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(PULSE_INPUT_PIN), pulseISR, RISING);

    // Configure anchor home position sensor
    pinMode(ANCHOR_HOME_PIN, INPUT_PULLUP);

    // Configure winch control output pins
    pinMode(WINCH_UP_PIN, OUTPUT);
    pinMode(WINCH_DOWN_PIN, OUTPUT);
    digitalWrite(WINCH_UP_PIN, LOW);
    digitalWrite(WINCH_DOWN_PIN, LOW);

    // Add configurable meters per pulse value
    String meters_per_pulse_path = "/anchor/meters_per_pulse";
    auto* meters_per_pulse_config = new NumberConfig(config_meters_per_pulse, meters_per_pulse_path);
    ConfigItem(meters_per_pulse_config)
        ->set_title("Meters per Pulse")
        ->set_description("Distance in meters that each pulse represents")
        ->set_sort_order(100);
    
    // Create the pulse counter sensor (update every 1000ms)
    auto* pulse_counter = new PulseCounter(1000, "/pulse_counter/config");
    
    // Connect to SignalK output for anchor chain length
    pulse_counter->connect_to(new SKOutputFloat("navigation.anchor.currentRode", "/pulse_counter/sk_path"));

    // Add SignalK value listener to reset the counter (receives PUT via delta)
    auto* reset_listener = new BoolSKListener("navigation.anchor.resetRode");
    reset_listener->connect_to(new LambdaConsumer<bool>([pulse_counter](bool reset_signal) {
        if (reset_signal) {
            pulse_counter->reset();
        }
    }));

    // Manual Windlass Control: Single path with three states (1=UP, 0=STOP, -1=DOWN)
    auto* manual_control_output = new SKOutputInt("navigation.anchor.manualControlStatus", "/manual_control_status/sk_path");
    auto* manual_control_listener = new IntSKListener("navigation.anchor.manualControl");
    
    manual_control_listener->connect_to(new LambdaTransform<int, int>([](int command) {
        if (!automatic_mode_enabled) {
            if (command == 1) {
                setWinchUp();
            } else if (command == -1) {
                setWinchDown();
            } else {
                stopWinch();
            }
            debugD("Manual control: %s", command == 1 ? "UP" : (command == -1 ? "DOWN" : "STOP"));
        } else {
            debugD("Manual control blocked - automatic mode active");
        }
        return command;
    }))->connect_to(manual_control_output);

    // Automatic Mode Control: Enable/disable automatic windlass control
    // Using FloatSKListener (value > 0.5 = enable, <= 0.5 = disable)
    auto* auto_mode_output = new SKOutputFloat("navigation.anchor.automaticModeStatus", "/automatic_mode_status/sk_path");
    auto_mode_output_ptr = auto_mode_output;
    auto* auto_mode_listener = new FloatSKListener("navigation.anchor.automaticModeCommand");
    
    auto_mode_listener->connect_to(new LambdaTransform<float, float>([pulse_counter](float value) {
        bool enable = (value > 0.5);
        automatic_mode_enabled = enable;
        
        if (enable) {
            debugD("Automatic mode ENABLED");
            // If target already armed, start winch immediately
            if (target_rode_length >= 0) {
                float current = pulse_count * pulse_counter->get_meters_per_pulse();
                debugD("Target armed: %.2f m, current: %.2f m", target_rode_length, current);
                
                if (current < target_rode_length) {
                    setWinchDown();
                } else if (current > target_rode_length) {
                    setWinchUp();
                } else {
                    debugD("Already at target");
                }
            }
        } else {
            debugD("Automatic mode DISABLED");
            stopWinch();
        }
        return value;
    }))->connect_to(auto_mode_output);

    // Target Rode Length: Arm target for automatic mode
    auto* target_output = new SKOutputFloat("navigation.anchor.targetRodeStatus", "/target_rode_status/sk_path");
    auto* target_listener = new FloatSKListener("navigation.anchor.targetRodeCommand");
    
    target_listener->connect_to(new LambdaTransform<float, float>([pulse_counter](float target) {
        target_rode_length = target;
        float current = pulse_count * pulse_counter->get_meters_per_pulse();
        
        debugD("Target armed: %.2f m (current: %.2f m)", target, current);
        
        // Start winch immediately if automatic mode already enabled
        if (automatic_mode_enabled && target >= 0) {
            if (current < target) {
                setWinchDown();
            } else if (current > target) {
                setWinchUp();
            } else {
                debugD("Already at target");
            }
        }
        
        return target;
    }))->connect_to(target_output);

    debugD("=== Anchor Chain Counter System ===");
    debugD("Pulse input: GPIO %d, Direction: GPIO %d", PULSE_INPUT_PIN, DIRECTION_PIN);
    debugD("Winch control: UP=GPIO %d, DOWN=GPIO %d", WINCH_UP_PIN, WINCH_DOWN_PIN);
    debugD("Home sensor: GPIO %d", ANCHOR_HOME_PIN);
    debugD("Mode: MANUAL (automatic disabled)");
}

void loop()
{
    static auto event_loop = sensesp_app->get_event_loop();
    event_loop->tick();
}
