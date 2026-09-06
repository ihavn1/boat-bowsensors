# Phase 5: Hardware Integration Validation - Anchor Windlass & Bow Thruster

**Status**: 🚀 **ACTIVE**  
**Objective**: Validate all features on actual ESP32 hardware with comprehensive manual testing for both anchor windlass and bow thruster systems
**Prerequisites**: Phase 4 (unit tests) complete, firmware successfully uploaded to device

**Scope**: This document covers hardware validation for:
- **Anchor Windlass System**: Pulse counting, motor control, home sensor, automatic positioning
- **Bow Thruster System**: Motor control, mutual exclusion, direction switching
- **Unified Safety**: Emergency stop affecting both systems

---

## 1. Pre-Hardware Checklist

### Physical Setup
- [ ] ESP32 Dev Kit connected via USB to PC
- [ ] Serial monitor ready (COM port identified)
- [ ] Pulse counter test setup ready (simulated chain movement)
- [ ] Remote control buttons accessible (GPIO 12/13 for anchor, GPIO 15/16 for bow)
- [ ] SignalK server running on network (if available)
- [ ] Relay outputs connected for testing (GPIO 4/5 for bow, GPIO 14/27 for anchor)

### Recommended Test Equipment Setup

**Minimum Setup (Budget ~$100-150)**:
1. **Logic Analyzer** (Saleae Clone or DreamSourceLab)
   - 24-channel @ 100MHz sampling
   - ~$30-50 on Amazon/AliExpress
   - **Why**: Captures all GPIO transitions with precise timing (down to microseconds)
   - **Use**: Verify relay activation timing, pulse counting accuracy, motor switching transitions

2. **Function Generator** (Optional but highly recommended)
   - Frequency range: 0.1 Hz - 10 MHz (adjust cap: 1-100 Hz for pulse testing)
   - ~$25-40 (basic units like KeItley or clone)
   - **Why**: Simulate chain movement at controlled speeds (e.g., 2 Hz = 20 pulses/sec)
   - **Alternative**: Use PC software like SIGGEN (free) via audio jack with circuit

3. **Multimeter** (Digital, $10-20)
   - Voltage/resistance/continuity testing
   - **Why**: Verify GPIO voltage levels (3.3V HIGH, 0V LOW), relay circuit continuity

4. **Breadboard + Jumper Wires + Button Switches** (~$15)
   - Build manual pulse simulator and test button circuits
   - Test remote button presses without physical hardware

5. **Relay Module (Optional)** (~$10-20)
   - Opto-isolated 4-channel relay (active-LOW)
   - **Why**: Test actual relay switching under load (with dummy load resistors)

**Complete Mid-Range Setup (~$200-300)**:
- Add: USB oscilloscope (e.g., Hantek 6022BE) for waveform analysis
- Add: Power supply (adjustable 3.3V) for independent GPIO testing
- Add: Current clamp for measuring motor drive current

---

## 1.1 Windows Setup for Logic Analysis

**Recommended Workflow**:

