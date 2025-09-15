#ifndef HAMQTTDISCOVERY_H
#define HAMQTTDISCOVERY_H

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

enum ControlType {
  CONTROL_SWITCH,
  CONTROL_NUMBER,
  CONTROL_SENSOR,
  CONTROL_BINARY_SENSOR
};

struct HADevice {
  String uniqueId;
  String name;
  String manufacturer;
  String model;
  String swVersion;

  String toJson() const;
  static String escape(const String& input);
};

struct HAControl {
  ControlType type;
  String objectId;
  String name;
  String uniqueId;
  String icon;
  String stateTopic;
  String commandTopic;
  String availabilityTopic;
  HADevice* device;

  // Number-specific properties
  float minValue;
  float maxValue;
  float step;
  String unit;
  String mode;

  // Switch/Binary Sensor specific
  String payloadOn;
  String payloadOff;

  // Current state tracking
  String currentState;
  bool isOnline;

  HAControl();
  String getDiscoveryTopic() const;
  String getDiscoveryPayload() const;
  String getEntityId() const;
};

class HAMQTTDiscovery {
public:
  HAMQTTDiscovery();
  ~HAMQTTDiscovery();

  bool begin(const String& serverUrl, const String& token);

  HAControl* createSwitch(const String& objectId, const String& name, const String& uniqueId,
                         const String& icon = "", const String& stateTopic = "",
                         const String& commandTopic = "", const String& availabilityTopic = "",
                         const String& payloadOn = "ON", const String& payloadOff = "OFF",
                         HADevice* device = nullptr);
  HAControl* createNumber(const String& objectId, const String& name, const String& uniqueId,
                         float minVal = 0, float maxVal = 100, float step = 1,
                         const String& unit = "", const String& mode = "slider",
                         const String& icon = "", const String& stateTopic = "",
                         const String& commandTopic = "", const String& availabilityTopic = "",
                         HADevice* device = nullptr);
  HAControl* createSensor(const String& objectId, const String& name, const String& uniqueId,
                         const String& unit = "", const String& icon = "",
                         const String& stateTopic = "", const String& availabilityTopic = "",
                         HADevice* device = nullptr);
  HAControl* createBinarySensor(const String& objectId, const String& name, const String& uniqueId,
                               const String& icon = "", const String& stateTopic = "",
                               const String& availabilityTopic = "",
                               const String& payloadOn = "ON", const String& payloadOff = "OFF",
                               HADevice* device = nullptr);

  bool writeControl(HAControl* control, const String& value);
  String readControl(HAControl* control);

  bool isControlOnline(HAControl* control);

  void setDevice(const String& uniqueId, const String& name,
                 const String& manufacturer = "", const String& model = "",
                 const String& swVersion = "");

private:
  String _serverUrl;
  String _token;
  HADevice _defaultDevice;

  static const int MAX_CONTROLS = 50;
  HAControl* _controls[MAX_CONTROLS];
  int _controlCount;

  String getAuthHeader() const;
  bool postToHA(const String& endpoint, const String& payload);
  bool getFromHA(const String& endpoint, String& response);
  bool postHelperBuffer(int bufferIndex, const String& content);
  bool publishDiscovery(HAControl* control);
  bool controlExists(const String& entityId);
  bool waitForControlCreation(const String& entityId, int timeoutSeconds = 10);
  String extractJsonValue(const String& json, const String& key);
};

#endif