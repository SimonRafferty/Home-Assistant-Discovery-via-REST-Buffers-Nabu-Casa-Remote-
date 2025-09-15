/*
  HAMQTTDiscovery Library - Full Parameter Example

  This example demonstrates the complete usage of the HAMQTTDiscovery library
  with all parameters specified for maximum Home Assistant compatibility.

  It creates a switch and number control with custom icons, MQTT topics,
  and device information, then demonstrates reading and writing values.

  Requirements:
  1. WiFi connection
  2. Home Assistant with input_text helpers: mqtt_buffer_1 through mqtt_buffer_6
  3. Home Assistant automation to process discovery messages from buffers
  4. Long-lived access token from Home Assistant

  Hardware:
  - ESP32 board

  Author: Your Name
  Date: 2024
  License: MIT
*/

#include <WiFi.h>
#include <HAMQTTDiscovery.h>

// WiFi credentials - Update these with your network details
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// Home Assistant configuration - Update these with your HA details
const char* ha_server = "https://homeassistant.local:8123";  // No /api/ suffix needed
const char* ha_token = "YOUR_LONG_LIVED_ACCESS_TOKEN";

// Create library instance
HAMQTTDiscovery ha;

// Control pointers - we'll store references to our created controls
HAControl* livingRoomLight = nullptr;
HAControl* thermostatTemp = nullptr;
HAControl* outdoorTemp = nullptr;
HAControl* motionSensor = nullptr;

