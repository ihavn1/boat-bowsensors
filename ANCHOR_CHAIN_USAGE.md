# Boat Control System - Detailed Usage Guide

## Overview
Comprehensive ESP32-based control system combining:
- **Anchor Windlass Control** - Automatic or manual deployment/retrieval via SignalK or remote buttons
- **Bow Thruster Control** - Port/starboard directional control via SignalK or remote buttons
- **Emergency Stop** - Unified emergency stop affects both systems simultaneously

This guide covers both subsystems and their integration.

## SignalK Paths

### Anchor Windlass - Output Paths (Device → SignalK)
| Path | Type | Units | Update Rate | Description |
|------|------|-------|-------------|-------------|
| `navigation.anchor.currentRode` | float | m | 1s | Current chain length deployed (meters) |
| `navigation.anchor.automaticModeStatus` | float | - | on change | Auto mode state (1.0=enabled, 0.0=disabled) |
| `navigation.anchor.targetRodeStatus` | float | m | on change | Armed target length (meters) |
| `navigation.anchor.manualControlStatus` | int | - | on change | Manual control state (1=UP, 0=STOP, -1=DOWN) |

### Anchor Windlass - Input Paths (SignalK → Device)
| Path | Type | Values | Description |
|------|------|--------|-------------|
| `navigation.anchor.automaticModeCommand` | float | >0.5=enable, ≤0.5=disable | Enable/disable automatic control |
| `navigation.anchor.targetRodeCommand` | float | meters | Arm target for automatic mode |
| `navigation.anchor.manualControl` | int | 1=UP, 0=STOP, -1=DOWN | Manual windlass control |
| `navigation.anchor.resetRode` | bool | true | Reset counter to zero |

### Bow Thruster - Output Paths (Device → SignalK)
| Path | Type | Units | Description |
|------|------|-------|-------------|
| `propulsion.bowThruster.status` | int | - | Current direction (1=STARBOARD, 0=STOP, -1=PORT) |

### Bow Thruster - Input Paths (SignalK → Device)
| Path | Type | Values | Description |
|------|------|--------|-------------|
| `propulsion.bowThruster.command` | int | -1=PORT, 0=STOP, 1=STARBOARD | Bow thruster command |

### Emergency Stop (Both Systems)
| Path | Type | Description |
|------|------|-------------|
| `navigation.bow.ecu.emergencyStopCommand` | bool | Activate (true) or clear (false) emergency stop |
| `navigation.bow.ecu.emergencyStopStatus` | bool | Current emergency stop state |

## Usage Examples

### Monitor Current Chain Length
Subscribe to `navigation.anchor.currentRode` in your SignalK dashboard for real-time chain length monitoring (updates every second).

### Automatic Windlass Control (Arm and Fire)

The recommended workflow is to **arm** the target first, then **fire** when ready:

#### Step 1: Arm Target (Prepare but don't start)
```json
{
  "context": "vessels.self",
  "updates": [{
    "values": [{
      "path": "navigation.anchor.targetRodeCommand",
      "value": 15.0
    }]
  }]
}
```
    ### OTA Firmware Updates (optional)
    1. Copy `src/secrets.example.h` to `src/secrets.h`.
    2. Set `AP_PASSWORD` and `OTA_PASSWORD` (AP password must be at least 8 characters).
    3. Upload over the network (device must be reachable):


This sets the target to 15 meters but doesn't start the windlass. The target is "armed" and ready.

#### Step 2: Fire (Enable automatic mode to start)
```json
{
  "context": "vessels.self",
  "updates": [{
    "values": [{
      "path": "navigation.anchor.automaticModeCommand",
      "value": 1.0
    }]
  }]
}
```

The windlass immediately starts moving toward the armed target. When the target is reached (±0.2m), the system automatically:
- Stops the windlass
- Disables automatic mode
- Updates `automaticModeStatus` to 0.0

#### Emergency Stop
```json
{
  "context": "vessels.self",
  "updates": [{
    "values": [{
      "path": "navigation.anchor.automaticModeCommand",
      "value": 0.0
    }]
  }]
}
```

Immediately stops the windlass and disables automatic mode.

