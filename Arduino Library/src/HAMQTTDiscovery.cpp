#include "HAMQTTDiscovery.h"

String HADevice::escape(const String& input) {
  String output;
  output.reserve(input.length() + 8);
  for (size_t i = 0; i < input.length(); i++) {
    char c = input[i];
    if (c == '"' || c == '\\') {
      output += '\\';
      output += c;
    } else if (c == '\n') {
      output += "\\n";
    } else if (c == '\r') {
      output += "\\r";
    } else {
      output += c;
    }
  }
  return output;
}

String HADevice::toJson() const {
  String json = "{\"identifiers\":[\"" + escape(uniqueId) + "\"]";
  if (name.length()) json += ",\"name\":\"" + escape(name) + "\"";
  if (manufacturer.length()) json += ",\"manufacturer\":\"" + escape(manufacturer) + "\"";
  if (model.length()) json += ",\"model\":\"" + escape(model) + "\"";
  if (swVersion.length()) json += ",\"sw_version\":\"" + escape(swVersion) + "\"";
  json += "}";
  return json;
}

HAControl::HAControl() {
  type = CONTROL_SWITCH;
  device = nullptr;
  minValue = 0;
  maxValue = 100;
  step = 1;
  mode = "slider";
  payloadOn = "ON";
  payloadOff = "OFF";
  isOnline = false;
}

String HAControl::getDiscoveryTopic() const {
  String component;
  switch (type) {
    case CONTROL_SWITCH: component = "switch"; break;
    case CONTROL_NUMBER: component = "number"; break;
    case CONTROL_SENSOR: component = "sensor"; break;
    case CONTROL_BINARY_SENSOR: component = "binary_sensor"; break;
  }
  return "homeassistant/" + component + "/" + objectId + "/config";
}

String HAControl::getEntityId() const {
  String component;
  switch (type) {
    case CONTROL_SWITCH: component = "switch"; break;
    case CONTROL_NUMBER: component = "number"; break;
    case CONTROL_SENSOR: component = "sensor"; break;
    case CONTROL_BINARY_SENSOR: component = "binary_sensor"; break;
  }
  return component + "." + objectId;
}

String HAControl::getDiscoveryPayload() const {
  String json = "{";

  if (name.length()) json += "\"name\":\"" + HADevice::escape(name) + "\",";
  if (uniqueId.length()) json += "\"unique_id\":\"" + HADevice::escape(uniqueId) + "\",";
  if (icon.length()) json += "\"icon\":\"" + HADevice::escape(icon) + "\",";
  if (stateTopic.length()) json += "\"state_topic\":\"" + HADevice::escape(stateTopic) + "\",";
  if (commandTopic.length()) json += "\"command_topic\":\"" + HADevice::escape(commandTopic) + "\",";
  if (availabilityTopic.length()) json += "\"availability_topic\":\"" + HADevice::escape(availabilityTopic) + "\",";

  if (device) {
    json += "\"device\":" + device->toJson() + ",";
  }

  switch (type) {
    case CONTROL_NUMBER:
      json += "\"min\":" + String(minValue, 3) + ",";
      json += "\"max\":" + String(maxValue, 3) + ",";
      json += "\"step\":" + String(step, 3) + ",";
      if (unit.length()) json += "\"unit_of_measurement\":\"" + HADevice::escape(unit) + "\",";
      if (mode.length()) json += "\"mode\":\"" + HADevice::escape(mode) + "\",";
      break;
    case CONTROL_SWITCH:
    case CONTROL_BINARY_SENSOR:
      json += "\"payload_on\":\"" + HADevice::escape(payloadOn) + "\",";
      json += "\"payload_off\":\"" + HADevice::escape(payloadOff) + "\",";
      break;
    case CONTROL_SENSOR:
      if (unit.length()) json += "\"unit_of_measurement\":\"" + HADevice::escape(unit) + "\",";
      break;
  }

  if (json.endsWith(",")) {
    json.remove(json.length() - 1);
  }
  json += "}";

  return json;
}

HAMQTTDiscovery::HAMQTTDiscovery() {
  _controlCount = 0;
  for (int i = 0; i < MAX_CONTROLS; i++) {
    _controls[i] = nullptr;
  }
}

HAMQTTDiscovery::~HAMQTTDiscovery() {
  for (int i = 0; i < _controlCount; i++) {
    if (_controls[i]) {
      delete _controls[i];
    }
  }
}

bool HAMQTTDiscovery::begin(const String& serverUrl, const String& token) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("HAMQTTDiscovery: WiFi not connected");
    return false;
  }

  _serverUrl = serverUrl;
  if (_serverUrl.endsWith("/")) {
    _serverUrl.remove(_serverUrl.length() - 1);
  }

  _token = token;

  Serial.println("HAMQTTDiscovery: Initialized successfully");
  return true;
}