// State variables for demonstration
unsigned long lastUpdate = 0;
bool lightState = false;
float currentTemp = 21.5;
int motionCounter = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("=== HAMQTTDiscovery Full Parameter Example ===");

  // Connect to WiFi first
  connectWiFi();

  // Initialize the library
  Serial.println("Initializing HAMQTTDiscovery library...");
  if (!ha.begin(ha_server, ha_token)) {
    Serial.println("❌ Failed to initialize HAMQTTDiscovery library");
    Serial.println("Check WiFi connection and try again");
    while (1) {
      delay(1000);
    }
  }
  Serial.println("✅ Library initialized successfully");

  // Set comprehensive device information for better HA integration
  Serial.println("Setting device information...");
  ha.setDevice(
    "esp32_home_controller_001",    // uniqueId - must be globally unique
    "ESP32 Home Controller",        // name - displayed in HA
    "YourCompany",                  // manufacturer
    "ESP32-DevKit-V1",             // model
    "1.2.0"                        // software version
  );
  Serial.println("✅ Device information set");

  Serial.println("\nCreating Home Assistant controls...");

  // Create a switch with all parameters specified
  Serial.println("Creating living room light switch...");
  livingRoomLight = ha.createSwitch(
    "living_room_light",              // objectId (becomes switch.living_room_light in HA)
    "Living Room Light",              // name (friendly name in HA)
    "lr_light_esp32_001",            // uniqueId (globally unique identifier)
    "mdi:lightbulb",                 // icon (Material Design Icons)
    "home/living_room/light/state",   // stateTopic (custom MQTT state topic)
    "home/living_room/light/set",     // commandTopic (custom MQTT command topic)
    "home/living_room/light/avail",   // availabilityTopic (MQTT availability)
    "ON",                            // payloadOn (state when switch is on)
    "OFF"                           // payloadOff (state when switch is off)
    // device parameter uses the default device we set above
  );

  if (livingRoomLight) {
    Serial.println("✅ Living room light switch created successfully");
    // Set initial state to OFF
    ha.writeControl(livingRoomLight, "OFF");
    Serial.println("   Initial state set to OFF");
  } else {
    Serial.println("❌ Failed to create living room light switch");
  }

  // Create a number control (thermostat) with all parameters
  Serial.println("Creating thermostat temperature control...");
  thermostatTemp = ha.createNumber(
    "thermostat_setpoint",            // objectId (becomes number.thermostat_setpoint in HA)
    "Thermostat Temperature",         // name (friendly name in HA)
    "thermo_temp_esp32_001",         // uniqueId (globally unique identifier)
    15.0,                            // minValue (minimum temperature)
    30.0,                            // maxValue (maximum temperature)
    0.5,                             // step (increment/decrement step)
    "°C",                           // unit (unit of measurement)
    "slider",                        // mode ("slider", "box", or "auto")
    "mdi:thermometer",               // icon (Material Design Icons)
    "home/thermostat/temp/state",    // stateTopic (custom MQTT state topic)
    "home/thermostat/temp/set",      // commandTopic (custom MQTT command topic)
    "home/thermostat/temp/avail"     // availabilityTopic (MQTT availability)
    // device parameter uses the default device we set above
  );

  if (thermostatTemp) {
    Serial.println("✅ Thermostat temperature control created successfully");
    // Set initial temperature
    ha.writeControl(thermostatTemp, String(currentTemp, 1));
    Serial.printf("   Initial temperature set to %.1f°C\n", currentTemp);
  } else {
    Serial.println("❌ Failed to create thermostat temperature control");
  }

  // Create a sensor (read-only) for outdoor temperature
  Serial.println("Creating outdoor temperature sensor...");
  outdoorTemp = ha.createSensor(
    "outdoor_temperature",           // objectId (becomes sensor.outdoor_temperature in HA)
    "Outdoor Temperature",           // name (friendly name in HA)
    "outdoor_temp_esp32_001",       // uniqueId (globally unique identifier)
    "°C",                          // unit (unit of measurement)
    "mdi:thermometer-lines",        // icon (Material Design Icons)
    "home/outdoor/temp/state",      // stateTopic (custom MQTT state topic)
    "home/outdoor/temp/avail"       // availabilityTopic (MQTT availability)
    // device parameter uses the default device we set above
  );

  if (outdoorTemp) {
    Serial.println("✅ Outdoor temperature sensor created successfully");
    // Set initial outdoor temperature
    ha.writeControl(outdoorTemp, "18.3");
    Serial.println("   Initial outdoor temperature set to 18.3°C");
  } else {
    Serial.println("❌ Failed to create outdoor temperature sensor");
  }

  // Create a binary sensor for motion detection
  Serial.println("Creating motion sensor...");
  motionSensor = ha.createBinarySensor(
    "motion_detector",               // objectId (becomes binary_sensor.motion_detector in HA)
    "Motion Detector",               // name (friendly name in HA)
    "motion_detect_esp32_001",      // uniqueId (globally unique identifier)
    "mdi:motion-sensor",            // icon (Material Design Icons)
    "home/motion/state",            // stateTopic (custom MQTT state topic)
    "home/motion/avail",            // availabilityTopic (MQTT availability)
    "MOTION",                       // payloadOn (state when motion detected)
    "NO_MOTION"                     // payloadOff (state when no motion)
    // device parameter uses the default device we set above
  );

  if (motionSensor) {
    Serial.println("✅ Motion sensor created successfully");
    // Set initial state to no motion
    ha.writeControl(motionSensor, "NO_MOTION");
    Serial.println("   Initial state set to NO_MOTION");
  } else {
    Serial.println("❌ Failed to create motion sensor");
  }

  Serial.println("\n=== Setup Complete ===");
  Serial.println("Check your Home Assistant for the new entities:");
  if (livingRoomLight) Serial.println("- switch.living_room_light");
  if (thermostatTemp) Serial.println("- number.thermostat_setpoint");
  if (outdoorTemp) Serial.println("- sensor.outdoor_temperature");
  if (motionSensor) Serial.println("- binary_sensor.motion_detector");

  Serial.println("\nStarting control updates every 15 seconds...");
  Serial.println("Monitor Serial output to see read/write operations");
}

