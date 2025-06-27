// Increase MQTT buffer size for large discovery messages
#define MQTT_MAX_PACKET_SIZE 1024

#include <WiFi.h>
#include <WebServer.h>
#include <PZEM004Tv30.h>
#include <time.h>
#include <PubSubClient.h>

// Wi-Fi credentials
const char* ssid = "YOUR_SSID";
const char* password = "YOUR_PASSWORD";

// MQTT Configuration
const char* mqtt_server = "YOUR_MQTT_SERVER";  // Change to your Home Assistant IP
const int mqtt_port = 1883;
const char* mqtt_username = "YOUR_MQTT_USERNAME";  // Leave empty if no authentication
const char* mqtt_password = "YOUR_MQTT_PASSWORD";  // Leave empty if no authentication
const char* mqtt_client_id = "esp32_power_monitor";
const char* mqtt_device_name = "ESP32 Power Monitor";
const char* mqtt_device_id = "esp32_power_monitor_001";

// MQTT Topics
const char* mqtt_base_topic = "esp32/power";
const char* mqtt_status_topic = "esp32/status";
const char* mqtt_command_topic = "esp32/command";
const char* mqtt_discovery_prefix = "homeassistant";

// MQTT Client
WiFiClient espClient;
PubSubClient mqtt_client(espClient);

// MQTT State
bool mqtt_connected = false;
unsigned long last_mqtt_publish = 0;
const unsigned long mqtt_publish_interval = 5000; // Publish every 5 seconds
unsigned long last_discovery_publish = 0;
const unsigned long discovery_republish_interval = 300000; // Re-publish discovery every 5 minutes

// Time configuration, change if needed
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -28800; // GMT-8 for Los Angeles (PST)
const int daylightOffset_sec = 3600; // 1 hour for PDT
const char* timezone = "America/Los_Angeles";

// PZEM with specific address 0x07 - using Serial2 pins (RX2/TX2)
PZEM004Tv30 pzem(Serial2, 16, 17, 0x07); // RX2=16, TX2=17, Address=0x07

// Web server on port 80
WebServer server(80);

// Global variables to store latest readings
struct PowerData {
  float voltage = 0;
  float current = 0;
  float power = 0;
  float energy = 0;
  float frequency = 0;
  float pf = 0;
  uint8_t address = 0;
  unsigned long timestamp = 0;
  String isoTime = "";
  bool valid = false;
} latestData;

// Recording functionality
struct RecordingState {
  bool isRecording = false;
  unsigned long startTime = 0;
  unsigned long lastReadTime = 0;
  unsigned long interval = 5000; // Default 5 seconds
  int maxRecords = 150; // Maximum records to store (increased from 100)
  int recordCount = 0;
} recording;

// Data storage for recording
struct RecordedData {
  unsigned long timestamp;
  String isoTime;
  float voltage;
  float current;
  float power;
  float energy;
  float frequency;
  float pf;
  uint8_t address;
  bool valid;
};

// Circular buffer for storing recorded data
RecordedData recordedData[150]; // Store up to 150 records (increased from 100)
int dataIndex = 0;
bool bufferFull = false;

// Function declarations
void startRecording(int interval = 5000);
void stopRecording();
void readPowerData();
void storeRecordedData();
String getISOTime();
String getFormattedTime();

// MQTT Functions
void mqtt_callback(char* topic, byte* payload, unsigned int length) {
  Serial.printf("Debug: MQTT message received on topic: %s\n", topic);
  
  // Convert payload to string
  String message = "";
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  
  Serial.printf("Debug: MQTT message: %s\n", message.c_str());
  
  // Handle commands
  if (String(topic) == String(mqtt_command_topic) + "/record/start") {
    if (message == "true" || message == "1") {
      Serial.println("Debug: MQTT command - Start recording");
      // Parse interval from message if provided
      int interval = 5000; // default
      if (message.indexOf("interval:") != -1) {
        String intervalStr = message.substring(message.indexOf("interval:") + 9);
        interval = intervalStr.toInt();
      }
      startRecording(interval);
    }
  }
  else if (String(topic) == String(mqtt_command_topic) + "/record/stop") {
    if (message == "true" || message == "1") {
      Serial.println("Debug: MQTT command - Stop recording");
      stopRecording();
    }
  }
  else if (String(topic) == String(mqtt_command_topic) + "/interval") {
    int newInterval = message.toInt();
    if (newInterval >= 1000 && newInterval <= 60000) {
      Serial.printf("Debug: MQTT command - Set interval to %d ms\n", newInterval);
      recording.interval = newInterval;
    }
  }
}

void mqtt_connect() {
  Serial.println("Debug: Attempting MQTT connection...");
  
  if (mqtt_client.connect(mqtt_client_id, mqtt_username, mqtt_password)) {
    Serial.println("Debug: MQTT connected successfully!");
    mqtt_connected = true;
    
    // Subscribe to command topics
    mqtt_client.subscribe((String(mqtt_command_topic) + "/record/start").c_str());
    mqtt_client.subscribe((String(mqtt_command_topic) + "/record/stop").c_str());
    mqtt_client.subscribe((String(mqtt_command_topic) + "/interval").c_str());
    
    Serial.println("Debug: MQTT subscriptions created");
    
    // Publish device discovery
    publish_discovery();
    last_discovery_publish = millis();
    
  } else {
    Serial.printf("Debug: MQTT connection failed, rc=%d\n", mqtt_client.state());
    mqtt_connected = false;
  }
}

void publish_discovery() {
  Serial.println("Debug: Publishing Home Assistant discovery messages...");
  
  // Ensure MQTT client is connected and loop is called
  if (!mqtt_client.connected()) {
    Serial.println("Debug: MQTT not connected during discovery publish!");
    return;
  }
  
  mqtt_client.loop(); // Process any pending MQTT messages
  
  // Test simple publish first
  Serial.println("Debug: Testing simple MQTT publish...");
  bool test_result = mqtt_client.publish("esp32/test", "Hello MQTT", false);
  Serial.println("Debug: Test publish result: " + String(test_result));
  mqtt_client.loop();
  delay(500);
  
  // Simple device config
  String device_config = "{\"identifiers\":[\"" + String(mqtt_device_id) + "\"],\"name\":\"" + String(mqtt_device_name) + "\"}";
  
  // Publish just one sensor first to test
  String power_topic = String(mqtt_discovery_prefix) + "/sensor/" + mqtt_device_id + "_power/config";
  String power_config = "{\"device\":" + device_config + ",\"name\":\"Power\",\"state_topic\":\"" + String(mqtt_base_topic) + "/power\",\"unit_of_measurement\":\"W\"}";
  
  Serial.println("Debug: Publishing power sensor config...");
  bool power_result = mqtt_client.publish(power_topic.c_str(), power_config.c_str(), true);
  Serial.println("Debug: Power publish result: " + String(power_result));
  mqtt_client.loop();
  delay(1000);
  
  Serial.println("Debug: Discovery test completed");
}

