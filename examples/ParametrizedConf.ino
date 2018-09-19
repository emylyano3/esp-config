#include "ESPConfig.h"

ESPConfig moduleConfig;

ESPConfigParam param1 (Text, "mqtt_host", "MQTT Host", "192.168.0.1", 12, "required");
ESPConfigParam param2 (Text, "mqtt_port", "MQTT Port", "1883", 6, "required");

const uint8_t LED_PIN = 2;

void setup() {
    moduleConfig.setTimeout(30000);
    moduleConfig.setPortalSSID("ConfigTesting");
    moduleConfig.setPortalPassword("mistery");
    moduleConfig.addParameter(param1);
    moduleConfig.addParameter(param2);
    moduleConfig.connectWifiNetwork(false);
    moduleConfig.blockingFeedback(LED_PIN, 500, 5);
}

void loop() {

}