1. **Install Logic Analyzer Software** (FREE):
   - [Saleae Logic 2](https://www.saleae.com/downloads/) (Windows, Mac, Linux)
   - Or [PulseView](https://sigrok.org/wiki/PulseView) (free, open source)

2. **Connect Logic Analyzer to ESP32**:
   ```
   Logic Analyzer Channel → ESP32 GPIO (through 1kΩ resistor for protection)
   
   ANCHOR CONTROL (Channels 0-2):
   - CH0: GPIO 27 (ANCHOR_UP relay output)
   - CH1: GPIO 14 (ANCHOR_DOWN relay output)
   - CH2: GPIO 33 (HOME_SENSOR input - hall effect)
   
   BOW CONTROL (Channels 3-4):
   - CH3: GPIO 4 (BOW_PORT relay output)
   - CH4: GPIO 5 (BOW_STARBOARD relay output)
   
   REMOTE BUTTONS (Channels 5-8):
   - CH5: GPIO 12 (Anchor UP button)
   - CH6: GPIO 13 (Anchor DOWN button)
   - CH7: GPIO 15 (Bow PORT button)
   - CH8: GPIO 16 (Bow STARBOARD button)
   
   PULSE COUNTER (Channels 9-10):
   - CH9: GPIO 25 (PULSE_INPUT - chain counter)
   - CH10: GPIO 26 (DIRECTION - deploy/retrieve sense)
   
   FUTURE/SPARE (Channels 11-15):
   - CH11: GPIO 32 (spare - future sensor)
   - CH12-15: Available for expansion
   - GND: Connect to ESP32 GND (shared reference)
   ```

   **Total channels used**: 11 out of 16 (69% utilization)
   **Benefit**: Captures entire system state in single capture window

3. **Sampling Configuration**:
   - **Sample rate**: 1 MHz (captures 1µs resolution)
   - **Capture duration**: 10-30 seconds per test
   - **Protocol analyzers**: Enable SPI/I2C if testing those buses

4. **Capture Strategies with 16-Channel Analyzer**:

   **Strategy A: Complete System Snapshot** (All 11 channels)
   - Captures everything simultaneously
   - Ideal for: Identifying timing issues between anchor/bow/buttons
   - Start capture → Send command → Analyze interactions across all 11 channels
   - Example: See if remote button press causes relay to activate precisely when expected
   
   **Strategy B: Focus Capture** (Sub-groups of channels)
   - Reduce noise by capturing only relevant subsystem
   - **Anchor-only**: CH0-2, CH9-10 (motor + home + pulses)
   - **Bow-only**: CH3-4 (motor control - simplest for timing analysis)
   - **Remote input testing**: CH5-8 (button presses vs. relay response)
   
   **Strategy C: Temporal Correlation**
   - Use spare channels (CH11-15) for:
     - Serial TX monitoring (if available, optional)
     - Power rail observation (measure current via inline resistor)
     - Synchronization pulse from external source
   
   **Tips for 16-Channel Captures**:
   - Export CSV → import to Excel/spreadsheet for timeline analysis
   - Look for timing violations: 
     - Motor output should activate within 50ms of command
     - Relay switching should avoid "floating" states (no short HIGH→LOW glitch)
     - Button input to motor output latency: < 200ms (worst case SensESP event loop)
   - Use Saleae's "Timing Measurement" tool to quantify latencies

---

## 1.2 Pulse Counter Simulation Setup

**Hardware Option 1: Function Generator**:
```
Function Gen OUT → 1kΩ resistor → ESP32 GPIO 25 (PULSE_INPUT)
                                 → ESP32 GND
DIR control pin (GPIO 26):
  - Connect to GPIO 26 directly
  - Or use second function gen output for simultaneous direction changes
```

**Hardware Option 2: Manual Test (Breadboard)**:
```
ESP32 3.3V → Push button → GPIO 25 (through 1kΩ resistor)
GPIO 25 → 10kΩ pulldown to GND
GPIO 26 → Manual jumper (HIGH = deploy, LOW = retrieve)

Press button repeatedly to simulate pulses
Monitor via serial: "Pulse count: X, Rode: X.XXm"
```

**Hardware Option 3: Arduino Pulse Generator** (Most Flexible):
```cpp
// Upload to second Arduino to generate test pulses
#define PULSE_PIN 9
#define DIR_PIN 10

void setup() {
  pinMode(PULSE_PIN, OUTPUT);
  pinMode(DIR_PIN, OUTPUT);
}

void loop() {
  // 2 Hz pulse (0.5 sec HIGH, 0.5 sec LOW)
  digitalWrite(PULSE_PIN, HIGH);
  delay(500);
  digitalWrite(PULSE_PIN, LOW);
  delay(500);
  
  // Change direction every 10 pulses
  digitalWrite(DIR_PIN, (millis() / 5000) % 2);
}
```

---

## 1.3 Remote Control Button Simulation

**Physical Button Test** (Accurate but manual):
```
GPIO 12 ← Button (active HIGH, release = LOW)
GPIO 13 ← Button
GPIO 15 ← Button
GPIO 16 ← Button

ESP32 3.3V → 10kΩ resistor → Button → GPIO
                            → 10kΩ pulldown to GND
```

**Breadboard Setup**:
- Place 4 pushbuttons on breadboard
- Wire to GPIO 12/13/15/16
- Connect to 3.3V and GND rails
- Serial monitor shows: "Remote: UP pressed", "Remote: DOWN released"

---

## 1.4 SignalK/HTTP Testing

**Windows Tools**:
1. **Postman** (free, GUI REST client):
   - Send PUT requests to device HTTP API
   - Example: `PUT http://bow-ecu.local:3000/navigation/anchor/manualControl -d '{"value": 1}'`

2. **curl** (command line):
   ```bash
   # Retrieve rode length
   curl http://bow-ecu.local:3000/navigation/anchor/currentRode
   
   # Send manual UP command
   curl -X PUT http://bow-ecu.local:3000/navigation/anchor/manualControl \
        -H "Content-Type: application/json" \
        -d '{"value": 1}'
   ```

3. **SignalK Server** (Node.js, ~5 min setup):
   - Download: [SignalK Server](https://github.com/SignalK/signalk-server)
   - `npm install -g signalk-server`
   - `signalk-server` (starts web UI at localhost:3000)
   - Connect ESP32 via WiFi
   - Real-time monitoring dashboard

---

## 1.5 Complete 16-Channel Test Workflow

**Objective**: Use all 16 channels to capture entire system behavior in a single test sequence

**Setup** (all 11 channels connected as described in section 1.1):
```
Channels 0-10: CONNECTED (as per allocation)
Channels 11-15: SPARE/AVAILABLE
Sampling: 1 MHz, 30-second capture
```

**Test Sequence**:

### Phase A: Boot Validation (Channels 0-4)
1. **Pre-capture**: System powered off
2. **Start capture** in Saleae
3. **Power on** ESP32 via USB
4. **Expected on capture**:
   - All relay outputs (CH0-4) remain HIGH for 500ms+ (safe default)
   - No spurious LOW pulses during boot
   - Stable state within 2 seconds

**Analysis** (check for):
- ✅ All relay pins start HIGH (inactive)
- ✅ No glitches or noise on GPIO lines
- ✅ Smooth transition to stable state

---

### Phase B: Pulse Counting with Remote Button (Channels 5-10)
1. **Setup**: Function generator or Arduino pulse generator on GPIO 25 @ 2 Hz
2. **Direction**: GPIO 26 set HIGH (deploy mode)
3. **Remote button**: Connect pushbutton to GPIO 12 (Anchor UP)
4. **Start capture**
5. **Execute sequence**:
   - Wait 2 seconds (baseline)
   - Press GPIO 12 button for 1 second (simulating anchor UP command)
   - Release button
   - Function gen pulses continue for 10 seconds
   - Stop capture at 15 seconds

**Expected timing on analyzer**:
```
T=0s:   Boot complete, all relays HIGH
T=2s:   Button press: CH5 (GPIO 12) LOW
T=2.05s: Relay activation: CH0 (ANCHOR_UP) drops to LOW (< 50ms latency)
T=2.05s-3.05s: Motor active, pulses arriving on CH9
T=3.05s: Button release: CH5 (GPIO 12) HIGH
T=3.10s: Relay deactivation: CH0 (ANCHOR_UP) back to HIGH (~50ms delay)
T=3.10s+: Pulses stop correlating to motor activity
```

**Analysis** (check for):
- ✅ Button press (CH5) → Motor activation (CH0) latency < 100ms
- ✅ Motor deactivates within 100ms of button release
- ✅ Pulse counter (CH9) shows clean square wave
- ✅ Direction sense (CH10) stable throughout
- ✅ NO floating states (motor pins never both LOW simultaneously)

---

### Phase C: Bow System Testing (Channels 3-8)
1. **Setup**: Connect GPIO 15 and GPIO 16 buttons
2. **Start capture**
3. **Execute sequence**:
   - T=0s: Press GPIO 15 (BOW_PORT)
   - T=0.5s: Release GPIO 15
   - T=1s: Press GPIO 16 (BOW_STARBOARD)
   - T=1.5s: Release GPIO 16
   - T=2s: Rapid toggle GPIO 15 & 16 (test mutual exclusion)
   - T=3s: Stop

**Expected on analyzer**:
```
T=0s:     CH7 (GPIO 15) LOW, CH3 (BOW_PORT relay) LOW (< 50ms)
T=0.5s:   CH7 HIGH, CH3 HIGH
T=1s:     CH8 (GPIO 16) LOW, CH4 (BOW_STARBOARD relay) LOW
T=1.5s:   CH8 HIGH, CH4 HIGH
T=2.0-2.3s: Rapid toggles on CH7/CH8
           → Ch3/4 should NEVER BOTH be LOW (mutual exclusion enforced)
           → Transitions smooth, no intermediate glitches
```

**Analysis** (check for):
- ✅ Port and starboard relays never both active
- ✅ < 100ms response to button press/release
- ✅ Direction transitions are clean (no intermediate float state)

---

### Phase D: Cross-System Interference (All 11 channels)
**Objective**: Verify anchor and bow systems don't interfere

1. **Start capture**
2. **Sequence**:
   - T=0s: Activate anchor UP (GPIO 12)
   - T=1s: While anchor running, activate bow PORT (GPIO 15)
   - T=2s: Release bow button (GPIO 15)
   - T=3s: Release anchor button (GPIO 12)
   - T=4s: Stop capture

**Expected** (all channels):
- Anchor relays (CH0-1) stay consistent while bow operates (CH3-4)
- Pulse counting (CH9-10) unaffected by bow motor
- No crosstalk between subsystems

---

**Export & Analysis** (Post-Capture):
1. Export capture as CSV from Saleae
2. Open in Excel or Python/Pandas
3. Create timeline spreadsheet:
   ```
   Time(s) | GPIO27 | GPIO14 | GPIO4 | GPIO5 | Button | Pulse | Direction
   0.00    | 1      | 1      | 1     | 1     | 0      | 0     | 1
   2.05    | 0      | 1      | 1     | 1     | 1      | 1     | 1
   ...
   ```
4. Measure latencies:
   - Button press to motor output (goal: < 100ms)
   - Pulse-to-pulse interval (goal: 500ms @ 2 Hz)
   - Direction change response (goal: < 50ms)

---

### Firmware Status
- [ ] Latest firmware built and uploaded
- [ ] Device boots without errors
- [ ] Serial logs show initialization complete (both anchor and bow systems)
- [ ] No runtime crashes or watchdog resets

---

## 2. Manual Feature Testing

### 2.1 Hardware Initialization (Critical Safety)

**Objective**: Verify device boots safely with all relays inactive

**Test Steps**:
1. Connect ESP32 via USB
2. Open serial monitor at 115200 baud
3. Observe boot sequence in logs
4. Check GPIO pin states (use multimeter or logic analyzer):
   - ANCHOR_UP (GPIO 27): Should read HIGH (inactive)
   - ANCHOR_DOWN (GPIO 14): Should read HIGH (inactive)
   - BOW_PORT (GPIO 4): Should read HIGH (inactive)
   - BOW_STARBOARD (GPIO 5): Should read HIGH (inactive)

**Success Criteria**:
- ✅ Device boots without errors
- ✅ All relay pins read HIGH on boot
- ✅ No watchdog resets detected
- ✅ Logs show initialization of both anchor and bow subsystems

**Commands** (if available via SignalK):
```
Send: GET /navigation/anchor/status
Expected: Safe state, all outputs inactive
```

---

### 2.2 Pulse Counter & Rode Length Calculation

**Objective**: Verify pulse counting accuracy and rode length calculation

**Test Equipment**:
- Function generator (or manual pulse simulation via GPIO)
- Pulse frequency: 1-10 Hz (simulates chain moving at 1-10 m/min)

**Test Procedure**:

#### Test 2.2.1: Forward Counting (Chain Out)
```
Setup:
  - DIRECTION pin (GPIO 26) = HIGH (chain out)
  - PULSE_INPUT pin (GPIO 35) = toggle at 2 Hz for 10 seconds (20 pulses)
  - Meters per pulse configured as: 0.01m (from config)

Expected:
  - Pulse count increases by 20
  - Rode length increases by 0.2m (20 pulses × 0.01m/pulse)
  - SignalK output: navigation.anchor.rodeLength = 0.2

Verify via logs:
  - Serial should show: "Pulse count: 20, Rode: 0.20m"
  - SignalK output shows matching value
```

#### Test 2.2.2: Reverse Counting (Chain In)
```
Setup:
  - DIRECTION pin (GPIO 26) = LOW (chain in)
  - PULSE_INPUT pin (GPIO 35) = toggle at 2 Hz for 10 seconds (20 pulses)

Expected:
  - Pulse count decreases by 20
  - Rode length decreases by 0.2m
  - Cannot go negative (clamped to 0)
```

#### Test 2.2.3: Home Position Protection
```
Setup:
  - Pulse count at 5.0m (500 pulses)
  - DIRECTION = HIGH (chain out)
  - Home sensor: NOT triggered

Results:
  - Can retrieve chain (count decreases)
  - Rode length goes down correctly

Then:
  - Simulate home sensor triggered (ANCHOR_HOME pin = LOW)
  - Try to extend chain via command

Expected:
  - Motor should not activate (blocked by home sensor)
  - Logs show: "Home sensor blocking WINCH_UP"
  - Rode length stays constant
```

**Success Criteria**:
- ✅ Pulse counts match input frequency
- ✅ Rode length accurate ±0.02m over 1m range
- ✅ Negative counts prevented (clamped to 0)
- ✅ Home sensor blocks deployment

**Commands**:
```
# Check rode length via SignalK
GET /navigation/anchor/currentRode
```

---

### 2.3 Manual Control (UP/DOWN/STOP)

**Objective**: Verify motor control responds to commands

**Test Setup**:
- Motor GPIO pins monitored (logic analyzer recommended)
- WINCH_UP: GPIO 27 (retrieve chain)
- WINCH_DOWN: GPIO 14 (deploy chain)

**Test 2.3.1: Manual UP Command**
```
Command via SignalK:
  PUT /navigation/anchor/manualControl = 1

Expected:
  - WINCH_UP (GPIO 27) = LOW (within 100ms)
  - WINCH_DOWN (GPIO 14) = HIGH (stays inactive)
  - Serial log: "Manual control: UP"
  - Device continues until STOP command

Timing:
  - Activation latency: < 50ms (SensESP event loop)
```

**Test 2.3.2: Manual DOWN Command**
```
Command via SignalK:
  PUT /navigation/anchor/manualControl = -1

Expected:
  - WINCH_DOWN (GPIO 14) = LOW (within 100ms)
  - WINCH_UP (GPIO 27) = HIGH (stays inactive)
  - Serial log: "Manual control: DOWN"
```

**Test 2.3.3: Manual STOP Command**
```
Command via SignalK:
  PUT /navigation/anchor/manualControl = 0

Expected:
  - Both WINCH_UP (GPIO 27) and WINCH_DOWN (GPIO 14) = HIGH (inactive)
  - Serial log: "Manual control: STOP"
  - Motor stops immediately
```

**Test 2.3.4: Control Override**
```
Setup:
  - Send UP command (motor active)
  - While motor running, send DOWN command

Expected:
  - UP signal drops (deactivates)
  - DOWN signal activates
  - No intermediate "both HIGH" window > 10ms (prevent motor jerk)
  - Mode switches smoothly without glitches
```

**Success Criteria**:
- ✅ Commands activate correct relay pins
- ✅ Timing < 100ms from command to GPIO change
- ✅ No "floating" states (both pins high/low simultaneously)
- ✅ Smooth transitions between modes

**Timing Goals**:
```
Command → Parsing: 5-10ms
Parsing → GPIO change: 20-50ms
Total latency: < 100ms
```

---

### 2.4 Emergency Stop

**Objective**: Verify emergency stop activates and prevents control

**Test 2.4.1: Emergency Stop Activation**
```
Setup:
  - Send UP command (motor active)
  - While motor running, activate emergency stop

Command (SignalK):
  PUT /systems/boatBowEcu/emergencyStopCommand = true

Expected Immediately:
  - Both WINCH_UP (GPIO 27) and WINCH_DOWN (GPIO 14) → HIGH (motor stops)
  - Both BOW_PORT (GPIO 4) and BOW_STARBOARD (GPIO 5) → HIGH (both stop)
  - Serial log: "EMERGENCY STOP ACTIVATED"
  - SignalK output: systems.boatBowEcu.emergencyStop = true
  - SignalK notification: notifications.systems.boatBowEcu.emergencyStop.state = "emergency"
  - All control inputs blocked (both anchor and bow)

Timing:
  - Motor stop latency: < 100ms
  - Relay deactivation: immediate
```

**Test 2.4.2: Control Blocked During Emergency Stop**
```
Setup:
  - Emergency stop is active
  - Try to send UP/DOWN/manual commands to anchor
  - Try to send PORT/STARBOARD commands to bow

Expected:
  - Motors don't activate
  - All relays stay HIGH (inactive)
  - SignalK commands rejected
  - Logs show: "Emergency stop active - blocking command"
```

**Test 2.4.3: Clear Emergency Stop**
```
Command (SignalK):
  PUT /systems/boatBowEcu/emergencyStopCommand = false

Expected:
  - SignalK output: systems.boatBowEcu.emergencyStop = false
  - SignalK notification: notifications.systems.boatBowEcu.emergencyStop.state = "normal"
  - Control inputs accepted again
  - Both anchor and bow motors can be commanded

Timing:
  - Response < 100ms
```

**Success Criteria**:
- ✅ Emergency stop activates immediately
- ✅ Motor stops within 100ms
- ✅ All control blocked while active
- ✅ Clear command restores control
- ✅ No "stuck" states (watchdog resets if needed)

---

### 2.5 Physical Remote Control (If Equipped)

**Objective**: Verify physical buttons override SignalK

**Hardware Required**:
- Remote control buttons wired to GPIO (see pin_config.h)

**Test 2.5.1: Physical UP Button**
```
Press physical UP button

Expected:
  - WINCH_UP → LOW within 10ms
  - Motor activates
  - SignalK windlass state updates to `up`
  - Works even if SignalK sends STOP command
```

**Test 2.5.2: Physical DOWN Button**
```
Press physical DOWN button

Expected:
  - WINCH_DOWN → LOW within 10ms
  - Motor activates
  - SignalK windlass state updates to `down`
```

**Test 2.5.3: Button Release Stops Motor**
```
Press and hold UP button
Release button

Expected:
  - Motor stops immediately on release
  - Both relays → HIGH
  - Logs show: "Physical remote released - STOP"
  - Even if SignalK still sending UP command, stop takes priority
```

**Test 2.5.4: Physical Overrides SignalK**
```
Setup:
  - Send UP via SignalK (motor running)
  - Press DOWN button

Expected:
  - Motor switches to DOWN mode
  - Physical input takes priority
  - Logs show: "Physical remote overriding SignalK"
```

**Success Criteria**:
- ✅ Buttons responsive (< 50ms latency)
- ✅ Release stops motor immediately
- ✅ Override behavior works correctly
- ✅ No conflicting signals to motor

---

### 2.6 Automatic Mode Test

**Objective**: Verify automatic deployment/retrieval to target

**Test 2.6.1: Arm Target**
```
Command (SignalK):
  PUT /navigation/anchor/targetRodeLengthCommand = 5.0

Expected:
  - SignalK output: navigation.anchor.targetRodeLength = 5.0
  - Logs show: "Target armed: 5.0m"
  - Motor does NOT activate (not yet fired)
  - Current rode: 2.0m → Target: 5.0m

Timing:
  - Acknowledge within 50ms
```

**Test 2.6.2: Fire Deploy (Below Target)**
```
Prerequisites:
  - Target armed at 5.0m
  - Current rode: 2.0m

Command (SignalK):
  PUT /navigation/anchor/windlass/automaticModeCommand = true

Expected:
  - WINCH_DOWN (GPIO 14) → LOW (deploy chain)
  - Motor activates and runs  
  - Pulse counter increases rode length
  - Logs show: "Automatic mode: DEPLOYING to 5.0m"

Continue until:
  - Rode length reaches 5.0m ± tolerance (0.02m)
  - WINCH_DOWN (GPIO 14) → HIGH (motor stops)
  - SignalK output: navigation.anchor.windlass.automaticMode = false
  - Logs show: "Target reached, auto-mode disabled"

Timing:
  - Deployment rate: ~1m per 60+ pulses (depends on motor/chain speed)
```

**Test 2.6.3: Fire Retrieve (Above Target)**
```
Prerequisites:
  - Current rode: 8.0m
  - Target armed at 5.0m

Command (SignalK):
  PUT /navigation/anchor/windlass/automaticModeCommand = true

Expected:
  - WINCH_UP (GPIO 27) → LOW (retrieve chain)
  - Motor activates and runs
  - Pulse counter decreases rode length
  - Logs show: "Automatic mode: RETRIEVING to 5.0m"

Continue until:
  - Rode length reaches 5.0m ± tolerance
  - WINCH_UP (GPIO 27) → HIGH (motor stops)
  - SignalK output: navigation.anchor.windlass.automaticMode = false
```

**Test 2.6.4: Automatic Mode Blocked at Home**
```
Prerequisites:
  - Home sensor near
  - Current rode: 1.5m
  - Target armed at 2.0m

If ANCHOR_HOME pin triggered during deploy:
  - WINCH_UP → HIGH (motor stops)
  - SignalK shows: "Home sensor blocking deployment"
  - Automatic mode disables
  - Rode length stops changing
```

**Test 2.6.5: Manual Override Auto Mode**
```
Prerequisites:
  - Automatic mode running (motor active)
  - Deploying to target

Send manual command (SignalK):
  PUT /navigation/anchor/windlass/command = "down"

Expected:
  - Automatic mode disables immediately
  - Motor switches to DOWN mode (manual command)
  - Logs show: "Manual control overriding automatic mode"
  - Auto deployment stopped
```

**Success Criteria**:
- ✅ Target arms without activating motor
- ✅ Deploy reaches target ±0.02m
- ✅ Retrieve reaches target ±0.02m
- ✅ Stops exactly at tolerance
- ✅ Blocked by home sensor
- ✅ Manual override works

---

### 2.7 SignalK Connection & Status

**Objective**: Verify device connects to SignalK server and publishes status

**Prerequisites**:
- SignalK server running on network (Node-RED or SignalK server)
- Device on same WiFi network
- Network WiFi credentials in secrets.h

**Test 2.7.1: WiFi Connection**
```
Expected in logs:
  - "WiFi connecting to: [SSID]"
  - "WiFi connected!"
  - "IP: 192.168.x.x"
```

**Test 2.7.2: SignalK Connection**
```
Expected in logs:
  - "Connecting to SignalK server..."
  - "SignalK connection established"
  - "Publishing: navigation.anchor.rodeLength = X"
```

**Test 2.7.3: Status Outputs**
```
Check SignalK server for values:
  - navigation.anchor.rodeLength (rode deployed, metres)
  - systems.boatBowEcu.emergencyStop (boolean)
  - navigation.anchor.windlass.state (current manual state)
  - navigation.anchor.windlass.automaticMode (auto mode running)
  - navigation.anchor.targetRodeLength (armed target)
```

**Test 2.7.4: Input Listeners**
```
Send values from SignalK server:
  - navigation.anchor.windlass.command = "up" (expect motor UP)
  - systems.boatBowEcu.emergencyStopCommand = true (expect all motors stop)
  - navigation.anchor.windlass.automaticModeCommand = true (expect auto-mode)
  - navigation.anchor.targetRodeLengthCommand = X (expect target armed)

Verify device responds to each
```

**Test 2.7.5: Connection Loss Recovery**
```
Setup:
  - Device connected and communicating
  - SignalK server running

Disconnect WiFi (reboot server or change password)
  - Device should detect disconnect
  - No crashes or infinite loops
  - Logs show reconnection attempts

Reconnect WiFi
  - Auto-reconnect succeeds
  - Status resumes publishing
  - Commands accepted again
```

**Success Criteria**:
- ✅ WiFi connects in < 15 seconds
- ✅ SignalK connects after WiFi
- ✅ All status outputs published
- ✅ All input listeners functional
- ✅ Graceful disconnect/reconnect

---

## 2.8 Bow Thruster Hardware Tests

**Objective**: Validate bow thruster control and safety features

### 2.8.1: Bow Motor Relay Control

**Test: Port Direction Activation**
```
Command (SignalK):
  PUT /propulsion/bowThruster/command = -1

Expected:
  - BOW_PORT (GPIO 4) → LOW (active)
  - BOW_STARBOARD (GPIO 5) → HIGH (inactive)
  - Serial log: "Bow thruster: PORT"

Timing:
  - Response latency: < 100ms
```

**Test: Starboard Direction Activation**
```
Command (SignalK):
  PUT /propulsion/bowThruster/command = 1

Expected:
  - BOW_PORT (GPIO 4) → HIGH (inactive)
  - BOW_STARBOARD (GPIO 5) → LOW (active)
  - Serial log: "Bow thruster: STARBOARD"
```

**Test: Stop Command**
```
Command (SignalK):
  PUT /propulsion/bowThruster/command = 0

Expected:
  - BOW_PORT (GPIO 4) → HIGH (inactive)
  - BOW_STARBOARD (GPIO 5) → HIGH (inactive)
  - Serial log: "Bow thruster: STOP"
```

**Success Criteria**:
- ✅ PORT activates only GPIO 4
- ✅ STARBOARD activates only GPIO 5
- ✅ STOP deactivates both
- ✅ No simultaneous activation of both directions

### 2.8.2: Mutual Exclusion Protection

**Test: Prevent Simultaneous Activation**
```
Setup:
  - Thruster currently at PORT
  - Try to activate STARBOARD immediately

Expected:
  - Old direction (PORT) → deactivates first
  - New direction (STARBOARD) → activates
  - Never both relays active simultaneously
  - Logs show direction transition

Timing:
  - Direction switch: < 100ms total
  - No "spark" window
```

**Test: Rapid Direction Switching**
```
Commands in sequence (50ms apart):
  1. PORT
  2. STARBOARD
  3. PORT
  4. STARBOARD
  5. STOP

Expected:
  - Each transition smooth
  - GPIO states always safe (never both active)
  - No motor glitches
  - All commands processed

Success:
  - ✅ Safe transitions throughout
  - ✅ Correct final state (STOP)
```

### 2.8.3: Remote Control (Physical Buttons)

**Test: Bow PORT Button**
```
Setup:
  - Press physical button on remote (GPIO 15 - Bow PORT)
  
Expected:
  - BOW_PORT (GPIO 4) → LOW (activate)
  - Serial log: "BOW PORT activated"
  
While button held:
  - Thruster remains in PORT
  
When released:
  - BOW_PORT (GPIO 4) → HIGH (deactivate)
  - Serial log: "BOW STOP (release)"
```

**Test: Bow STARBOARD Button**
```
Setup:
  - Press physical button on remote (GPIO 16 - Bow STARBOARD)
  
Expected:
  - BOW_STARBOARD (GPIO 5) → LOW (activate)
  - Serial log: "BOW STARBOARD activated"

When released:
  - BOW_STARBOARD (GPIO 5) → HIGH (deactivate)
  - Serial log: "BOW STOP (release)"
```

**Test: Button Release = Stop (Deadman Switch)**
```
Setup:
  - Thruster actively running (either direction)
  - Release button immediately

Expected:
  - Motor stops within 50ms
  - Both GPIO pins return to HIGH
  - Demonstrates safety deadman switch

Success:
  - ✅ Responsive release behavior
  - ✅ Safe shutdown confirmed
```

**Test: Remote Priority Over SignalK**
```
Setup:
  - Send STARBOARD via SignalK
  - Verify thruster activates
  - While running, press Bow PORT button (GPIO 15)

Expected:
  - PORT command from button wins
  - STARBOARD is overridden
  - Port direction activates
  - Remote takes priority

Success:
  - ✅ Remote priority verified
  - ✅ Safety by priority confirmed
```

### 2.8.4: Emergency Stop (Affects Bow Thruster)

**Test: Emergency Stop Halts Bow Thruster**
```
Setup:
  - Activate BOW_PORT via SignalK
  - Motor running
  - Send emergency stop command

Command (SignalK):
  PUT /navigation/bow/ecu/emergencyStopCommand = true

Expected:
  - Both BOW_PORT and BOW_STARBOARD → HIGH immediately
  - Motor stops within 50ms
  - Serial log: "EMERGENCY STOP - shutting down all systems"
  - SignalK output: /navigation/bow/ecu/emergencyStopStatus = true
```

**Test: Bow Thruster Blocked During Emergency Stop**
```
Setup:
  - Emergency stop is active
  - Try to send PORT/STARBOARD commands

Expected:
  - Motor doesn't activate
  - Relays stay HIGH (inactive)
  - Logs show: "Command rejected - emergency stop active"
```

**Test: Unified Emergency Stop (Both Systems)**
```
Setup:
  - Anchor windlass running (WINCH_UP active)
  - Bow thruster running (BOW_PORT active)
  - Send emergency stop

Expected:
  - All 4 relay outputs → HIGH (all inactive)
  - Both systems stop simultaneously (atomic)
  - Logs confirm both systems disabled
  - No partial stop condition

Success:
  - ✅ Atomic emergency stop verified
  - ✅ Both subsystems affected equally
```

### 2.8.5: SignalK Integration (Bow Thruster)

**Test: Bow Status Output**
```
Setup:
  - Activate STARBOARD via remote
  - Subscribe to propulsion.bowThruster.direction

Expected in SignalK:
  - propulsion.bowThruster.direction = "starboard"
  
Switch direction:
  - Send PORT command
  - propulsion.bowThruster.direction = "port"

Switch to STOP:
  - Send STOP command
  - propulsion.bowThruster.direction = "stopped"

Success:
  - ✅ Status reflects actual direction
  - ✅ Updates within 100ms of command
```

**Test: SignalK Command Values**
```
Verify command mapping:
  - PUT command = "port" → PORT
  - PUT command = "stop" → STOP
  - PUT command = "starboard" → STARBOARD
  - All other values → STOP (safe fallback)
```

**Test: Connection Loss Blocking**
```
Setup:
  - Device connected to SignalK
  - Send PORT command (thruster activates)
  - Disconnect SignalK (stop server/reboot)
  - Try to send STARBOARD command via SignalK

Expected:
  - Command blocked (no active connection)
  - Thruster stays in current state (PORT)
  - Logs show: "SignalK disconnected - blocking new commands"
  - After reconnect, commands accepted again
```

---

## 3. Load Testing

### 3.1 Sustained Motor Operation (Both Systems)
```
Objective: Motor stability under continuous operation

Test (Anchor + Bow):
  - Activate anchor WINCH_UP at 50% duty
  - Activate bow PORT at 50% duty  
  - Run both for 5 minutes continuous
  - Monitor:
    - Temperature (device shouldn't get hot)
    - Current draw (total amperage)
    - Watchdog resets (should be zero)
    - Serial logs (should be smooth)

Success:
  - ✅ No watchdog resets
  - ✅ No temperature warnings
  - ✅ Smooth operation throughout
  - ✅ Both systems operate independently
```

### 3.2 Rapid Command Switching (Both Systems)
```
Objective: Handle fast mode changes without glitches

Test (Interleaved):
  - Anchor: UP, STOP, DOWN, STOP
  - Bow: PORT, STOP, STARBOARD, STOP
  - Interleave commands (send alternating)
  - Repeat 10 times in rapid succession

Success:
  - ✅ All transitions smooth
  - ✅ No stuck states in either system
  - ✅ GPIO changes correct on all pins
  - ✅ No motor "jerks"
  - ✅ Systems independent (one doesn't affect the other)
```

### 3.3 High Pulse Rate
```
Objective: ISR handling at high frequencies

Setup:
  - Pulse input at 100 Hz (10 meter/sec equivalent)
  - Direction alternating

Expected:
  - All pulses counted correctly
  - No dropped counts
  - No overflow issues
  - Rode length accurate
```

---

## 4. Safety Validation

### 4.1 Relay Defaults
```
Test:
  - Power cycle device 10 times
  - Measure relay GPIO pins on each boot

Expected:
  - ✅ All relays HIGH (inactive) on every boot
  - ✅ No momentary activation
  - ✅ Safe by default always
```

### 4.2 Watchdog/Crash Recovery
```
Test:
  - Monitor for watchdog resets (logs should show)
  - If watchdog triggers, device reboots to safe state
  - Verify recovery: all relays inactive, system responsive

Expected:
  - ✅ No unnecessary resets during normal operation
  - ✅ Recovery is clean and safe
  - ✅ No data corruption
```

### 4.3 Power Supply Isolation
```
Test (if relay module isolated):
  - Monitor current draw under different conditions
  - Peak current: motor active + all outputs
  - Sustained current: idle with WiFi connected

Expected:
  - ✅ USB power sufficient for operation
  - ✅ No voltage sag on device
  - ✅ Relay isolation working (galvanic separation)
```

---

## 5. Performance Metrics

### 5.1 Timing Measurements

| Metric | Target | Tolerance |
|--------|--------|-----------|
| WiFi connection | 10-15s | < 20s OK |
| SignalK connect | 5-10s | < 15s OK |
| Manual control latency | < 100ms | < 200ms acceptable |
| Emergency stop latency | < 100ms | < 200ms critical |
| Pulse ISR response | < 100µs | < 1000µs OK |
| Rode length update | < 100ms | < 500ms OK |

### 5.2 Memory Usage

```
Expected (from Phase 3):
  - Flash: ~70% (69.8 KB / ~100 KB available)
  - RAM: ~12% (30+ KB / 250+ KB available)

Monitor during runtime:
  - No progressive RAM loss (heap fragmentation)
  - No stack overflows
  - Stable operation > 1 hour
```

### 5.3 Power Consumption

| State | Current |  Notes |
|-------|---------|--------|
| Idle (WiFi connected) | 40-60 mA | Normal operation |
| Motor UP | ~200-300 mA | Depends on motor load |
| Motor DOWN | ~200-300 mA | May vary by load |
| WiFi searching | 200+ mA | Higher power during connect |

---

## 6. Troubleshooting Guide

### Issue: Device boots but motor won't respond to commands

**Possible Causes**:
1. Relay module not powered (check external 12V supply)
2. Motor GPIO pins damaged (test with multimeter)
3. Remote control override active (press release to deactivate)
4. Emergency stop active (send clear command)
5. Home sensor triggering falsely (check wiring)

**Debug Steps**:
```cpp
// Check logs for errors
Serial.println("Debug: Checking motor relay state");
digitalWrite(WINCH_UP, LOW);   // Force relay on
delay(100);
// Use multimeter to verify relay activation
```

### Issue: Pulse count wrong or stuck

**Possible Causes**:
1. PULSE_INPUT pin floating or damaged
2. Direction pin stuck
3. ISR not firing
4. Pull-up resistors weak

**Debug Steps**:
```cpp
// Monitor pulse ISR in logs
debugD("Pulse ISR firing: count=%d", pulse_count);
// Check pin states with logic analyzer
```

### Issue: SignalK won't connect

**Possible Causes**:
1. WiFi not connected (check logs for WiFi status)
2. SignalK server IP/port wrong
3. Device on wrong network
4. Firewall blocking connection

**Debug Steps**:
```
In logs, should see:
  - "WiFi connected: IP 192.168.x.x"
  - "Connecting to SignalK: 192.168.x.x:3000"
  - "SignalK connected"
```

---

## 7. Test Documentation

### For Each Test, Record:
- ✅ Test name and objective
- ✅ Procedure followed
- ✅ Results (pass/fail)
- ✅ Any anomalies or timing issues
- ✅ Device logs (save serial output if test fails)
- ✅ Date and firmware version tested

### Create Test Log File:
```
test_log_2026-02-16.txt:

Test: Manual UP Command
  - Sent command via SignalK
  - Expected: WINCH_UP → LOW
  - Observed: WINCH_UP → LOW at T+45ms
  - Status: ✅ PASS
  
Test: Emergency Stop
  - Sent emergency stop command
  - Expected: All relays → HIGH
  - Observed: Motor stopped, relays HIGH
  - Latency: 87ms
  - Status: ✅ PASS
  
...etc
```

---

## 8. Success Criteria (Phase 5 Complete)

Phase 5 is complete when:

✅ Hardware Initialization
  - Device boots safely every time
  - All relays inactive on start-up
  - No watchdog resets during normal operation

✅ Pulse Counting
  - Accurate within ±0.02m over 1+ meter range
  - Both directions (chain out/in) work
  - Home sensor prevents unsafe states

✅ Manual Motor Control
  - UP/DOWN/STOP commands work via SignalK
  - Motor responds within 100ms
  - Smooth transitions between modes

✅ Emergency Stop
  - Activates immediately (< 100ms)
  - Prevents all control
  - Clear command restores control

✅ Automatic Mode
  - Deploys to target accurately
  - Retrieves to target accurately
  - Respects home sensor limits
  - Manual override works

✅ SignalK Integration
  - Device connects to SignalK server
  - All status inputs/outputs work
  - Commands processed correctly
  - Connection recovers from WiFi loss

✅ Safety
  - All relays default inactive on boot
  - No dangerous floating states
  - Graceful error handling
  - No crashes in 1+ hour sustained operation

✅ Performance
  - Manual control latency < 150ms
  - Emergency stop latency < 100ms
  - Pulse ISR no dropped counts
  - Memory stable (no leaks)

---

## Next Steps

After Phase 5 validation:
- **Phase 6**: Deploy to boat and perform real-world field testing
- Create user documentation and operation manual
- Set up OTA update mechanism for firmware patches
- Monitor system in production for 1+ week before considering "stable"