void HAMQTTDiscovery::setDevice(const String& uniqueId, const String& name,
                               const String& manufacturer, const String& model,
                               const String& swVersion) {
  _defaultDevice.uniqueId = uniqueId;
  _defaultDevice.name = name;
  _defaultDevice.manufacturer = manufacturer;
  _defaultDevice.model = model;
  _defaultDevice.swVersion = swVersion;
}

String HAMQTTDiscovery::getAuthHeader() const {
  return "Bearer " + _token;
}

bool HAMQTTDiscovery::postToHA(const String& endpoint, const String& payload) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("HAMQTTDiscovery: WiFi not connected");
    return false;
  }

  WiFiClientSecure* client = new WiFiClientSecure();
  client->setInsecure();

  HTTPClient https;
  String url = _serverUrl + endpoint;
  https.begin(*client, url);
  https.addHeader("Authorization", getAuthHeader());
  https.addHeader("Content-Type", "application/json");

  int httpCode = https.POST(payload);
  bool success = (httpCode >= 200 && httpCode < 300);

  if (!success) {
    Serial.printf("HAMQTTDiscovery: POST %s failed with code %d\n", url.c_str(), httpCode);
  }

  https.end();
  delete client;
  return success;
}

bool HAMQTTDiscovery::getFromHA(const String& endpoint, String& response) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("HAMQTTDiscovery: WiFi not connected");
    return false;
  }

  WiFiClientSecure* client = new WiFiClientSecure();
  client->setInsecure();

  HTTPClient https;
  String url = _serverUrl + endpoint;
  https.begin(*client, url);
  https.addHeader("Authorization", getAuthHeader());

  int httpCode = https.GET();
  bool success = (httpCode >= 200 && httpCode < 300);

  if (success) {
    response = https.getString();
  } else {
    Serial.printf("HAMQTTDiscovery: GET %s failed with code %d\n", url.c_str(), httpCode);
  }

  https.end();
  delete client;
  return success;
}

bool HAMQTTDiscovery::postHelperBuffer(int bufferIndex, const String& content) {
  if (bufferIndex < 1 || bufferIndex > 6) return false;

  String entityId = "input_text.mqtt_buffer_" + String(bufferIndex);
  String escaped = HADevice::escape(content);
  String payload = "{\"state\":\"" + escaped + "\"}";

  return postToHA("/api/states/" + entityId, payload);
}

bool HAMQTTDiscovery::publishDiscovery(HAControl* control) {
  String topic = control->getDiscoveryTopic();
  String payload = control->getDiscoveryPayload();

  String envelope = "{\"topic\":\"" + topic + "\",\"payload\":" + payload + "}";

  const size_t MAX_CHUNK = 255;
  size_t len = envelope.length();

  if (len > MAX_CHUNK * 5) {
    Serial.println("HAMQTTDiscovery: Discovery payload too large");
    return false;
  }

  bool success = true;
  for (int i = 1; i <= 5; i++) {
    String chunk = "";
    if (len > 0) {
      size_t start = (i - 1) * MAX_CHUNK;
      size_t count = min((size_t)MAX_CHUNK, len - start);
      if (start < len) {
        chunk = envelope.substring(start, start + count);
      }
    }
    success &= postHelperBuffer(i, chunk);
  }

  success &= postHelperBuffer(6, "END");
  return success;
}

bool HAMQTTDiscovery::controlExists(const String& entityId) {
  String response;
  bool exists = getFromHA("/api/states/" + entityId, response);
  return exists && response.length() > 0 && !response.startsWith("null");
}

bool HAMQTTDiscovery::waitForControlCreation(const String& entityId, int timeoutSeconds) {
  unsigned long startTime = millis();
  unsigned long timeout = timeoutSeconds * 1000;

  while (millis() - startTime < timeout) {
    if (controlExists(entityId)) {
      return true;
    }
    delay(500);
  }
  return false;
}

