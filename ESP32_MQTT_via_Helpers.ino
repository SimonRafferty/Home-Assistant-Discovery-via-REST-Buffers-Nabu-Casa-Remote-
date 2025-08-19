/*
  ESP32 → Home Assistant (via REST) “Discovery-over-Helpers” Framework
  -------------------------------------------------------------------
  - Mimics ArduinoHA-like structures: HADevice, HASwitch, HANumber
  - Builds MQTT Discovery JSON (topic + payload) for each entity
  - Splits into ≤5 chunks (≤255 chars each) and posts to:
        input_text.mqtt_buffer_1 ... _5
    then posts "END" to input_text.mqtt_buffer_6 to trigger publish

  REQUIREMENTS:
    - Your Home Assistant has the 6 buffers set up:
        input_text.mqtt_buffer_1 .. input_text.mqtt_buffer_6
    - The HA automation that assembles & publishes discovery JSON from buffers

  SECURITY:
    - Uses HTTPS with setInsecure() (certificate not validated).
      For production, install the HA certificate and validate it.

  TEST SETUP:
    - Creates a device "ESP Test Device" with:
        * switch: "esp_test_switch"
        * number: "esp_test_level" (0..100, step 1)
    - Publishes their discovery configs once in setup()
    - Every 5s, toggles the switch and posts a random number state

  NOTE on REST state updates:
    - The function sendStateToHomeAssistant(entity_id, state, attributes)
      updates HA’s state machine via /api/states/<entity_id>.
    - MQTT entities in HA normally expect state via MQTT; REST updates
      won’t substitute for MQTT messages to state_topic.
*/

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

// -------------------- USER CONFIG --------------------
const char* WIFI_SSID     = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

// Base URL for Home Assistant (include protocol + host[:port])
// e.g. "https://homeassistant.local:8123"
const char* serverName   = "https://homeassistant.local:8123";

// Long-Lived Access Token (Profile → Security in Home Assistant)
const char* bearerToken  = "YOUR_LONG_LIVED_TOKEN";
// -----------------------------------------------------

// ---------- Minimal ArduinoHA-like Structures ----------
struct HADevice {
  String uniqueId;
  String name;
  String manufacturer;
  String model;
  String swVersion;

  void setUniqueId(const String& id)     { uniqueId = id; }
  void setName(const String& n)          { name = n; }
  void setManufacturer(const String& m)  { manufacturer = m; }
  void setModel(const String& m)         { model = m; }
  void setSoftwareVersion(const String& v){ swVersion = v; }

  // Device object used inside discovery payloads
  String deviceJson() const {
    // "identifiers" must be an array; we use uniqueId
    String j = F("{\"identifiers\":[\"");
    j += uniqueId; j += F("\"],");
    if (manufacturer.length()) { j += F("\"manufacturer\":\""); j += escape(manufacturer); j += F("\","); }
    if (model.length())        { j += F("\"model\":\"");        j += escape(model);        j += F("\","); }
    if (name.length())         { j += F("\"name\":\"");         j += escape(name);         j += F("\","); }
    if (swVersion.length())    { j += F("\"sw_version\":\"");   j += escape(swVersion);    j += F("\","); }
    // Trim trailing comma if present
    if (j.endsWith(",")) j.remove(j.length()-1);
    j += "}";
    return j;
  }

  // Simple JSON string escaper for quotes/backslashes/newlines
  static String escape(const String& in) {
    String out; out.reserve(in.length()+8);
    for (size_t i=0;i<in.length();++i) {
      char c = in[i];
      if (c=='\"' || c=='\\') { out += '\\'; out += c; }
      else if (c=='\n') { out += "\\n"; }
      else if (c=='\r') { out += "\\r"; }
      else { out += c; }
    }
    return out;
  }
};

struct HAEntityBase {
  String component;        // "switch", "number", etc.
  String objectId;         // e.g., "esp_test_switch"
  String name;             // Friendly name
  String icon;             // mdi:...
  String uniqueId;         // unique per entity
  String stateTopic;       // mqtt state topic
  String commandTopic;     // mqtt command topic (where applicable)
  String availabilityTopic;
  HADevice* device = nullptr;

  void setName(const String& n)     { name = n; }
  void setIcon(const String& i)     { icon = i; }
  void setUniqueId(const String& u) { uniqueId = u; }

  String discoveryTopic() const {
    // homeassistant/<component>/<object_id>/config
    String t = F("homeassistant/");
    t += component; t += '/'; t += objectId; t += F("/config");
    return t;
  }

