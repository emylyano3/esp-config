#include "ESPConfig.h"

ESPConfigParam::ESPConfigParam (InputType type, const char* name, const char* label, const char* defVal, uint8_t length, const char* html) {
  _type = type;
  _name = name;
  _label = label;
  _customHTML = html;
  _length = length;
  _value = new char[length + 1];
  updateValue(defVal);
}

ESPConfigParam::~ESPConfigParam() {
  if (_value != NULL) {
    delete[] _value;
  }
}

InputType ESPConfigParam::getType() {
  return _type;
}

const char* ESPConfigParam::getName() {
  return _name;
}

const char* ESPConfigParam::getValue() {
  return _value;
}

const char* ESPConfigParam::getLabel() {
  return _label;
}

int ESPConfigParam::getValueLength() {
  return _length;
}

const char* ESPConfigParam::getCustomHTML() {
  return _customHTML;
}

std::vector<char*> ESPConfigParam::getOptions() {
  return _options;
}

void ESPConfigParam::updateValue (const char *v) {
  String s = String(v);
  s.toCharArray(_value, _length);
}

ESPConfig::ESPConfig() {
  _max_params = ESP_CONFIG_MAX_PARAMS;
  _configParams = (ESPConfigParam**)malloc(_max_params * sizeof(ESPConfigParam*));
  _apName = String(ESP.getChipId()).c_str();
}

ESPConfig::~ESPConfig() {
  if (_configParams != NULL) {
    #ifdef LOGGING
    debug(F("Freeing allocated params!"));
    #endif
    free(_configParams);
  }
}

bool ESPConfig::connectWifiNetwork (bool existsConfig) {
  #ifdef LOGGING
  debug(F("Connecting to wifi network"));
  debug(F("Previous config found"), existsConfig);
  #endif
  bool connected = false;
  while (!connected) {
    if (existsConfig) {
      #ifdef LOGGING
      debug(F("Connecting to saved network"));
      #endif
      if (connectWiFi() == WL_CONNECTED) {
        connected = true;
      } else {
        #ifdef LOGGING
        debug(F("Could not connect to saved network. Going into config mode."));
        #endif
        connected = startConfigPortal();
        if (configPortalHasTimeout()) {
          break;
        }
      }
    } else {
      #ifdef LOGGING
      debug(F("Going into config mode cause no config was found"));
      #endif
      WiFi.persistent(false);
      connected = startConfigPortal();
    }
  }
  if (!connected) {
    WiFi.mode(WIFI_OFF);
  }
  return connected;
}

bool ESPConfig::startConfigPortal() {
  WiFi.mode(WIFI_AP);
  _connect = false;
  setupConfigPortal();
  while(1) {
    if (configPortalHasTimeout()) {
      break;
    }
    _dnsServer->processNextRequest();
    _server->handleClient();
    if (_connect) {
      _connect = false;
      delay(1000);
      #ifdef LOGGING
      debug(F("Connecting to new AP"));
      #endif
      // using user-provided  _ssid, _pass in place of system-stored ssid and pass
      //end the led feedback
      if (_feedbackPin != INVALID_PIN_NO) {
        //stop feedback
        digitalWrite(_feedbackPin, LOW);
      }
      if (connectWifi(_server->arg("s").c_str(), _server->arg("p").c_str()) != WL_CONNECTED) {
        #ifdef LOGGING
        debug(F("Failed to connect."));
        #endif
        break;
      } else {
        WiFi.mode(WIFI_STA);
        //notify that configuration has changed and any optional parameters should be saved
        if ( _savecallback != NULL) {
          //todo: check if any custom parameters actually exist, and check if they really changed maybe
          _savecallback();
        }
        break;
      }
    }
    if (_feedbackPin != INVALID_PIN_NO) {
      nonBlockingFeedback(_feedbackPin, 1000);
    }
    yield();
  }
  _server.reset();
  _dnsServer.reset();
  return  WiFi.status() == WL_CONNECTED;
}

