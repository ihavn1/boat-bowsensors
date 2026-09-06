# Boat Control System - Anchor Windlass & Bow Thruster Controller

An ESP32-based dual-system controller for anchor chain management and bow thruster control with SignalK integration.

## Features

### Anchor Windlass Control
- **Bidirectional pulse counting** - Accurate chain length measurement with direction sensing
- **Real-time tracking** - Continuous monitoring of deployed chain length
- **Automatic positioning** - Auto-retrieve or deploy to reach target length
- **Home position detection** - Prevents over-retrieval with dedicated sensor
- **Manual or automatic modes** - Flexible operation via remote, buttons, or SignalK commands

### Bow Thruster Control
- **DirectionalLocomotion** - Independent port/starboard control via relays
- **Multiple command sources** - Remote buttons (FUNC3/FUNC4) and SignalK integration
- **Safety interlocks** - Emergency stop blocks all commands immediately
- **Status reporting** - Real-time direction and state via SignalK
- **Deadman switch behavior** - Remote buttons auto-stop on release

### Safety Features
- **Home position detection** - Prevents anchor over-retrieval
- **Automatic counter reset** - Resets to zero when anchor reaches home
- **Emergency stop integration** - Immediately stops all motors (anchor + bow)
- **Active-low relay safety** - All relays default to inactive state
- **Connection stability checking** - SignalK commands blocked until stable connection

## Hardware Requirements

- **ESP32 Board**: MH ET LIVE ESP32MiniKit (or compatible)
- **Pulse Sensor**: Chain counter pulse sensor (e.g., Hall effect or optical)
- **Direction Sensor**: Detects chain in/out direction
- **Home Position Sensor**: Detects when anchor is fully retrieved
- **Relay/Contactor**: For windlass motor control (2 channels: UP/DOWN)

## GPIO Pin Configuration

| Function | GPIO | Direction | Description |
|----------|------|-----------|-------------|
| **Anchor/Winch System** | | | |
| Pulse Input | 25 | Input | Chain counter pulse sensor |
| Direction | 26 | Input | HIGH = chain out, LOW = chain in |
| Anchor Home | 33 | Input | LOW = anchor at home position |
| Winch UP | 27 | Output | Activate to retrieve chain (active LOW) |
| Winch DOWN | 14 | Output | Activate to deploy chain (active LOW) |
| **Bow Thruster System** | | | |
| Bow Port | 4 | Output | Turn bow port/left (active LOW) |
| Bow Starboard | 5 | Output | Turn bow starboard/right (active LOW) |
| **Remote Control Inputs** | | | |
| Remote UP | 12 | Input | Winch UP button (active HIGH) |
| Remote DOWN | 13 | Input | Winch DOWN button (active HIGH) |
| Remote Func 3 | 15 | Input | Bow PORT button (active HIGH) |
| Remote Func 4 | 16 | Input | Bow STARBOARD button (active HIGH) |

## Quick Start

1. **Hardware Setup**: Connect sensors and relays according to GPIO pin configuration
2. **Flash Firmware**: Upload code to ESP32 using PlatformIO
3. **Configure WiFi**: Connect to `bow-ecu` access point on first boot
4. **Calibrate**: Set meters-per-pulse value via SensESP web UI at `http://bow-ecu.local/` 
   - Navigate to **System > Calibration** section
   - Adjust **Meters Per Pulse** value (default: 0.01m per pulse)
   - Value persists in device memory across reboots
5. **SignalK Integration**: Device automatically connects and publishes/subscribes to SignalK paths
6. **OTA Setup (optional)**: Copy `src/secrets.example.h` to `src/secrets.h` and set `AP_PASSWORD` and `OTA_PASSWORD`

## SignalK Paths

### Anchor Windlass - Outputs (Device → SignalK)
| Path | Type | Units | Description |
|------|------|-------|-------------|
| `navigation.anchor.rodeLength` | float | m | Current deployed rode length |
| `navigation.anchor.windlass.automaticMode` | bool | - | Whether automatic mode is enabled |
| `navigation.anchor.targetRodeLength` | float | m | Current armed target length |
| `navigation.anchor.windlass.state` | string | - | Current state (`up`, `down`, or `stopped`) |

### Anchor Windlass - Inputs (SignalK → Device)
| Path | Type | Values | Description |
|------|------|--------|-------------|
| `navigation.anchor.windlass.automaticModeCommand` | bool | true/false | Enable or disable automatic mode |
| `navigation.anchor.targetRodeLengthCommand` | float | metres | Arm target length for automatic winching |
| `navigation.anchor.windlass.command` | string | `up`, `stop`, `down` | Manual winch control command |
| `navigation.anchor.resetRode` | bool | true | Reset chain counter to zero |

### Bow Thruster - Outputs (Device → SignalK)
| Path | Type | Units | Description |
|------|------|-------|-------------|
| `propulsion.bowThruster.direction` | string | - | Current direction (`starboard`, `stopped`, or `port`) |

### Bow Thruster - Inputs (SignalK → Device)
| Path | Type | Values | Description |
|------|------|--------|-------------|
| `propulsion.bowThruster.command` | string | `starboard`, `stop`, `port` | Bow thruster command |

### Emergency Stop - Both Systems
| Path | Type | Description |
|------|------|-------------|
| `systems.boatBowEcu.emergencyStopCommand` | bool | Command emergency stop (true=activate, false=clear) |
| `systems.boatBowEcu.emergencyStop` | bool | Current emergency stop state |
| `notifications.systems.boatBowEcu.emergencyStop` | notification | Standard Signal K `normal`/`emergency` notification |