#### Alternative Workflow
You can also enable automatic mode first, then set the target - the windlass will start immediately when the target is set.

### Manual Windlass Control

Manual control can be performed via SignalK or a physical remote. Both methods only work when automatic mode is **disabled**.

#### SignalK Manual Control

SignalK manual control uses a single path with integer values.

| Value | Action |
|-------|--------|
| `1` | Retrieve chain (UP) |
| `0` | Stop windlass |
| `-1` | Deploy chain (DOWN) |

#### Retrieve Chain
```json
{"path": "navigation.anchor.manualControl", "value": 1}
```

#### Stop
```json
{"path": "navigation.anchor.manualControl", "value": 0}
```

#### Deploy Chain
```json
{"path": "navigation.anchor.manualControl", "value": -1}
```

**Note:** SignalK manual commands are blocked when automatic mode is enabled.

#### Physical Remote Control
The system supports a hardwired remote control for direct manual operation.

- **Operation**: The winch is active only as long as the corresponding UP or DOWN button is held down. Releasing the button stops the winch.
- **Priority**: The physical remote overrides any active SignalK manual command.
- **Safety**: The remote is disabled when the system is in automatic mode. The anchor home safety feature remains active, preventing over-retrieval.

The home sensor automatically stops retrieval at home position.

### Reset Counter
```json
{"path": "navigation.anchor.resetRode", "value": true}
```

Resets counter to zero, stops windlass, and clears target. The counter also auto-resets when anchor reaches home position.

## Bow Thruster Control

### Bow Thruster via SignalK

The bow thruster can be controlled via SignalK commands with three states:

| Command Value | Direction |
|---------------|-----------|
| `1` | Thruster to STARBOARD (right) |
| `0` | Stop thruster |
| `-1` | Thruster to PORT (left) |

#### Activate Starboard
```json
{"path": "propulsion.bowThruster.command", "value": 1}
```

#### Stop
```json
{"path": "propulsion.bowThruster.command", "value": 0}
```

#### Activate Port
```json
{"path": "propulsion.bowThruster.command", "value": -1}
```

### Bow Thruster via Physical Remote Control

The system includes two dedicated remote buttons for simple thruster control:

- **FUNC3 Button**: Activate thruster to PORT. Active only while button is held down (deadman switch).
- **FUNC4 Button**: Activate thruster to STARBOARD. Active only while button is held down (deadman switch).
- **Button Release**: Automatically stops thruster.

**Safety Notes:**
- The thruster cannot move in both port and starboard directions simultaneously
- Activating the opposite direction automatically stops the current direction
- Physical remote buttons take immediate priority over SignalK commands
- All relay outputs are inactive (HIGH) by default, preventing accidental activation on startup

## SignalK REST API Examples

### Read Current Chain Length
```bash
curl http://signalk:3000/signalk/v1/api/vessels/self/navigation/anchor/currentRode
```

### Arm and Fire (Automatic Deployment)
```bash
# Arm target to 20m
curl -X POST http://signalk:3000/signalk/v1/api/vessels/self \
  -H "Content-Type: application/json" \
  -d '{"updates":[{"values":[{"path":"navigation.anchor.targetRodeCommand","value":20.0}]}]}'

# Fire (enable automatic mode)
curl -X POST http://signalk:3000/signalk/v1/api/vessels/self \
  -H "Content-Type: application/json" \
  -d '{"updates":[{"values":[{"path":"navigation.anchor.automaticModeCommand","value":1.0}]}]}'
```

### Manual Control
```bash
# UP
curl -X POST http://signalk:3000/signalk/v1/api/vessels/self \
  -H "Content-Type: application/json" \
  -d '{"updates":[{"values":[{"path":"navigation.anchor.manualControl","value":1}]}]}'

# STOP
curl -X POST http://signalk:3000/signalk/v1/api/vessels/self \
  -H "Content-Type: application/json" \
  -d '{"updates":[{"values":[{"path":"navigation.anchor.manualControl","value":0}]}]}'

# DOWN
curl -X POST http://signalk:3000/signalk/v1/api/vessels/self \
  -H "Content-Type: application/json" \
  -d '{"updates":[{"values":[{"path":"navigation.anchor.manualControl","value":-1}]}]}'
```

