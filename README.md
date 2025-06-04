# FAO56 Pump Control System

An ESP32-based smart irrigation pump controller that receives commands via MQTT and manages automated irrigation cycles. This is part of the FAO56 IoT system for precision agricultural irrigation management.

## Overview

The Pump Control System acts as the final actuator in the FAO56 irrigation network. It receives irrigation commands from the central system via MQTT, controls a relay to operate irrigation pumps, and provides real-time status feedback. The system includes time-based safety features, web-based configuration, and automatic scheduling validation.

## Architecture

```
[Central System] ---> [MQTT Broker] ---> [Pump Controller] ---> [Irrigation Pump]
    (Commands)         (Network)          (This Device)         (Physical Output)
```

## Features

- **MQTT Communication**: Receives irrigation commands and publishes status updates
- **Web Configuration Portal**: Easy setup via captive portal interface
- **Time-Based Safety**: Restricts irrigation to safe hours (7-9 AM, 4-7 PM)
- **Emergency Controls**: Immediate halt and resume functionality
- **Duration Limits**: Configurable minimum and maximum irrigation times
- **Real-Time Status**: Continuous monitoring and reporting
- **Persistent Configuration**: Settings stored in SPIFFS flash memory
- **Manual Override**: Physical button for configuration access

## Hardware Requirements

- ESP32 development board (any variant)
- Relay module (5V or 3.3V compatible)
- Power supply (12V/24V for pump, 5V for ESP32)
- Irrigation pump or solenoid valve
- Optional: Status LED, configuration button
- Waterproof enclosure for field deployment

## Pin Configuration

```cpp
#define RELAY_PIN 32        // Relay control pin
#define LED_PIN 2           // Status LED pin (built-in)
#define CONFIG_BUTTON_PIN 0 // Configuration button (BOOT button)
```

## Safety Parameters

```cpp
#define minIrrMinutes 0     // Minimum irrigation time
#define maxIrrMinutes 480   // Maximum irrigation time (8 hours)

// Allowed irrigation hours
// Morning: 7:00 AM - 9:00 AM
// Evening: 4:00 PM - 7:00 PM
```

## Configuration Structure

```cpp
struct Config {
    String deviceId;        // Pump identifier (e.g., "P-1")
    String wifiSSID;        // WiFi network name
    String wifiPassword;    // WiFi network password
    String mqttServer;      // MQTT broker IP address
    int mqttPort;           // MQTT broker port (default: 1883)
    String mqttTopicSub;    // Subscribe topic for commands
    String mqttTopicPub;    // Publish topic for status updates
};
```

## System States

The pump controller operates in four distinct states:

1. **IDLE**: Ready to receive commands, pump off
2. **IRRIGATING**: Active irrigation cycle, pump running
3. **EMERGENCY_HALT**: Irrigation paused, can be resumed
4. **FAULT**: Error state, pump disabled

## MQTT Communication

### Command Topic (Subscribe)
**Topic**: `topic/pump/command` (configurable)

**Command Format**:
```json
{
    "id": "P-1",
    "signal": "On",
    "irr_time": 30.0
}
```

**Supported Signals**:
- `"On"` - Start irrigation (requires `irr_time` in minutes)
- `"Emergency Halt"` - Pause current irrigation
- `"Stop"` - Stop and reset irrigation

### Status Topic (Publish)
**Topic**: `topic/pump/status` (configurable)

**Status Format**:
```json
{
    "id": "P-1",
    "state": "IRRIGATING",
    "pump_active": true,
    "remaining_time_minutes": 25,
    "irrigation_allowed": true,
    "current_time": "14:30:45"
}
```

## Configuration Portal

### Automatic Portal Activation
- Activates automatically when no valid configuration exists
- Creates WiFi AP: `PumpConfig` with password: `12345678`

### Manual Portal Activation
**Button Method**:
1. Press and hold the configuration button (GPIO 0)
2. Portal starts immediately when button is pressed
3. Connect to `PumpConfig` WiFi network
4. Navigate to `http://192.168.4.1` for configuration

### Web Interface Features
- **Current Status Display**: Shows existing configuration
- **Form Validation**: Ensures all required fields are filled
- **Password Security**: Existing passwords are hidden but preserved
- **Immediate Restart**: Device restarts after saving configuration
- **Factory Reset**: Option to clear all settings

## Operation Flow

