# ESP32 Sensor Examples

This folder contains examples showing how to use the same base code with different sensors. The key principle: **WiFi and MQTT code stays the same, only sensor reading changes**.

## Available Examples

### 1. `esp32_dht22.ino` - DHT22 Sensor
- **Sensors**: Temperature + Humidity
- **Library**: `DHT sensor library` by Adafruit
- **Wiring**: Data pin to GPIO 18, VCC to 3.3V, GND to GND
- **JSON Output**:
  ```json
  {"uuid":"24:6F:28:AE:52:7C","unixtime":1726270800,"temperatura":22.3,"umidade":72}
  ```

### 2. `esp32_bmp280.ino` - BMP280 Sensor
- **Sensors**: Temperature + Pressure
- **Library**: `Adafruit BMP280 Library`
- **Wiring**: I2C connection (SDA to GPIO 21, SCL to GPIO 22)
- **JSON Output**:
  ```json
  {"uuid":"24:6F:28:AE:52:7D","unixtime":1726270801,"temperatura":21.8,"pressao":1013.25}
  ```

### 3. `esp32_ds18b20.ino` - DS18B20 Sensor
- **Sensors**: Temperature only (waterproof)
- **Library**: `OneWire` + `DallasTemperature`
- **Wiring**: Data pin to GPIO 18 with 4.7kΩ pullup resistor
- **JSON Output**:
  ```json
  {"uuid":"24:6F:28:AE:52:7E","unixtime":1726270802,"temperatura":20.5}
  ```

### 4. `esp32_custom_sensors.ino` - Multiple Custom Sensors
- **Sensors**: Light, Motion, Soil Moisture, Battery Level
- **Libraries**: None (uses analog/digital reads)
- **JSON Output**:
  ```json
  {"uuid":"24:6F:28:AE:52:7F","unixtime":1726270803,"luz":450,"movimento":true,"umidade_solo":65,"bateria":85}
  ```

## How to Adapt for Your Sensors

### Step 1: Start with Base Template
Copy `../esp32_base_template.ino` as your starting point.

### Step 2: Modify Sensor-Specific Sections
Look for comments marked `// SENSOR-SPECIFIC:` and replace with your sensor code:

```cpp
// SENSOR-SPECIFIC: Add your sensor libraries
#include <YourSensorLibrary.h>

// SENSOR-SPECIFIC: Add sensor variables
YourSensor sensor;

// SENSOR-SPECIFIC: Initialize sensors in setup()
sensor.begin();

// SENSOR-SPECIFIC: Read sensors in readAndSendSensorData()
float value = sensor.readValue();
if (!isnan(value)) {
  payload += "\"your_field\":" + String(value, 1) + ",";
}
```

### Step 3: Customize JSON Fields
Add your sensor fields to the JSON payload:

```cpp
// Temperature sensors
payload += "\"temperatura\":" + String(temp, 1) + ",";

// Humidity sensors
payload += "\"umidade\":" + String(humidity, 1) + ",";

// Pressure sensors
payload += "\"pressao\":" + String(pressure, 2) + ",";

// Light sensors
payload += "\"luz\":" + String(light, 0) + ",";

// Motion sensors
payload += "\"movimento\":" + String(motion ? "true" : "false") + ",";

// Air quality
payload += "\"co2\":" + String(co2_ppm, 0) + ",";
payload += "\"pm25\":" + String(pm25, 1) + ",";

// Custom fields
payload += "\"bateria\":" + String(battery_percent, 1) + ",";
payload += "\"sinal\":" + String(wifi_rssi, 0) + ",";
```

## Common Sensor Types

| Sensor Type | Library | Field Name | Example Value |
|-------------|---------|------------|---------------|
| DHT22 | DHT sensor library | temperatura, umidade | 22.3, 65 |
| BMP280 | Adafruit BMP280 | temperatura, pressao | 21.8, 1013.25 |
| DS18B20 | DallasTemperature | temperatura | 20.5 |
| Light (LDR) | None | luz | 450 |
| PIR Motion | None | movimento | true/false |
| Soil Moisture | None | umidade_solo | 65 |
| Battery | None | bateria | 85 |
| CO2 (MH-Z19) | MHZ19 | co2 | 420 |
| PM2.5 (PMS5003) | PMS Library | pm25 | 12.5 |

## Testing Your Code

1. **Serial Monitor**: Check sensor readings are correct
2. **MQTT Explorer**: Verify messages are published to correct topics
3. **MongoDB**: Confirm data is saved with your custom fields

## Remember

- **Same WiFi/MQTT code** for all sensors
- **Only change sensor reading and JSON fields**
- **Server automatically adapts** to any JSON structure
- **No server changes needed** for new sensor types!