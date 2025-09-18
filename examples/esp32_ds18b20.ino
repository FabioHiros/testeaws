// ESP32 with DS18B20 Sensor (Temperature Only)
// Based on the base template - only sensor-specific parts are modified

#include <WiFi.h>
#include <PubSubClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#define ONE_WIRE_BUS 18  // Data wire is connected to pin 18

// WiFi credentials - UPDATE THESE!
const char* ssid = "Suhefa1";
const char* password = "Fa147258@";

// MQTT broker settings - UPDATE IP!
const char* mqtt_server = "192.168.1.100";
const int mqtt_port = 1883;

WiFiClient espClient;
PubSubClient client(espClient);

// DS18B20 sensor setup
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

String macAddress;
uint32_t delayMS = 30000; // 30 seconds between readings
unsigned long lastReconnectAttempt = 0;

void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println(F("ESP32 DS18B20 MQTT Client"));
  Serial.println(F("========================="));

  WiFi.mode(WIFI_MODE_STA);
  setupWiFi();

  macAddress = WiFi.macAddress();
  Serial.print(F("MAC Address: "));
  Serial.println(macAddress);

  client.setServer(mqtt_server, mqtt_port);

  // Initialize DS18B20 sensor
  sensors.begin();

  // Check if sensor is connected
  int deviceCount = sensors.getDeviceCount();
  Serial.print(F("Found "));
  Serial.print(deviceCount);
  Serial.println(F(" DS18B20 sensor(s)"));

  if (deviceCount == 0) {
    Serial.println(F("No DS18B20 sensors found! Check wiring."));
    while (1) delay(10);
  }

  // Set resolution (9-12 bits, higher = more accurate but slower)
  sensors.setResolution(12);

  Serial.println(F("DS18B20 sensor initialized"));
  Serial.println(F("Starting sensor readings..."));
  Serial.println();
}

void loop() {
  if (!client.connected()) {
    long now = millis();
    if (now - lastReconnectAttempt > 5000) {
      lastReconnectAttempt = now;
      if (reconnectMQTT()) {
        lastReconnectAttempt = 0;
      }
    }
  } else {
    client.loop();
  }

  static unsigned long lastReading = 0;
  if (millis() - lastReading > delayMS) {
    lastReading = millis();
    readAndSendSensorData();
  }

  delay(1000);
}

void setupWiFi() {
  WiFi.begin(ssid, password);
  Serial.print(F("Connecting to WiFi"));

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(F("."));
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    Serial.println(F("WiFi connected!"));
    Serial.print(F("IP address: "));
    Serial.println(WiFi.localIP());
  } else {
    Serial.println();
    Serial.println(F("WiFi connection failed!"));
    ESP.restart();
  }
}

bool reconnectMQTT() {
  if (WiFi.status() != WL_CONNECTED) {
    setupWiFi();
    return false;
  }

  String clientId = "ESP32_" + macAddress;
  if (client.connect(clientId.c_str())) {
    Serial.println(F("MQTT connected!"));
    return true;
  } else {
    Serial.print(F("MQTT failed, rc="));
    Serial.println(client.state());
    return false;
  }
}

void readAndSendSensorData() {
  // DS18B20-SPECIFIC: Read temperature from first sensor
  sensors.requestTemperatures(); // Request temperature from all devices

  float temperatura = sensors.getTempCByIndex(0); // Get temperature from first sensor
  bool hasValidData = false;

  // Check if reading is valid (DS18B20 returns -127 or 85 for errors)
  if (temperatura != DEVICE_DISCONNECTED_C && temperatura != 85.0) {
    hasValidData = true;
  }

  // Print readings
  Serial.println(F("--- DS18B20 Reading ---"));
  Serial.print(F("Device: "));
  Serial.println(macAddress);

  if (hasValidData) {
    Serial.print(F("Temperature: "));
    Serial.print(temperatura);
    Serial.println(F("°C"));
  } else {
    Serial.println(F("Error reading temperature from DS18B20!"));
    Serial.print(F("Raw value: "));
    Serial.println(temperatura);
  }

  // Send to MQTT if we have valid data
  if (hasValidData && client.connected()) {
    unsigned long unixtime = millis() / 1000;

    // Create JSON payload for DS18B20 (temperature only)
    String payload = "{";
    payload += "\"uuid\":\"" + macAddress + "\",";
    payload += "\"unixtime\":" + String(unixtime) + ",";
    payload += "\"temperatura\":" + String(temperatura, 2); // 2 decimal places
    payload += "}";

    // Send to MQTT
    String topicMac = macAddress;
    topicMac.replace(":", "");
    String topic = "weather/" + topicMac + "/data";

    if (client.publish(topic.c_str(), payload.c_str())) {
      Serial.print(F("✅ Data sent: "));
      Serial.println(payload);
    } else {
      Serial.println(F("❌ Failed to send data"));
    }
  }

  Serial.println(F("----------------------"));
  Serial.println();
}