### Emergency Stop
```bash
curl -X POST http://signalk:3000/signalk/v1/api/vessels/self \
  -H "Content-Type: application/json" \
  -d '{"updates":[{"values":[{"path":"navigation.anchor.automaticModeCommand","value":0.0}]}]}'
```

### Bow Thruster Control via REST API
```bash
# Activate thruster to starboard
curl -X POST http://signalk:3000/signalk/v1/api/vessels/self \
  -H "Content-Type: application/json" \
  -d '{"updates":[{"values":[{"path":"propulsion.bowThruster.command","value":1}]}]}'

# Stop thruster
curl -X POST http://signalk:3000/signalk/v1/api/vessels/self \
  -H "Content-Type: application/json" \
  -d '{"updates":[{"values":[{"path":"propulsion.bowThruster.command","value":0}]}]}'

# Activate thruster to port
curl -X POST http://signalk:3000/signalk/v1/api/vessels/self \
  -H "Content-Type: application/json" \
  -d '{"updates":[{"values":[{"path":"propulsion.bowThruster.command","value":-1}]}]}'
```

### Emergency Stop (All Systems)
```bash
curl -X POST http://signalk:3000/signalk/v1/api/vessels/self \
  -H "Content-Type: application/json" \
  -d '{"updates":[{"values":[{"path":"navigation.bow.ecu.emergencyStopCommand","value":true}]}]}'
```

Activating emergency stop immediately halts both anchor windlass and bow thruster.

## Node-RED Integration

### Dashboard Components

**Anchor Windlass Control**

Preset Target Buttons (recommended for arm and fire operation):
```
[Button: 5m]  [Button: 10m]  [Button: 15m]  [Button: 20m]
```
Each button sends to `navigation.anchor.targetRodeCommand` with respective value.

Auto Mode Switch:
```
[Toggle: AUTO MODE]  ← Sends 1.0 (on) or 0.0 (off) to navigation.anchor.automaticModeCommand
```

Manual Control Buttons (only when auto mode off):
```
[UP]  [STOP]  [DOWN]
```
Button group sending 1, 0, -1 to `navigation.anchor.manualControl`

Status Display:
- Gauge showing `navigation.anchor.currentRode`
- Text showing `navigation.anchor.automaticModeStatus`
- Text showing `navigation.anchor.targetRodeStatus`

**Bow Thruster Control**

Direction Buttons:
```
[PORT]  [STOP]  [STARBOARD]
```
Buttons sending -1, 0, 1 to `propulsion.bowThruster.command`

Status Display:
- Text showing `propulsion.bowThruster.status` (active direction)

**Emergency Stop**

Red Emergency Stop Button:
```
[EMERGENCY STOP]  ← Sends true to navigation.bow.ecu.emergencyStopCommand
```

Status Indicator:
- Red warning showing `navigation.bow.ecu.emergencyStopStatus`

### Using signalk-send-pathvalue Node

The `signalk-send-pathvalue` node (from @signalk/node-red-embedded-signalk) works well with this system:

```javascript
// Example: Preset target button for anchor
msg.payload = {
    "path": "navigation.anchor.targetRodeCommand",
    "value": 15.0
};
return msg;

// Example: Bow thruster command
msg.payload = {
    "path": "propulsion.bowThruster.command",
    "value": 1  // 1=STARBOARD, 0=STOP, -1=PORT
};
return msg;
```

Connect to `signalk-send-pathvalue` node configured for PUT requests.

### Example: Arm and Fire Flow (Anchor)

```
[5m] ──┐
[10m] ─┼──> [Set Target] ──> [send-pathvalue]
[15m] ─┘

[AUTO] ──> [Auto Mode] ──> [send-pathvalue]
```

**Function: Set Target**
```javascript
msg.payload = {
    "path": "navigation.anchor.targetRodeCommand",
    "value": parseFloat(msg.payload)
};
return msg;
```

**Function: Auto Mode**
```javascript
msg.payload = {
    "path": "navigation.anchor.automaticModeCommand",
    "value": msg.payload ? 1.0 : 0.0
};
return msg;
```

### Example: Bow Thruster Control Flow