void publish_mqtt_data() {
  if (!mqtt_connected || !latestData.valid) {
    return;
  }
  
  // Publish power data
  mqtt_client.publish((String(mqtt_base_topic) + "/voltage").c_str(), String(latestData.voltage, 2).c_str());
  mqtt_client.publish((String(mqtt_base_topic) + "/current").c_str(), String(latestData.current, 3).c_str());
  mqtt_client.publish((String(mqtt_base_topic) + "/power").c_str(), String(latestData.power, 2).c_str());
  mqtt_client.publish((String(mqtt_base_topic) + "/energy").c_str(), String(latestData.energy, 3).c_str());
  mqtt_client.publish((String(mqtt_base_topic) + "/frequency").c_str(), String(latestData.frequency, 1).c_str());
  mqtt_client.publish((String(mqtt_base_topic) + "/pf").c_str(), String(latestData.pf, 2).c_str());
  
  // Publish status
  mqtt_client.publish((String(mqtt_status_topic) + "/recording").c_str(), recording.isRecording ? "ON" : "OFF");
  mqtt_client.publish((String(mqtt_status_topic) + "/wifi").c_str(), WiFi.status() == WL_CONNECTED ? "ON" : "OFF");
  mqtt_client.publish((String(mqtt_status_topic) + "/memory").c_str(), String(ESP.getFreeHeap()).c_str());
  mqtt_client.publish((String(mqtt_status_topic) + "/uptime").c_str(), String(millis()).c_str());
  
  Serial.println("Debug: MQTT data published");
}

// Standalone recording functions for MQTT
void startRecording(int interval) {
  Serial.printf("Debug: startRecording called with interval %d ms\n", interval);
  
  if (recording.isRecording) {
    Serial.println("Debug: Recording already in progress");
    return;
  }
  
  // Set interval if provided
  if (interval >= 1000 && interval <= 60000) {
    recording.interval = interval;
  }
  
  // Start recording
  recording.isRecording = true;
  recording.startTime = millis();
  recording.lastReadTime = 0;
  recording.recordCount = 0;
  dataIndex = 0;
  bufferFull = false;
  
  Serial.printf("Debug: Recording started with interval %lu ms\n", recording.interval);
}

void stopRecording() {
  Serial.println("Debug: stopRecording called");
  
  if (!recording.isRecording) {
    Serial.println("Debug: No recording in progress");
    return;
  }
  
  recording.isRecording = false;
  unsigned long duration = millis() - recording.startTime;
  
  Serial.printf("Debug: Recording stopped. Duration: %lu ms, Records: %d\n", duration, recording.recordCount);
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=== ESP32 Power Monitor Starting ===");
  Serial.println("Debug: Serial communication initialized");

  // Initialize Serial2 for PZEM communication
  Serial.println("Debug: Initializing Serial2 for PZEM...");
  Serial2.begin(9600, SERIAL_8N1, 16, 17); // RX2=16, TX2=17
  delay(1000);
  Serial.println("Debug: Serial2 initialized");

  // Check memory at start of loop
  int startHeap = ESP.getFreeHeap();
  Serial.printf("Debug: Free heap at start: %d bytes\n", startHeap);

  // Connect to Wi-Fi
  Serial.println("Debug: Attempting WiFi connection...");
  Serial.printf("Debug: SSID: %s\n", ssid);
  WiFi.begin(ssid, password);
  
  int wifiAttempts = 0;
  while (WiFi.status() != WL_CONNECTED && wifiAttempts < 20) {
    delay(500);
    Serial.print(".");
    wifiAttempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nDebug: WiFi connected successfully!");
    Serial.print("Debug: IP Address: ");
    Serial.println(WiFi.localIP());
    
    // Initialize time synchronization
    Serial.println("Debug: Initializing time synchronization...");
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    
    // Wait for time to be set
    Serial.println("Debug: Waiting for NTP time sync...");
    int timeSyncAttempts = 0;
    while (!time(nullptr) && timeSyncAttempts < 10) {
      delay(1000);
      Serial.print(".");
      timeSyncAttempts++;
    }
    
    if (time(nullptr)) {
      Serial.println("\nDebug: Time synchronized successfully!");
      time_t now = time(nullptr);
      Serial.printf("Debug: Current time: %s", ctime(&now));
    } else {
      Serial.println("\nDebug: Time synchronization failed!");
    }
  } else {
    Serial.println("\nDebug: WiFi connection failed!");
    Serial.printf("Debug: WiFi status: %d\n", WiFi.status());
  }

  // Test PZEM communication
  Serial.println("Debug: Testing PZEM communication...");
  testPZEMCommunication();

  // Initialize MQTT
  Serial.println("Debug: Initializing MQTT...");
  mqtt_client.setServer(mqtt_server, mqtt_port);
  mqtt_client.setCallback(mqtt_callback);
  
  // Attempt MQTT connection
  mqtt_connect();
  
  // Force discovery publish after a short delay
  delay(2000);
  if (mqtt_connected) {
    Serial.println("Debug: Forcing discovery publish...");
    publish_discovery();
  }

  // Setup web server routes
  Serial.println("Debug: Setting up web server routes...");
  setupWebServer();

  // Start web server
  server.begin();
  Serial.println("Debug: HTTP server started on port 80");

  Serial.printf("Debug: Free heap after setup: %d bytes\n", ESP.getFreeHeap());
  Serial.println("Debug: Setup complete!");
}