bool ESPConfig::configPortalHasTimeout() {
    if(_configPortalTimeout == 0 || WiFi.softAPgetStationNum() > 0){
      _configPortalStart = millis(); // kludge, bump configportal start time to skew timeouts
      return false;
    }
    bool to = (millis() > _configPortalStart + _configPortalTimeout);
    #ifdef LOGGING
    if (to) {
      debug(F("Config portal has timed out"));
    }
    #endif
    return to;
}

void ESPConfig::setConfigPortalTimeout(unsigned long seconds) {
  _configPortalTimeout = seconds * 1000;
}

void ESPConfig::setWifiConnectTimeout(unsigned long seconds) {
  _wifiConnectTimeout = seconds * 1000;
}

void ESPConfig::setPortalSSID(const char *apName) {
  _apName = apName;
}

void ESPConfig::setPortalPassword(const char *apPass) {
  _apPass = apPass;
}

void ESPConfig::setMinimumSignalQuality(int quality) {
  _minimumQuality = quality;
}

void ESPConfig::setAPStaticIP(IPAddress ip, IPAddress gw, IPAddress sn) {
  _ap_static_ip = ip;
  _ap_static_gw = gw;
  _ap_static_sn = sn;
}

void ESPConfig::setFeedbackPin(uint8_t pin) {
  _feedbackPin = pin;
}

void ESPConfig::setAPCallback (std::function<void(ESPConfig* espConfig)> callback) {
  _apcallback = callback;
}

void ESPConfig::setSaveConfigCallback (std::function<void(void)> callback) {
  _savecallback = callback;
}

void ESPConfig::setStationNameCallback(std::function<const char*(void)> callback) {
  _stationNameCallback = callback;
}

ESPConfigParam* ESPConfig::getParameter(uint8_t index) {
  if (index >= _paramsCount) {
    return NULL;
  } else {
    return _configParams[index];
  }
}

uint8_t ESPConfig::getParamsCount() {
  return _paramsCount;
}

bool ESPConfig::addParameter(ESPConfigParam *p) {
  if (_paramsCount + 1 > _max_params) {
    // rezise the params array
    _max_params += ESP_CONFIG_MAX_PARAMS;
    #ifdef LOGGING
    debug(F("Increasing _max_params to:"), _max_params);
    #endif
    ESPConfigParam** newParams = (ESPConfigParam**)realloc(_configParams, _max_params * sizeof(ESPConfigParam*));
    if (newParams != NULL) {
      _configParams = newParams;
    } else {
      #ifdef LOGGING
      debug(F("ERROR: failed to realloc params, size not increased!"));
      #endif
      return false;
    }
  }
  _configParams[_paramsCount] = p;
  _paramsCount++;
  #ifdef LOGGING
  debug(F("Adding parameter"), p->getName());
  #endif
  return true;
}

void ESPConfig::blockingFeedback (uint8_t pin, long stepTime, uint8_t times) {
  for (uint8_t i = 0; i < times; ++i) {
    digitalWrite(pin, HIGH);
    delay(stepTime);
    digitalWrite(pin, LOW);
    delay(stepTime);
  }
}

void ESPConfig::nonBlockingFeedback(uint8_t pin, int stepTime) {
  if (millis() > _sigfbkStepControl + stepTime) {
    _sigfbkIsOn = !_sigfbkIsOn;
    _sigfbkStepControl = millis();
    digitalWrite(pin, _sigfbkIsOn ? HIGH : LOW);
  }
}

uint8_t ESPConfig::connectWifi(String ssid, String pass) {
  #ifdef LOGGING
  debug(F("Connecting as wifi client..."));
  #endif
  if (WiFi.isConnected()) {;
    #ifdef LOGGING
    debug(F("Already connected. Bailing out."));
    #endif
    return WL_CONNECTED;
  }
  if (_stationNameCallback) {
    WiFi.hostname(_stationNameCallback());
  }
  WiFi.persistent(true);
  WiFi.begin(ssid.c_str(), pass.c_str());
  WiFi.persistent(false);
  return waitForConnectResult();
}

