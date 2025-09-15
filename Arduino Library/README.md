# HAMQTTDiscovery Library

An Arduino library for ESP32 that enables Home Assistant MQTT Discovery via REST API and input_text helpers. This library allows you to create and manage Home Assistant entities (switches, numbers, sensors, binary sensors) without requiring direct MQTT broker access.

This partially removes the MQTT restriction when connecting via Nabu Casa, without compromising security (as much). 

## Features

- **Multiple Entity Types**: Support for switches, numbers, sensors, and binary sensors
- **Full Parameter Control**: Complete control over all entity properties (icons, topics, payloads, etc.)
- **Automatic Existence Checking**: Prevents duplicate entity creation
- **State Management**: Read and write entity states via REST API
- **Device Grouping**: Organize entities under device categories
- **Validation**: Waits for entity creation and validates success
- **Memory Management**: Automatic cleanup of control structures

## Requirements

### Hardware
- ESP32 board with WiFi capability

### Home Assistant Setup
1. **Input Text Helpers**: Create 6 input_text helpers named:
   - `input_text.mqtt_buffer_1`
   - `input_text.mqtt_buffer_2`
   - `input_text.mqtt_buffer_3`
   - `input_text.mqtt_buffer_4`
   - `input_text.mqtt_buffer_5`
   - `input_text.mqtt_buffer_6`

2. **Automation**: Create an automation that monitors `input_text.mqtt_buffer_6` for "END" signal and publishes the assembled discovery JSON to your MQTT broker.

3. **Long-Lived Access Token**: Generate a token from Home Assistant Profile → Security

## Installation

1. Download or clone this repository
2. Place the `HAMQTTDiscovery` folder in your Arduino libraries directory
3. Restart Arduino IDE
4. Include the library: `#include <HAMQTTDiscovery.h>`

## Complete Usage Example

```cpp
#include <WiFi.h>
#include <HAMQTTDiscovery.h>

// WiFi credentials
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// Home Assistant configuration
const char* ha_server = "https://homeassistant.local:8123";
const char* ha_token = "YOUR_LONG_LIVED_ACCESS_TOKEN";

// Create library instance
HAMQTTDiscovery ha;

// Control pointers
HAControl* livingRoomLight = nullptr;
HAControl* thermostatTemp = nullptr;

void setup() {
  Serial.begin(115200);

  // Connect to WiFi
  connectWiFi();

  // Initialize the library
  if (!ha.begin(ha_server, ha_token)) {
    Serial.println("Failed to initialize HAMQTTDiscovery library");
    return;
  }

  // Set device information (all parameters required for HA compatibility)
  ha.setDevice("esp32_home_controller", "ESP32 Home Controller",
               "YourCompany", "ESP32-DevKit", "1.0.0");

  // Create a switch with full parameters
  livingRoomLight = ha.createSwitch(
    "living_room_light",           // objectId (must be unique)
    "Living Room Light",           // name (displayed in HA)
    "lr_light_unique_001",         // uniqueId (must be globally unique)
    "mdi:lightbulb",              // icon (Material Design Icons)
    "home/living_room/light/state", // stateTopic (custom MQTT topic)
    "home/living_room/light/set",   // commandTopic (custom MQTT topic)
    "home/living_room/light/avail", // availabilityTopic
    "ON",                          // payloadOn
    "OFF"                          // payloadOff
    // device parameter defaults to the one set above
  );

  if (livingRoomLight) {
    Serial.println("✓ Living room light switch created");
    // Set initial state
    ha.writeControl(livingRoomLight, "OFF");
  }

  // Create a number control with full parameters
  thermostatTemp = ha.createNumber(
    "thermostat_setpoint",         // objectId
    "Thermostat Temperature",      // name
    "thermo_temp_unique_001",      // uniqueId
    15.0,                          // minValue
    30.0,                          // maxValue
    0.5,                           // step
    "°C",                          // unit
    "slider",                      // mode ("slider", "box", "auto")
    "mdi:thermometer",             // icon
    "home/thermostat/temp/state",  // stateTopic
    "home/thermostat/temp/set",    // commandTopic
    "home/thermostat/temp/avail"   // availabilityTopic
    // device parameter defaults to the one set above
  );

  if (thermostatTemp) {
    Serial.println("✓ Thermostat temperature control created");
    // Set initial temperature
    ha.writeControl(thermostatTemp, "21.5");
  }

  Serial.println("Setup complete!");
}

void loop() {
  static unsigned long lastUpdate = 0;
  static bool lightState = false;

  // Update every 10 seconds
  if (millis() - lastUpdate >= 10000) {
    lastUpdate = millis();

    // Toggle light state
    if (livingRoomLight) {
      lightState = !lightState;
      String newState = lightState ? "ON" : "OFF";

      if (ha.writeControl(livingRoomLight, newState)) {
        Serial.printf("Light switched %s\n", newState.c_str());

        // Read back the state to verify
        String currentState = ha.readControl(livingRoomLight);
        Serial.printf("Current light state: %s\n", currentState.c_str());
      }
    }

    // Update thermostat with a random temperature between 18-25°C
    if (thermostatTemp) {
      float temp = 18.0 + (random(0, 71) / 10.0); // 18.0 to 25.0 in 0.1 steps

      if (ha.writeControl(thermostatTemp, String(temp, 1))) {
        Serial.printf("Thermostat set to %.1f°C\n", temp);

        // Read back the value
        String currentTemp = ha.readControl(thermostatTemp);
        Serial.printf("Current thermostat setting: %s°C\n", currentTemp.c_str());
      }
    }

    // Check if controls are still online
    if (livingRoomLight && !ha.isControlOnline(livingRoomLight)) {
      Serial.println("Warning: Living room light is offline");
    }

    if (thermostatTemp && !ha.isControlOnline(thermostatTemp)) {
      Serial.println("Warning: Thermostat is offline");
    }
  }

  delay(100);
}

void connectWiFi() {
  Serial.printf("Connecting to WiFi: %s\n", ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.printf("✓ WiFi connected! IP: %s\n", WiFi.localIP().toString().c_str());
}
```

