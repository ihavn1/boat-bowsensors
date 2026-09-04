# Copilot Instructions

This document provides guidance for AI agents to effectively contribute to the `boat.bow-ecu` codebase.

## 1. The Big Picture: Architecture & Core Concepts

This project is an ESP32-based **boat control system** supporting dual subsystems:
- **Anchor Windlass Control** - Chain deployment/retrieval with automatic positioning
- **Bow Thruster Control** - Port/starboard directional control for maneuvering
- **Unified Emergency Stop** - Atomic safety control affecting both systems

It's built on the **SensESP framework**, which uses a reactive, dataflow-oriented model.

### Core Architecture

**Main Application** (`src/main.cpp` and `include/services/BoatBowControlApp.h`)
- **BoatBowControlApp**: Main orchestrator initializing all hardware controllers and services
- Coordinates: Anchor windlass, bow thruster, remote control, emergency stop, SignalK integration

**Anchor Windlass System**
- **AnchorWinchController** - Windlass motor control with home sensor safety (active-LOW relays)
- **HomeSensor** - Anchor home position detection (GPIO 33)
- **AutomaticModeController** - Automatic positioning to reach target rode length
- **PulseCounterService** - Reads pulse counts (GPIO 25), converts to meters, coordinates controllers

**Bow Thruster System** (New)
- **BowPropellerController** - High-level thruster command interface (PORT/STARBOARD/STOP)
- **ESP32BowPropellerMotor** - GPIO hardware abstraction (GPIO 4/5, active-LOW relays)
- Mutual exclusion: Never both directions active simultaneously

**Control Input Layer**
- **RemoteControl** - Physical buttons (GPIO 12/13 for anchor, GPIO 15/16 for bow)
  - All inputs are deadman switch (stop when released)
  - Physical buttons take priority over SignalK commands
- **SignalKService** - SignalK server communication (both command and status)
  - Anchor paths: `navigation.anchor.*` 
  - Bow paths: `propulsion.bowThruster.*`
  - Emergency stop paths: `navigation.bow.ecu.emergencyStop*`

**Safety Layer**
- **EmergencyStopService** - Unified emergency stop for all systems
  - Blocks all commands when active
  - Affects both anchor and bow systems atomically
  - Can be triggered by SignalK, physical remote, or system failure

### Control Hierarchy

Each command goes through a safety hierarchy:
1. Emergency stop check - if active, STOP everything
2. Connection validation - if SignalK disconnected >5s, block new commands
3. Subsystem-specific safety checks (home sensor for anchor, mutual exclusion for bow)
4. Physical remote takes priority over SignalK

### SOLID Principles Applied

- **SRP**: Each class has single responsibility (controller, sensor, service)
- **OCP**: Open for extension (new subsystems/services) without modifying existing code
- **DIP**: High-level modules depend on abstractions, not concrete implementations
- **ISP**: Minimal, focused interfaces - controllers expose only necessary methods

## 2. Developer Workflows

The project uses **PlatformIO** for all build and development tasks.

### Building & Flashing
```bash
# Build for ESP32
pio run -e az-delivery-devkit-v4

# Build and upload firmware
pio run -e az-delivery-devkit-v4 --target upload

# View serial output (115200 baud)
pio device monitor
```

### Testing
```bash
# Run all 71 tests
pio test

# Run tests for specific environment
pio test -e native          # Fast testing on host machine
pio test -e esp32dev        # Hardware testing on ESP32

# Tests include:
# - Anchor windlass (32 tests)
# - Bow propeller (39 tests)
# - Both systems cover: hardware, controller logic, SignalK, emergency stop, remote control
```

### Dependencies
All dependencies in `platformio.ini`:
- **SensESP 3.2.2** - Framework with SignalK integration
- **ArduinoJson 7.4.2** - JSON handling
- **esp32** core for platform definitions
- **unity** - Test framework (built-in to PlatformIO)

## 3. Code Organization & Patterns