uint8_t ESPConfig::connectWiFi() {
  WiFi.mode(WIFI_STA);
  if (_stationNameCallback) {
    WiFi.hostname(_stationNameCallback());
  }
  if (WiFi.SSID()) {
    #ifdef LOGGING
    debug(F("Using last saved values, should be faster"));
    #endif
    //trying to fix connection in progress hanging
    ETS_UART_INTR_DISABLE();
    wifi_station_disconnect();
    ETS_UART_INTR_ENABLE();
    WiFi.begin();
    return waitForConnectResult();
  } else {
    #ifdef LOGGING
    debug(F("No saved credentials"));
    #endif
    return WL_CONNECT_FAILED;
  }
}

uint8_t ESPConfig::waitForConnectResult() {
  if (_wifiConnectTimeout == 0) {
    return WiFi.waitForConnectResult();
  } else {
    unsigned long start = millis();
    bool keepConnecting = true;
    uint8_t status;
    #ifdef LOGGING
    debug(F("Waiting for connection result with time out"));
    uint8_t retry = 0;
    #endif
    while (keepConnecting) {
      if (millis() > start + _wifiConnectTimeout) {
        keepConnecting = false;
        #ifdef LOGGING
        debug(F("Connection timed out"));
        #endif
      }
      status = WiFi.status();
      if (status == WL_CONNECTED) {
        keepConnecting = false;
      } else if (status == WL_NO_SSID_AVAIL) { // in case configured SSID cannot be reached
        #ifdef LOGGING
        debug(F("Connection failed. SSID provided not available"), WiFi.SSID());
        debug(F("Retrying"), ++retry);
        #endif
        WiFi.begin();
      } else if (status == WL_IDLE_STATUS) { // when Wi-Fi is in process of changing between statuses
        #ifdef LOGGING
        debug(F("Status IDLE. Waiting to final state"));
        #endif
        delay(500);
      } else if (status == WL_CONNECT_FAILED) { // if password is incorrect
        #ifdef LOGGING
        debug(F("Credentials provided wrong. Stop trying to connect"));
        #endif
        keepConnecting = false;
      }
      delay(100);
    }
    return status;
  }
}

void ESPConfig::setupConfigPortal() {
  _server.reset(new ESP8266WebServer(80));
  _dnsServer.reset(new DNSServer());
  #ifdef LOGGING
  debug(F("Configuring access point... "), _apName);
  #endif
  if (_apPass != NULL) {
    if (strlen(_apPass) < 8 || strlen(_apPass) > 63) {
      #ifdef LOGGING
      debug(F("Invalid AccessPoint password. Ignoring"));
      #endif
      _apPass = NULL;
    }
    #ifdef LOGGING
    debug(_apPass);
    #endif
  }
  if (_ap_static_ip) {
    #ifdef LOGGING
    debug(F("Custom AP IP/GW/Subnet"));
    #endif
    WiFi.softAPConfig(_ap_static_ip, _ap_static_gw, _ap_static_sn);
  }
  if (_apPass != NULL) {
    WiFi.softAP(_apName, _apPass);
  } else {
    WiFi.softAP(_apName);
  }
  // Without delay I've seen the IP address blank
  delay(500); 
  #ifdef LOGGING
  debug(F("AP IP address"), WiFi.softAPIP());
  #endif
  /* Setup the DNS server redirecting all the domains to the apIP */
  _dnsServer->setErrorReplyCode(DNSReplyCode::NoError);
  _dnsServer->start(DNS_PORT, "*", WiFi.softAPIP());
  /* Setup web pages */
  _server->on("/", std::bind(&ESPConfig::handleWifi, this, false));
  _server->on("/config", std::bind(&ESPConfig::handleWifi, this, false));
  _server->on("/scan", std::bind(&ESPConfig::handleWifi, this, true)); 
  _server->on("/wifisave", std::bind(&ESPConfig::handleWifiSave, this));
  _server->onNotFound(std::bind(&ESPConfig::handleNotFound, this));
  _configPortalStart = millis();
  _server->begin();
  #ifdef LOGGING
  debug(F("HTTP server started"));
  #endif
}

