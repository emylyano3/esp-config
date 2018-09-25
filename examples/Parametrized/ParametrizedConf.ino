#include "ESPConfig.h"

const uint8_t   LED_PIN = 2;

ESPConfigParam  _param1 (Text, "mqtt_host", "MQTT Host", "192.168.0.1", 12, "required");
ESPConfigParam  _param2 (Text, "mqtt_port", "MQTT Port", "1883", 6, "required");
char            _stationName[5];

void setup() {
    ESPConfig moduleConfig;
    moduleConfig.setConnectionTimeout(30000);
    moduleConfig.setPortalSSID("ConfigTesting");
    moduleConfig.setPortalPassword("mistery");
    moduleConfig.addParameter(&_param1);
    moduleConfig.addParameter(&_param2);
    moduleConfig.getParamsCount();
    moduleConfig.setStationNameCallback(stationName);
    moduleConfig.setAPCallback(apCallback);
    moduleConfig.setSaveConfigCallback(saveConfig);
    moduleConfig.connectWifiNetwork(false);
    moduleConfig.blockingFeedback(LED_PIN, 500, 5);
}

void loop() {

}

void apCallback (ESPConfig* espConfig) {
    espConfig->getParamsCount();
}

char* stationName() {
    return _stationName;
}

void saveConfig() {
}