void loop() {
  unsigned long now = millis();

  // Update controls every 15 seconds
  if (now - lastUpdate >= 15000) {
    lastUpdate = now;

    Serial.println("\n--- Control Update Cycle ---");

    // 1. Toggle living room light and verify state
    if (livingRoomLight) {
      lightState = !lightState;
      String newState = lightState ? "ON" : "OFF";

      Serial.printf("Setting light to: %s\n", newState.c_str());
      if (ha.writeControl(livingRoomLight, newState)) {
        Serial.println("✅ Light state updated successfully");

        // Read back the state to verify
        delay(500); // Small delay for HA to process
        String currentState = ha.readControl(livingRoomLight);
        Serial.printf("   Current light state from HA: %s\n", currentState.c_str());
      } else {
        Serial.println("❌ Failed to update light state");
      }
    }

    // 2. Update thermostat with a new temperature
    if (thermostatTemp) {
      // Simulate temperature changes between 18-25°C
      currentTemp += (random(-20, 21) / 10.0); // ±2.0°C change
      if (currentTemp < 18.0) currentTemp = 18.0;
      if (currentTemp > 25.0) currentTemp = 25.0;

      Serial.printf("Setting thermostat to: %.1f°C\n", currentTemp);
      if (ha.writeControl(thermostatTemp, String(currentTemp, 1))) {
        Serial.println("✅ Thermostat updated successfully");

        // Read back the value
        delay(500);
        String currentValue = ha.readControl(thermostatTemp);
        Serial.printf("   Current thermostat setting from HA: %s°C\n", currentValue.c_str());
      } else {
        Serial.println("❌ Failed to update thermostat");
      }
    }

    // 3. Update outdoor temperature sensor (simulate weather changes)
    if (outdoorTemp) {
      float outdoor = 15.0 + (random(0, 101) / 10.0); // 15.0 to 25.0°C
      Serial.printf("Updating outdoor temperature to: %.1f°C\n", outdoor);
      if (ha.writeControl(outdoorTemp, String(outdoor, 1))) {
        Serial.println("✅ Outdoor temperature updated successfully");
      } else {
        Serial.println("❌ Failed to update outdoor temperature");
      }
    }

    // 4. Simulate motion detection (30% chance of motion)
    if (motionSensor) {
      bool motionDetected = (random(0, 10) < 3);
      String motionState = motionDetected ? "MOTION" : "NO_MOTION";

      Serial.printf("Motion sensor: %s\n", motionDetected ? "Motion detected!" : "No motion");
      if (ha.writeControl(motionSensor, motionState)) {
        Serial.println("✅ Motion sensor updated successfully");
        if (motionDetected) {
          motionCounter++;
          Serial.printf("   Motion events today: %d\n", motionCounter);
        }
      } else {
        Serial.println("❌ Failed to update motion sensor");
      }
    }

    // 5. Health check - verify all controls are still online
    Serial.println("\n--- Health Check ---");
    checkControlHealth("Living Room Light", livingRoomLight);
    checkControlHealth("Thermostat", thermostatTemp);
    checkControlHealth("Outdoor Temperature", outdoorTemp);
    checkControlHealth("Motion Sensor", motionSensor);
  }

  delay(100); // Small delay to prevent overwhelming the system
}

void connectWiFi() {
  Serial.printf("Connecting to WiFi network: %s\n", ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startTime < 30000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("✅ WiFi connected successfully!\n");
    Serial.printf("   IP Address: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("   Signal Strength: %d dBm\n", WiFi.RSSI());
  } else {
    Serial.println("❌ WiFi connection failed!");
    Serial.println("Please check your credentials and try again");
    while (1) {
      delay(1000);
    }
  }
}

void checkControlHealth(const String& name, HAControl* control) {
  if (!control) {
    Serial.printf("   %s: ❌ Not created\n", name.c_str());
    return;
  }

  if (ha.isControlOnline(control)) {
    Serial.printf("   %s: ✅ Online\n", name.c_str());
  } else {
    Serial.printf("   %s: ⚠️  Offline or unreachable\n", name.c_str());
  }
}