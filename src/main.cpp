#include <memory>
// Boilerplate #includes:
#include "sensesp_app_builder.h"
#include "sensesp/signalk/signalk_output.h"
#include "sensesp/ui/config_item.h"
#include "sensesp/ui/ui_controls.h"

// Sensor-specific #includes:
#include "sensesp/sensors/sensor.h"
#include "sensesp/signalk/signalk_value_listener.h"
#include "sensesp/transforms/lambda_transform.h"
#include "sensesp/system/valueconsumer.h"

using namespace sensesp;

// GPIO pins for pulse input and direction
#define PULSE_INPUT_PIN 25  // Change this to your desired GPIO pin
#define DIRECTION_PIN 26    // GPIO pin for direction (HIGH = chain out, LOW = chain in)
#define WINCH_UP_PIN 27     // GPIO pin to control winch UP (chain in)
#define WINCH_DOWN_PIN 14   // GPIO pin to control winch DOWN (chain out)
#define ANCHOR_HOME_PIN 33  // GPIO pin for anchor home position sensor (LOW = anchor home)

// Pulse counter variables
volatile long pulse_count = 0;  // Changed to signed long for up/down counting
unsigned long last_pulse_count = 0;

// Winch control variables
float target_rode_length = -1.0;  // Target length in meters (-1 = no target)
bool winch_active = false;
bool automatic_mode_enabled = false;  // Enable/disable automatic control (defaults to false/manual mode)
SKOutputFloat* auto_mode_output_ptr = nullptr;  // Global pointer to auto mode output

// Interrupt handler for pulse counting with direction
void IRAM_ATTR pulseISR() {
    // Read direction pin: HIGH = increment (chain out), LOW = decrement (chain in)
    // Add a small delay to ensure direction signal has stabilized
    delayMicroseconds(10);
    
    bool direction_high = digitalRead(DIRECTION_PIN);
    
    if (direction_high) {
        pulse_count++;
    } else {
        pulse_count--;
        if (pulse_count < 0) {
            pulse_count = 0;  // Prevent negative values
        }
    }
}

// Winch control functions
void stopWinch() {
    digitalWrite(WINCH_UP_PIN, LOW);
    digitalWrite(WINCH_DOWN_PIN, LOW);
    winch_active = false;
    debugD("Winch stopped");
}

