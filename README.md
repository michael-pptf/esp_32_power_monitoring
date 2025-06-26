# ESP32 Power Monitor - Pull Model

A power consumption monitoring system using ESP32 and PZEM-004T v3.0 that serves data via HTTP API.

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
- ✅ **Memory efficient** - Circular buffer for 100 data points
- ✅ **Configurable intervals** - 1-60 second sampling rates

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

## 🚀 Quick Start

### 1. Upload the Code
1. Open `power_monitoring.ino` in Arduino IDE
2. Install required libraries:
   - `PZEM004Tv30` by `olehs`
   - `WiFi` (built-in)
   - `WebServer` (built-in)
3. Update WiFi credentials:
   ```cpp
   const char* ssid = "YOUR_WIFI_SSID";
   const char* password = "YOUR_WIFI_PASSWORD";
   ```
4. Upload to ESP32

### 2. Test Basic Functionality
```bash
# Get current power reading
curl http://192.168.1.236/data

# Check system status
curl http://192.168.1.236/status
```

### 3. Test Recording Mode
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

#### 2. WiFi Connection Issues
- Verify SSID and password
- Check WiFi signal strength
- Ensure network allows HTTP traffic

#### 3. Memory Issues
- Monitor heap usage in serial output
- Reduce recording buffer size if needed
- Restart ESP32 if memory gets low

### Debug Endpoints:
```bash
# Check PZEM communication
curl http://ESP_IP/pzem_test

# Check system status
curl http://ESP_IP/status
```

## ⚙️ Configuration

### Recording Settings:
- **Default interval**: 5 seconds
- **Configurable range**: 1-60 seconds
- **Buffer size**: 100 records (circular)
- **Memory usage**: ~4KB for data storage

### WiFi Settings:
- **SSID**: Update in code
- **Password**: Update in code
- **IP Address**: DHCP assigned (check serial output)

## 📈 Performance

### Sampling Rates:
- **1 second**: High detail, 100 records = 1.7 minutes
- **2 seconds**: Good balance, 100 records = 3.3 minutes
- **5 seconds**: Standard, 100 records = 8.3 minutes
- **10 seconds**: Long-term, 100 records = 16.7 minutes

### Memory Usage:
- **Base system**: ~200KB
- **Data buffer**: ~4KB
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