### Directory Structure
```
include/
  bow_propeller_controller.h        # Bow thruster high-level control
  winch_controller.h                # AnchorWinchController (was WinchController)
  home_sensor.h                      # Anchor home position detection
  automatic_mode_controller.h        # Anchor automatic positioning
  remote_control.h                   # Physical remote button processing
  pin_config.h                       # GPIO pin definitions (centralized)
  hardware/
    ESP32BowPropellerMotor.h        # GPIO implementation for bow control
    ESP32Motor.h                     # Generic motor interface
  interfaces/
    IMotor.h                         # Motor interface
    ISensor.h                        # Sensor interface
  services/
    BoatBowControlApp.h             # Main orchestrator
    EmergencyStopService.h          # Unified emergency stop
    SignalKService.h                # SignalK integration
    PulseCounterService.h           # Anchor pulse counting
    StateManager.h                  # State management

src/
  main.cpp                           # Setup and main loop
  bow_propeller_controller.cpp       # Bow controller implementation
  winch_controller.cpp               # AnchorWinchController implementation
  home_sensor.cpp                    # Home sensor implementation
  automatic_mode_controller.cpp      # Auto positioning implementation
  remote_control.cpp                 # Remote control implementation  
  hardware/
    ESP32BowPropellerMotor.cpp      # Bow motor GPIO implementation
  services/
    BoatBowControlApp.cpp           # Application orchestration
    EmergencyStopService.cpp        # Emergency stop implementation
    SignalKService.cpp              # SignalK bindings
    PulseCounterService.cpp         # Pulse counting service
```

### Key Design Patterns

**Active-LOW Relay Control**
- All relay outputs (GPIO 4, 5, 14, 27) default HIGH (inactive) on startup
- Write LOW to activate, HIGH to deactivate
- Critical for safety: system is safe-by-default on boot/crash
- Applied in: `AnchorWinchController`, `ESP32BowPropellerMotor`

**Hardware Abstraction**
- Concrete hardware classes (ESP32Motor, ESP32BowPropellerMotor) implement interfaces
- Controllers use abstract interfaces, not concrete hardware classes
- Enables mocking for testing, easy hardware swapping

**Dependency Injection**
- Controllers receive dependencies through constructors
- Example: `AutomaticModeController(AnchorWinchController&)`
- Example: `RemoteControl(AnchorWinchController&, BowPropellerController&)`
- Enables loose coupling and comprehensive testing

**Centralized Pin Configuration**
- All GPIO pin definitions in `pin_config.h` as `constexpr` constants
- Organized semantically: `ANCHOR_*`, `BOW_*`, `REMOTE_*`
- Single point of change if pins remap

**State Encapsulation**
- State lives in appropriate class instances, not globals
- Minimal global state: only `pulse_count` (ISR), `config_meters_per_pulse`
- Controllers track their own state internally

**Interrupt Handling Pattern**
- High-frequency events (pulse counting) use ISR (`pulseISR()`)
- ISR calls `remote_control->processInputs()` for deadman switch polling
- ISR calls `event_loop()->tick()` for SensESP event loop
- Recurring tasks (monitoring, SignalK polling) use `event_loop()->onRepeat()`

**Initialization Order** (critical for safety)
```cpp
// 1. Hardware initialization (GPIO pins set to safe defaults)
winch_controller.initialize();          
bow_propeller_controller.initialize();   
home_sensor.initialize();
remote_control.initialize();

// 2. Service initialization (after hardware ready)
emergency_stop_service.initialize();
signalk_service.initialize();

// 3. Logic initialization (depends on services)
auto_mode_controller = new AutomaticModeController(winch_controller);
```

### SignalK Communication Patterns

**Sending Status to SignalK** (Device Data → SignalK Server)
```cpp
// Create output producer
auto rode_output = new SKOutputFloat(
    "navigation.anchor.currentRode",
    "Current rode deployed",
    nullptr, nullptr);

// Connect producer to data source (lambda transform)
pulse_counter->connect_to(rode_output);
```

**Receiving Commands from SignalK** (SignalK Server → Device Action)
```cpp
// Create command listener
auto thrust_listener = new IntSKListener("propulsion.bowThruster.command");

// Process commands through lambda
thrust_listener->connect_to(
    new LambdaConsumer<int>([&](const int& value) {
        if (value == 1) bow_controller_->turnStarboard();
        else if (value == -1) bow_controller_->turnPort();
        else bow_controller_->stop();
    }));
```

### Testing Patterns

**Test File Organization**
- `test_anchor_counter.cpp` - Main test harness (32 anchor windlass tests)
- `test_bow_propeller.cpp` - Bow motor/controller unit tests (20 tests)
- `test_bow_integration.cpp` - Bow system integration tests (19 tests)
- Other test files: SignalK service, app composition, etc.

**Mock Hardware Pattern**
```cpp
// Tests use mock GPIO arrays instead of real hardware
extern uint8_t mock_gpio_states[];
extern uint8_t mock_gpio_modes[];

// Test setup simulates hardware states
void setUp(void) {
    memset(mock_gpio_states, 0, sizeof(mock_gpio_states));
    memset(mock_gpio_modes, 0, sizeof(mock_gpio_modes));
}

// Test code interacts with mocks like real hardware
TEST_ASSERT_EQUAL_INT32(LOW, mock_gpio_states[BOW_PORT]);
```