void setWinchUp() {
    // Check if anchor is already home - don't allow retrieval if home
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

// Global configuration for meters per pulse (can be changed via web UI)
float config_meters_per_pulse = 0.1;

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
        // Read the pulse count and convert to meters
        float meters = pulse_count * config_meters_per_pulse;
        
        // Debug: periodically log pulse count and direction state
        static unsigned long last_debug = 0;
        if (millis() - last_debug > 5000) {  // Every 5 seconds
            bool dir_state = digitalRead(DIRECTION_PIN);
            debugD("Pulse count: %ld, Direction pin: %s, Chain length: %.2f m", 
                   pulse_count, dir_state ? "HIGH (out)" : "LOW (in)", meters);
            last_debug = millis();
        }
        
        this->emit(meters);
        
        // Check if anchor reached home position while retrieving
        if (winch_active && digitalRead(WINCH_UP_PIN) == HIGH) {
            if (digitalRead(ANCHOR_HOME_PIN) == LOW) {
                stopWinch();
                pulse_count = 0;  // Reset counter when anchor reaches home
                debugD("Anchor home position reached - stopped and reset counter");
                return;
            }
        }
        
        // Also check if anchor is home while not winching (manual operation or startup)
        static bool was_home = false;
        bool is_home = (digitalRead(ANCHOR_HOME_PIN) == LOW);
        if (is_home && !was_home) {
            pulse_count = 0;  // Reset counter when anchor first detected at home
            debugD("Anchor detected at home position - counter reset");
        }
        was_home = is_home;
        
        // Check if we need to control the winch to reach target length
        if (automatic_mode_enabled && target_rode_length >= 0) {
            float tolerance = config_meters_per_pulse * 2.0;  // 2 pulse tolerance
            
            if (fabs(meters - target_rode_length) <= tolerance) {
                // Target reached
                if (winch_active) {
                    stopWinch();
                    automatic_mode_enabled = false;  // Turn off automatic mode when target reached
                    if (auto_mode_output_ptr) {
                        auto_mode_output_ptr->set_input(0.0);  // Update status to disabled (0.0 = disabled)
                    }
                    debugD("Target rode length reached: %.2f m - automatic mode disabled", meters);
                }
            } else if (meters < target_rode_length) {
                // Need more chain out
                if (digitalRead(WINCH_DOWN_PIN) == LOW) {
                    setWinchDown();
                }
            } else {
                // Need chain in
                if (digitalRead(WINCH_UP_PIN) == LOW) {
                    setWinchUp();
                }
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

    // Add SignalK value listener for manual windlass control - single path with three states
    // Use IntSKListener: 1 = UP, 0 = STOP, -1 = DOWN
    auto* manual_control_output = new SKOutputInt("navigation.anchor.manualControlStatus", "/manual_control_status/sk_path");
    auto* manual_control_listener = new IntSKListener("navigation.anchor.manualControl");
    
    manual_control_listener->connect_to(new LambdaTransform<int, int>([](int command) {
        if (!automatic_mode_enabled) {
            if (command == 1) {
                setWinchUp();
                debugD("Manual windlass UP activated");
            } else if (command == -1) {
                setWinchDown();
                debugD("Manual windlass DOWN activated");
            } else {
                stopWinch();
                debugD("Manual windlass STOPPED");
            }
        } else {
            debugD("Cannot manual control - automatic mode is enabled");
        }
        return command;
    }))->connect_to(manual_control_output);

    // Add SignalK value listener to enable/disable automatic mode - listens on command path
    // Using FloatSKListener with numeric values due to BoolSKListener bug (always returns true)
    // Send numeric values from Node-RED: any value > 0.5 = enable, <= 0.5 = disable
    debugD("Creating automatic mode listener on path: navigation.anchor.automaticModeCommand");
    auto* auto_mode_output = new SKOutputFloat("navigation.anchor.automaticModeStatus", "/automatic_mode_status/sk_path");
    auto_mode_output_ptr = auto_mode_output;  // Store globally for access from update loop
    auto* auto_mode_listener = new FloatSKListener("navigation.anchor.automaticModeCommand");
    
    auto_mode_listener->connect_to(new LambdaTransform<float, float>([pulse_counter](float value) {
        debugD("*** AUTOMATIC MODE LISTENER TRIGGERED ***");
        debugD("    Raw value received: %.10f", value);
        debugD("    Hex representation: 0x%08X", *(uint32_t*)&value);
        bool enable = (value > 0.5);  // Values > 0.5 are true, <= 0.5 are false
        debugD("    Interpreted as: %s (current state was: %s)", 
               enable ? "ENABLED" : "DISABLED",
               automatic_mode_enabled ? "ENABLED" : "DISABLED");
        automatic_mode_enabled = enable;
        if (enable) {
            debugD("Automatic windlass control ENABLED");
            // If target already set when enabling automatic mode, start winch immediately
            if (target_rode_length >= 0) {
                float current_length = pulse_count * pulse_counter->get_meters_per_pulse();
                debugD("    Target already set to %.2f m, current: %.2f m - starting winch", 
                       target_rode_length, current_length);
                if (current_length < target_rode_length) {
                    setWinchDown();  // Need more chain out
                } else if (current_length > target_rode_length) {
                    setWinchUp();    // Need chain in
                } else {
                    debugD("    Already at target length");
                }
            }
        } else {
            debugD("Automatic windlass control DISABLED");
            stopWinch();  // Stop winch when disabling automatic mode
        }
        return value;  // Return the float value for status output
    }))->connect_to(auto_mode_output);

    // Add SignalK value listener for target rode length - listens on command path
    debugD("Creating target rode listener on path: navigation.anchor.targetRodeCommand");
    auto* target_output = new SKOutputFloat("navigation.anchor.targetRodeStatus", "/target_rode_status/sk_path");
    auto* target_listener = new FloatSKListener("navigation.anchor.targetRodeCommand");
    
    // When target rode length is received, save it and start if automatic mode is enabled
    target_listener->connect_to(new LambdaTransform<float, float>([pulse_counter](float target_length) {
        debugD("*** TARGET RODE LISTENER TRIGGERED ***");
        debugD("    Raw value received: %.10f", target_length);
        debugD("    Hex representation: 0x%08X", *(uint32_t*)&target_length);
        
        target_rode_length = target_length;
        float current_length = pulse_count * pulse_counter->get_meters_per_pulse();
        
        debugD("    Target set to: %.2f m (current: %.2f m)", target_length, current_length);
        debugD("    Automatic mode: %s", automatic_mode_enabled ? "ENABLED" : "DISABLED");
        
        // Only start winch if automatic mode is enabled
        if (automatic_mode_enabled && target_length >= 0) {
            // Immediately start winch in correct direction
            if (current_length < target_length) {
                setWinchDown();  // Need more chain out
            } else if (current_length > target_length) {
                setWinchUp();    // Need chain in
            } else {
                debugD("Already at target length");
            }
        }
        
        return target_length;
    }))->connect_to(target_output);

    // Publish initial states so SignalK recognizes device as source for these paths
    event_loop()->onDelay(2000, []() {
        // These initial publishes establish the device as the data source
        debugD("Publishing initial control states to SignalK");
    });
    
    debugD("Anchor chain counter initialized - Pulse: GPIO %d, Direction: GPIO %d", PULSE_INPUT_PIN, DIRECTION_PIN);
    debugD("Winch control initialized - UP: GPIO %d, DOWN: GPIO %d", WINCH_UP_PIN, WINCH_DOWN_PIN);
    debugD("Anchor home sensor: GPIO %d", ANCHOR_HOME_PIN);
    debugD("System started in MANUAL mode (automatic mode disabled)");
}

void loop()
{
    static auto event_loop = sensesp_app->get_event_loop();
    event_loop->tick();
}
