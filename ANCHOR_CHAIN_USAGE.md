# Anchor Chain Counter - SignalK Usage Guide

## Overview
This ESP32-based anchor chain counter measures the deployed anchor rode length and provides automatic windlass control via SignalK.

## SignalK Paths

### Output (Device → SignalK)
- **`navigation.anchor.currentRode`** - Current anchor chain length in meters (updated every 1 second)

### Inputs (SignalK → Device)
- **`navigation.anchor.automaticMode`** - Enable/disable automatic windlass control (boolean)
- **`navigation.anchor.targetRode`** - Set target chain length for automatic control (float, meters)
- **`navigation.anchor.resetRode`** - Reset the counter to zero (boolean)

## Usage Examples

### Monitor Current Chain Length
The device continuously publishes the current rode length to `navigation.anchor.currentRode`. Subscribe to this path in your SignalK dashboard to monitor the chain length in real-time.

### Automatic Windlass Control

#### Enable Automatic Mode
```json
{
  "context": "vessels.self",
  "updates": [{
    "values": [{
      "path": "navigation.anchor.automaticMode",
      "value": true
    }]
  }]
}
```

Automatic mode must be enabled before the windlass will respond to target commands.

#### Deploy 15 meters of chain
```json
{
  "context": "vessels.self",
  "updates": [{
    "values": [{
      "path": "navigation.anchor.targetRode",
      "value": 15.0
    }]
  }]
}
```

The windlass will automatically deploy chain until 15 meters is reached, then stop (only if automatic mode is enabled).

#### Retrieve to 5 meters
```json
{
  "context": "vessels.self",
  "updates": [{
    "values": [{
      "path": "navigation.anchor.targetRode",
      "value": 5.0
    }]
  }]
}
```

The windlass will automatically retrieve chain until 5 meters remains, then stop (only if automatic mode is enabled).

#### Disable Automatic Mode (Stop Windlass)
```json
{
  "context": "vessels.self",
  "updates": [{
    "values": [{
      "path": "navigation.anchor.automaticMode",
      "value": false
    }]
  }]
}
```

Disabling automatic mode immediately stops the windlass.

### Reset Counter
```json
{
  "context": "vessels.self",
  "updates": [{
    "values": [{
      "path": "navigation.anchor.resetRode",
      "value": true
    }]
  }]
}
```

This resets the counter to zero, stops the windlass, and clears any active target.

## Manual Operation

When operating the windlass manually (using physical switches), the counter continues to measure and report the chain length accurately. The automatic control will not interfere when automatic mode is disabled.

**Best Practice:** Before manual operation, disable automatic mode (`automaticMode: false`) to ensure automatic control cannot activate.

## Using with SignalK REST API

### GET Current Chain Length
```bash
curl http://your-signalk-server:3000/signalk/v1/api/vessels/self/navigation/anchor/currentRode
```

### PUT Enable Automatic Mode
```bash
curl -X PUT http://your-signalk-server:3000/signalk/v1/api/vessels/self/navigation/anchor/automaticMode \
  -H "Content-Type: application/json" \
  -d '{"value": true}'
```

### PUT Target Chain Length (Deploy 20m)
```bash
curl -X PUT http://your-signalk-server:3000/signalk/v1/api/vessels/self/navigation/anchor/targetRode \
  -H "Content-Type: application/json" \
  -d '{"value": 20.0}'
```

### PUT Disable Automatic Mode (Stop Windlass)
```bash
curl -X PUT http://your-signalk-server:3000/signalk/v1/api/vessels/self/navigation/anchor/automaticMode \
  -H "Content-Type: application/json" \
  -d '{"value": false}'
```

### PUT Reset Counter
```bash
curl -X PUT http://your-signalk-server:3000/signalk/v1/api/vessels/self/navigation/anchor/resetRode \
  -H "Content-Type: application/json" \
  -d '{"value": true}'
```

## Configuration
### Calibration
The meters per pulse conversion factor (default: 0.1) can be configured via the SensESP web interface at:
```
http://bow-sensors.local/
```

Look for the "Meters per Pulse" configuration item to adjust the value based on your windlass gypsy diameter and pulse sensor.
### Calibration
## Safety Notes

1. **Always test in safe conditions** before relying on automatic control
2. **Keep manual override accessible** - the system respects manual operation when automatic mode is disabled
3. **Monitor the process** - automatic control stops when target is reached (±0.2m tolerance)
4. **Emergency stop** - send `automaticMode: false` to stop automatic operation immediately
5. **Reset after anchor retrieval** - reset the counter when the anchor is fully retrieved
6. **Enable automatic mode only when ready** - the windlass will not move automatically unless explicitly enabled
## Safety Notes

1. **Always test in safe conditions** before relying on automatic control
2. **Keep manual override accessible** - the system respects manual operation
3. **Monitor the process** - automatic control stops when target is reached (±0.2m tolerance)
4. **Emergency stop** - send `targetRode: -1` to stop automatic operation immediately
5. **Reset after anchor retrieval** - reset the counter when the anchor is fully retrieved

## Troubleshooting

### Counter not incrementing
- Check pulse sensor connection on GPIO 25
- Verify direction sensor on GPIO 26 shows correct state
### Windlass not responding to automatic control
- **Verify automatic mode is enabled** (`automaticMode: true`)
- Check that a valid target was set (`targetRode >= 0`)
- Verify GPIO 27 (UP) and GPIO 14 (DOWN) connections
- Ensure no manual control is active(`targetRode >= 0`)
- Ensure no manual control is active

### Wrong direction
- Check direction sensor polarity on GPIO 26
- Swap if needed: HIGH should = chain out, LOW = chain in