**Test Naming Convention**
- `test_<subsystem>_<feature>_<scenario>` (e.g., `test_bow_propeller_controller_startup_defaults`)
- Names clearly describe what's being tested and expected behavior

### Class Responsibilities

| Class | Responsibility | GPIO/Input |
|-------|-----------------|-----------|
| AnchorWinchController | Windlass motor control | GPIO 27 (UP), 14 (DOWN) |
| BowPropellerController | Thruster control interface | Delegates to ESP32BowPropellerMotor |
| ESP32BowPropellerMotor | GPIO hardware control | GPIO 4 (PORT), 5 (STARBOARD) |
| HomeSensor | Anchor home detection | GPIO 33 input |
| PulseCounterService | Chain length calculation | GPIO 25 (pulses), 26 (direction) |
| AutomaticModeController | Automatic positioning logic | Uses AnchorWinchController |
| RemoteControl | Physical button processing | GPIO 12/13 (anchor), 15/16 (bow) |
| EmergencyStopService | Unified safety control | - (controls other subsystems) |
| SignalKService | SignalK communication | Handles all SignalK bindings |
| BoatBowControlApp | Main orchestrator | Initializes all components |

## 4. Common Tasks & Solutions

### Adding a New Remote Button

1. Update `pin_config.h` - Add new pin constant
2. Update `remote_control.h` - Add button state tracking
3. Update `remote_control.cpp` - Add button polling and action
4. Update `test_anchor_counter.cpp` - Add test cases

### Extending SignalK Bindings

1. Define SignalK path (follow spec at https://signalk.org/specification/latest/doc/)
2. Update `SignalKService.h` - Add listener/output member
3. Update `SignalKService.cpp` - Add setup method and binding
4. Call setup method from `BoatBowControlApp`
5. Add tests for command reception and status output

### Adding a New Hardware Control

1. Create hardware abstraction class in `include/hardware/`
2. Implement in `src/hardware/`
3. Create controller class using the hardware
4. Add to `BoatBowControlApp` initialization
5. Write unit tests (with mocks)
6. Write integration tests

### Modifying Emergency Stop Behavior

Emergency stop is centralized in `EmergencyStopService`. Key methods:
- `setActive(true, source)` - Activate emergency stop
- `isActive()` - Check if emergency stop is engaged
- `clear()` - Clear emergency stop (manual reset)

All systems check emergency stop before executing commands. Modify `SignalKService` and `RemoteControl` to add new safety checks.

## 5. Safety & Design Considerations

### Hard Rules (Never Break)

1. **All relay outputs default HIGH (inactive)** - pins safe on boot
2. **Deadman switch on all physical buttons** - stop when released
3. **Emergency stop is atomic** - affects all systems simultaneously
4. **Mutual exclusion on bow thruster** - never both directions active
5. **Home sensor blocks anchor retrieval** - cannot over-extend chain
6. **SignalK connection check** - block commands if disconnected >5s

### Debugging Tips

1. **Serial output** (115200 baud) - Status updates every cycle if connected
2. **Node-RED dashboard** - Real-time visualization of all commands/status
3. **SensESP web UI** - Access at `http://bow-ecu.local/` for configuration
4. **PlatformIO monitor** - `pio device monitor` to watch serial output

### Performance Notes

- Flash usage: ~70% (plenty of headroom for features)
- RAM usage: ~9% (minimal memory footprint)
- Test execution: <3 seconds (fast feedback loop)
- No blocking operations in main loop (all async via event loop)

## 6. Key Files & References

- [README.md](../../README.md) - Usage guide and API documentation
- [ANCHOR_CHAIN_USAGE.md](../../ANCHOR_CHAIN_USAGE.md) - Detailed usage examples
- [REFACTORING_SUMMARY.md](../../REFACTORING_SUMMARY.md) - Architecture documentation
- Hardware GPIO pins - check `pin_config.h`

## 7. Common Gotchas

1. **Active-LOW relays** - HIGH = inactive, LOW = active (counterintuitive)
2. **Deadman switches** - Remote buttons auto-stop when released (safety feature)
3. **Physical buttons override SignalK** - Remote takes priority (safety)
4. **Emergency stop is sticky** - Must be manually cleared after activation
5. **SignalK connection loss blocks new commands** - Re-enables after reconnect +5s
6. **Bow thruster mutual exclusion** - Cannot activate both port AND starboard simultaneously
7. **Home sensor protection** - Anchor cannot retrieve past home (automatic stop)


````