1. **Boot Sequence**: Initialize hardware and load configuration
2. **Configuration Check**: Start portal if no valid config exists
3. **Network Connection**: Connect to configured WiFi network
4. **MQTT Setup**: Connect to broker and subscribe to command topic
5. **Time Synchronization**: Sync with NTP server for accurate timing
6. **Command Processing**: 
   - Validate device ID matches
   - Check irrigation time limits
   - Verify current time is within allowed hours
   - Execute irrigation or return error
7. **Status Monitoring**: Continuously monitor and report system state

## Command Validation

The system performs multiple validation checks:

1. **Device ID**: Command must match configured device ID
2. **Time Range**: Irrigation only allowed during safe hours
3. **Duration Limits**: Must be within min/max irrigation time
4. **State Validation**: Commands only accepted in appropriate states

## Building and Deployment

### Prerequisites
- PlatformIO IDE or Arduino IDE with ESP32 support
- Required libraries:
  - WiFi (ESP32 built-in)
  - PubSubClient (MQTT client)
  - ArduinoJson (JSON parsing)
  - SPIFFS (Configuration storage)
  - WebServer (Configuration portal)

### Build Configuration
Project uses PlatformIO with configuration in platformio.ini:
- Target: ESP32 (any variant)
- Framework: Arduino
- Monitor speed: 115200 baud

### Upload Process
1. Connect ESP32 to computer via USB
2. Select correct COM port and board
3. Build and upload firmware
4. Connect relay module to GPIO 32
5. Connect pump/valve to relay output
6. Power on and configure via web portal

### Hardware Setup
1. **Relay Connection**: GPIO 32 to relay control input
2. **Power Supply**: 
   - ESP32: 5V via USB or VIN pin
   - Relay: 5V or 3.3V depending on module
   - Pump: 12V/24V/120V/240V as required
3. **Safety**: Use appropriate fuses and contactors for high-power pumps

## Field Deployment

### Installation Checklist
- [ ] Waterproof enclosure installed
- [ ] All electrical connections secured
- [ ] Pump operation tested manually
- [ ] WiFi signal strength verified
- [ ] MQTT connectivity confirmed
- [ ] Time synchronization working
- [ ] Test irrigation cycle completed

### Safety Considerations
- Install appropriate electrical protection
- Ensure proper grounding of all equipment
- Use weatherproof connectors for outdoor installation
- Test emergency stop functionality
- Verify pump pressure relief systems

## Troubleshooting

### Common Issues

1. **Configuration Portal Won't Start**
   - Press and hold GPIO 0 button during power-on
   - Check serial monitor for debug messages
   - Verify SPIFFS is functioning

2. **WiFi Connection Failed**
   - Check signal strength at installation location
   - Verify SSID and password in configuration
   - Try manual portal activation

3. **MQTT Commands Not Received**
   - Verify broker IP address and port
   - Check topic configuration matches central system
   - Monitor serial output for connection status

4. **Pump Won't Start**
   - Check relay connections and power supply
   - Verify current time is within allowed hours
   - Ensure irrigation duration is within limits
   - Check device ID in MQTT commands

5. **Time Synchronization Issues**
   - Verify internet connectivity
   - Check NTP server accessibility
   - Monitor serial output for time sync status

### Debug Information
Monitor serial output at 115200 baud for:
- Configuration loading status
- WiFi connection attempts
- MQTT broker connections
- Command reception and validation
- State transitions and timing
- Button press detection

### Recovery Procedures
1. **Factory Reset**: Use web portal reset button
2. **Manual Reconfiguration**: Hold button to access portal
3. **SPIFFS Issues**: Reflash firmware to rebuild filesystem
4. **Network Issues**: Device automatically attempts reconnection

## Integration

This pump controller integrates with:
- **FAO56 Central System**: Receives irrigation schedules and commands
- **MQTT Brokers**: Local Mosquitto, cloud services, or embedded brokers
- **Monitoring Systems**: Status updates for dashboards and logging
- **Mobile Apps**: Remote control and monitoring interfaces

## Performance Specifications

- **Response Time**: <1 second command processing
- **Accuracy**: ±1 second timing precision
- **Reliability**: Auto-reconnection for network failures
- **Power Consumption**: ~150mA average operation
- **Operating Temperature**: -10°C to +60°C
- **Maximum Irrigation**: 8 hours continuous operation
- **Network Range**: Standard WiFi coverage area

## License

Part of the FAO56 IoT agricultural monitoring and irrigation management system.