## API Reference

### Class: HAMQTTDiscovery

#### Initialization
```cpp
bool begin(const String& serverUrl, const String& token)
```
- `serverUrl`: Home Assistant base URL (e.g., "https://homeassistant.local:8123")
- `token`: Long-lived access token
- Returns: `true` if initialization successful, `false` if WiFi not connected

#### Device Configuration
```cpp
void setDevice(const String& uniqueId, const String& name,
               const String& manufacturer = "", const String& model = "",
               const String& swVersion = "")
```
Sets default device information for all entities. **All parameters recommended for HA compatibility.**

#### Control Creation Functions

##### Switch
```cpp
HAControl* createSwitch(const String& objectId, const String& name, const String& uniqueId,
                       const String& icon = "", const String& stateTopic = "",
                       const String& commandTopic = "", const String& availabilityTopic = "",
                       const String& payloadOn = "ON", const String& payloadOff = "OFF",
                       HADevice* device = nullptr)
```

##### Number (Slider/Box)
```cpp
HAControl* createNumber(const String& objectId, const String& name, const String& uniqueId,
                       float minVal = 0, float maxVal = 100, float step = 1,
                       const String& unit = "", const String& mode = "slider",
                       const String& icon = "", const String& stateTopic = "",
                       const String& commandTopic = "", const String& availabilityTopic = "",
                       HADevice* device = nullptr)
```

##### Sensor (Read-only)
```cpp
HAControl* createSensor(const String& objectId, const String& name, const String& uniqueId,
                       const String& unit = "", const String& icon = "",
                       const String& stateTopic = "", const String& availabilityTopic = "",
                       HADevice* device = nullptr)
```

##### Binary Sensor (On/Off)
```cpp
HAControl* createBinarySensor(const String& objectId, const String& name, const String& uniqueId,
                             const String& icon = "", const String& stateTopic = "",
                             const String& availabilityTopic = "",
                             const String& payloadOn = "ON", const String& payloadOff = "OFF",
                             HADevice* device = nullptr)
```

**Default Values:**
- Icons: "mdi:toggle-switch", "mdi:gauge", "mdi:gauge", "mdi:motion-sensor"
- Topics: Auto-generated as "virt/{objectId}/{state|set|avail}"
- All empty string parameters use sensible defaults

#### State Management
```cpp
bool writeControl(HAControl* control, const String& value)  // Set control value
String readControl(HAControl* control)                      // Get current value
bool isControlOnline(HAControl* control)                    // Check if accessible
```

## Best Practices

1. **Always provide full parameters** - HA works best with complete entity definitions
2. **Use meaningful objectIds** - they become part of the entity_id in HA
3. **Make uniqueIds globally unique** - include device info or random elements
4. **Choose appropriate icons** - use Material Design Icons (mdi:...)
5. **Set custom MQTT topics** - if integrating with existing MQTT structure
6. **Check return values** - functions return nullptr/false on failure
7. **Monitor entity status** - use isControlOnline() for health checks

## Error Handling

The library includes comprehensive error handling:
- WiFi connection validation in begin()
- Entity existence checking before creation
- HTTP response validation
- Timeout management for entity creation
- Memory management and cleanup

## Limitations

- Discovery payload limited to 1275 characters (5 × 255)
- Requires Home Assistant automation for MQTT publishing

## Examples

See `examples/BasicUsage/` for a complete working example with multiple entity types.

## Security Notes

- Currently uses `setInsecure()` for HTTPS connections

## License

This library is released under the MIT License.