void testPZEMCommunication() {
  Serial.println("Debug: === PZEM Communication Test ===");
  
  // Test different baud rates
  int baudRates[] = {9600, 2400, 4800, 19200};
  
  for (int baud : baudRates) {
    Serial.printf("Debug: Testing baud rate: %d\n", baud);
    Serial2.begin(baud, SERIAL_8N1, 16, 17);
    delay(1000);
    
    // Test address reading
    uint8_t addr = pzem.readAddress();
    Serial.printf("Debug: Address at %d baud: 0x%02X\n", baud, addr);
    
    if (addr != 0x00) {
      Serial.printf("Debug: SUCCESS! Found PZEM at address 0x%02X with baud rate %d\n", addr, baud);
      break;
    }
    
    delay(500);
  }
  
  // Test with swapped TX/RX
  Serial.println("Debug: Testing with swapped TX/RX connections...");
  Serial2.begin(9600); // Swap RX/TX
  delay(1000);
  
  uint8_t addrSwapped = pzem.readAddress();
  Serial.printf("Debug: Address with swapped TX/RX: 0x%02X\n", addrSwapped);
  
  // Test if we can set the address
  Serial.println("Debug: Attempting to set PZEM address to 0x07...");
  if (pzem.setAddress(0x07)) {
    Serial.println("Debug: Address set successfully to 0x07");
  } else {
    Serial.println("Debug: Failed to set address to 0x07");
  }
  delay(1000);
  
  // Test reading address again
  uint8_t newAddr = pzem.readAddress();
  Serial.printf("Debug: Address after setting: 0x%02X\n", newAddr);
  
  // Test basic readings
  Serial.println("Debug: Testing basic readings...");
  float testVoltage = pzem.voltage();
  Serial.printf("Debug: Test voltage: %f\n", testVoltage);
  
  // Test Serial2 communication directly
  Serial.println("Debug: Testing Serial2 communication...");
  Serial2.println("Test message");
  delay(100);
  if (Serial2.available()) {
    Serial.println("Debug: Serial2 has data available");
    while (Serial2.available()) {
      Serial.printf("Debug: Serial2 data: 0x%02X\n", Serial2.read());
    }
  } else {
    Serial.println("Debug: No data available on Serial2");
  }
  
  // Test sending raw PZEM commands
  Serial.println("Debug: Testing raw PZEM commands...");
  uint8_t cmd[] = {0xA0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  Serial2.write(cmd, 16);
  delay(100);
  
  if (Serial2.available()) {
    Serial.println("Debug: Raw command got response!");
    while (Serial2.available()) {
      Serial.printf("Debug: Raw response: 0x%02X\n", Serial2.read());
    }
  } else {
    Serial.println("Debug: No response to raw command");
  }
  
  Serial.println("Debug: === PZEM Test Complete ===");
}

void setupWebServer() {
  Serial.println("Debug: Configuring web server endpoints...");
  
  // Root endpoint - basic info
  server.on("/", HTTP_GET, []() {
    Serial.println("Debug: Root endpoint accessed");
    String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'><title>ESP32 Power Monitor</title>";
    html += "<style>";
    html += "*{margin:0;padding:0;box-sizing:border-box;}";
    html += "body{font-family:Arial,sans-serif;background:linear-gradient(135deg,#667eea 0%,#764ba2 100%);min-height:100vh;padding:20px;}";
    html += ".container{max-width:1200px;margin:0 auto;background:white;border-radius:15px;box-shadow:0 20px 40px rgba(0,0,0,0.1);overflow:hidden;}";
    html += ".header{background:linear-gradient(135deg,#2c3e50 0%,#34495e 100%);color:white;padding:30px;text-align:center;}";
    html += ".header h1{font-size:2.5em;margin-bottom:10px;}";
    html += ".content{padding:30px;}";
    html += ".status-bar{display:flex;justify-content:space-between;align-items:center;background:#f8f9fa;padding:15px 20px;border-radius:10px;margin-bottom:30px;flex-wrap:wrap;gap:10px;}";
    html += ".status-item{display:flex;align-items:center;gap:8px;}";
    html += ".status-indicator{width:12px;height:12px;border-radius:50%;display:inline-block;}";
    html += ".status-online{background:#28a745;}";
    html += ".status-offline{background:#dc3545;}";
    html += ".main-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(300px,1fr));gap:20px;margin-bottom:30px;}";
    html += ".card{background:white;border:1px solid #e9ecef;border-radius:10px;padding:20px;box-shadow:0 4px 6px rgba(0,0,0,0.05);}";
    html += ".card h3{color:#2c3e50;margin-bottom:15px;font-size:1.3em;}";
    html += ".power-value{font-size:2.5em;font-weight:bold;color:#e74c3c;margin:10px 0;}";
    html += ".unit{font-size:0.5em;color:#7f8c8d;font-weight:normal;}";
    html += ".metric{display:flex;justify-content:space-between;align-items:center;padding:8px 0;border-bottom:1px solid #f1f1f1;}";
    html += ".metric:last-child{border-bottom:none;}";
    html += ".metric-label{color:#7f8c8d;font-weight:500;}";
    html += ".metric-value{font-weight:bold;color:#2c3e50;}";
    html += ".button-group{display:flex;gap:10px;flex-wrap:wrap;margin-top:20px;}";
    html += ".btn{padding:12px 24px;border:none;border-radius:8px;cursor:pointer;font-size:1em;font-weight:500;transition:all 0.3s ease;}";
    html += ".btn-primary{background:#007bff;color:white;}";
    html += ".btn-success{background:#28a745;color:white;}";
    html += ".btn-danger{background:#dc3545;color:white;}";
    html += ".btn-warning{background:#ffc107;color:#212529;}";
    html += ".recording-status{padding:15px;border-radius:8px;margin:15px 0;text-align:center;font-weight:bold;}";
    html += ".recording-active{background:#d4edda;color:#155724;border:1px solid #c3e6cb;}";
    html += ".recording-inactive{background:#f8d7da;color:#721c24;border:1px solid #f5c6cb;}";
    html += ".message{background:#d4edda;color:#155724;padding:15px;border-radius:8px;margin:15px 0;border:1px solid #c3e6cb;}";
    html += ".error{background:#f8d7da;color:#721c24;border:1px solid #f5c6cb;}";
    html += "@media(max-width:768px){.main-grid{grid-template-columns:1fr;}.status-bar{flex-direction:column;}.button-group{flex-direction:column;}.btn{width:100%;}}";
    html += "</style></head><body>";
    html += "<div class='container'>";
    html += "<div class='header'><h1>⚡ ESP32 Power Monitor</h1><p>Real-time power monitoring with Los Angeles timezone</p></div>";
    html += "<div class='content'>";
    html += "<div class='status-bar'>";
    html += "<div class='status-item'><span class='status-indicator' id='wifi-status'></span><span id='wifi-text'>WiFi: Checking...</span></div>";
    html += "<div class='status-item'><span class='status-indicator' id='data-status'></span><span id='data-text'>Data: Checking...</span></div>";
    html += "<div class='status-item'><span id='current-time'>Time: Loading...</span></div>";
    html += "<div class='status-item'><span id='uptime'>Uptime: Loading...</span></div>";
    html += "</div>";
    html += "<div class='main-grid'>";
    html += "<div class='card'><h3>Power Consumption</h3>";
    html += "<div class='power-value' id='power-value'>--<span class='unit'>W</span></div>";
    html += "<div class='metric'><span class='metric-label'>Current:</span><span class='metric-value' id='current-value'>-- A</span></div>";
    html += "<div class='metric'><span class='metric-label'>Voltage:</span><span class='metric-value' id='voltage-value'>-- V</span></div>";
    html += "<div class='metric'><span class='metric-label'>Power Factor:</span><span class='metric-value' id='pf-value'>--</span></div>";
    html += "<div class='button-group'>";
    html += "<button class='btn btn-primary' onclick='refreshData()'>🔄 Refresh</button>";
    html += "<button class='btn btn-success' id='recording-button' onclick='toggleRecording()'>⏺️ Start Recording</button>";
    html += "</div></div>";
    html += "<div class='card'><h3>Energy & Frequency</h3>";
    html += "<div class='metric'><span class='metric-label'>Energy:</span><span class='metric-value' id='energy-value'>-- kWh</span></div>";
    html += "<div class='metric'><span class='metric-label'>Frequency:</span><span class='metric-value' id='frequency-value'>-- Hz</span></div>";
    html += "<div class='metric'><span class='metric-label'>PZEM Address:</span><span class='metric-value' id='address-value'>--</span></div>";
    html += "<div class='metric'><span class='metric-label'>Last Update:</span><span class='metric-value' id='last-update'>--</span></div>";
    html += "<div class='button-group'>";
    html += "<button class='btn btn-warning' onclick='testPZEM()'>🔧 Test PZEM</button>";
    html += "</div></div></div>";
    html += "<div class='card'><h3>Data Recording</h3>";
    html += "<div id='recording-status' class='recording-inactive'>Recording: Inactive</div>";
    html += "<div id='recording-info' style='display:none;'>";
    html += "<div class='metric'><span class='metric-label'>Interval:</span><span class='metric-value' id='recording-interval'>--</span></div>";
    html += "<div class='metric'><span class='metric-label'>Records Collected:</span><span class='metric-value' id='records-count'>--</span></div>";
    html += "<div class='metric'><span class='metric-label'>Buffer Status:</span><span class='metric-value' id='buffer-status'>--</span></div>";
    html += "</div>";
    html += "<div class='metric' style='margin-bottom:15px;'><span class='metric-label'>Poll Interval:</span><select id='poll-interval' style='padding:5px;border:1px solid #ddd;border-radius:4px;margin-left:10px;'><option value='1000'>1 second</option><option value='2000'>2 seconds</option><option value='5000' selected>5 seconds</option><option value='10000'>10 seconds</option><option value='15000'>15 seconds</option><option value='30000'>30 seconds</option><option value='60000'>60 seconds</option></select></div>";
    html += "<div class='button-group'>";
    html += "<button class='btn btn-primary' onclick='getRecordedData()'>📊 View Data</button>";
    html += "<button class='btn btn-warning' onclick='getAnalysis()'>📈 Analysis</button>";
    html += "<button class='btn btn-danger' onclick='clearData()'>🗑️ Clear Data</button>";
    html += "</div></div>";
    html += "<div id='message-container'></div>";
    html += "</div></div>";
    html += "<script>";
    html += "let refreshInterval;";
    html += "document.addEventListener('DOMContentLoaded',function(){loadStatus();loadData();updateRecordingStatus();startAutoRefresh();});";
    html += "function showMessage(message,type='success'){const container=document.getElementById('message-container');const div=document.createElement('div');div.className=type;div.textContent=message;container.appendChild(div);setTimeout(()=>div.remove(),5000);}";
    html += "async function loadStatus(){try{const response=await fetch('/status');const data=await response.json();const wifiStatus=document.getElementById('wifi-status');const wifiText=document.getElementById('wifi-text');if(data.wifi_connected){wifiStatus.className='status-indicator status-online';wifiText.textContent='WiFi: '+data.ip_address;}else{wifiStatus.className='status-indicator status-offline';wifiText.textContent='WiFi: Disconnected';}const uptime=Math.floor(data.uptime/1000);const hours=Math.floor(uptime/3600);const minutes=Math.floor((uptime%3600)/60);const seconds=uptime%60;document.getElementById('uptime').textContent='Uptime: '+hours+'h '+minutes+'m '+seconds+'s';document.getElementById('current-time').textContent='Time: '+data.current_time;}catch(error){console.error('Error loading status:',error);document.getElementById('wifi-status').className='status-indicator status-offline';document.getElementById('wifi-text').textContent='WiFi: Error';}}";
    html += "async function loadData(){try{const response=await fetch('/data');const data=await response.json();if(data.error)throw new Error(data.error);document.getElementById('power-value').textContent=data.power.toFixed(2)+'W';document.getElementById('current-value').textContent=data.current.toFixed(3)+' A';document.getElementById('voltage-value').textContent=data.voltage.toFixed(1)+' V';document.getElementById('pf-value').textContent=data.power_factor.toFixed(2);document.getElementById('energy-value').textContent=data.energy.toFixed(3)+' kWh';document.getElementById('frequency-value').textContent=data.frequency.toFixed(1)+' Hz';document.getElementById('address-value').textContent='0x'+data.address.toString(16).toUpperCase();if(data.iso_time){const date=new Date(data.iso_time);document.getElementById('last-update').textContent=date.toLocaleTimeString();}const dataStatus=document.getElementById('data-status');const dataText=document.getElementById('data-text');if(data.valid){dataStatus.className='status-indicator status-online';dataText.textContent='Data: Valid';}else{dataStatus.className='status-indicator status-offline';dataText.textContent='Data: Invalid';}}catch(error){console.error('Error loading data:',error);showMessage('Error loading power data: '+error.message,'error');document.getElementById('data-status').className='status-indicator status-offline';document.getElementById('data-text').textContent='Data: Error';}}";
    html += "async function refreshData(){await loadData();showMessage('Data refreshed successfully');}";
    html += "async function toggleRecording(){try{const statusResponse=await fetch('/record/status');const statusData=await statusResponse.json();if(statusData.is_recording){const response=await fetch('/record/stop',{method:'POST'});const data=await response.json();if(data.error)throw new Error(data.error);showMessage('Recording stopped. Collected '+data.records_collected+' records.');}else{const interval=parseInt(document.getElementById('poll-interval').value);const response=await fetch('/record/start',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({interval:interval})});const data=await response.json();if(data.error)throw new Error(data.error);showMessage('Recording started successfully');}updateRecordingStatus();}catch(error){console.error('Error toggling recording:',error);showMessage('Error toggling recording: '+error.message,'error');}}";
    html += "async function updateRecordingStatus(){try{const response=await fetch('/record/status');const data=await response.json();const statusDiv=document.getElementById('recording-status');const infoDiv=document.getElementById('recording-info');const recordingButton=document.getElementById('recording-button');if(data.is_recording){statusDiv.className='recording-status recording-active';statusDiv.textContent='Recording: Active';infoDiv.style.display='block';document.getElementById('recording-interval').textContent=(data.interval/1000)+' seconds';document.getElementById('records-count').textContent=data.records_collected;document.getElementById('buffer-status').textContent=data.buffer_full?'Full':'Available';recordingButton.textContent='⏹️ Stop Recording';recordingButton.className='btn btn-danger';}else{statusDiv.className='recording-status recording-inactive';statusDiv.textContent='Recording: Inactive';infoDiv.style.display='none';recordingButton.textContent='⏺️ Start Recording';recordingButton.className='btn btn-success';}}catch(error){console.error('Error updating recording status:',error);}}";
    html += "async function getRecordedData(){try{const response=await fetch('/record/data');const data=await response.json();if(data.error)throw new Error(data.error);if(data.records&&data.records.length>0){const csv=convertToCSV(data.records);downloadCSV(csv,'power_data.csv');showMessage('Downloaded '+data.records.length+' records');}else{showMessage('No recorded data available','error');}}catch(error){console.error('Error getting recorded data:',error);showMessage('Error getting recorded data: '+error.message,'error');}}";
    html += "async function getAnalysis(){try{const response=await fetch('/analysis');const data=await response.json();if(data.error)throw new Error(data.error);const analysis=data.analysis;const message='Analysis: Min '+analysis.min_power+'W, Max '+analysis.max_power+'W, Avg '+analysis.avg_power+'W, Energy '+analysis.total_energy+'kWh, Duration '+(analysis.duration_seconds/60).toFixed(1)+'min';showMessage(message);}catch(error){console.error('Error getting analysis:',error);showMessage('Error getting analysis: '+error.message,'error');}}";
    html += "async function clearData(){if(!confirm('Are you sure you want to clear all recorded data?'))return;try{const response=await fetch('/record/clear',{method:'POST'});const data=await response.json();if(data.error)throw new Error(data.error);showMessage('All recorded data cleared');updateRecordingStatus();}catch(error){console.error('Error clearing data:',error);showMessage('Error clearing data: '+error.message,'error');}}";
    html += "async function testPZEM(){try{const response=await fetch('/pzem_test');const data=await response.json();if(data.error)throw new Error(data.error);const results=data.test_results;const message='PZEM Test: Address 0x'+results.address+', Voltage '+results.voltage+'V, Current '+results.current+'A, Power '+results.power+'W';showMessage(message);}catch(error){console.error('Error testing PZEM:',error);showMessage('Error testing PZEM: '+error.message,'error');}}";
    html += "function convertToCSV(records){const headers=['Timestamp','ISO Time','Voltage (V)','Current (A)','Power (W)','Energy (kWh)','Frequency (Hz)','Power Factor','Address'];const csvRows=[headers.join(',')];records.forEach(record=>{const row=[record.timestamp,record.iso_time,record.voltage,record.current,record.power,record.energy,record.frequency,record.power_factor,record.address];csvRows.push(row.join(','));});return csvRows.join('\\n');}";
    html += "function downloadCSV(csv,filename){const blob=new Blob([csv],{type:'text/csv'});const url=window.URL.createObjectURL(blob);const a=document.createElement('a');a.href=url;a.download=filename;a.click();window.URL.revokeObjectURL(url);}";
    html += "function startAutoRefresh(){refreshInterval=setInterval(()=>{loadStatus();loadData();updateRecordingStatus();},5000);}";
    html += "window.addEventListener('beforeunload',()=>{if(refreshInterval)clearInterval(refreshInterval);});";
    html += "</script></body></html>";
    server.send(200, "text/html", html);
    Serial.println("Debug: Root endpoint response sent");
  });

  // Get latest data
  server.on("/data", HTTP_GET, []() {
    Serial.println("Debug: /data endpoint accessed - forcing fresh reading");
    readPowerData(); // Force fresh reading for true pull model
    
    if (!latestData.valid) {
      Serial.println("Debug: Reading failed, sending error");
      server.send(503, "application/json", "{\"error\":\"Failed to read power data\"}");
      return;
    }

    Serial.println("Debug: Building JSON response for /data");
    String json = "{";
    json += "\"voltage\":" + String(latestData.voltage, 2) + ",";
    json += "\"current\":" + String(latestData.current, 3) + ",";
    json += "\"power\":" + String(latestData.power, 2) + ",";
    json += "\"energy\":" + String(latestData.energy, 3) + ",";
    json += "\"frequency\":" + String(latestData.frequency, 1) + ",";
    json += "\"power_factor\":" + String(latestData.pf, 2) + ",";
    json += "\"address\":" + String(latestData.address) + ",";
    json += "\"timestamp\":" + String(latestData.timestamp) + ",";
    json += "\"iso_time\":\"" + latestData.isoTime + "\",";
    json += "\"valid\":" + String(latestData.valid ? "true" : "false");
    json += "}";
    
    Serial.printf("Debug: Sending JSON response: %s\n", json.c_str());
    server.send(200, "application/json", json);
    Serial.println("Debug: /data response sent");
  });

  // Get system status
  server.on("/status", HTTP_GET, []() {
    Serial.println("Debug: /status endpoint accessed");
    String json = "{";
    json += "\"wifi_connected\":" + String(WiFi.status() == WL_CONNECTED ? "true" : "false") + ",";
    json += "\"ip_address\":\"" + WiFi.localIP().toString() + "\",";
    json += "\"free_heap\":" + String(ESP.getFreeHeap()) + ",";
    json += "\"uptime\":" + String(millis()) + ",";
    json += "\"data_age\":" + String(latestData.valid ? (millis() - latestData.timestamp) : -1) + ",";
    json += "\"current_time\":\"" + getFormattedTime() + "\",";
    json += "\"iso_time\":\"" + getISOTime() + "\",";
    json += "\"timezone\":\"" + String(timezone) + "\"";
    json += "}";
    
    Serial.printf("Debug: Sending status response: %s\n", json.c_str());
    server.send(200, "application/json", json);
    Serial.println("Debug: /status response sent");
  });

  // Get current time endpoint
  server.on("/time", HTTP_GET, []() {
    Serial.println("Debug: /time endpoint accessed");
    String json = "{";
    json += "\"formatted_time\":\"" + getFormattedTime() + "\",";
    json += "\"iso_time\":\"" + getISOTime() + "\",";
    json += "\"timezone\":\"" + String(timezone) + "\",";
    json += "\"timestamp\":" + String(time(nullptr));
    json += "}";
    
    Serial.printf("Debug: Sending time response: %s\n", json.c_str());
    server.send(200, "application/json", json);
    Serial.println("Debug: /time response sent");
  });

  // Force new reading
  server.on("/read", HTTP_GET, []() {
    Serial.println("Debug: /read endpoint accessed - forcing new reading");
    readPowerData();
    
    if (!latestData.valid) {
      Serial.println("Debug: Reading failed, sending error");
      server.send(503, "application/json", "{\"error\":\"Failed to read power data\"}");
      return;
    }

    Serial.println("Debug: Building JSON response for /read");
    String json = "{";
    json += "\"voltage\":" + String(latestData.voltage, 2) + ",";
    json += "\"current\":" + String(latestData.current, 3) + ",";
    json += "\"power\":" + String(latestData.power, 2) + ",";
    json += "\"energy\":" + String(latestData.energy, 3) + ",";
    json += "\"frequency\":" + String(latestData.frequency, 1) + ",";
    json += "\"power_factor\":" + String(latestData.pf, 2) + ",";
    json += "\"address\":" + String(latestData.address) + ",";
    json += "\"timestamp\":" + String(latestData.timestamp) + ",";
    json += "\"iso_time\":\"" + latestData.isoTime + "\",";
    json += "\"valid\":" + String(latestData.valid ? "true" : "false");
    json += "}";
    
    Serial.printf("Debug: Sending JSON response: %s\n", json.c_str());
    server.send(200, "application/json", json);
    Serial.println("Debug: /read response sent");
  });

  // PZEM troubleshooting endpoint
  server.on("/pzem_test", HTTP_GET, []() {
    Serial.println("Debug: /pzem_test endpoint accessed");
    
    // Read current power data for testing
    readPowerData();
    
    String response = "{";
    response += "\"test_results\":{";
    response += "\"address\":\"" + String(latestData.address, HEX) + "\",";
    response += "\"voltage\":" + String(latestData.voltage, 2) + ",";
    response += "\"current\":" + String(latestData.current, 3) + ",";
    response += "\"power\":" + String(latestData.power, 2) + ",";
    response += "\"valid\":" + String(latestData.valid ? "true" : "false");
    response += "}";
    response += "}";
    
    server.send(200, "application/json", response);
  });

  // Manual MQTT discovery trigger endpoint
  server.on("/mqtt_discovery", HTTP_POST, []() {
    Serial.println("Debug: /mqtt_discovery endpoint accessed - triggering discovery");
    
    if (mqtt_connected) {
      publish_discovery();
      server.send(200, "application/json", "{\"status\":\"discovery_triggered\"}");
    } else {
      server.send(503, "application/json", "{\"error\":\"MQTT not connected\"}");
    }
  });

  // Start recording endpoint
  server.on("/record/start", HTTP_POST, []() {
    Serial.println("Debug: /record/start endpoint accessed");
    
    if (recording.isRecording) {
      server.send(400, "application/json", "{\"error\":\"Recording already in progress\"}");
      return;
    }
    
    // Parse interval from request body if provided
    if (server.hasArg("plain")) {
      String body = server.arg("plain");
      Serial.printf("Debug: Request body: %s\n", body.c_str());
      
      // Simple JSON parsing for interval
      int intervalIndex = body.indexOf("\"interval\":");
      if (intervalIndex != -1) {
        int startIndex = body.indexOf(":", intervalIndex) + 1;
        int endIndex = body.indexOf(",", startIndex);
        if (endIndex == -1) endIndex = body.indexOf("}", startIndex);
        if (endIndex != -1) {
          String intervalStr = body.substring(startIndex, endIndex);
          intervalStr.trim();
          unsigned long newInterval = intervalStr.toInt();
          if (newInterval >= 1000 && newInterval <= 60000) { // 1s to 60s
            recording.interval = newInterval;
            Serial.printf("Debug: Set recording interval to %lu ms\n", recording.interval);
          }
        }
      }
    }
    
    // Start recording
    recording.isRecording = true;
    recording.startTime = millis();
    recording.lastReadTime = 0;
    recording.recordCount = 0;
    dataIndex = 0;
    bufferFull = false;
    
    Serial.printf("Debug: Recording started with interval %lu ms\n", recording.interval);
    
    String response = "{";
    response += "\"status\":\"recording_started\",";
    response += "\"interval\":" + String(recording.interval) + ",";
    response += "\"start_time\":" + String(recording.startTime);
    response += "}";
    
    server.send(200, "application/json", response);
  });

  // Stop recording endpoint
  server.on("/record/stop", HTTP_POST, []() {
    Serial.println("Debug: /record/stop endpoint accessed");
    
    if (!recording.isRecording) {
      server.send(400, "application/json", "{\"error\":\"No recording in progress\"}");
      return;
    }
    
    recording.isRecording = false;
    unsigned long duration = millis() - recording.startTime;
    
    Serial.printf("Debug: Recording stopped. Duration: %lu ms, Records: %d\n", duration, recording.recordCount);
    
    String response = "{";
    response += "\"status\":\"recording_stopped\",";
    response += "\"duration\":" + String(duration) + ",";
    response += "\"records_collected\":" + String(recording.recordCount) + ",";
    response += "\"buffer_full\":" + String(bufferFull ? "true" : "false");
    response += "}";
    
    server.send(200, "application/json", response);
  });

  // Get recording status
  server.on("/record/status", HTTP_GET, []() {
    Serial.println("Debug: /record/status endpoint accessed");
    
    String response = "{";
    response += "\"is_recording\":" + String(recording.isRecording ? "true" : "false") + ",";
    response += "\"interval\":" + String(recording.interval) + ",";
    response += "\"records_collected\":" + String(recording.recordCount) + ",";
    response += "\"buffer_full\":" + String(bufferFull ? "true" : "false");
    
    if (recording.isRecording) {
      response += ",\"elapsed_time\":" + String(millis() - recording.startTime);
    }
    
    response += "}";
    
    server.send(200, "application/json", response);
  });

  // Get recorded data
  server.on("/record/data", HTTP_GET, []() {
    Serial.println("Debug: /record/data endpoint accessed");
    
    if (recording.recordCount == 0) {
      server.send(404, "application/json", "{\"error\":\"No recorded data available\"}");
      return;
    }
    
    String response = "{\"records\":[";
    
    int count = 0;
    int maxRecords = bufferFull ? 150 : recording.recordCount;
    
    for (int i = 0; i < maxRecords; i++) {
      int idx = (dataIndex - maxRecords + i + 150) % 150;
      RecordedData& data = recordedData[idx];
      
      if (data.valid) {
        if (count > 0) response += ",";
        response += "{";
        response += "\"timestamp\":" + String(data.timestamp) + ",";
        response += "\"iso_time\":\"" + data.isoTime + "\",";
        response += "\"voltage\":" + String(data.voltage, 2) + ",";
        response += "\"current\":" + String(data.current, 3) + ",";
        response += "\"power\":" + String(data.power, 2) + ",";
        response += "\"energy\":" + String(data.energy, 3) + ",";
        response += "\"frequency\":" + String(data.frequency, 1) + ",";
        response += "\"power_factor\":" + String(data.pf, 2) + ",";
        response += "\"address\":" + String(data.address);
        response += "}";
        count++;
      }
    }
    
    response += "],\"total_records\":" + String(recording.recordCount) + ",";
    response += "\"buffer_full\":" + String(bufferFull ? "true" : "false");
    response += "}";
    
    Serial.printf("Debug: Sending %d recorded data points\n", count);
    server.send(200, "application/json", response);
  });

  // PC Review Analysis endpoint
  server.on("/analysis", HTTP_GET, []() {
    Serial.println("Debug: /analysis endpoint accessed");
    
    if (recording.recordCount == 0) {
      server.send(404, "application/json", "{\"error\":\"No recorded data available for analysis\"}");
      return;
    }
    
    // Calculate min, max, average power and total energy
    float minPower = 9999.0;
    float maxPower = 0.0;
    float totalPower = 0.0;
    float startEnergy = 0.0;
    float endEnergy = 0.0;
    int validCount = 0;
    unsigned long startTime = 0;
    unsigned long endTime = 0;
    
    int maxRecords = bufferFull ? 150 : recording.recordCount;
    
    for (int i = 0; i < maxRecords; i++) {
      int idx = (dataIndex - maxRecords + i + 150) % 150;
      RecordedData& data = recordedData[idx];
      
      if (data.valid) {
        if (validCount == 0) {
          startTime = data.timestamp;
          startEnergy = data.energy;
        }
        
        if (data.power < minPower) minPower = data.power;
        if (data.power > maxPower) maxPower = data.power;
        totalPower += data.power;
        validCount++;
        
        endTime = data.timestamp;
        endEnergy = data.energy;
      }
    }
    
    float avgPower = validCount > 0 ? totalPower / validCount : 0.0;
    float totalEnergy = endEnergy - startEnergy;
    unsigned long duration = endTime - startTime;
    
    String response = "{";
    response += "\"analysis\":{";
    response += "\"min_power\":" + String(minPower, 2) + ",";
    response += "\"max_power\":" + String(maxPower, 2) + ",";
    response += "\"avg_power\":" + String(avgPower, 2) + ",";
    response += "\"total_energy\":" + String(totalEnergy, 3) + ",";
    response += "\"duration_ms\":" + String(duration) + ",";
    response += "\"duration_seconds\":" + String(duration / 1000.0, 1) + ",";
    response += "\"data_points\":" + String(validCount) + ",";
    response += "\"start_time\":" + String(startTime) + ",";
    response += "\"end_time\":" + String(endTime);
    response += "}";
    response += "}";
    
    Serial.printf("Debug: Analysis - Min: %.2fW, Max: %.2fW, Avg: %.2fW, Energy: %.3fkWh\n", 
                  minPower, maxPower, avgPower, totalEnergy);
    server.send(200, "application/json", response);
  });

  // Clear recorded data
  server.on("/record/clear", HTTP_POST, []() {
    Serial.println("Debug: /record/clear endpoint accessed");
    
    // Clear all recorded data
    for (int i = 0; i < 150; i++) {
      recordedData[i].valid = false;
    }
    
    recording.recordCount = 0;
    dataIndex = 0;
    bufferFull = false;
    
    Serial.println("Debug: All recorded data cleared");
    server.send(200, "application/json", "{\"status\":\"data_cleared\"}");
  });

  // Handle not found
  server.onNotFound([]() {
    Serial.println("Debug: 404 - Endpoint not found");
    server.send(404, "application/json", "{\"error\":\"Endpoint not found\"}");
  });
  
  Serial.println("Debug: Web server endpoints configured");
}

void readPowerData() {
  Serial.println("Debug: Starting power data reading...");
  
  uint8_t addr = pzem.readAddress();
  Serial.printf("Debug: PZEM Address: 0x%02X\n", addr);

  Serial.println("Debug: Reading voltage...");
  float voltage = pzem.voltage();
  Serial.printf("Debug: Voltage raw value: %f\n", voltage);
  
  Serial.println("Debug: Reading current...");
  float current = pzem.current();
  Serial.printf("Debug: Current raw value: %f\n", current);
  
  Serial.println("Debug: Reading power...");
  float power = pzem.power();
  Serial.printf("Debug: Power raw value: %f\n", power);
  
  Serial.println("Debug: Reading energy...");
  float energy = pzem.energy();
  Serial.printf("Debug: Energy raw value: %f\n", energy);
  
  Serial.println("Debug: Reading frequency...");
  float frequency = pzem.frequency();
  Serial.printf("Debug: Frequency raw value: %f\n", frequency);
  
  Serial.println("Debug: Reading power factor...");
  float pf = pzem.pf();
  Serial.printf("Debug: Power factor raw value: %f\n", pf);

  if (isnan(voltage) || isnan(current) || isnan(power) || isnan(energy) || isnan(frequency) || isnan(pf)) {
    Serial.println("Debug: ERROR - One or more readings are NaN!");
    Serial.printf("Debug: Voltage: %s\n", isnan(voltage) ? "NaN" : "OK");
    Serial.printf("Debug: Current: %s\n", isnan(current) ? "NaN" : "OK");
    Serial.printf("Debug: Power: %s\n", isnan(power) ? "NaN" : "OK");
    Serial.printf("Debug: Energy: %s\n", isnan(energy) ? "NaN" : "OK");
    Serial.printf("Debug: Frequency: %s\n", isnan(frequency) ? "NaN" : "OK");
    Serial.printf("Debug: Power Factor: %s\n", isnan(pf) ? "NaN" : "OK");
    latestData.valid = false;
  } else {
    Serial.println("Debug: All readings successful!");
    Serial.print("Debug: Voltage: ");      Serial.print(voltage);      Serial.println("V");
    Serial.print("Debug: Current: ");      Serial.print(current);      Serial.println("A");
    Serial.print("Debug: Power: ");        Serial.print(power);        Serial.println("W");
    Serial.print("Debug: Energy: ");       Serial.print(energy, 3);    Serial.println("kWh");
    Serial.print("Debug: Frequency: ");    Serial.print(frequency, 1); Serial.println("Hz");
    Serial.print("Debug: PF: ");           Serial.println(pf);

    // Update global data
    latestData.voltage = voltage;
    latestData.current = current;
    latestData.power = power;
    latestData.energy = energy;
    latestData.frequency = frequency;
    latestData.pf = pf;
    latestData.address = addr;
    latestData.timestamp = millis();
    latestData.isoTime = getISOTime();
    latestData.valid = true;
    
    Serial.println("Debug: Global data structure updated");
  }
  
  Serial.printf("Debug: Free heap after reading: %d bytes\n", ESP.getFreeHeap());
}

void loop() {
  // Handle web server clients
  server.handleClient();

  // Handle MQTT
  if (!mqtt_client.connected()) {
    mqtt_connected = false;
    if (millis() - last_mqtt_publish > 30000) { // Try to reconnect every 30 seconds
      Serial.println("Debug: MQTT disconnected, attempting to reconnect...");
      mqtt_connect();
      last_mqtt_publish = millis();
    }
  } else {
    mqtt_client.loop();
    
    // Publish MQTT data periodically
    if (millis() - last_mqtt_publish >= mqtt_publish_interval) {
      publish_mqtt_data();
      last_mqtt_publish = millis();
    }
    
    // Re-publish discovery messages periodically
    if (millis() - last_discovery_publish >= discovery_republish_interval) {
      Serial.println("Debug: Re-publishing discovery messages...");
      publish_discovery();
      last_discovery_publish = millis();
    }
  }

  // Recording logic - check if we need to read data
  if (recording.isRecording) {
    if (millis() - recording.lastReadTime >= recording.interval) {
      Serial.println("Debug: Recording interval reached - reading data");
      readPowerData();
      
      if (latestData.valid) {
        storeRecordedData();
        recording.recordCount++;
        Serial.printf("Debug: Stored record %d\n", recording.recordCount);
      }
      
      recording.lastReadTime = millis();
    }
  }

  // Optional: Add some delay to prevent watchdog issues
  delay(10);
}

void storeRecordedData() {
  // Store data in circular buffer
  recordedData[dataIndex].timestamp = latestData.timestamp;
  recordedData[dataIndex].isoTime = latestData.isoTime;
  recordedData[dataIndex].voltage = latestData.voltage;
  recordedData[dataIndex].current = latestData.current;
  recordedData[dataIndex].power = latestData.power;
  recordedData[dataIndex].energy = latestData.energy;
  recordedData[dataIndex].frequency = latestData.frequency;
  recordedData[dataIndex].pf = latestData.pf;
  recordedData[dataIndex].address = latestData.address;
  recordedData[dataIndex].valid = true;
  
  // Update circular buffer index
  dataIndex = (dataIndex + 1) % 150;
  if (dataIndex == 0) {
    bufferFull = true;
    Serial.println("Debug: Recording buffer full - overwriting oldest data");
  }
}

// Function to get ISO formatted time string
String getISOTime() {
  time_t now = time(nullptr);
  if (now == 0) {
    // Time not synchronized, return empty string
    return "";
  }
  
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return "";
  }
  
  char isoTime[32];
  strftime(isoTime, sizeof(isoTime), "%Y-%m-%dT%H:%M:%S%z", &timeinfo);
  return String(isoTime);
}

// Function to get formatted time for display
String getFormattedTime() {
  time_t now = time(nullptr);
  if (now == 0) {
    return "Time not synced";
  }
  
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return "Time error";
  }
  
  char timeStr[32];
  strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeinfo);
  return String(timeStr);
}
