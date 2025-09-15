/*
  HAMQTTDiscovery Library - Basic Usage Example

  This example demonstrates how to use the HAMQTTDiscovery library to create
  and manage Home Assistant entities via MQTT Discovery using REST API helpers.

  Requirements:
  1. WiFi connection configured before calling library functions
  2. Home Assistant with input_text helpers: mqtt_buffer_1 through mqtt_buffer_6
  3. Home Assistant automation to process discovery messages from buffers
  4. Long-lived access token from Home Assistant

  Hardware:
  - ESP32 board

  Created: 2024
  Author: Your Name
*/

#include <WiFi.h>
#include <HAMQTTDiscovery.h>

// WiFi credentials
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// Home Assistant configuration
const char* ha_server = "https://homeassistant.local:8123";  // No /api/ suffix
const char* ha_token = "YOUR_LONG_LIVED_ACCESS_TOKEN";

// Create library instance
HAMQTTDiscovery ha;

// Control pointers
HAControl* testSwitch = nullptr;
HAControl* testNumber = nullptr;
HAControl* testSensor = nullptr;
HAControl* testBinarySensor = nullptr;

unsigned long lastUpdate = 0;
bool switchState = false;
float sensorValue = 0.0;

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("HAMQTTDiscovery Basic Usage Example");

  // Connect to WiFi
  connectWiFi();

  // Initialize the library
  if (!ha.begin(ha_server, ha_token)) {
    Serial.println("Failed to initialize HAMQTTDiscovery library");
    while (1) delay(1000);
  }

  // Set up device information (optional, but recommended)
  ha.setDevice("esp32_test_device", "ESP32 Test Device", "YourCompany", "ESP32", "1.0.0");

  Serial.println("Creating Home Assistant controls...");

  // Create a switch control
  testSwitch = ha.createSwitch("test_switch", "Test Switch", "test_switch_uid");
  if (testSwitch) {
    Serial.println("✓ Switch created successfully");
  } else {
    Serial.println("✗ Failed to create switch");
  }

  // Create a number control (slider)
  testNumber = ha.createNumber("test_level", "Test Level", "test_level_uid", 0, 100, 1, "%");
  if (testNumber) {
    Serial.println("✓ Number control created successfully");
  } else {
    Serial.println("✗ Failed to create number control");
  }

  // Create a sensor (read-only)
  testSensor = ha.createSensor("test_temperature", "Test Temperature", "test_temp_uid", "°C");
  if (testSensor) {
    Serial.println("✓ Sensor created successfully");
  } else {
    Serial.println("✗ Failed to create sensor");
  }

  // Create a binary sensor
  testBinarySensor = ha.createBinarySensor("test_motion", "Test Motion", "test_motion_uid");
  if (testBinarySensor) {
    Serial.println("✓ Binary sensor created successfully");
  } else {
    Serial.println("✗ Failed to create binary sensor");
  }

  Serial.println("Setup complete! Check Home Assistant for new entities.");
  Serial.println("Starting control updates every 10 seconds...");
}

void loop() {
  unsigned long now = millis();

  // Update controls every 10 seconds
  if (now - lastUpdate >= 10000) {
    lastUpdate = now;

    Serial.println("\n--- Updating Controls ---");

    // Toggle switch state
    if (testSwitch) {
      switchState = !switchState;
      String value = switchState ? "ON" : "OFF";
      if (ha.writeControl(testSwitch, value)) {
        Serial.printf("✓ Switch set to: %s\n", value.c_str());
      } else {
        Serial.println("✗ Failed to update switch");
      }
    }

    // Update number with random value
    if (testNumber) {
      int randomLevel = random(0, 101);
      if (ha.writeControl(testNumber, String(randomLevel))) {
        Serial.printf("✓ Level set to: %d%%\n", randomLevel);
      } else {
        Serial.println("✗ Failed to update level");
      }
    }

    // Update sensor with simulated temperature
    if (testSensor) {
      sensorValue += random(-50, 51) / 100.0;  // Random walk
      if (sensorValue < 15.0) sensorValue = 15.0;
      if (sensorValue > 35.0) sensorValue = 35.0;

      if (ha.writeControl(testSensor, String(sensorValue, 1))) {
        Serial.printf("✓ Temperature updated to: %.1f°C\n", sensorValue);
      } else {
        Serial.println("✗ Failed to update temperature");
      }
    }

    // Update binary sensor randomly
    if (testBinarySensor) {
      bool motionDetected = (random(0, 10) < 3);  // 30% chance of motion
      String value = motionDetected ? "ON" : "OFF";
      if (ha.writeControl(testBinarySensor, value)) {
        Serial.printf("✓ Motion sensor: %s\n", motionDetected ? "Motion detected" : "No motion");
      } else {
        Serial.println("✗ Failed to update motion sensor");
      }
    }

    // Demonstrate reading control values
    if (testSwitch) {
      String currentState = ha.readControl(testSwitch);
      Serial.printf("Switch current state: %s\n", currentState.c_str());
    }

    if (testNumber) {
      String currentLevel = ha.readControl(testNumber);
      Serial.printf("Number current value: %s\n", currentLevel.c_str());
    }
  }

  delay(100);  // Small delay to prevent overwhelming the system
}

void connectWiFi() {
  Serial.printf("Connecting to WiFi: %s\n", ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startTime < 20000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("✓ WiFi connected! IP: %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("✗ WiFi connection failed!");
    while (1) delay(1000);
  }
}