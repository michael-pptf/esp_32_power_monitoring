# ESP32 Power Monitor - Pull Model

A power consumption monitoring system using ESP32 and PZEM-004T v3.0 that serves data via HTTP API.

See my blog post here for more details: [Building a Power Monitoring System with ESP32 and PZEM-004T: A Pull-Based Approach](https://www.michaelstinkerings.org/building-a-power-monitoring-system-with-esp32-and-pzem-004t-a-pull-based-approach/)

## 🎯 Overview

The **Pull Model** provides a web server interface where external systems can request power data on-demand. This approach is ideal for:
- **Pull Model** - Measure idle/load power consumption by polling
- **On-demand Monitoring** - Get data only when needed
- **Cost-effective Setup** - No external database required

## 🔧 Hardware Requirements

### Components:
- **ESP32 Development Board** (any model with Serial2, I prefer WiFi models for convenience)
- **PZEM-004T v3.0** Power Monitor
- **Power Supply** from ESP32 board 3.3V and GND
- **AC Power Connection** (for PZEM to measure)

### Wiring:
```
PZEM-004T v3.0    →    ESP32
VCC               →    5V pin (IMPORTANT: Use 5V, not 3.3V!)
GND               →    GND
TX                →    GPIO 16 (RX2)
RX                →    GPIO 17 (TX2)
```

## 📋 Features

### Core Functionality:
- ✅ **Real-time power readings** - Voltage, current, power, energy, frequency, PF
- ✅ **HTTP API endpoints** - RESTful interface for data access
- ✅ **Recording mode** - Start/stop data collection with timestamps
- ✅ **PC review analysis** - Min/max/avg power calculations
- ✅ **Memory efficient** - Circular buffer for 150 data points (increased from 100)
- ✅ **Configurable intervals** - 1-60 second sampling rates
- ✅ **Smart UI** - Conditional recording buttons and poll interval selection

### API Endpoints:

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/` | GET | Basic info page with available endpoints |
| `/data` | GET | Get latest power reading (forces fresh read) |
| `/read` | GET | Force new reading and return data |
| `/status` | GET | System status (WiFi, memory, uptime) |
| `/pzem_test` | GET | Detailed PZEM communication diagnostics |
| `/record/start` | POST | Start recording with configurable interval |
| `/record/stop` | POST | Stop recording |
| `/record/status` | GET | Recording status and statistics |
| `/record/data` | GET | Get all recorded data points |
| `/record/clear` | POST | Clear all recorded data |
| `/analysis` | GET | Power analysis (min/max/avg/total energy) |

## 🎨 Frontend Features

### Web Dashboard:
- **Real-time Monitoring** - Live power consumption display with auto-refresh
- **Conditional Recording Controls** - Single button that toggles between Start/Stop recording
- **Configurable Poll Intervals** - Dropdown to select 1, 2, 5, 10, 15, 30, or 60 second intervals
- **Recording Status** - Visual indicators for active/inactive recording state
- **Data Export** - CSV download functionality for recorded data
- **Power Analysis** - Min/max/average power calculations with duration tracking
- **Responsive Design** - Mobile-friendly interface with modern UI

### Interactive Elements:
- **Status Indicators** - WiFi connection and data validity status
- **Real-time Updates** - Auto-refresh every 5 seconds
- **Error Handling** - User-friendly error messages and status feedback
- **Confirmation Dialogs** - Safe data clearing with confirmation prompts

## 🔌 MQTT Integration

### Home Assistant Auto-Discovery:
The ESP32 automatically publishes device configuration to Home Assistant using MQTT Discovery, creating the following sensors:

#### Power Sensors:
- **Power Consumption** (`sensor.esp32_power_monitor_001_power`) - Real-time power in Watts
- **Voltage** (`sensor.esp32_power_monitor_001_voltage`) - AC voltage in Volts
- **Current** (`sensor.esp32_power_monitor_001_current`) - Current draw in Amperes
- **Energy Consumption** (`sensor.esp32_power_monitor_001_energy`) - Total energy in kWh
- **Frequency** (`sensor.esp32_power_monitor_001_frequency`) - AC frequency in Hz
- **Power Factor** (`sensor.esp32_power_monitor_001_pf`) - Power factor (0-1)

#### Status Sensors:
- **Recording Status** (`binary_sensor.esp32_power_monitor_001_recording`) - ON/OFF recording state
- **WiFi Status** (`sensor.esp32_power_monitor_001_wifi`) - WiFi connection status
- **Memory Usage** (`sensor.esp32_power_monitor_001_memory`) - Free heap memory
- **Uptime** (`sensor.esp32_power_monitor_001_uptime`) - Device uptime in milliseconds

### MQTT Topics:

#### Data Topics (Published by ESP32):
```
esp32/power/voltage      - Voltage reading (V)
esp32/power/current      - Current reading (A)
esp32/power/power        - Power consumption (W)
esp32/power/energy       - Energy consumption (kWh)
esp32/power/frequency    - AC frequency (Hz)
esp32/power/pf           - Power factor
esp32/status/recording   - Recording status (ON/OFF)
esp32/status/wifi        - WiFi status (ON/OFF)
esp32/status/memory      - Free memory (bytes)
esp32/status/uptime      - Uptime (ms)
```

#### Command Topics (Subscribed by ESP32):
```
esp32/command/record/start  - Start recording (send "true" or "1")
esp32/command/record/stop   - Stop recording (send "true" or "1")
esp32/command/interval      - Set recording interval (send milliseconds)
```

### Configuration:
Update the MQTT settings in the firmware:
```cpp
const char* mqtt_server = "192.168.1.100";  // Your Home Assistant IP
const int mqtt_port = 1883;
const char* mqtt_username = "";  // Leave empty if no authentication
const char* mqtt_password = "";  // Leave empty if no authentication
```

### Home Assistant Setup:
1. **Install MQTT Integration** - Add MQTT integration in Home Assistant
2. **Configure MQTT Broker** - Point to your MQTT broker (usually Home Assistant's built-in broker)
3. **Auto-Discovery** - Devices will appear automatically in the MQTT integration
4. **Create Dashboards** - Use the discovered sensors in your Home Assistant dashboards

### Example Home Assistant Automations:
```yaml
# Alert when power consumption exceeds 1000W
automation:
  - alias: "High Power Alert"
    trigger:
      platform: numeric_state
      entity_id: sensor.esp32_power_monitor_001_power
      above: 1000
    action:
      - service: notify.mobile_app
        data:
          message: "High power consumption detected: {{ states('sensor.esp32_power_monitor_001_power') }}W"

# Start recording when power exceeds 500W
automation:
  - alias: "Start Power Recording"
    trigger:
      platform: numeric_state
      entity_id: sensor.esp32_power_monitor_001_power
      above: 500
    action:
      - service: mqtt.publish
        data:
          topic: esp32/command/record/start
          payload: "true"
```

## 🚀 Quick Start

### 1. Configuration Setup
1. Copy the configuration template:
   ```bash
   cp esp32/power_monitor/config.h.template esp32/power_monitor/config.h
   ```
2. Edit `esp32/power_monitor/config.h` with your credentials:
   ```cpp
   // Wi-Fi credentials
   const char* ssid = "YOUR_WIFI_SSID";
   const char* password = "YOUR_WIFI_PASSWORD";
   
   // MQTT Configuration
   const char* mqtt_server = "192.168.1.100";  // Your Home Assistant IP
   const char* mqtt_username = "your_mqtt_username";  // Leave empty if no authentication
   const char* mqtt_password = "your_mqtt_password";  // Leave empty if no authentication
   ```

### 2. Upload the Code
1. Open `esp32/power_monitor/power_monitor.ino` in Arduino IDE
2. Install required libraries:
   - `PZEM004Tv30` by `olehs`
   - `WiFi` (built-in)
   - `WebServer` (built-in)
   - `PubSubClient` by `Nick O'Leary`
3. Upload to ESP32

### 3. Test Basic Functionality
```bash
# Get current power reading
curl http://192.168.1.236/data

# Check system status
curl http://192.168.1.236/status
```

### 4. Test Recording Mode
```bash
# Start recording every 2 seconds
curl -X POST http://192.168.1.236/record/start \
  -H "Content-Type: application/json" \
  -d '{"interval": 2000}'

# Wait for data collection...

# Stop recording
curl -X POST http://192.168.1.236/record/stop

# Get analysis
curl http://192.168.1.236/analysis
```

## 📊 Data Format

### Single Reading (`/data`, `/read`)
```json
{
  "voltage": 120.5,
  "current": 2.34,
  "power": 282.0,
  "energy": 0.123,
  "frequency": 60.0,
  "power_factor": 0.95,
  "address": 7,
  "timestamp": 1234567,
  "valid": true
}
```

### Recording Analysis (`/analysis`)
```json
{
  "analysis": {
    "min_power": 45.2,
    "max_power": 285.7,
    "avg_power": 165.3,
    "total_energy": 0.023,
    "duration_ms": 12500,
    "duration_seconds": 12.5,
    "data_points": 13,
    "start_time": 1000,
    "end_time": 13500
  }
}
```

### Recorded Data (`/record/data`)
```json
{
  "records": [
    {
      "timestamp": 5000,
      "voltage": 120.5,
      "current": 2.34,
      "power": 282.0,
      "energy": 0.123,
      "frequency": 60.0,
      "power_factor": 0.95,
      "address": 7
    }
  ],
  "total_records": 12,
  "buffer_full": false
}
```

## 🔍 Troubleshooting

### Common Issues:

#### 1. PZEM Not Responding
- **Check power**: PZEM needs 5V, but can be powered by 3.3V from the board
- **Check wiring**: TX→RX2 (GPIO 16), RX→TX2 (GPIO 17)
- **Check AC power**: PZEM must be connected to live AC
- **Check baud rate**: PZEM-004T v3.0 typically uses 9600 baud
- **Check address**: Default address is 0x01, but can be changed to 0x07

#### 2. Energy Reset Not Working
If you get "No response received from PZEM" when trying to reset energy:
- **Check PZEM power**: Ensure PZEM is properly powered and connected to AC
- **Check wiring**: Verify TX/RX connections are correct
- **Try diagnostic endpoint**: Use `/pzem/diagnostic` to test communication
- **Check address**: Ensure PZEM address matches configuration (default 0x07)
- **Try multiple attempts**: The reset function now retries 3 times automatically

#### 3. WiFi Connection Issues
- Verify SSID and password in `config.h`
- Check WiFi signal strength
- Ensure network allows HTTP traffic

#### 4. Memory Issues
- Monitor heap usage in serial output
- Reduce recording buffer size if needed
- Restart ESP32 if memory gets low

### Debug Endpoints:
```bash
# Check PZEM communication
curl http://ESP_IP/pzem_test

# Comprehensive PZEM diagnostic
curl http://ESP_IP/pzem/diagnostic

# Modbus-RTU specific diagnostic
curl http://ESP_IP/pzem/modbus_test

# Test library reset function
curl -X POST http://ESP_IP/pzem/reset_library

# Test comprehensive reset (multiple methods)
curl -X POST http://ESP_IP/pzem/reset_energy

# Check system status
curl http://ESP_IP/status
```

### PZEM Diagnostic Response:
The `/pzem/diagnostic` endpoint provides detailed information:
```json
{
  "diagnostic": {
    "serial2_status": "OK",
    "pzem_address": "0x07",
    "address_set_test": "SUCCESS",
    "voltage_reading": "120.5V",
    "current_reading": "2.34A",
    "power_reading": "282.0W",
    "energy_reset_test": "SUCCESS",
    "serial2_buffer": "0 bytes available",
    "free_heap": "250000 bytes"
  }
}
```

### Modbus-RTU Test Response:
The `/pzem/modbus_test` endpoint provides Modbus-RTU specific information:
```json
{
  "modbus_test": {
    "protocol": "Modbus-RTU",
    "baud_rate": "9600",
    "data_bits": "8",
    "stop_bits": "1",
    "parity": "None",
    "address_read": "0x07",
    "address_set": "SUCCESS",
    "energy_reset_detailed": "SUCCESS",
    "voltage_modbus": "120.5V",
    "current_modbus": "2.34A",
    "power_modbus": "282.0W"
  }
}
```

### PZEM Modbus-RTU Protocol:
The PZEM-004T v3.0 uses Modbus-RTU protocol:
- **Baud Rate**: 9600
- **Data Bits**: 8
- **Stop Bits**: 1
- **Parity**: None
- **Function Code 0x42**: Reset energy counter
- **Address Range**: 0x01-0xF7 (0x00 = broadcast, 0xF8 = general address)
- **CRC**: Modbus-RTU CRC16 (polynomial 0xA001)

### Common PZEM Issues and Solutions:

| Issue | Symptoms | Solution |
|-------|----------|----------|
| **No Response** | Address reads 0x00 | Check power, wiring, AC connection |
| **Wrong Address** | Address not 0x07 | Use `/pzem/diagnostic` to set address |
| **Reset Fails** | "No response" error | Check AC power, try multiple times |
| **NaN Readings** | Invalid readings | Check AC connection, restart PZEM |
| **Intermittent** | Works sometimes | Check wiring, power supply stability |

## ⚙️ Configuration

### Configuration File:
All credentials and settings are stored in `esp32/power_monitor/config.h`:

```cpp
// Wi-Fi credentials
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// MQTT Configuration
const char* mqtt_server = "192.168.1.100";  // Your Home Assistant IP
const int mqtt_port = 1883;
const char* mqtt_username = "your_mqtt_username";
const char* mqtt_password = "your_mqtt_password";
const char* mqtt_client_id = "esp32_power_monitor";
const char* mqtt_device_name = "ESP32 Power Monitor";
const char* mqtt_device_id = "esp32_power_monitor_001";

// MQTT Topics
const char* mqtt_base_topic = "esp32/power";
const char* mqtt_status_topic = "esp32/status";
const char* mqtt_command_topic = "esp32/command";
const char* mqtt_discovery_prefix = "homeassistant";

// Time configuration
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -28800; // GMT-8 for Los Angeles (PST)
const int daylightOffset_sec = 3600; // 1 hour for PDT
const char* timezone = "America/Los_Angeles";

// PZEM Configuration
const uint8_t pzem_address = 0x07; // PZEM address
```

**Security Note**: The `config.h` file is excluded from version control to prevent credentials from being committed. Always use the template file to create your configuration.

### Recording Settings:
- **Default interval**: 5 seconds
- **Configurable range**: 1-60 seconds
- **Buffer size**: 100 records (circular)
- **Memory usage**: ~4KB for data storage

## 📈 Performance

### Sampling Rates:
- **1 second**: High detail, 150 records = 2.5 minutes
- **2 seconds**: Good balance, 150 records = 5 minutes
- **5 seconds**: Standard, 150 records = 12.5 minutes
- **10 seconds**: Long-term, 150 records = 25 minutes

### Memory Usage:
- **Base system**: ~200KB
- **Data buffer**: ~6KB (increased from ~4KB)
- **Web server**: ~50KB
- **Available heap**: ~250KB typical

## 🔄 vs Push Model

| Feature | Pull Model | Push Model |
|---------|------------|------------|
| **Setup** | Simple | Complex (InfluxDB) |
| **Cost** | Low | High (server) |
| **Data Control** | On-demand | Continuous |
| **Historical Data** | Limited (100 records) | Unlimited |
| **Real-time Access** | Yes | Yes |
| **PC Reviews** | Perfect | Overkill |
| **Production Monitoring** | Basic | Professional |

## 📝 License

This project is licensed under GPL 3.0.

## 🤝 Contributing

Contributions welcome! Areas for improvement:
- Additional analysis endpoints
- Data export formats
- Web interface improvements
- Mobile app integration

---

**Happy Power Monitoring! ⚡** 