void ESPConfig::handleWifi(bool scan) {
  // If captive portal redirect instead of displaying the page.
  if (captivePortal()) { 
    return;
  }
  String page = FPSTR(HTTP_HEADER);
  page.replace("{v}", "Proeza Domotics");
  page += FPSTR(HTTP_SCRIPT);
  page += FPSTR(HTTP_STYLE);
  page += "<h2>Module config</h2>";
  page += FPSTR(HTTP_HEADER_END);
  if (scan) {
    int n = WiFi.scanNetworks();
    #ifdef LOGGING
    debug(F("Scan done"));
    #endif
    if (n == 0) {
      #ifdef LOGGING
      debug(F("No networks found"));
      #endif
      page += F("No networks found. Refresh to scan again.");
    } else {
      //sort networks
      int indices[n];
      for (int i = 0; i < n; i++) {
        indices[i] = i;
      }
      // old sort
      for (int i = 0; i < n; i++) {
        for (int j = i + 1; j < n; j++) {
          if (WiFi.RSSI(indices[j]) > WiFi.RSSI(indices[i])) {
            std::swap(indices[i], indices[j]);
          }
        }
      }
      // remove duplicates ( must be RSSI sorted )
      String cssid;
      for (int i = 0; i < n; i++) {
        if (indices[i] == -1) continue;
        cssid = WiFi.SSID(indices[i]);
        for (int j = i + 1; j < n; j++) {
          if (cssid == WiFi.SSID(indices[j])) {
            #ifdef LOGGING
            debug(F("DUP AP"), WiFi.SSID(indices[j]));
            #endif
            indices[j] = -1; // set dup aps to index -1
          }
        }
      }
      //display networks in page
      for (int i = 0; i < n; i++) {
        if (indices[i] == -1) continue; // skip dups
        #ifdef LOGGING
        debug(WiFi.SSID(indices[i]));
        debug(WiFi.RSSI(indices[i]));
        #endif
        int quality = getRSSIasQuality(WiFi.RSSI(indices[i]));
        if (_minimumQuality == -1 || _minimumQuality < quality) {
          String item = FPSTR(HTTP_ITEM);
          String rssiQ;
          rssiQ += quality;
          item.replace("{v}", WiFi.SSID(indices[i]));
          item.replace("{r}", rssiQ);
          if (WiFi.encryptionType(indices[i]) != ENC_TYPE_NONE) {
            item.replace("{i}", "l");
          } else {
            item.replace("{i}", "");
          }
          page += item;
        } else {
          #ifdef LOGGING
          debug(F("Skipping due to quality"));
          #endif
        }
      }
      page += "<br/>";
    }
  }
  page += FPSTR(HTTP_FORM_START);
  char parLength[5];
  // add the extra parameters to the form
  for (int i = 0; i < _paramsCount; i++) {
    if (_configParams[i]->getName() != NULL) {
      if (_configParams[i]->getType() == Combo) {
        String pitem = FPSTR(HTTP_FORM_INPUT_LIST);
        pitem.replace("{i}", _configParams[i]->getName());
        pitem.replace("{n}", _configParams[i]->getName());
        String ops = "";
        for (size_t j = 0; j < _configParams[i]->getOptions().size(); ++j) {
          String op = FPSTR(HTTP_FORM_INPUT_LIST_OPTION);
          op.replace("{o}", _configParams[i]->getOptions()[j]);
          ops.concat(op);
        }
        pitem.replace("{p}", _configParams[i]->getLabel());
        pitem.replace("{o}", ops);
        pitem.replace("{c}", _configParams[i]->getCustomHTML());
        page += pitem;
      } else {
        String pitem = FPSTR(HTTP_FORM_INPUT);
        pitem.replace("{i}", _configParams[i]->getName());
        pitem.replace("{n}", _configParams[i]->getName());
        pitem.replace("{p}", _configParams[i]->getLabel());
        snprintf(parLength, 5, "%d", _configParams[i]->getValueLength());
        pitem.replace("{l}", parLength);
        pitem.replace("{v}", _configParams[i]->getValue());
        pitem.replace("{c}", _configParams[i]->getCustomHTML());
        page += pitem;
      }
    } 
  }
  page += FPSTR(HTTP_FORM_END);
  page += FPSTR(HTTP_SCAN_LINK);
  page += FPSTR(HTTP_END);
  _server->sendHeader("Content-Length", String(page.length()));
  _server->send(200, "text/html", page);
  #ifdef LOGGING
  debug(F("Sent config page"));
  #endif
}

