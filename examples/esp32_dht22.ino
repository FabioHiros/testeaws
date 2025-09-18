// ESP32 with DHT22 Sensor (Temperature + Humidity)
// Based on the base template - only sensor-specific parts are modified

#include <WiFi.h>
#include <PubSubClient.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>

#define DHTPIN 18
#define DHTTYPE DHT22

// WiFi credentials - UPDATE THESE!
const char* ssid = "Suhefa1";
const char* password = "Fa147258@";

// MQTT broker settings - UPDATE IP!
const char* mqtt_server = "192.168.1.100";
const int mqtt_port = 1883;

WiFiClient espClient;
PubSubClient client(espClient);

// DHT22 sensor
DHT_Unified dht(DHTPIN, DHTTYPE);

String macAddress;
uint32_t delayMS = 30000; // 30 seconds between readings
unsigned long lastReconnectAttempt = 0;

void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println(F("ESP32 DHT22 MQTT Client"));
  Serial.println(F("======================="));

  WiFi.mode(WIFI_MODE_STA);
  setupWiFi();

  macAddress = WiFi.macAddress();
  Serial.print(F("MAC Address: "));
  Serial.println(macAddress);

  client.setServer(mqtt_server, mqtt_port);

  // Initialize DHT22 sensor
  dht.begin();
  delay(1000);

  // Set delay based on sensor specifications
  sensor_t sensor;
  dht.temperature().getSensor(&sensor);
  uint32_t sensorDelay = sensor.min_delay / 1000;
  if (sensorDelay > delayMS) {
    delayMS = sensorDelay;
  }

  Serial.println(F("DHT22 sensor initialized"));
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
  // DHT22-SPECIFIC: Read temperature and humidity
  sensors_event_t event;
  float temperatura = NAN;
  float umidade = NAN;
  bool hasValidData = false;

  // Get temperature
  dht.temperature().getEvent(&event);
  if (!isnan(event.temperature)) {
    temperatura = event.temperature;
    hasValidData = true;
  }

  // Get humidity
  dht.humidity().getEvent(&event);
  if (!isnan(event.relative_humidity)) {
    umidade = event.relative_humidity;
    hasValidData = true;
  }

  // Print readings
  Serial.println(F("--- DHT22 Reading ---"));
  Serial.print(F("Device: "));
  Serial.println(macAddress);

  if (!isnan(temperatura)) {
    Serial.print(F("Temperature: "));
    Serial.print(temperatura);
    Serial.println(F("°C"));
  } else {
    Serial.println(F("Error reading temperature!"));
  }

  if (!isnan(umidade)) {
    Serial.print(F("Humidity: "));
    Serial.print(umidade);
    Serial.println(F("%"));
  } else {
    Serial.println(F("Error reading humidity!"));
  }

  // Send to MQTT if we have valid data
  if (hasValidData && client.connected()) {
    unsigned long unixtime = millis() / 1000;

    // Create JSON payload for DHT22
    String payload = "{";
    payload += "\"uuid\":\"" + macAddress + "\",";
    payload += "\"unixtime\":" + String(unixtime) + ",";

    if (!isnan(temperatura)) {
      payload += "\"temperatura\":" + String(temperatura, 1) + ",";
    }
    if (!isnan(umidade)) {
      payload += "\"umidade\":" + String(umidade, 1);
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

  Serial.println(F("--------------------"));
  Serial.println();
}