```
[PORT] ─┐
[STOP] ─┼──> [Thruster Cmd] ──> [send-pathvalue]
[STARBOARD] ┘
```

**Function: Thruster Cmd**
```javascript
// msg.payload should be -1, 0, or 1
msg.payload = {
    "path": "propulsion.bowThruster.command",
    "value": msg.payload
};
return msg;
```

## Configuration

### Hardware Setup

**Anchor Windlass System**
- **Pulse Input (Input):** GPIO 25 - Connect chain counter pulse sensor
- **Direction Input (Input):** GPIO 26 - HIGH = chain out, LOW = chain in
- **Anchor Home Sensor (Input):** GPIO 33 - Detects when anchor is fully retrieved (active LOW)
- **Winch UP Output (Output):** GPIO 27 - Activates windlass to retrieve chain (active LOW)
- **Winch DOWN Output (Output):** GPIO 14 - Activates windlass to deploy chain (active LOW)

**Bow Thruster System**
- **Bow Port Output (Output):** GPIO 4 - Activates thruster to port (active LOW)
- **Bow Starboard Output (Output):** GPIO 5 - Activates thruster to starboard (active LOW)

**Remote Control Inputs**
- **Remote UP Input (Input):** GPIO 12 - Anchor UP button from remote (active HIGH)
- **Remote DOWN Input (Input):** GPIO 13 - Anchor DOWN button from remote (active HIGH)
- **Remote Func 3 Input (Input):** GPIO 15 - Bow PORT button from remote (active HIGH)
- **Remote Func 4 Input (Input):** GPIO 16 - Bow STARBOARD button from remote (active HIGH)

### Calibration
The meters per pulse conversion factor (default: 0.1) can be configured via the SensESP web interface at:
```
http://bow-ecu.local/
```

Look for the "Meters per Pulse" configuration item to adjust the value based on your windlass gypsy diameter and pulse sensor.

### OTA Firmware Updates (optional)
1. Copy `src/secrets.example.h` to `src/secrets.h`.
2. Set `AP_PASSWORD` and `OTA_PASSWORD` (AP password must be at least 8 characters).
3. Upload over the network (device must be reachable):

```bash
pio run --target upload --upload-port bow-ecu.local
```

## Safety Features

1. **Manual mode by default** - System starts in manual mode; automatic control must be explicitly enabled
2. **Arm and fire operation** - Set target first, enable auto mode when ready; prevents accidental deployment
3. **Auto-disable on completion** - Automatic mode turns off when target reached; returns to manual mode
4. **Home position protection** - Cannot retrieve past home position; auto-stops and resets counter
5. **Emergency stop** - Send `automaticModeCommand: 0.0` to immediately stop and disable auto mode
6. **Manual control lockout** - Manual commands blocked when automatic mode active
7. **Status feedback** - All mode changes reflected in status paths for monitoring
8. **Stopping tolerance** - Stops within ±0.2m of target (configurable via meters-per-pulse setting)

## Troubleshooting

| Problem | Check | Solution |
|---------|-------|----------|
| Counter not incrementing | Pulse sensor (GPIO 25) | Verify sensor connection and signal |
| Wrong count direction | Direction sensor (GPIO 26) | HIGH = out, LOW = in; swap if reversed |
| Windlass won't start (auto) | Automatic mode status | Send `automaticModeCommand: 1.0` to enable |
| | Target armed | Send `targetRodeCommand` with valid target |
| | Current position | If already at target, no movement occurs |
| Windlass won't retrieve | Home sensor (GPIO 33) | May already be home; check sensor state |
| | Sensor wiring | Should be LOW at home, HIGH otherwise |
| Auto mode won't enable | Manual control active | Ensure no manual commands pending |
| Counter won't reset | Home sensor | Verify LOW signal when anchor home |
| | Manual reset | Send `resetRode: true` |
| Emergency: Can't stop | Auto mode disable | Send `automaticModeCommand: 0.0` |
| | Physical switches | Use hardwired manual controls |

### Debug Output
Monitor serial output (115200 baud) for status messages:
- Pulse count and direction every 5 seconds
- Auto mode enable/disable events
- Target arm/fire events
- Manual control actions
- Home position detection