  // Base payload fields shared by all
  String basePayloadJson() const {
    String j = "{";
    if (name.length())     { j += F("\"name\":\""); j += HADevice::escape(name); j += F("\","); }
    if (icon.length())     { j += F("\"icon\":\""); j += HADevice::escape(icon); j += F("\","); }
    if (uniqueId.length()) { j += F("\"unique_id\":\""); j += HADevice::escape(uniqueId); j += F("\","); }
    if (availabilityTopic.length()) { j += F("\"availability_topic\":\""); j += HADevice::escape(availabilityTopic); j += F("\","); }
    if (stateTopic.length())        { j += F("\"state_topic\":\"");        j += HADevice::escape(stateTopic);        j += F("\","); }
    if (commandTopic.length())      { j += F("\"command_topic\":\"");      j += HADevice::escape(commandTopic);      j += F("\","); }
    if (device) { j += F("\"device\":"); j += device->deviceJson(); j += F(","); }
    // Trim trailing comma if present
    if (j.endsWith(",")) j.remove(j.length()-1);
    j += "}";
    return j;
  }
};

struct HASwitch : public HAEntityBase {
  // Switch-specific: usually just needs state/command topics
  HASwitch() { component = "switch"; }
  void setStateTopic(const String& t)   { stateTopic = t; }
  void setCommandTopic(const String& t) { commandTopic = t; }
  void setAvailabilityTopic(const String& t) { availabilityTopic = t; }

  // For discovery, base payload is sufficient
  String discoveryPayloadJson() const {
    return basePayloadJson();
  }
};

struct HANumber : public HAEntityBase {
  float minVal = 0.0f;
  float maxVal = 100.0f;
  float step   = 1.0f;
  String unit;
  String mode; // "slider", "box", "auto"
  String valueTemplate; // optional for state parsing

  HANumber() { component = "number"; }
  void setStateTopic(const String& t)   { stateTopic = t; }
  void setCommandTopic(const String& t) { commandTopic = t; }
  void setUnitOfMeasurement(const String& u) { unit = u; }
  void setMin(float v)  { minVal = v; }
  void setMax(float v)  { maxVal = v; }
  void setStep(float v) { step   = v; }
  void setMode(const String& m) { mode = m; }
  void setAvailabilityTopic(const String& t) { availabilityTopic = t; }
  void setValueTemplate(const String& vt) { valueTemplate = vt; }

  String discoveryPayloadJson() const {
    // Start from base then add number-specific keys
    String base = basePayloadJson();
    // Insert fields before final '}'
    if (base.endsWith("}")) base.remove(base.length()-1);
    String j = base;
    j += F(",\"min\":");  j += String(minVal, 3);
    j += F(",\"max\":");  j += String(maxVal, 3);
    j += F(",\"step\":"); j += String(step,   3);
    if (unit.length())        { j += F(",\"unit_of_measurement\":\""); j += HADevice::escape(unit); j += F("\""); }
    if (mode.length())        { j += F(",\"mode\":\"");               j += HADevice::escape(mode); j += F("\""); }
    if (valueTemplate.length()){ j += F(",\"value_template\":\"");    j += HADevice::escape(valueTemplate); j += F("\""); }
    j += "}";
    return j;
  }
};
// ------------------------------------------------------


// ----------------- REST/HTTP Helpers ------------------

static String authHeader() {
  return String("Bearer ") + bearerToken;
}

// POST JSON to /api/states/<entity_id> with {"state":..., "attributes":{...}}
bool postStateToHA(const String& entityId, const String& stateJson, const String& attributesJson = "{}") {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.printf("WiFi not connected — cannot send state for %s\n", entityId.c_str());
    return false;
  }

  String url = String(serverName) + "/api/states/" + entityId;

  WiFiClientSecure* client = new WiFiClientSecure();
  client->setInsecure();

  HTTPClient https;
  https.begin(*client, url);
  https.addHeader("Authorization", authHeader());
  https.addHeader("Content-Type", "application/json");

  String payload = "{\"state\":" + stateJson + ",\"attributes\":" + attributesJson + "}";

  int httpCode = https.POST(payload);
  if (httpCode > 0) {
    Serial.printf("POST %s → HTTP %d\n", url.c_str(), httpCode);
  } else {
    Serial.printf("POST error %s → %s\n", url.c_str(), https.errorToString(httpCode).c_str());
  }
  https.end();
  delete client;
  return httpCode >= 200 && httpCode < 300;
}

// POST a raw string into an input_text helper via /api/states/<helper_entity>
bool postHelperBuffer(uint8_t idx, const String& value) {
  if (idx < 1 || idx > 6) return false;
  String entityId = "input_text.mqtt_buffer_" + String(idx);
  // "state" must be a JSON string → we wrap and escape
  String escaped; escaped.reserve(value.length()+8);
  for (size_t i=0;i<value.length();++i) {
    char c = value[i];
    if (c=='\\' || c=='\"') { escaped += '\\'; escaped += c; }
    else if (c=='\n') { escaped += "\\n"; }
    else if (c=='\r') { escaped += "\\r"; }
    else { escaped += c; }
  }
  String stateJson = "\"" + escaped + "\"";
  return postStateToHA(entityId, stateJson, "{}");
}

// Convenience: clear buffers 1..5 (empty string) and set 6 to "END"
bool sendBuffersAndEnd(const String parts[5]) {
  bool ok = true;
  for (int i=0;i<5;i++) {
    ok &= postHelperBuffer(i+1, parts[i]);
  }
  ok &= postHelperBuffer(6, "END");
  return ok;
}