HAControl* HAMQTTDiscovery::createSwitch(const String& objectId, const String& name, const String& uniqueId,
                                        const String& icon, const String& stateTopic,
                                        const String& commandTopic, const String& availabilityTopic,
                                        const String& payloadOn, const String& payloadOff,
                                        HADevice* device) {
  if (_controlCount >= MAX_CONTROLS) {
    Serial.println("HAMQTTDiscovery: Maximum number of controls reached");
    return nullptr;
  }

  HAControl* control = new HAControl();
  control->type = CONTROL_SWITCH;
  control->objectId = objectId;
  control->name = name;
  control->uniqueId = uniqueId;
  control->icon = icon.length() ? icon : "mdi:toggle-switch";
  control->stateTopic = stateTopic.length() ? stateTopic : ("virt/" + objectId + "/state");
  control->commandTopic = commandTopic.length() ? commandTopic : ("virt/" + objectId + "/set");
  control->availabilityTopic = availabilityTopic.length() ? availabilityTopic : ("virt/" + objectId + "/avail");
  control->payloadOn = payloadOn.length() ? payloadOn : "ON";
  control->payloadOff = payloadOff.length() ? payloadOff : "OFF";
  control->device = device ? device : &_defaultDevice;

  String entityId = control->getEntityId();

  if (controlExists(entityId)) {
    Serial.printf("HAMQTTDiscovery: Control %s already exists\n", entityId.c_str());
    delete control;
    return nullptr;
  }

  if (!publishDiscovery(control)) {
    Serial.printf("HAMQTTDiscovery: Failed to publish discovery for %s\n", entityId.c_str());
    delete control;
    return nullptr;
  }

  Serial.printf("HAMQTTDiscovery: Waiting for control %s to be created...\n", entityId.c_str());
  delay(3000);

  if (!waitForControlCreation(entityId, 10)) {
    Serial.printf("HAMQTTDiscovery: Control %s was not created within timeout\n", entityId.c_str());
    delete control;
    return nullptr;
  }

  control->isOnline = true;
  _controls[_controlCount++] = control;
  Serial.printf("HAMQTTDiscovery: Control %s created successfully\n", entityId.c_str());
  return control;
}

HAControl* HAMQTTDiscovery::createNumber(const String& objectId, const String& name, const String& uniqueId,
                                        float minVal, float maxVal, float step,
                                        const String& unit, const String& mode,
                                        const String& icon, const String& stateTopic,
                                        const String& commandTopic, const String& availabilityTopic,
                                        HADevice* device) {
  if (_controlCount >= MAX_CONTROLS) {
    Serial.println("HAMQTTDiscovery: Maximum number of controls reached");
    return nullptr;
  }

  HAControl* control = new HAControl();
  control->type = CONTROL_NUMBER;
  control->objectId = objectId;
  control->name = name;
  control->uniqueId = uniqueId;
  control->minValue = minVal;
  control->maxValue = maxVal;
  control->step = step;
  control->unit = unit;
  control->mode = mode.length() ? mode : "slider";
  control->icon = icon.length() ? icon : "mdi:gauge";
  control->stateTopic = stateTopic.length() ? stateTopic : ("virt/" + objectId + "/state");
  control->commandTopic = commandTopic.length() ? commandTopic : ("virt/" + objectId + "/set");
  control->availabilityTopic = availabilityTopic.length() ? availabilityTopic : ("virt/" + objectId + "/avail");
  control->device = device ? device : &_defaultDevice;

  String entityId = control->getEntityId();

  if (controlExists(entityId)) {
    Serial.printf("HAMQTTDiscovery: Control %s already exists\n", entityId.c_str());
    delete control;
    return nullptr;
  }

  if (!publishDiscovery(control)) {
    Serial.printf("HAMQTTDiscovery: Failed to publish discovery for %s\n", entityId.c_str());
    delete control;
    return nullptr;
  }

  Serial.printf("HAMQTTDiscovery: Waiting for control %s to be created...\n", entityId.c_str());
  delay(3000);

  if (!waitForControlCreation(entityId, 10)) {
    Serial.printf("HAMQTTDiscovery: Control %s was not created within timeout\n", entityId.c_str());
    delete control;
    return nullptr;
  }

  control->isOnline = true;
  _controls[_controlCount++] = control;
  Serial.printf("HAMQTTDiscovery: Control %s created successfully\n", entityId.c_str());
  return control;
}

HAControl* HAMQTTDiscovery::createSensor(const String& objectId, const String& name, const String& uniqueId,
                                        const String& unit, const String& icon,
                                        const String& stateTopic, const String& availabilityTopic,
                                        HADevice* device) {
  if (_controlCount >= MAX_CONTROLS) {
    Serial.println("HAMQTTDiscovery: Maximum number of controls reached");
    return nullptr;
  }

  HAControl* control = new HAControl();
  control->type = CONTROL_SENSOR;
  control->objectId = objectId;
  control->name = name;
  control->uniqueId = uniqueId;
  control->unit = unit;
  control->icon = icon.length() ? icon : "mdi:gauge";
  control->stateTopic = stateTopic.length() ? stateTopic : ("virt/" + objectId + "/state");
  control->availabilityTopic = availabilityTopic.length() ? availabilityTopic : ("virt/" + objectId + "/avail");
  control->device = device ? device : &_defaultDevice;

  String entityId = control->getEntityId();

  if (controlExists(entityId)) {
    Serial.printf("HAMQTTDiscovery: Control %s already exists\n", entityId.c_str());
    delete control;
    return nullptr;
  }

  if (!publishDiscovery(control)) {
    Serial.printf("HAMQTTDiscovery: Failed to publish discovery for %s\n", entityId.c_str());
    delete control;
    return nullptr;
  }

  Serial.printf("HAMQTTDiscovery: Waiting for control %s to be created...\n", entityId.c_str());
  delay(3000);

  if (!waitForControlCreation(entityId, 10)) {
    Serial.printf("HAMQTTDiscovery: Control %s was not created within timeout\n", entityId.c_str());
    delete control;
    return nullptr;
  }

  control->isOnline = true;
  _controls[_controlCount++] = control;
  Serial.printf("HAMQTTDiscovery: Control %s created successfully\n", entityId.c_str());
  return control;
}

