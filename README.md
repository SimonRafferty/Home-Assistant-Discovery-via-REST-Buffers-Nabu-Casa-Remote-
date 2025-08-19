# ESP32 → Home Assistant Discovery via REST Buffers (Nabu Casa Remote)

This project demonstrates how to **publish Home Assistant MQTT Discovery messages** using an **ESP32** remotely via **Nabu Casa Cloud**, without needing direct access to your local MQTT broker.  

Instead of sending MQTT directly, the ESP32 posts JSON fragments into a set of `input_text` helpers via the **Home Assistant REST API** (proxied by Nabu Casa).  
A **Home Assistant automation** assembles these fragments, validates them, and publishes them to MQTT (with `retain=true`) so that entities are created or removed dynamically.

---

## Overview

- **ESP32 Code**  
  - Mimics the [`ArduinoHA`](https://github.com/dawidchyrzynski/arduino-home-assistant) library structures (`HADevice`, `HASwitch`, `HANumber`).
  - Builds MQTT Discovery JSON messages for entities.
  - Splits the JSON into up to **5 chunks (≤255 chars each)** and posts them to HA helpers (`mqtt_buffer_1..5`).
  - Triggers publishing by writing `"END"` into `mqtt_buffer_6`.
  - Posts example state updates to HA using the REST API (mainly for demonstration).

- **Home Assistant Automation**  
  - Watches for `"END"` in `mqtt_buffer_6`.
  - Concatenates buffers 1–5 into a JSON envelope:
    ```json
    {
      "topic": "homeassistant/sensor/example/config",
      "payload": { ... discovery config ... }
    }
    ```
  - Validates the JSON and publishes to MQTT with `retain=true`.
  - Clears the buffers afterwards.

- **Helpers**  
  - Six `input_text` helpers store the ESP32-sent fragments.
  - These act as a "mailbox" between the ESP32 and the HA automation.

---

## Home Assistant Setup

### 1. Create Helpers (via UI)

1. In Home Assistant, go to **Settings → Devices & Services → Helpers**.  
2. Click **Create Helper → Text**.  
3. Create **6 helpers** named exactly:
   - `mqtt_buffer_1` (maximum length: **255**)
   - `mqtt_buffer_2` (maximum length: **255**)
   - `mqtt_buffer_3` (maximum length: **255**)
   - `mqtt_buffer_4` (maximum length: **255**)
   - `mqtt_buffer_5` (maximum length: **255**)
   - `mqtt_buffer_6` (maximum length: **10**, used as a trigger)  

These helpers will hold the JSON fragments and trigger the automation.

---

### 2. Add the Automation (via UI)

1. Go to **Settings → Automations & Scenes → Automations**.  
2. Click **Add Automation → Start with an empty automation**.  
3. Click the three dots (⋮) in the top-right and choose **Edit in YAML**.  
4. Paste the full automation code from this repo (see `automation.yaml`).  
5. Save.

This automation:
- Joins buffers 1–5 into one JSON string.  
- Validates JSON.  
- Ensures the topic matches `homeassistant/<component>/<object_id>/config`.  
- Publishes retained discovery payload to MQTT.  
- Deletes entities if payload is an empty string.  
- Clears the buffers afterwards.

---

## ESP32 Setup

### 1. Install Arduino IDE Libraries

- ESP32 board support package  
- WiFi, WiFiClientSecure, HTTPClient (bundled with ESP32 core)

### 2. Configure the Sketch

In `esp32_ha_discovery.ino`:

```cpp
const char* WIFI_SSID     = "YOUR_WIFI";
const char* WIFI_PASSWORD = "YOUR_PASS";

// Use your Nabu Casa URL (found under Settings → Home Assistant Cloud)
// e.g. "https://<your-subdomain>.ui.nabu.casa"
const char* serverName    = "https://<your-subdomain>.ui.nabu.casa";

// Long-Lived Access Token from Home Assistant (Profile → Security)
const char* bearerToken   = "YOUR_LONG_LIVED_TOKEN";
3. Upload and Run

Flash the ESP32 with the provided sketch.

On boot, it will:

Connect to WiFi.

Create a test device (ESP Test Device) with:

switch.esp_test_switch

number.esp_test_level

Publish discovery for both entities.

Every 5 seconds, update their states with random/demo values.

Example Discovery JSON

What the ESP32 ultimately delivers (via the buffers):
{
  "topic": "homeassistant/number/esp_test_level/config",
  "payload": {
    "name": "ESP Test Level",
    "unique_id": "esp_test_level_uid",
    "state_topic": "virt/esp_test/level/state",
    "command_topic": "virt/esp_test/level/set",
    "availability_topic": "virt/esp_test/avail",
    "device": {
      "identifiers": ["esp32_test_device_001"],
      "manufacturer": "YourCo",
      "model": "ESP32",
      "name": "ESP Test Device",
      "sw_version": "1.0.0"
    },
    "min": 0,
    "max": 100,
    "step": 1,
    "unit_of_measurement": "%",
    "mode": "slider"
  }
}

Removing Entities

To remove an entity, publish the same discovery topic with a retained empty payload.
For example, to remove the number.esp_test_level entity:
{
  "topic": "homeassistant/number/esp_test_level/config",
  "payload": ""
}

This publishes a retained empty message to
homeassistant/number/esp_test_level/config, which instructs Home Assistant to delete the entity.

Security Notes

The ESP32 only talks to Home Assistant’s REST API (HTTPS + bearer token) via Nabu Casa Cloud.

The automation only allows publishing to discovery topics; no arbitrary MQTT control is permitted.

Retained empty payloads remove entities cleanly.

If needed, enforce broker ACLs so this HA client may only write to homeassistant/+/+/config.

License

MIT — do what you like, but no warranty.
