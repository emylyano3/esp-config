#include "ESPConfig.h"

ESPConfig moduleConfig;

void setup() {
    moduleConfig.setConnectionTimeout(30000);
    moduleConfig.setPortalSSID("ConfigTesting");
    moduleConfig.setPortalPassword("mistery");
    moduleConfig.connectWifiNetwork(false);
}

void loop() {

}