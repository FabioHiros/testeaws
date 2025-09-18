// ESP32 with BMP280 Sensor (Temperature + Pressure)
// Based on the base template - only sensor-specific parts are modified

#include <WiFi.h>
#include <PubSubClient.h>
#include <Adafruit_BMP280.h>

// WiFi credentials - UPDATE THESE!
const char* ssid = "Suhefa1";
const char* password = "Fa147258@";

// MQTT broker settings - UPDATE IP!
const char* mqtt_server = "192.168.1.100";
const int mqtt_port = 1883;

WiFiClient espClient;
PubSubClient client(espClient);

// BMP280 sensor
Adafruit_BMP280 bmp; // I2C connection

String macAddress;
uint32_t delayMS = 30000; // 30 seconds between readings
unsigned long lastReconnectAttempt = 0;

void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println(F("ESP32 BMP280 MQTT Client"));
  Serial.println(F("========================"));

  WiFi.mode(WIFI_MODE_STA);
  setupWiFi();

  macAddress = WiFi.macAddress();
  Serial.print(F("MAC Address: "));
  Serial.println(macAddress);

  client.setServer(mqtt_server, mqtt_port);

  // Initialize BMP280 sensor
  if (!bmp.begin(0x76)) { // Default I2C address, try 0x77 if this fails
    Serial.println(F("Could not find a valid BMP280 sensor, check wiring!"));
    while (1) delay(10);
  }

  // Configure BMP280 settings for weather monitoring
  bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,     // Operating Mode
                  Adafruit_BMP280::SAMPLING_X2,     // Temperature oversampling
                  Adafruit_BMP280::SAMPLING_X16,    // Pressure oversampling
                  Adafruit_BMP280::FILTER_X16,      // Filtering
                  Adafruit_BMP280::STANDBY_MS_500); // Standby time

  Serial.println(F("BMP280 sensor initialized"));
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
  // BMP280-SPECIFIC: Read temperature and pressure
  float temperatura = bmp.readTemperature();
  float pressao = bmp.readPressure() / 100.0F; // Convert Pa to hPa

  bool hasValidData = false;
  if (!isnan(temperatura) && !isnan(pressao)) {
    hasValidData = true;
  }

  // Print readings
  Serial.println(F("--- BMP280 Reading ---"));
  Serial.print(F("Device: "));
  Serial.println(macAddress);

  if (!isnan(temperatura)) {
    Serial.print(F("Temperature: "));
    Serial.print(temperatura);
    Serial.println(F("°C"));
  } else {
    Serial.println(F("Error reading temperature!"));
  }

  if (!isnan(pressao)) {
    Serial.print(F("Pressure: "));
    Serial.print(pressao);
    Serial.println(F(" hPa"));
  } else {
    Serial.println(F("Error reading pressure!"));
  }

  // Send to MQTT if we have valid data
  if (hasValidData && client.connected()) {
    unsigned long unixtime = millis() / 1000;

    // Create JSON payload for BMP280
    String payload = "{";
    payload += "\"uuid\":\"" + macAddress + "\",";
    payload += "\"unixtime\":" + String(unixtime) + ",";

    if (!isnan(temperatura)) {
      payload += "\"temperatura\":" + String(temperatura, 1) + ",";
    }
    if (!isnan(pressao)) {
      payload += "\"pressao\":" + String(pressao, 2); // 2 decimal places for pressure
    }

    // Remove trailing comma if needed
    if (payload.endsWith(",")) {
      payload = payload.substring(0, payload.length() - 1);
    }

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

  Serial.println(F("---------------------"));
  Serial.println();
}