void ESPConfig::handleNotFound() {
  // If captive portal redirect instead of displaying the error page.
  if (captivePortal()) { 
    return;
  }
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += _server->uri();
  message += "\nMethod: ";
  message += _server->method() == HTTP_GET ? "GET" : "POST";
  message += "\nArguments: ";
  message += _server->args();
  message += "\n";
  for (int i = 0; i < _server->args(); i++) {
    message += " " + _server->argName(i) + ": " + _server->arg(i) + "\n";
  }
  _server->sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  _server->sendHeader("Pragma", "no-cache");
  _server->sendHeader("Expires", "-1");
  _server->sendHeader("Content-Length", String(message.length()));
  _server->send(404, "text/plain", message);
}

/** Handle the WLAN save form and redirect to WLAN config page again */
void ESPConfig::handleWifiSave() {
  for (int i = 0; i < _paramsCount; i++) {
    _configParams[i]->updateValue(_server->arg(_configParams[i]->getName()).c_str());
    #ifdef LOGGING
    debug(_configParams[i]->getName(), _configParams[i]->getValue());
    #endif
  }
  String page = FPSTR(HTTP_HEADER);
  page.replace("{v}", "Credentials Saved");
  page += FPSTR(HTTP_SCRIPT);
  page += FPSTR(HTTP_STYLE);
  page += "<h2>Module config</h2>";
  page += FPSTR(HTTP_HEADER_END);
  page += FPSTR(HTTP_SAVED);
  page += FPSTR(HTTP_END);
  _server->sendHeader("Content-Length", String(page.length()));
  _server->send(200, "text/html", page);
  _connect = true; //signal ready to connect/reset
}

/** Redirect to captive portal if we got a request for another domain. Return true in that case so the page handler do not try to handle the request again. */
bool ESPConfig::captivePortal() {
  if (!isIp(_server->hostHeader()) ) {
    #ifdef LOGGING
    debug(F("Request redirected to captive portal"));
    #endif
    _server->sendHeader("Location", String("http://") + toStringIp(_server->client().localIP()), true);
    _server->send(302, "text/plain", ""); // Empty content inhibits Content-length header so we have to close the socket ourselves.
    _server->client().stop(); // Stop is needed because we sent no content length
    return true;
  }
  return false;
}

bool ESPConfig::isIp(String str) {
  for (unsigned int i = 0; i < str.length(); i++) {
    char c = str.charAt(i);
    if (c != '.' && (c < '0' || c > '9')) {
      return false;
    }
  }
  return true;
}

/** IP to String? */
String ESPConfig::toStringIp(IPAddress ip) {
  String res = "";
  for (int i = 0; i < 3; i++) {
    res += String((ip >> (8 * i)) & 0xFF) + ".";
  }
  res += String(((ip >> 8 * 3)) & 0xFF);
  return res;
}

int ESPConfig::getRSSIasQuality(int RSSI) {
  int quality = 0;
  if (RSSI <= -100) {
    quality = 0;
  } else if (RSSI >= -50) {
    quality = 100;
  } else {
    quality = 2 * (RSSI + 100);
  }
  return quality;
}

#ifdef LOGGING
template <class T> void ESPConfig::debug (T text) {
  Serial.print("*CONF: ");
  Serial.println(text);
}

template <class T, class U> void ESPConfig::debug (T key, U value) {
  Serial.print("*CONF: ");
  Serial.print(key);
  Serial.print(": ");
  Serial.println(value);
}
#endif