These application-specific paths extend Signal K; they are not part of the official schema. The `propulsion.bowThruster` segment follows the standard `propulsion.<instance>` structure.

## Usage Examples

### Bow Thruster Control (Automatic/SignalK)
```json
// Activate bow thruster to starboard
{"path": "propulsion.bowThruster.command", "value": "starboard"}

// Stop bow thruster
{"path": "propulsion.bowThruster.command", "value": "stop"}

// Activate bow thruster to port
{"path": "propulsion.bowThruster.command", "value": "port"}
```

### Bow Thruster Control (Remote Buttons)
The physical remote control provides immediate control:
- **FUNC3 Button**: Activates bow thruster port
- **FUNC4 Button**: Activates bow thruster starboard
- **Button Release**: Automatically stops thruster (deadman switch)

### Anchor Windlass - Automatic Deployment (Arm and Fire)
```json
// 1. Arm target (prepare but don't start)
{"path": "navigation.anchor.targetRodeLengthCommand", "value": 15.0}

// 2. Fire when ready (starts windlass automatically)
{"path": "navigation.anchor.windlass.automaticModeCommand", "value": true}

// System will automatically disable when target reached
```

### Anchor Windlass - Manual Control
```json
// Retrieve chain
{"path": "navigation.anchor.windlass.command", "value": "up"}

// Stop windlass
{"path": "navigation.anchor.windlass.command", "value": "stop"}

// Deploy chain
{"path": "navigation.anchor.windlass.command", "value": "down"}
```

### Physical Remote Control (Anchor)
The system supports physical remote buttons for anchor control:
- **UP Button**: Retrieves chain (active while held)
- **DOWN Button**: Deploys chain (active while held)
- **Button Release**: Automatically stops winch (deadman switch)

Note: Remote control takes priority over SignalK commands and automatically disables during automatic mode.

### Emergency Stop (Both Systems)
```json
// Immediately stop all motors (anchor + bow thruster)
{"path": "systems.boatBowEcu.emergencyStopCommand", "value": true}

// Resume operations
{"path": "systems.boatBowEcu.emergencyStopCommand", "value": false}
```

## Safety Considerations

1. **Emergency Stop Priority**: Activating emergency stop immediately halts all motors
2. **Connection Blocking**: SignalK commands are blocked during startup or connection loss (waits for 5-second stable connection)
3. **Remote Override**: Physical remote buttons take immediate priority over SignalK
4. **Mutual Exclusion**: Bow thruster can never activate both port and starboard simultaneously
5. **Idle Safety**: All relay outputs default to inactive (HIGH) on startup or power loss



## Documentation

See [ANCHOR_CHAIN_USAGE.md](ANCHOR_CHAIN_USAGE.md) for detailed usage instructions, REST API examples, safety notes, and troubleshooting.

## Data Persistence

The system persists the following configuration across reboots:

| Data | Storage | Persistence |
|------|---------|-------------|
| **Meters Per Pulse** (calibration) | SensESP ConfigItem (SPIFFS) | ✅ Persists across reboots |
| WiFi Settings (SSID, password) | SensESP SPIFFS | ✅ Persists across reboots |
| AP Mode Settings | SensESP SPIFFS | ✅ Persists across reboots |

The following operational data is **volatile** and resets on each boot:

| Data | Reset Value | Rationale |
|------|-------------|-----------|
| Pulse Count | 0 | Must restart counting each session |
| Rode Length | 0.0m | Calculated from pulse count |
| Emergency Stop State | Inactive | Safety default on boot |
| Auto Mode State | Disabled | Safety default |
| Manual Control State | STOP | Safety default |

**Note**: Chain deployment state is not persisted. After a reboot, the system assumes the anchor is at home (0m). Use the `navigation.anchor.resetRode` SignalK command to explicitly set the counter if needed.

## Technology Stack

- **Platform**: ESP32 (Arduino framework)
- **Framework**: [SensESP v3.1.1](https://github.com/SignalK/SensESP)
- **Protocol**: SignalK WebSocket/HTTP
- **Build System**: PlatformIO
- **Web Interface**: Built-in configuration UI

## Development

```bash
# Build project
pio run

# Upload to device
pio run --target upload

# Upload OTA (device must be on the network)
pio run --target upload --upload-port bow-ecu.local

# Monitor serial output
pio device monitor

# Run tests (requires ESP32 connected via USB)
pio test -e test
```

### Testing

The project uses the **Unity** test framework with 71 comprehensive tests covering:

**Anchor Windlass Tests** (32 tests)
- Pulse counting and ISR behavior
- Physical remote control operations
- Home sensor safety features
- Manual and automatic winch control
- Mode transitions and edge cases

**Bow Propeller Tests** (39 tests)
- Motor hardware control (GPIO, relay logic)
- Controller command dispatch
- SignalK command mapping and integration
- Safety features (mutual exclusion, startup state)
- Emergency stop integration
- Remote control integration (FUNC3/FUNC4 buttons)
- System-level scenarios (multi-control, error recovery)

Run tests with:
```bash
# Run all tests on native platform (fast, no hardware required)
pio test -e native

# Run on connected ESP32 hardware
pio test
```

**Note:** Native platform tests don't require sensors or hardware connected, but ESP32 hardware tests require a board connected via USB.

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Author

ihavn1" 
