// ESP32 DHT22 sensor with MQTT
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <time.h>

#define DHTPIN 18
#define DHTTYPE DHT22

// WiFi credentials
const char* ssid = "Suhefa1";
const char* password = "Fa147258@";

// HiveMQ Cloud MQTT broker settings
const char* mqtt_server = "166d9acce84b47e48593e715d2114d59.s1.eu.hivemq.cloud";
const int mqtt_port = 8883;
const char* mqtt_user = "skytrack";
const char* mqtt_pass = "123456789Skytrack";

WiFiClientSecure espClient;
PubSubClient client(espClient);

DHT_Unified dht(DHTPIN, DHTTYPE);
uint32_t delayMS;
String macAddress;

unsigned long lastReconnectAttempt = 0;

void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println(F("ESP32 DHT22 MQTT Client"));
  Serial.println(F("======================="));

  // Initialize WiFi mode first
  WiFi.mode(WIFI_MODE_STA);

  // Connect to WiFi
  setupWiFi();

  // Get MAC address AFTER WiFi is connected (keep colons for UUID format)
  macAddress = WiFi.macAddress();

  Serial.print(F("MAC Address: "));
  Serial.println(macAddress);
  String topicMac = macAddress;
  topicMac.replace(":", "");
  Serial.print(F("Topic: weather/"));
  Serial.print(topicMac);
  Serial.println(F("/data"));

  // Setup MQTT with SSL (insecure mode)
  espClient.setInsecure(); // Skip certificate validation for now
  client.setServer(mqtt_server, mqtt_port);

  // Initialize NTP time synchronization
  Serial.println(F("Synchronizing time with NTP..."));
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");

  // Wait for time synchronization
  Serial.print(F("Waiting for time sync"));
  time_t now = time(nullptr);
  int attempts = 0;
  while (now < 1000000000L && attempts < 20) { // Valid Unix timestamp should be > 1 billion
    delay(500);
    Serial.print(F("."));
    now = time(nullptr);
    attempts++;
  }
  Serial.println();

  if (now > 1000000000L) {
    Serial.print(F("✅ Time synchronized: "));
    Serial.println(ctime(&now));
  } else {
    Serial.println(F("⚠️ Time sync failed, using boot time"));
  }

  // Initialize DHT sensor
  dht.begin();
  delay(1000);

  // Set delay between readings
  sensor_t sensor;
  dht.temperature().getSensor(&sensor);
  delayMS = sensor.min_delay / 1000;
  if (delayMS < 30000) {
    delayMS = 30000; // 30 seconds for MQTT (less frequent than HTTP)
  }

  Serial.println(F("Starting sensor readings..."));
  Serial.println();
}

void loop() {
  // Ensure MQTT connection
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

  // Read and send sensor data
  static unsigned long lastReading = 0;
  if (millis() - lastReading > delayMS) {
    lastReading = millis();
    readAndSendSensorData();
  }

  delay(1000); // Small delay for loop stability
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
    Serial.print(F("MQTT Server: "));
    Serial.print(mqtt_server);
    Serial.print(F(":"));
    Serial.println(mqtt_port);
  } else {
    Serial.println();
    Serial.println(F("WiFi connection failed!"));
    Serial.println(F("Restarting ESP32..."));
    ESP.restart();
  }
  Serial.println(F("======================="));
}

bool reconnectMQTT() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println(F("WiFi disconnected, reconnecting..."));
    setupWiFi();
    return false;
  }

  Serial.print(F("Attempting MQTT connection..."));

  // Create unique client ID
  String clientId = "ESP32_" + macAddress;

  if (client.connect(clientId.c_str(), mqtt_user, mqtt_pass)) {
    Serial.println(F(" connected!"));
    return true;
  } else {
    Serial.print(F(" failed, rc="));
    Serial.print(client.state());
    Serial.println(F(" retrying in 5 seconds"));
    return false;
  }
}

void readAndSendSensorData() {
  // Read sensor data
  sensors_event_t event;
  float temperature = NAN;
  float humidity = NAN;
  bool hasValidData = false;

  // Get temperature
  dht.temperature().getEvent(&event);
  if (!isnan(event.temperature)) {
    temperature = event.temperature;
    hasValidData = true;
  }

  // Get humidity
  dht.humidity().getEvent(&event);
  if (!isnan(event.relative_humidity)) {
    humidity = event.relative_humidity;
    hasValidData = true;
  }

  // Print to serial for debugging
  Serial.println(F("--- Sensor Reading ---"));
  Serial.print(F("Device: "));
  Serial.println(macAddress);

  if (!isnan(temperature)) {
    Serial.print(F("Temperature: "));
    Serial.print(temperature);
    Serial.println(F("°C"));
  } else {
    Serial.println(F("Error reading temperature!"));
  }

  if (!isnan(humidity)) {
    Serial.print(F("Humidity: "));
    Serial.print(humidity);
    Serial.println(F("%"));
  } else {
    Serial.println(F("Error reading humidity!"));
  }

  // Send to MQTT broker if we have valid data and connection
  if (hasValidData && client.connected()) {
    // Get current Unix timestamp from NTP
    time_t now = time(nullptr);
    unsigned long unixtime = now;

    // Create single JSON message with all data
    String payload = "{";
    payload += "\"uuid\":\"" + macAddress + "\",";
    payload += "\"unixtime\":" + String(unixtime) + ",";

    if (!isnan(temperature)) {
      payload += "\"temperatura\":" + String(temperature, 1) + ",";
    }
    if (!isnan(humidity)) {
      payload += "\"umidade\":" + String(humidity, 1);
    }

    // Remove trailing comma if humidity is NaN
    if (payload.endsWith(",")) {
      payload = payload.substring(0, payload.length() - 1);
    }

    payload += "}";

    // Send to single topic
    String topicMac = macAddress;
    topicMac.replace(":", "");
    String topic = "weather/" + topicMac + "/data";
    if (client.publish(topic.c_str(), payload.c_str())) {
      Serial.print(F("✅ Data sent: "));
      Serial.print(topic);
      Serial.print(F(" = "));
      Serial.println(payload);
      Serial.println(F("📡 All data sent successfully!"));
    } else {
      Serial.println(F("❌ Failed to send data"));
    }

  } else {
    if (!hasValidData) {
      Serial.println(F("⚠️  No valid sensor data to send"));
    }
    if (!client.connected()) {
      Serial.println(F("⚠️  MQTT not connected"));
    }
  }

  Serial.println(F("---------------------"));
  Serial.println();
}