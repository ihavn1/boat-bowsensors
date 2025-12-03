#include <memory>
// Boilerplate #includes:
#include "sensesp_app_builder.h"
#include "sensesp/signalk/signalk_output.h"
#include "sensesp/ui/config_item.h"

// Sensor-specific #includes:
#include "sensesp/sensors/sensor.h"
#include "sensesp/signalk/signalk_value_listener.h"
#include "sensesp/transforms/lambda_transform.h"
#include "sensesp/system/valueconsumer.h"

using namespace sensesp;

// GPIO pins for pulse input and direction
#define PULSE_INPUT_PIN 25  // Change this to your desired GPIO pin
#define DIRECTION_PIN 26    // GPIO pin for direction (HIGH = chain out, LOW = chain in)

// Pulse counter variables
volatile long pulse_count = 0;  // Changed to signed long for up/down counting
unsigned long last_pulse_count = 0;

// Interrupt handler for pulse counting with direction
void IRAM_ATTR pulseISR() {
    // Read direction pin: HIGH = increment (chain out), LOW = decrement (chain in)
    if (digitalRead(DIRECTION_PIN) == HIGH) {
        pulse_count++;
    } else {
        pulse_count--;
        if (pulse_count < 0) {
            pulse_count = 0;  // Prevent negative values
        }
    }
}

// Custom sensor class to read pulse count and convert to meters
class PulseCounter : public FloatSensor {
private:
    float meters_per_pulse_;
    uint read_delay_;

public:
    PulseCounter(float meters_per_pulse = 0.1, uint read_delay = 100, String config_path = "")
        : FloatSensor(config_path), meters_per_pulse_(meters_per_pulse), read_delay_(read_delay) {
        // Set up repeating task to read pulse count
        event_loop()->onRepeat(read_delay_, [this]() { this->update(); });
    }

    void update() {
        // Read the pulse count and convert to meters
        float meters = pulse_count * meters_per_pulse_;
        this->emit(meters);
    }

    // Method to reset the counter
    void reset() {
        pulse_count = 0;
        debugD("Pulse counter reset to 0");
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

    // Create the pulse counter sensor
    auto* pulse_counter = new PulseCounter(0.1, 100, "/pulse_counter/config");
    
    // Connect to SignalK output for anchor chain length
    pulse_counter->connect_to(new SKOutputFloat("navigation.anchor.currentRode", "/pulse_counter/sk_path"));

    // Add SignalK listener to reset the counter based on a digital input
    // Subscribe to a SignalK path that will trigger the reset
    auto* reset_listener = new BoolSKListener("navigation.anchor.resetRode");
    
    // Connect the reset listener to reset the counter when true is received
    reset_listener->connect_to(new LambdaTransform<bool, bool>([pulse_counter](bool reset_signal) {
        if (reset_signal) {
            pulse_counter->reset();
        }
        return reset_signal;
    }));

    debugD("Anchor chain counter initialized - Pulse: GPIO %d, Direction: GPIO %d", PULSE_INPUT_PIN, DIRECTION_PIN);
}

void loop()
{
    static auto event_loop = sensesp_app->get_event_loop();
    event_loop->tick();
}
