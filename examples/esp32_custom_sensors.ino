// ESP32 with Custom/Multiple Sensors
// Based on the base template - shows how to add any combination of sensors

#include <WiFi.h>
#include <PubSubClient.h>
// Add your sensor libraries here as needed

// WiFi credentials - UPDATE THESE!
const char* ssid = "Suhefa1";
const char* password = "Fa147258@";

// MQTT broker settings - UPDATE IP!
const char* mqtt_server = "192.168.1.100";
const int mqtt_port = 1883;

WiFiClient espClient;
PubSubClient client(espClient);

// Define your sensor pins and variables here
#define LIGHT_SENSOR_PIN A0
#define MOTION_SENSOR_PIN 19
#define SOIL_MOISTURE_PIN A3
#define BATTERY_PIN A6

String macAddress;
uint32_t delayMS = 30000; // 30 seconds between readings
unsigned long lastReconnectAttempt = 0;

void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println(F("ESP32 Custom Sensors MQTT Client"));
  Serial.println(F("================================="));

  WiFi.mode(WIFI_MODE_STA);
  setupWiFi();

  macAddress = WiFi.macAddress();
  Serial.print(F("MAC Address: "));
  Serial.println(macAddress);

  client.setServer(mqtt_server, mqtt_port);

  // Initialize your sensors here
  pinMode(MOTION_SENSOR_PIN, INPUT);
  pinMode(LIGHT_SENSOR_PIN, INPUT);
  // Add more sensor initializations as needed

  Serial.println(F("Custom sensors initialized"));
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
  // CUSTOM SENSORS: Read all your sensors here
  bool hasValidData = false;

  // Example sensor readings - replace with your actual sensors
  int luz = analogRead(LIGHT_SENSOR_PIN);
  bool movimento = digitalRead(MOTION_SENSOR_PIN);
  int umidade_solo = analogRead(SOIL_MOISTURE_PIN);
  int bateria_raw = analogRead(BATTERY_PIN);

  // Convert battery reading to percentage (adjust for your voltage divider)
  float bateria = map(bateria_raw, 0, 4095, 0, 100);

  // Calculate light percentage (adjust for your sensor)
  float luz_percent = map(luz, 0, 4095, 0, 100);

  // Convert soil moisture to percentage (adjust for your sensor)
  float umidade_solo_percent = map(umidade_solo, 0, 4095, 100, 0); // Inverted: wet=high, dry=low

  hasValidData = true; // We have sensor data

  // Print readings for debugging
  Serial.println(F("--- Custom Sensors Reading ---"));
  Serial.print(F("Device: "));
  Serial.println(macAddress);
  Serial.print(F("Light: "));
  Serial.print(luz_percent);
  Serial.println(F("%"));
  Serial.print(F("Motion: "));
  Serial.println(movimento ? "Detected" : "None");
  Serial.print(F("Soil Moisture: "));
  Serial.print(umidade_solo_percent);
  Serial.println(F("%"));
  Serial.print(F("Battery: "));
  Serial.print(bateria);
  Serial.println(F("%"));

  // Send to MQTT if we have valid data
  if (hasValidData && client.connected()) {
    unsigned long unixtime = millis() / 1000;

    // Create JSON payload with all your sensor data
    String payload = "{";
    payload += "\"uuid\":\"" + macAddress + "\",";
    payload += "\"unixtime\":" + String(unixtime) + ",";

    // Add all your sensor fields
    payload += "\"luz\":" + String(luz_percent, 1) + ",";
    payload += "\"movimento\":" + String(movimento ? "true" : "false") + ",";
    payload += "\"umidade_solo\":" + String(umidade_solo_percent, 1) + ",";
    payload += "\"bateria\":" + String(bateria, 1);

    // Add more sensors as needed:
    // payload += "\"temperatura\":" + String(temperatura, 1) + ",";
    // payload += "\"gas_co2\":" + String(co2_ppm, 0) + ",";
    // payload += "\"ruido\":" + String(noise_db, 1) + ",";

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

  Serial.println(F("-----------------------------"));
  Serial.println();
}

// Example function to read a complex sensor (like I2C sensors)
float readComplexSensor() {
  // Example: Reading from an I2C sensor
  // Wire.beginTransmission(SENSOR_ADDRESS);
  // Wire.write(REGISTER_ADDRESS);
  // Wire.endTransmission();
  // Wire.requestFrom(SENSOR_ADDRESS, 2);
  // if (Wire.available() == 2) {
  //   uint16_t rawValue = Wire.read() << 8 | Wire.read();
  //   return convertToPhysicalValue(rawValue);
  // }
  return NAN; // Return NAN if sensor read fails
}

// Example function to average multiple readings (for noisy sensors)
float readStableSensor(int pin, int samples = 10) {
  long total = 0;
  for (int i = 0; i < samples; i++) {
    total += analogRead(pin);
    delay(10);
  }
  return total / samples;
}