// --------------- Discovery Packaging ------------------

// Takes a single discovery message { "topic": "...", "payload": {...} }
// Serializes and splits across up to 5 chunks (≤255 chars each).
// Empty-fill unused chunks so buffers are always written.
bool publishDiscoveryViaHelpers(const String& topic, const String& payloadJson) {
  // Build discovery envelope expected by your HA automation
  String envelope = "{\"topic\":\"" + topic + "\",\"payload\":" + payloadJson + "}";
  // Split into ≤5 chunks
  const size_t MAX_CHUNK = 255;
  String parts[5] = {"","","","",""};
  size_t len = envelope.length();
  if (len > MAX_CHUNK * 5) {
    Serial.printf("Discovery envelope too large (%u bytes). Trim or simplify payload.\n", (unsigned)len);
    return false;
  }
  for (int i=0;i<5 && (i*MAX_CHUNK)<len; ++i) {
    size_t start = i * MAX_CHUNK;
    size_t count = min((size_t)MAX_CHUNK, len - start);
    parts[i] = envelope.substring(start, start + count);
  }
  return sendBuffersAndEnd(parts);
}

// Overloads for our ArduinoHA-like objects
bool publishDiscovery(const HASwitch& sw) {
  return publishDiscoveryViaHelpers(sw.discoveryTopic(), sw.discoveryPayloadJson());
}
bool publishDiscovery(const HANumber& num) {
  return publishDiscoveryViaHelpers(num.discoveryTopic(), num.discoveryPayloadJson());
}

// ---------------- Example / Test Setup ----------------

HADevice device;
HASwitch testSwitch;
HANumber testNumber;

unsigned long lastUpdateMs = 0;
bool switchState = false;

void connectWiFi() {
  Serial.printf("Connecting to WiFi: %s\n", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis()-start < 20000) {
    delay(250);
    Serial.print(".");
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("WiFi connected, IP: %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("WiFi connect failed.");
  }
}

void setupDeviceAndEntities() {
  // Device
  device.setUniqueId("esp32_test_device_001");
  device.setName("ESP Test Device");
  device.setManufacturer("YourCo");
  device.setModel("ESP32");
  device.setSoftwareVersion("1.0.0");

  // Switch entity
  testSwitch.device = &device;
  testSwitch.objectId = "esp_test_switch";
  testSwitch.setName("ESP Test Switch");
  testSwitch.setIcon("mdi:toggle-switch");
  testSwitch.setUniqueId("esp_test_switch_uid");
  testSwitch.setStateTopic("virt/esp_test/switch/state");
  testSwitch.setCommandTopic("virt/esp_test/switch/set");
  testSwitch.setAvailabilityTopic("virt/esp_test/avail");

  // Number entity
  testNumber.device = &device;
  testNumber.objectId = "esp_test_level";
  testNumber.setName("ESP Test Level");
  testNumber.setIcon("mdi:gauge");
  testNumber.setUniqueId("esp_test_level_uid");
  testNumber.setStateTopic("virt/esp_test/level/state");
  testNumber.setCommandTopic("virt/esp_test/level/set");
  testNumber.setAvailabilityTopic("virt/esp_test/avail");
  testNumber.setMin(0);
  testNumber.setMax(100);
  testNumber.setStep(1);
  testNumber.setUnitOfMeasurement("%");
  testNumber.setMode("slider");
}

void setup() {
  Serial.begin(115200);
  delay(200);

  connectWiFi();
  setupDeviceAndEntities();

  // Publish discovery for both entities (retained will be handled by HA automation)
  if (!publishDiscovery(testSwitch)) {
    Serial.println("Failed to publish discovery for switch.");
  } else {
    Serial.println("Discovery for switch sent.");
  }

  if (!publishDiscovery(testNumber)) {
    Serial.println("Failed to publish discovery for number.");
  } else {
    Serial.println("Discovery for number sent.");
  }

  randomSeed(esp_random());
}

void loop() {
  unsigned long now = millis();
  if (now - lastUpdateMs >= 5000) {
    lastUpdateMs = now;

    // Toggle the switch state (for demo)
    switchState = !switchState;
    // Update switch via REST state (for dashboards/testing)
    // IMPORTANT: This does NOT publish to the MQTT state_topic.
    // It only updates HA's state machine view of the entity.
    // For production, publish to MQTT (from HA) or extend your helper flow.
    {
      String entityId = "switch." + testSwitch.objectId;
      String stateJson = switchState ? "\"on\"" : "\"off\"";
      String attrs = "{\"friendly_name\":\"" + testSwitch.name + "\"}";
      postStateToHA(entityId, stateJson, attrs);
    }

    // Post a random number (0..100)
    {
      int val = random(0, 101);
      String entityId = "number." + testNumber.objectId;
      String stateJson = String(val);
      String attrs = "{\"friendly_name\":\"" + testNumber.name + "\",\"unit_of_measurement\":\"%\"}";
      postStateToHA(entityId, stateJson, attrs);
    }
  }
}