HAControl* HAMQTTDiscovery::createBinarySensor(const String& objectId, const String& name, const String& uniqueId,
                                              const String& icon, const String& stateTopic,
                                              const String& availabilityTopic,
                                              const String& payloadOn, const String& payloadOff,
                                              HADevice* device) {
  if (_controlCount >= MAX_CONTROLS) {
    Serial.println("HAMQTTDiscovery: Maximum number of controls reached");
    return nullptr;
  }

  HAControl* control = new HAControl();
  control->type = CONTROL_BINARY_SENSOR;
  control->objectId = objectId;
  control->name = name;
  control->uniqueId = uniqueId;
  control->icon = icon.length() ? icon : "mdi:motion-sensor";
  control->stateTopic = stateTopic.length() ? stateTopic : ("virt/" + objectId + "/state");
  control->availabilityTopic = availabilityTopic.length() ? availabilityTopic : ("virt/" + objectId + "/avail");
  control->payloadOn = payloadOn.length() ? payloadOn : "ON";
  control->payloadOff = payloadOff.length() ? payloadOff : "OFF";
  control->device = device ? device : &_defaultDevice;

  String entityId = control->getEntityId();

  if (controlExists(entityId)) {
    Serial.printf("HAMQTTDiscovery: Control %s already exists\n", entityId.c_str());
    delete control;
    return nullptr;
  }

  if (!publishDiscovery(control)) {
    Serial.printf("HAMQTTDiscovery: Failed to publish discovery for %s\n", entityId.c_str());
    delete control;
    return nullptr;
  }

  Serial.printf("HAMQTTDiscovery: Waiting for control %s to be created...\n", entityId.c_str());
  delay(3000);

  if (!waitForControlCreation(entityId, 10)) {
    Serial.printf("HAMQTTDiscovery: Control %s was not created within timeout\n", entityId.c_str());
    delete control;
    return nullptr;
  }

  control->isOnline = true;
  _controls[_controlCount++] = control;
  Serial.printf("HAMQTTDiscovery: Control %s created successfully\n", entityId.c_str());
  return control;
}

bool HAMQTTDiscovery::writeControl(HAControl* control, const String& value) {
  if (!control || !control->isOnline) {
    return false;
  }

  String entityId = control->getEntityId();
  String payload = "{\"state\":\"" + HADevice::escape(value) + "\"}";

  bool success = postToHA("/api/states/" + entityId, payload);
  if (success) {
    control->currentState = value;
  }
  return success;
}

String HAMQTTDiscovery::readControl(HAControl* control) {
  if (!control || !control->isOnline) {
    return "";
  }

  String entityId = control->getEntityId();
  String response;

  if (getFromHA("/api/states/" + entityId, response)) {
    String state = extractJsonValue(response, "state");
    control->currentState = state;
    return state;
  }

  return "";
}

bool HAMQTTDiscovery::isControlOnline(HAControl* control) {
  if (!control) return false;

  String entityId = control->getEntityId();
  String response;
  bool online = getFromHA("/api/states/" + entityId, response);
  control->isOnline = online;
  return online;
}

String HAMQTTDiscovery::extractJsonValue(const String& json, const String& key) {
  String searchKey = "\"" + key + "\":";
  int startIndex = json.indexOf(searchKey);
  if (startIndex == -1) return "";

  startIndex += searchKey.length();
  while (startIndex < json.length() && (json[startIndex] == ' ' || json[startIndex] == '\t')) {
    startIndex++;
  }

  if (startIndex >= json.length()) return "";

  if (json[startIndex] == '"') {
    startIndex++;
    int endIndex = json.indexOf('"', startIndex);
    if (endIndex == -1) return "";
    return json.substring(startIndex, endIndex);
  } else {
    int endIndex = startIndex;
    while (endIndex < json.length() &&
           json[endIndex] != ',' &&
           json[endIndex] != '}' &&
           json[endIndex] != ']') {
      endIndex++;
    }
    return json.substring(startIndex, endIndex);
  }
}