#include "services/BoatBowControlApp.h"
#include "services/SignalKService.h"

// Global app instance (needed for ISR access)
static BoatBowControlApp* g_app = nullptr;

void emergencyStopChangedThunk(bool is_active, const char* reason) {
    if (g_app) {
        g_app->onEmergencyStopChanged(is_active, reason);
    }
}

// Interrupt Service Routine - Pulse Counter with Direction Sensing
// Called from ISR context - must be very fast
void IRAM_ATTR pulseISR() {
    if (!g_app) return;
    
    delayMicroseconds(10);  // Allow direction signal to stabilize
    
    if (digitalRead(PinConfig::DIRECTION)) {
        g_app->getStateManager().incrementPulse();  // Chain out
    } else {
        g_app->getStateManager().decrementPulse();  // Chain in
    }
}

BoatBowControlApp::BoatBowControlApp()
    : motor_(),
      home_sensor_impl_(),
      winch_controller_(motor_, home_sensor_impl_),
      home_sensor_(home_sensor_impl_) {
    g_app = this;  // Store global pointer for ISR
}

void BoatBowControlApp::initialize() {
    initializeHardware();
    initializeControllers();
    initializeServices();
    attachPulseISR();

    debugD("=== Boat Bow Control App Initialized ===");
    debugD("Pulse input: GPIO %d, Direction: GPIO %d", 
           PinConfig::PULSE_INPUT, PinConfig::DIRECTION);
    debugD("Winch: UP=GPIO %d, DOWN=GPIO %d", 
           PinConfig::WINCH_UP, PinConfig::WINCH_DOWN);
    debugD("Home sensor: GPIO %d", PinConfig::ANCHOR_HOME);
    debugD("Bow propeller: PORT=GPIO %d, STARBOARD=GPIO %d", 
           PinConfig::BOW_PORT, PinConfig::BOW_STARBOARD);
    debugD("Remote inputs: UP=GPIO %d, DOWN=GPIO %d, FUNC3=GPIO %d, FUNC4=GPIO %d",
           PinConfig::REMOTE_UP, PinConfig::REMOTE_DOWN,
           PinConfig::REMOTE_FUNC3, PinConfig::REMOTE_FUNC4);
}

void BoatBowControlApp::startSignalK() {
    if (!signalk_service_) {
        debugD("ERROR: SignalK service not initialized!");
        return;
    }

    signalk_service_->startConnectionMonitoring();
    debugD("SignalK integration started - waiting for connection...");
}

void BoatBowControlApp::processInputs() {
    // Process physical remote control inputs
    if (remote_control_) {
        remote_control_->processInputs();
    }
    
    // Tick the SensESP event loop
    event_loop()->tick();
}

void BoatBowControlApp::initializeHardware() {
    // SAFETY FIRST: Initialize all hardware to safe inactive state
    
    // Initialize motor first
    motor_.initialize();
    home_sensor_impl_.initialize();
    
    // Initialize bow propeller motor
    bow_propeller_motor_.initialize();

    debugD("Hardware initialized - all outputs inactive");
}

void BoatBowControlApp::initializeControllers() {
    // Initialize automatic mode controller
    auto_mode_controller_ = new AutomaticModeController(winch_controller_, home_sensor_);
    // Note: meters_per_pulse is set by main.cpp from SensESP ConfigItem after initialize()
    auto_mode_controller_->setTolerance(state_manager_.getMetersPerPulse() * 2.0);
    
    // Initialize remote control
    remote_control_ = new RemoteControl(state_manager_, winch_controller_);
    remote_control_->initialize();
    
    // Initialize bow propeller controller
    bow_propeller_controller_ = new BowPropellerController(bow_propeller_motor_);
    
    // Wire bow propeller to remote control
    remote_control_->setBowPropellerController(bow_propeller_controller_);

    debugD("Controllers initialized");
}

void BoatBowControlApp::initializeServices() {
    // Initialize emergency stop service (without callback - SignalK will handle updates)
    emergency_stop_service_ = new EmergencyStopService(state_manager_, winch_controller_);
    // Wire bow propeller to emergency stop service
    emergency_stop_service_->setBowPropellerController(bow_propeller_controller_);
    // Use a thunk to forward the callback to the app instance
    emergency_stop_service_->onStateChange(emergencyStopChangedThunk);
    
    // Initialize pulse counter service
    pulse_counter_service_ = new PulseCounterService(state_manager_, winch_controller_, 
                                                      home_sensor_, 100);
    pulse_counter_service_->initialize();
    
    // Initialize SignalK service with bow propeller controller
    signalk_service_ = new SignalKService(state_manager_, winch_controller_, home_sensor_,
                                         auto_mode_controller_, emergency_stop_service_,
                                         pulse_counter_service_, bow_propeller_controller_);
    signalk_service_->initialize();

    debugD("Services initialized");
}

void BoatBowControlApp::attachPulseISR() {
    // Configure the pulse input pin and direction pin
    pinMode(PinConfig::PULSE_INPUT, INPUT_PULLUP);
    pinMode(PinConfig::DIRECTION, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(PinConfig::PULSE_INPUT), pulseISR, RISING);

    debugD("Pulse ISR attached to GPIO %d", PinConfig::PULSE_INPUT);
}

void BoatBowControlApp::onEmergencyStopChanged(bool is_active, const char* reason) {
    // Update SignalK status when emergency stop state changes
    if (signalk_service_) {
        signalk_service_->publishEmergencyStopStatus(is_active);
    }
    
    if (is_active) {
        debugD("EMERGENCY STOP ACTIVATED (%s)", reason);
    } else {
        debugD("Emergency stop deactivated");
    }
}
