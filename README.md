"# Boat Bow Sensors - Anchor Chain Counter & Windlass Control

An ESP32-based anchor chain counter and automatic windlass control system with SignalK integration.

## Features

### Anchor Chain Length Monitoring
- **Pulse-based counting** - Accurate chain length measurement using pulse sensor
- **Bidirectional tracking** - Counts up when deploying, down when retrieving
- **SignalK integration** - Reports current rode length to `navigation.anchor.currentRode`
- **Automatic calibration** - Configurable meters-per-pulse via web interface

### Automatic Windlass Control
- **Target-based deployment** - Set desired chain length via SignalK
- **Enable/disable mode** - Separate control for automatic operation
- **Smart direction control** - Automatically deploys or retrieves to reach target
- **Tolerance-based stopping** - Stops within ±2 pulses of target

### Safety Features
- **Home position detection** - Prevents over-retrieval with dedicated sensor
- **Automatic counter reset** - Resets to zero when anchor reaches home position
- **Manual operation support** - Full manual control with automatic mode disabled
- **Hardware limit protection** - Cannot retrieve past home position

## Hardware Requirements

- **ESP32 Board**: MH ET LIVE ESP32MiniKit (or compatible)
- **Pulse Sensor**: Chain counter pulse sensor (e.g., Hall effect or optical)
- **Direction Sensor**: Detects chain in/out direction
- **Home Position Sensor**: Detects when anchor is fully retrieved
- **Relay/Contactor**: For windlass motor control (2 channels: UP/DOWN)

## GPIO Pin Configuration

| Function | GPIO | Description |
|----------|------|-------------|
| Pulse Input | 25 | Chain counter pulse sensor |
| Direction | 26 | HIGH = chain out, LOW = chain in |
| Anchor Home | 33 | LOW = anchor at home position |
| Winch UP | 27 | Activate to retrieve chain |
| Winch DOWN | 14 | Activate to deploy chain |

## Quick Start

1. **Hardware Setup**: Connect sensors and relays according to GPIO pin configuration
2. **Flash Firmware**: Upload code to ESP32 using PlatformIO
3. **Configure WiFi**: Connect to `bow-sensors` access point on first boot
4. **Calibrate**: Set meters-per-pulse value via web interface at `http://bow-sensors.local/`
5. **SignalK Integration**: Device automatically connects and publishes/subscribes to SignalK paths

## SignalK Paths

### Output (Device → SignalK)
- `navigation.anchor.currentRode` - Current chain length in meters

### Inputs (SignalK → Device)
- `navigation.anchor.automaticMode` - Enable/disable automatic control (boolean)
- `navigation.anchor.targetRode` - Set target chain length in meters (float)
- `navigation.anchor.resetRode` - Reset counter to zero (boolean)

## Usage Examples

### Deploy 15 meters automatically
```json
// 1. Enable automatic mode
{"path": "navigation.anchor.automaticMode", "value": true}

// 2. Set target
{"path": "navigation.anchor.targetRode", "value": 15.0}
```

### Stop and disable automatic mode
```json
{"path": "navigation.anchor.automaticMode", "value": false}
```

## Documentation

See [ANCHOR_CHAIN_USAGE.md](ANCHOR_CHAIN_USAGE.md) for detailed usage instructions, REST API examples, safety notes, and troubleshooting.

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

# Monitor serial output
pio device monitor
```

## License

[Add your license here]

## Author

[Add your name/contact here]" 
