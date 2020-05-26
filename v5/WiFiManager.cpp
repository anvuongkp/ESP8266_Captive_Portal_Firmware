/**************************************************************
   Forked from Tzapu https://github.com/tzapu/WiFiManager
   Modified by Ken Taylor https://github.com/kentaylor
   Built by An Vuong
   Licensed under MIT license
 **************************************************************/

#include "WiFiManager.h"
#include "Time.h"
WiFiManagerParameter::WiFiManagerParameter(const char *custom) {
  _id = NULL;
  _placeholder = NULL;
  _length = 0;
  _value = NULL;
  _labelPlacement = WFM_LABEL_BEFORE;
  _customHTML = custom;
}

WiFiManagerParameter::WiFiManagerParameter(const char *id, const char *placeholder, const char *defaultValue, int length) {
  init(id, placeholder, defaultValue, length, "", WFM_LABEL_BEFORE);
}

WiFiManagerParameter::WiFiManagerParameter(const char *id, const char *placeholder, const char *defaultValue, int length, const char *custom) {
  init(id, placeholder, defaultValue, length, custom, WFM_LABEL_BEFORE);
}

WiFiManagerParameter::WiFiManagerParameter(const char *id, const char *placeholder, const char *defaultValue, int length, const char *custom, int labelPlacement) {
  init(id, placeholder, defaultValue, length, custom, labelPlacement);
}

void WiFiManagerParameter::init(const char *id, const char *placeholder, const char *defaultValue, int length, const char *custom, int labelPlacement) {
  _id = id;
  _placeholder = placeholder;
  _length = length;
  _labelPlacement = labelPlacement;
  _value = new char[length + 1];
  for (int i = 0; i < length; i++) {
    _value[i] = 0;
  }
  if (defaultValue != NULL) {
    strncpy(_value, defaultValue, length);
  }

  _customHTML = custom;
}

const char* WiFiManagerParameter::getValue() {
  return _value;
}
const char* WiFiManagerParameter::getID() {
  return _id;
}
const char* WiFiManagerParameter::getPlaceholder() {
  return _placeholder;
}
int WiFiManagerParameter::getValueLength() {
  return _length;
}
int WiFiManagerParameter::getLabelPlacement() {
  return _labelPlacement;
}
const char* WiFiManagerParameter::getCustomHTML() {
  return _customHTML;
}

WiFiManager::WiFiManager() {
	//Do a network scan before setting up an access point so as not to close WiFiNetwork while scanning.
	numberOfNetworks = scanWifiNetworks(networkIndicesptr);
	DEBUG_WM(F("SPIFFS Innitialize"));
	SPIFFS_List();
}
WiFiManager::~WiFiManager() {
    free(networkIndices); //indices array no longer required so free memory
}


void WiFiManager::addParameter(WiFiManagerParameter *p) {
  _params[_paramsCount] = p;
  _paramsCount++;
  DEBUG_WM("Adding parameter");
  DEBUG_WM(p->getID());
}

void WiFiManager::setupConfigPortal() {
  stopConfigPortal = false; //Signal not to close config portal
  /*This library assumes autoconnect is set to 1. It usually is
  but just in case check the setting and turn on autoconnect if it is off.
  Some useful discussion at https://github.com/esp8266/Arduino/issues/1615*/
  if (WiFi.getAutoConnect()==0)WiFi.setAutoConnect(1);
  dnsServer.reset(new DNSServer());
  server.reset(new ESP8266WebServer(80));

  DEBUG_WM(F(""));
  _configPortalStart = millis();

  DEBUG_WM(F("Configuring access point... "));
  DEBUG_WM(_apName);
  if (_apPassword != NULL) {
    if (strlen(_apPassword) < 8 || strlen(_apPassword) > 63) {
      // fail passphrase to short or long!
      DEBUG_WM(F("Invalid AccessPoint password. Ignoring"));
      _apPassword = NULL;
    }
    DEBUG_WM(_apPassword);
  }
  
  //optional soft ip config
  if (_ap_static_ip) {
    DEBUG_WM(F("Custom AP IP/GW/Subnet"));
    WiFi.softAPConfig(_ap_static_ip, _ap_static_gw, _ap_static_sn);
  }

  if (_apPassword != NULL) {
    WiFi.softAP(_apName, _apPassword);//password option
  } else {
    WiFi.softAP(_apName);
  }

  delay(500); // Without delay I've seen the IP address blank
  DEBUG_WM(F("AP IP address: "));
  DEBUG_WM(WiFi.softAPIP());

  /* Setup the DNS server redirecting all the domains to the apIP */
  dnsServer->setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer->start(DNS_PORT, "*", WiFi.softAPIP());

  /* setup server handler */
  server->on("/", std::bind(&WiFiManager::handleRoot, this));
  server->on("/wifi_configuration", std::bind(&WiFiManager::handleWifi, this));
  server->on("/wifi_save", std::bind(&WiFiManager::handleWifiSave, this));
  server->on("/exit_portal", std::bind(&WiFiManager::handleServerClose, this));
  server->on("/extra_functions", std::bind(&WiFiManager::handleInfo, this));
  server->on("/factory_reset", std::bind(&WiFiManager::handleReset, this));
  server->on("/json_module_wifi_info", std::bind(&WiFiManager::handleState, this));
  server->on("/json_wifi_scan_result", std::bind(&WiFiManager::handleScan, this));
  server->on("/gpio_control",std::bind(&WiFiManager::handleGPIOControl,this));
  server->on("/gpio_toggle",std::bind(&WiFiManager::handleGPIOToggle,this));
  server->on("/gpio_status",std::bind(&WiFiManager::handleGPIOStatus,this));
  server->on("/firmware_update",std::bind(&WiFiManager::handleFirmwareUpdatePage,this));
  server->on("/update",HTTP_POST,std::bind(&WiFiManager::handleUpdateHeader,this),std::bind(&WiFiManager::handleUpdate,this));
  server->on("/time", std::bind(&WiFiManager::handleTime, this));
  server->on("/restart",std::bind(&WiFiManager::handleRestart,this));
  server->on("/ip_configuration", std::bind(&WiFiManager::handleIPConfigurationPage,this));
  server->on("/ip_status",std::bind(&WiFiManager::handleIPStatus,this));
  server->on("/ip_change",std::bind(&WiFiManager::handleIPChange,this));
  
  
  server->on("/editor_page",HTTP_GET,std::bind(&WiFiManager::handleEditorPage,this));
  server->on("/ace.js",HTTP_GET,std::bind(&WiFiManager::handleEditorPage,this));
  server->on("/mode-html.js",HTTP_GET,std::bind(&WiFiManager::handleEditorPage,this));
  server->on("/theme-eclipse.js",HTTP_GET,std::bind(&WiFiManager::handleEditorPage,this));
  server->on("/worker-html.js",HTTP_GET,std::bind(&WiFiManager::handleEditorPage,this));
  server->on("/edit",HTTP_PUT,std::bind(&WiFiManager::handleFileCreate,this));
  server->on("/list",HTTP_GET,std::bind(&WiFiManager::handleFileList,this));
  server->on("/file_read",HTTP_GET,std::bind(&WiFiManager::handleFileRead,this));
  server->on("/file_download",HTTP_GET,std::bind(&WiFiManager::handleFileDownload,this));
  server->on("/edit",HTTP_POST,std::bind(&WiFiManager::handleUploadHeader,this),std::bind(&WiFiManager::handleFileUpload,this));
  server->on("/edit",HTTP_DELETE,std::bind(&WiFiManager::handleFileDelete,this));
  
  
  server->onNotFound (std::bind(&WiFiManager::handleNotFound, this));
  server->serveStatic("/", SPIFFS, "/","max-age=86400"); //cache all files in SPIFFS for 24 hrs
  server->begin(); // Web server start
  DEBUG_WM(F("HTTP server started"));
}

boolean WiFiManager::autoConnect() {
  String ssid = "ESP" + String(ESP.getChipId());
  return autoConnect(ssid.c_str(), NULL);
}
/* This is not very useful as there has been an assumption that device has to be
told to connect but Wifi already does it's best to connect in background. Calling this
method will block until WiFi connects. Sketch can avoid
blocking call then use (WiFi.status()==WL_CONNECTED) test to see if connected yet.
See some discussion at https://github.com/tzapu/WiFiManager/issues/68
*/
boolean WiFiManager::autoConnect(char const *apName, char const *apPassword) {
  DEBUG_WM(F(""));
  DEBUG_WM(F("AutoConnect"));

  // read eeprom for ssid and pass
  //String ssid = getSSID();
  //String pass = getPassword();

  // device will attempt to connect by itself; wait 10 secs
  // to see if it succeeds and should it fail, fall back to AP
  WiFi.mode(WIFI_STA);
  unsigned long startedAt = millis();
    while(millis() - startedAt < 10000)
    {
        delay(100);
        if (WiFi.status()==WL_CONNECTED) {
			float waited = (millis()- startedAt);
			DEBUG_WM(F("After waiting "));
			DEBUG_WM(waited/1000);
			DEBUG_WM(F(" secs local ip: "));
			DEBUG_WM(WiFi.localIP());
            return true;
		}
    }

  return startConfigPortal(apName, apPassword);
}

boolean  WiFiManager::startConfigPortal() {
  String ssid = "ESP" + String(ESP.getChipId());
  return startConfigPortal(ssid.c_str(),NULL);
}

boolean  WiFiManager::startConfigPortal(char const *apName, char const *apPassword) {
  //setup AP
  int connRes = WiFi.waitForConnectResult();
  ap_mode = true; // flag indicates we are running in ap mode so turn indicator on
  if (connRes == WL_CONNECTED){
	  WiFi.mode(WIFI_AP_STA); //Dual mode works fine if it is connected to WiFi
	  DEBUG_WM("SET AP STA");
  	}
  	else {
    WiFi.mode(WIFI_AP); // Dual mode becomes flaky if not connected to a WiFi network.
    // When ESP8266 station is trying to find a target AP, it will scan on every channel,
    // that means ESP8266 station is changing its channel to scan. This makes the channel of ESP8266 softAP keep changing too..
    // So the connection may break. From http://bbs.espressif.com/viewtopic.php?t=671#p2531
    DEBUG_WM("SET AP");
	}
  _apName = apName;
  _apPassword = apPassword;

  //notify we entered AP mode
  if ( _apcallback != NULL) {
    _apcallback(this);
  }

  connect = false;
  setupConfigPortal();
  bool TimedOut=true;
  SPIFFS_Credentials(); // if exist wifi credentials from spiffs, load it and save to ssid_, pass_
  if(wifi_credentials_ok)
  {	
	connectWifi(ssid_, pass_); // connect to wifi network.
	// this code test if any trail character like x0d in the ssid string.
	// if trailing exist server cannot connect to network since wrong credentials.
	//for (int i = 0; i < strlen(ssid_.c_str()); ++i) {
    //      Serial.printf("%02x ", (ssid_.c_str())[i]);
    //    }
    //    Serial.println("");
  }
  
  while (_configPortalTimeout == 0 || millis() < _configPortalStart + _configPortalTimeout) {
    //DNS
    dnsServer->processNextRequest();
    //HTTP
    server->handleClient();
	//indicator led blinking in AP mode
	AP_Led_Indicator(ap_mode);
	
    if (connect) {
      connect = false;
      TimedOut=false;
      delay(2000);
      DEBUG_WM(F("Connecting to new AP"));

		// using user-provided  _ssid, _pass in place of system-stored ssid and pass
      if (connectWifi(_ssid, _pass) != WL_CONNECTED) {
        DEBUG_WM(F("Failed to connect."));
		// Dual mode becomes flaky if not connected to a WiFi network. could be because too much 
		// processor resources used for network connecting.
        WiFi.mode(WIFI_AP); 
      } 
	  else{
        //notify that configuration has changed and any optional parameters should be saved
        if ( _savecallback != NULL) {
          //todo: check if any custom parameters actually exist, and check if they really changed maybe
          _savecallback();
        }
        //break;
      }

	   //flag set to exit after config
      if (_shouldBreakAfterConfig)
	  {
        if ( _savecallback != NULL) {
          //todo: check if any custom parameters actually exist, and check if they really changed maybe
          _savecallback();
        }
        break;
      }
    }
	
    if (stopConfigPortal) {
	  stopConfigPortal = false;
	  break;
    }
    yield();
  }
  WiFi.mode(WIFI_STA);
  if (TimedOut & WiFi.status() != WL_CONNECTED) {
	WiFi.begin();
    int connRes = waitForConnectResult();
    DEBUG_WM ("Timed out connection result: ");
    DEBUG_WM ( getStatus(connRes));
    }
  ap_mode = false;
  server.reset();
  dnsServer.reset();
  return  WiFi.status() == WL_CONNECTED;
}

int WiFiManager::connectWifi(String ssid, String pass) {
  DEBUG_WM(F("Connecting wifi with new parameters..."));
  if (ssid != "")
  {
	//Disconnect from network and wipe out old credentials.
	//if either ssid or password not provided on submit, esp8266 sometimes locked up if new values different to
	//previous stored values and device in the process of trying connecting to the network
	resetSettings();
	
	//check if we've got static_ip settings, if we do, use those.
	//using this way user only can change to dhcp/static ip on the first time logging to local network
	//after module already in local network, SPIFFS_IP_Innitialize function below 
	//will help user change to dhcp/static ip when needed.
	
	/*
	if (_sta_static_ip)
	{
	  DEBUG_WM(F("Custom STA IP/GW/Subnet"));
	  WiFi.config(_sta_static_ip, _sta_static_gw, _sta_static_sn);
	  DEBUG_WM(WiFi.localIP());
	}
	*/
	
	WiFi.mode(WIFI_AP_STA); //It will start in station mode if it was previously in AP mode.
	// interface for user manage ip address (dhcp/static) when module already in local network.
	SPIFFS_IP_Innitialize();
	if(is_Static_IP)
	{
		//WiFi.mode(WIFI_STA);
		WiFi.begin(ssid.c_str(), pass.c_str());
		WiFi.config(stringToIP(ip_info.static_ip), stringToIP(ip_info.gateway), stringToIP(ip_info.netmask));
	}
	else
	{
		WiFi.begin(ssid.c_str(), pass.c_str());// Start wifi with new values.
	}
  }
  else if(!WiFi.SSID()) 
  {
      DEBUG_WM(F("No saved credentials"));
  }
  int connRes = waitForConnectResult();
  DEBUG_WM ("Connection result: ");
  DEBUG_WM ( getStatus(connRes));
  if(WiFi.status() == WL_CONNECTED)
  {
	if(!wifi_credentials_ok) // this credentials not exists in SPIFFS so it's new one, should save it.
	{
		SPIFFS_Credentials(ssid,pass);
		DEBUG_WM(F("Wifi credentials saved to SPIFFS."));
	}
	else
	{
		wifi_credentials_ok = false; // turn off the flag for next use, SPIFFS_Credentials function will handle its state.
	}
	ap_mode = false;
	DEBUG_WM("Module can be accessed on local IP: ");
	DEBUG_WM(WiFi.localIP());
  }
  if (_tryWPS && connRes != WL_CONNECTED && pass == "") //not connected, WPS enabled, no pass - first attempt
  {
    startWPS();
    connRes = waitForConnectResult();//should be connected at the end of WPS
  }
  return connRes;
}

uint8_t WiFiManager::waitForConnectResult() 
{
  if (_connectTimeout == 0) 
  {
	unsigned long startedAt = millis();
	DEBUG_WM (F("After waiting..."));
	int connRes = WiFi.waitForConnectResult();
	float waited = (millis()- startedAt);
	DEBUG_WM (waited/1000);
	DEBUG_WM (F("seconds"));
    return connRes;
  }
  else 
  {
    DEBUG_WM (F("Waiting for connection result with time out"));
    unsigned long start = millis();
    boolean keepConnecting = true;
    uint8_t status;
    while (keepConnecting) {
      status = WiFi.status();
      if (millis() > start + _connectTimeout) {
        keepConnecting = false;
        DEBUG_WM (F("Connection timed out"));
      }
      if (status == WL_CONNECTED || status == WL_CONNECT_FAILED) {
        keepConnecting = false;
      }
      delay(100);
    }
    return status;
  }
}

void WiFiManager::startWPS() {
  DEBUG_WM(F("START WPS"));
  WiFi.beginWPSConfig();
  DEBUG_WM(F("END WPS"));
}

const char* WiFiManager::getStatus(int status)//Convenient for debugging but wasteful of program space.Remove if short of space
{
   switch (status)
   {
      case WL_IDLE_STATUS: return "WL_IDLE_STATUS";
      case WL_NO_SSID_AVAIL: return "WL_NO_SSID_AVAIL";
      case WL_CONNECTED: return "WL_CONNECTED";
	  case WL_CONNECT_FAILED: return "WL_CONNECT_FAILED";
      case WL_DISCONNECTED: return "WL_DISCONNECTED";
      default: return "UNKNOWN";
   }
}

String WiFiManager::getConfigPortalSSID() {
  return _apName;
}

void WiFiManager::resetSettings() {
  DEBUG_WM(F("previous settings invalidated"));
  WiFi.disconnect(true);
  WiFi.softAPdisconnect();
  WiFi.mode(WIFI_OFF);
  delay(500);
  return;
}
void WiFiManager::setTimeout(unsigned long seconds) {
  setConfigPortalTimeout(seconds);
}

void WiFiManager::setConfigPortalTimeout(unsigned long seconds) {
  _configPortalTimeout = seconds * 1000;
}

void WiFiManager::setConnectTimeout(unsigned long seconds) {
  _connectTimeout = seconds * 1000;
}

void WiFiManager::setDebugOutput(boolean debug) {
  _debug = debug;
}

void WiFiManager::setAPStaticIPConfig(IPAddress ip, IPAddress gw, IPAddress sn) {
  _ap_static_ip = ip;
  _ap_static_gw = gw;
  _ap_static_sn = sn;
}

void WiFiManager::setSTAStaticIPConfig(IPAddress ip, IPAddress gw, IPAddress sn) {
  _sta_static_ip = ip;
  _sta_static_gw = gw;
  _sta_static_sn = sn;
}

void WiFiManager::setMinimumSignalQuality(int quality) {
  _minimumQuality = quality;
}

void WiFiManager::setBreakAfterConfig(boolean shouldBreak) {
  _shouldBreakAfterConfig = shouldBreak;
}

void WiFiManager::reportStatus(String &page)
{
  if (WiFi.SSID() != "")
  {
	  page += F("Module has connected to AP <strong>");
	  page += WiFi.SSID();
	  if (WiFi.status() == WL_CONNECTED){
		  page += F(" </strong> and currently running on <strong>IP: </strong> <a href=\"http://");
		  page += WiFi.localIP().toString();
		  page += F("/\">");
		  page += WiFi.localIP().toString();
		  page += F("</a>");
	   }
	  else {
		  page += F(" but <strong>not currently connected</strong> to network.");
	  }
  }
  else 
  {
	page += F("No network currently configured.");
  }
}

/** Handle root or redirect to captive portal */
void WiFiManager::handleRoot() {
  DEBUG_WM(F("Load WiFi credentials from spiffs."));
  DEBUG_WM(F("Handle root"));
  if (captivePortal()) { // If caprive portal redirect instead of displaying the error page.
      return;
  }
  server->sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server->sendHeader("Pragma", "no-cache");
  server->sendHeader("Expires", "-1");
  String page = FPSTR(HTTP_HEAD);
  page.replace("{v}", "ESP8266 Captive Portal");
  page += FPSTR(HTTP_SCRIPT);
  page += FPSTR(HTTP_STYLE);
  page += _customHeadElement;
  page += FPSTR(HTTP_HEAD_END);
  page += "<h2>";
  page += _apName;
  page += "</h2>";
  page += FPSTR(HTTP_PORTAL_OPTIONS);
  page += F("<div class=\"msg\">");
  reportStatus(page);
  page += F("</div>");
  page += FPSTR(HTTP_END);

  server->send(200, "text/html", page);

}

/** Wifi config page handler */
void WiFiManager::handleWifi() {
  server->sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server->sendHeader("Pragma", "no-cache");
  server->sendHeader("Expires", "-1");
  String page = FPSTR(HTTP_HEAD);
  page.replace("{v}", "WiFi Configuration");
  page += FPSTR(HTTP_SCRIPT);
  page += FPSTR(HTTP_STYLE);
  page += FPSTR(HTTP_CUSTOM_STYLE);
  page += _customHeadElement;
  page += FPSTR(HTTP_HEAD_END);
  page += F("<div>");
  page += F("<ul class=\"breadcrumb\">");
  page += F("<li><a href=\"/\">Home</a></li>");
  page += F("<li><a href=\"#\">WiFi Configuration</a></li>");
  page += F("</ul>");
  page += F("</div>");
  page += F("<h2>WiFi Configuration</h2>");
  //Print list of WiFi networks that were found in earlier scan
    if (numberOfNetworks == 0) {
      page += F("WiFi scan found no networks. Restart configuration portal to scan again.");
    } else {
      //display networks in page
      for (int i = 0; i < numberOfNetworks; i++) {
        if(networkIndices[i] == -1) continue; // skip dups and those that are below the required quality

        DEBUG_WM(WiFi.SSID(networkIndices[i]));
        DEBUG_WM(WiFi.RSSI(networkIndices[i]));
        int quality = getRSSIasQuality(WiFi.RSSI(networkIndices[i]));

        String item = FPSTR(HTTP_ITEM);
        String rssiQ;
        rssiQ += quality;
        item.replace("{v}", WiFi.SSID(networkIndices[i]));
        item.replace("{r}", rssiQ);
        if (WiFi.encryptionType(networkIndices[i]) != ENC_TYPE_NONE) {
          item.replace("{i}", "l");
        } else {
            item.replace("{i}", "");
        }
        //DEBUG_WM(item);
        page += item;
        delay(0);
        }
    page += "<br/>";
    }

  page += FPSTR(HTTP_FORM_START);
  char parLength[2];
  // add the extra parameters to the form
  for (int i = 0; i < _paramsCount; i++) {
    if (_params[i] == NULL) {
      break;
    }

	String pitem;
	switch (_params[i]->getLabelPlacement()) {
    case WFM_LABEL_BEFORE:
	  pitem = FPSTR(HTTP_FORM_LABEL);
	  pitem += FPSTR(HTTP_FORM_PARAM);
      break;
    case WFM_LABEL_AFTER:
	  pitem = FPSTR(HTTP_FORM_PARAM);
	  pitem += FPSTR(HTTP_FORM_LABEL);
      break;
    default:
	  // WFM_NO_LABEL
      pitem = FPSTR(HTTP_FORM_PARAM);
    break;
  }

    if (_params[i]->getID() != NULL) {
      pitem.replace("{i}", _params[i]->getID());
      pitem.replace("{n}", _params[i]->getID());
      pitem.replace("{p}", _params[i]->getPlaceholder());
      snprintf(parLength, 2, "%d", _params[i]->getValueLength());
      pitem.replace("{l}", parLength);
      pitem.replace("{v}", _params[i]->getValue());
      pitem.replace("{c}", _params[i]->getCustomHTML());
    } else {
      pitem = _params[i]->getCustomHTML();
    }

    page += pitem;
  }
  if (_params[0] != NULL) {
    page += "<br/>";
  }

  if (_sta_static_ip) {

    String item = FPSTR(HTTP_FORM_PARAM);
    item.replace("{i}", "ip");
    item.replace("{n}", "ip");
    item.replace("{p}", "Static IP");
    item.replace("{l}", "15");
    item.replace("{v}", _sta_static_ip.toString());

    page += item;

    item = FPSTR(HTTP_FORM_PARAM);
    item.replace("{i}", "gw");
    item.replace("{n}", "gw");
    item.replace("{p}", "Static Gateway");
    item.replace("{l}", "15");
    item.replace("{v}", _sta_static_gw.toString());

    page += item;

    item = FPSTR(HTTP_FORM_PARAM);
    item.replace("{i}", "sn");
    item.replace("{n}", "sn");
    item.replace("{p}", "Subnet");
    item.replace("{l}", "15");
    item.replace("{v}", _sta_static_sn.toString());

    page += item;

    page += "<br/>";
  }

  page += FPSTR(HTTP_FORM_END);

  page += FPSTR(HTTP_END);

  server->send(200, "text/html", page);

  DEBUG_WM(F("Config page sent"));
}

/** Handle the WLAN save form and redirect to WLAN config page again */
void WiFiManager::handleWifiSave() {
  DEBUG_WM(F("WiFi save"));

  _ssid = server->arg("s").c_str();
  _pass = server->arg("p").c_str();

  //parameters
  for (int i = 0; i < _paramsCount; i++) {
    if (_params[i] == NULL) {
      break;
    }
    //read parameter
    String value = server->arg(_params[i]->getID()).c_str();
    //store it in array
    value.toCharArray(_params[i]->_value, _params[i]->_length);
    DEBUG_WM(F("Parameter"));
    DEBUG_WM(_params[i]->getID());
    DEBUG_WM(value);
  }

  if (server->arg("ip") != "") {
    DEBUG_WM(F("static ip"));
    DEBUG_WM(server->arg("ip"));
    //_sta_static_ip.fromString(server->arg("ip"));
    String ip = server->arg("ip");
    optionalIPFromString(&_sta_static_ip, ip.c_str());
  }
  if (server->arg("gw") != "") {
    DEBUG_WM(F("static gateway"));
    DEBUG_WM(server->arg("gw"));
    String gw = server->arg("gw");
    optionalIPFromString(&_sta_static_gw, gw.c_str());
  }
  if (server->arg("sn") != "") {
    DEBUG_WM(F("static netmask"));
    DEBUG_WM(server->arg("sn"));
    String sn = server->arg("sn");
    optionalIPFromString(&_sta_static_sn, sn.c_str());
  }

  String page = FPSTR(HTTP_HEAD);
  page.replace("{v}", "Credentials Saved");
  page += FPSTR(HTTP_SCRIPT);
  page += FPSTR(HTTP_STYLE);
  page += _customHeadElement;
  page += FPSTR(HTTP_HEAD_END);
  page += FPSTR(HTTP_SAVED);
  page.replace("{v}", _apName);
  page.replace("{x}", _ssid);
  page += FPSTR(HTTP_END);

  server->send(200, "text/html", page);

  DEBUG_WM(F("WiFi save page sent."));

  connect = true; //signal ready to connect/reset
}
/** Handle shut down the server page */
void WiFiManager::handleServerClose() {
    DEBUG_WM(F("Server Close"));
    server->sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    server->sendHeader("Pragma", "no-cache");
    server->sendHeader("Expires", "-1");
    String page = FPSTR(HTTP_HEAD);
    page.replace("{v}", "Close Server");
    page += FPSTR(HTTP_SCRIPT);
    page += FPSTR(HTTP_STYLE);
    page += _customHeadElement;
    page += FPSTR(HTTP_HEAD_END);
    page += F("<div class=\"msg\">");
    page += F("My network is <strong>");
    page += WiFi.SSID();
    page += F("</strong><br>");
    page += F("My IP address is <strong>");
    page += WiFi.localIP().toString();
    page += F("</strong><br><br>");
    page += F("Configuration server closed...<br><br>");
    //page += F("Push button on device to restart configuration server!");
    page += FPSTR(HTTP_END);
    server->send(200, "text/html", page);
    stopConfigPortal = true; //signal ready to shutdown config portal
	DEBUG_WM(F("Server close page sent."));

}
/** Handle the info page */
void WiFiManager::handleInfo() {
  DEBUG_WM(F("Info"));
  server->sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server->sendHeader("Pragma", "no-cache");
  server->sendHeader("Expires", "-1");
  String page = FPSTR(HTTP_HEAD);
  page.replace("{v}", "Extra Functions");
  page += FPSTR(HTTP_SCRIPT);
  page += FPSTR(HTTP_STYLE);
  page += FPSTR(HTTP_CUSTOM_STYLE);
  page += _customHeadElement;
  page += FPSTR(HTTP_HEAD_END);
  page += F("<div>");
  page += F("<ul class=\"breadcrumb\">");
  page += F("<li><a href=\"/\">Home</a></li>");
  page += F("<li><a href=\"#\">Extra Functions</a></li>");
  page += F("</ul>");
  page += F("</div>");
  
  
  page += F("<h2>WiFi Information</h2>");
  reportStatus(page);
  page += F("<h3>Device Data</h3>");
  page += F("<table class=\"gpio_table\" width=\"100%\">");
  page += F("<thead><tr><th>Name</th><th>Value</th></tr></thead><tbody><tr><td>Chip ID</td><td>");
  page += ESP.getChipId();
  page += F("</td></tr>");
  page += F("<tr><td>Flash Chip ID</td><td>");
  page += ESP.getFlashChipId();
  page += F("</td></tr>");
  page += F("<tr><td>IDE Flash Size</td><td>");
  page += ESP.getFlashChipSize();
  page += F(" bytes</td></tr>");
  page += F("<tr><td>Real Flash Size</td><td>");
  page += ESP.getFlashChipRealSize();
  page += F(" bytes</td></tr>");
  page += F("<tr><td>Access Point IP</td><td>");
  page += WiFi.softAPIP().toString();
  page += F("</td></tr>");
  page += F("<tr><td>Access Point MAC</td><td>");
  page += WiFi.softAPmacAddress();
  page += F("</td></tr>");

  page += F("<tr><td>SSID</td><td>");
  page += WiFi.SSID();
  page += F("</td></tr>");
  page += F("<tr><td>Station IP</td><td>");
  page += WiFi.localIP().toString();
  page += F("</td></tr>");
  page += F("<tr><td>Station MAC</td><td>");
  page += WiFi.macAddress();
  page += F("</td></tr>");
  page += F("</tbody></table>");
  page += F("<h3>Extra Functions</h3>");
  page += F("<table class=\"gpio_table\">");
  page += F("<thead><tr><th width=\"150px\">Functions</th><th>Description</th></tr></thead><tbody>");
  //page += F("<tr><td><a href=\"/\">/</a></td>");
  //page += F("<td>Menu page.</td></tr>");
  page += F("<tr><td><a href=\"/json_module_wifi_info\"> WiFi Info</a></td>");
  page += F("<td>Return the wifi configuration details of the module in JSON format. Interface for programmatic wifi configuration.</td></tr>");
  page += F("<tr><td><a href=\"/json_wifi_scan_result\"> WiFi Scan Result</a></td>");
  page += F("<td>Return the wifi scan result in JSON format. Interface for programmatic wifi configuration.</td></tr>");
  page += F("<tr><td><a href=\"/time\"> NTP Time</a></td>");
  page += F("<td>Return local time (GMT+7) in JSON format. Interface for programmatic time configuration.</td></tr>");
  page += F("<tr><td><a href=\"/restart\"> Restart</a></td>");
  page += F("<td>Restart the ESP device. All the WiFi configuration will be retained. The esp8266 module will automatically connect to previous known WiFi network.</td></tr>");  
  page += F("<tr><td><a href=\"/exit_portal\"> Exit Portal </a></td>");
  page += F("<td>Exit the captive portal, close the configuration server and configuration WiFi network.</td></tr>");
  page += F("<tr><td><a href=\"/factory_reset\"> Factory Reset</a></td>");
  page += F("<td>Factory reset esp8266 module, delete all wifi configuration and reboot. The esp8266 module will not reconnect to a network until new WiFi configuration data is entered.</td></tr>");
  page += F("</table>");
  page += F("<p/>");
  page += FPSTR(HTTP_END);
  server->send(200, "text/html", page);
  DEBUG_WM(F("Sent info page"));
}
/** Handle the state page */
void WiFiManager::handleState() {
  DEBUG_WM(F("State - json"));
  server->sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server->sendHeader("Pragma", "no-cache");
  server->sendHeader("Expires", "-1");
  String page = F("{\"Soft_AP_IP\":\"");
  page += WiFi.softAPIP().toString();
  page += F("\",\"Soft_AP_MAC\":\"");
  page += WiFi.softAPmacAddress();
  page += F("\",\"Station_IP\":\"");
  page += WiFi.localIP().toString();
  page += F("\",\"Station_MAC\":\"");
  page += WiFi.macAddress();
  page += F("\",");
  if (WiFi.psk()!=""){
  	  page += F("\"Password\":true,");
    }
  else {
  	  page += F("\"Password\":false,");
    }
  page += F("\"SSID\":\"");
  page += WiFi.SSID();
  page += F("\"}");
  server->send(200, "application/json", page);
  DEBUG_WM(F("States page in json format sent."));
}

/** Handle the scan page */
void WiFiManager::handleScan() {
  DEBUG_WM(F("State - json"));
  server->sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server->sendHeader("Pragma", "no-cache");
  server->sendHeader("Expires", "-1");

  int n;
  int *indices;
  int **indicesptr = &indices;
  //Space for indices array allocated on heap in scanWifiNetworks
  //and should be freed when indices no longer required.
  n = scanWifiNetworks(indicesptr);
  DEBUG_WM(F("In handleScan, scanWifiNetworks done"));
  String page = F("{\"Access_Points\":[");
  //display networks in page
  for (int i = 0; i < n; i++) {
          if(indices[i] == -1) continue; // skip duplicates and those that are below the required quality
          if(i != 0) page += F(", ");
          DEBUG_WM(WiFi.SSID(indices[i]));
          DEBUG_WM(WiFi.RSSI(indices[i]));
          int quality = getRSSIasQuality(WiFi.RSSI(indices[i]));
          String item = FPSTR(JSON_ITEM);
          String rssiQ;
          rssiQ += quality;
          item.replace("{v}", WiFi.SSID(indices[i]));
          item.replace("{r}", rssiQ);
          if (WiFi.encryptionType(indices[i]) != ENC_TYPE_NONE) {
            item.replace("{i}", "true");
          } else {
            item.replace("{i}", "false");
          }
          //DEBUG_WM(item);
          page += item;
          delay(0);
  }
  free(indices); //indices array no longer required so free memory
  page += F("]}");
  server->send(200, "application/json", page);
  DEBUG_WM(F("Sent WiFi scan data ordered by signal strength in json format"));
}

/** Handle the reset page */
void WiFiManager::handleReset() {
  DEBUG_WM(F("Reset"));
  server->sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server->sendHeader("Pragma", "no-cache");
  server->sendHeader("Expires", "-1");
  String page = FPSTR(HTTP_HEAD);
  page.replace("{v}", "WiFi Information");
  page += FPSTR(HTTP_SCRIPT);
  page += FPSTR(HTTP_STYLE);
  page += _customHeadElement;
  page += FPSTR(HTTP_HEAD_END);
  page += F("ESP module will be reset in a few seconds.");
  page += FPSTR(HTTP_END);
  server->send(200, "text/html", page);

  DEBUG_WM(F("Reset page sent."));
  delay(5000);
  WiFi.disconnect(true); // Wipe out WiFi credentials.
  ESP.reset();
  delay(2000);
}

String WiFiManager::getContentType(String filename){
  if(server->hasArg(F("download"))) return F("application/octet-stream");
  else if(filename.endsWith(F(".htm"))) return F("text/html");
  else if(filename.endsWith(F(".html"))) return F("text/html");
  else if(filename.endsWith(F(".css"))) return F("text/css");
  else if(filename.endsWith(F(".js"))) return F("application/javascript");
  else if(filename.endsWith(F(".png"))) return F("image/png");
  else if(filename.endsWith(F(".gif"))) return F("image/gif");
  else if(filename.endsWith(F(".jpg"))) return F("image/jpeg");
  else if(filename.endsWith(F(".ico"))) return F("image/x-icon");
  else if(filename.endsWith(F(".xml"))) return F("text/xml");
  else if(filename.endsWith(F(".pdf"))) return F("application/x-pdf");
  else if(filename.endsWith(F(".zip"))) return F("application/x-zip");
  else if(filename.endsWith(F(".gz"))) return F("application/x-gzip");
  return F("text/plain");
}

void WiFiManager::handleGPIOControl(){
  DEBUG_WM(F("GPIO Control"));
  String path = F("/gpio.html");
  String contentType = getContentType(path);
  String pathWithGz = path + F(".gz");
  if(SPIFFS.exists(pathWithGz) || SPIFFS.exists(path))
  {
    if(SPIFFS.exists(pathWithGz))
    {
      path += F(".gz");
    }
    File file = SPIFFS.open(path, "r");
    size_t sent = server->streamFile(file, contentType);
    file.close();
  }
  DEBUG_WM(F("GPIO page sent"));
}

void WiFiManager::handleGPIOToggle(){
	DEBUG_WM("GPIO Toggle");
	if(server->arg("status") == "On")
	{
		digitalWrite(atoi(server->arg("pin").c_str()), HIGH);
		DEBUG_WM(F("GPIO turn HIGH"));
		DEBUG_WM(server->arg("pin").c_str());
	}
	else
	{
		digitalWrite(atoi(server->arg("pin").c_str()), LOW);
		DEBUG_WM(F("GPIO turn LOW"));
		DEBUG_WM(server->arg("pin").c_str());
	}
	switch(atoi(server->arg("pin").c_str()))
	{
		case 0:
			gpio0.status = server->arg("status");
			gpio0.alias = server->arg("alias");
			break;
		case 2:
			gpio2.status = server->arg("status");
			gpio2.alias = server->arg("alias");
			break;
		case 5:
			gpio5.status = server->arg("status");
			gpio5.alias = server->arg("alias");
			break;
		case 12:
			gpio12.status = server->arg("status");
			gpio12.alias = server->arg("alias");
			break;
		case 14:
			gpio14.status = server->arg("status");
			gpio14.alias = server->arg("alias");
			break;
		case 15:
			gpio15.status = server->arg("status");
			gpio15.alias = server->arg("alias");
			break;
		case 16:
			gpio16.status = server->arg("status");
			gpio16.alias = server->arg("alias");
			break;
	}
	server->send(200,"text/html","");
}

void WiFiManager::handleFirmwareUpdatePage()
{
  DEBUG_WM(F("Firmare Update"));
  String path = F("/firmware_update.html");
  String contentType = getContentType(path);
  String pathWithGz = path + F(".gz");
  if(SPIFFS.exists(pathWithGz) || SPIFFS.exists(path))
  {
    if(SPIFFS.exists(pathWithGz))
    {
      path += F(".gz");
    }
    File file = SPIFFS.open(path, "r");
    size_t sent = server->streamFile(file, contentType);
    file.close();
  }
  DEBUG_WM(F("Firmare update page sent"));
}

void WiFiManager::handleUpdateHeader()
{
	server->sendHeader("Connection", "close");
    server->sendHeader("Access-Control-Allow-Origin", "*");
    server->send(200, "text/plain", (Update.hasError())?"Fail To Update Firmware.":"Firmware Updated Successfully.");
    ESP.restart();
}

void WiFiManager::handleUpdate()
{
	DEBUG_WM(F("Handle Firmare Update"));
	HTTPUpload& upload = server->upload();
      if(upload.status == UPLOAD_FILE_START)
	  {
		DEBUG_WM(F("Upload file start"));
        uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
        if(!Update.begin(maxSketchSpace))//start with max available size
		{
          Update.printError(Serial);
        }
      }
	  else if(upload.status == UPLOAD_FILE_WRITE)
	  {
        if(Update.write(upload.buf, upload.currentSize) != upload.currentSize)
		{
          Update.printError(Serial);
        }
      }
	  else if(upload.status == UPLOAD_FILE_END)
	  {
        if(Update.end(true))//true to set the size to the current progress
		{ 
          DEBUG_WM(F("Update done. Rebooting..."));
        }
		else 
		{
          Update.printError(Serial);
        }
      }
	  yield();
}

void WiFiManager::handleGPIOStatus()
{
	String json = "{\"GPIO0\":{\"Status\":\"" + gpio0.status + F("\",\"Alias\":\"") + gpio0.alias
		+ F("\"},\"GPIO2\":{\"Status\":\"") + gpio2.status + F("\",\"Alias\":\"") + gpio2.alias
		+ F("\"},\"GPIO5\":{\"Status\":\"") + gpio5.status + F("\",\"Alias\":\"") + gpio5.alias
        + F("\"},\"GPIO12\":{\"Status\":\"") + gpio12.status + F("\",\"Alias\":\"") + gpio12.alias
        + F("\"},\"GPIO14\":{\"Status\":\"") + gpio14.status + F("\",\"Alias\":\"") + gpio14.alias
		+ F("\"},\"GPIO15\":{\"Status\":\"") + gpio15.status + F("\",\"Alias\":\"") + gpio15.alias
		+ F("\"},\"GPIO16\":{\"Status\":\"") + gpio16.status + F("\",\"Alias\":\"") + gpio16.alias
        + "\"}}";
	server->send(200,"text/html", json);
	DEBUG_WM("GPIO status sent");
}

void WiFiManager::handleTime()
{
	ntpInnitialize();
	String hr, min, sec;
	if(hour() > 9) hr = hour(); else hr = '0' + hour();
	if(minute() > 9) min = minute(); else min = '0' + minute();
	if(second() > 9) sec = second(); else sec = '0' + second();
	String json = "{\"Day\":\"" + String(day())
				  + F("\",\"Month\":\"") + String(month())
				  + F("\",\"Year\":\"") + String(year())
				  + F("\",\"Hour\":\"") + String(hour())
				  + F("\",\"Minute\":\"") + String(minute())
				  + F("\",\"Second\":\"") + String(second())
				  + "\"}";
	server->send(200,"text/html",json);
	DEBUG_WM(F("Time request done."));
}

void WiFiManager::handleRestart()
{
  DEBUG_WM(F("Module Restart"));
  server->sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server->sendHeader("Pragma", "no-cache");
  server->sendHeader("Expires", "-1");
  String page = FPSTR(HTTP_HEAD);
  page.replace("{v}", "WiFi Information");
  page += FPSTR(HTTP_SCRIPT);
  page += FPSTR(HTTP_STYLE);
  page += _customHeadElement;
  page += FPSTR(HTTP_HEAD_END);
  page += F("ESP module will be restarted in a few seconds.");
  page += FPSTR(HTTP_END);
  server->send(200, "text/html", page);

  DEBUG_WM(F("Restart page sent."));
  delay(5000);
  ESP.restart();
  delay(2000);
}

void WiFiManager::handleIPConfigurationPage()
{
  DEBUG_WM(F("IP Configuration"));
  String path = F("/ip_configuration.html");
  String contentType = getContentType(path);
  String pathWithGz = path + F(".gz");
  if(SPIFFS.exists(pathWithGz) || SPIFFS.exists(path))
  {
    if(SPIFFS.exists(pathWithGz))
    {
      path += F(".gz");
    }
    File file = SPIFFS.open(path, "r");
    size_t sent = server->streamFile(file, contentType);
    file.close();
  }
  DEBUG_WM(F("IP configuration page sent"));
}

void WiFiManager::handleIPStatus()
{
	String json = "";
	// Static IP
	if(is_Static_IP)
	{
		json = "{\"IP_Info\":{\"IP\":\"" + ip_info.static_ip
		+ F("\",\"Netmask\":\"") + ip_info.netmask
		+ F("\",\"Gateway\":\"") + ip_info.gateway
		+ F("\",\"IP_Type\":\"")
        + "STATIC\"}}";
	}
	else //DHCP
	{
		json = "{\"IP_Info\":{\"IP\":\"" + toStringIP(WiFi.localIP())
		+ F("\",\"Netmask\":\"") + toStringIP(WiFi.subnetMask())
		+ F("\",\"Gateway\":\"") + toStringIP(WiFi.gatewayIP())
		+ F("\",\"IP_Type\":\"")
        + "DHCP\"}}";
	}
	
	server->send(200,"text/html", json);
	DEBUG_WM("IP status sent");
}

void WiFiManager::handleIPChange()
{
	if(server->arg("type") == "dhcp")
	{
		DEBUG_WM("dhcp mode");
		SPIFFS_IP_Configure("");
	}
	
	if(server->arg("type") == "static")
	{
		DEBUG_WM("Static ip mode");
		DEBUG_WM("IP submit from client:");
		DEBUG_WM(server->arg("ip"));
		SPIFFS_IP_Configure(server->arg("ip"));
	}
}

void WiFiManager::handleEditorPage()
{
  DEBUG_WM(F("SPIFFS Editor"));
  String path = server->uri();
  DEBUG_WM(F("handleFileRead: "));
  DEBUG_WM(path);
  if(path.endsWith("/editor_page")) 
  {
    DEBUG_WM(F("serving editor page"));
    path = "/ace.html";
  }
  
  if(path.endsWith("ace.js"))
  {
    DEBUG_WM(F("serving ace.js"));
  }

  if(path.endsWith("mode-html.js"))
  {
    DEBUG_WM(F("serving mode-html.js"));
  }

  if(path.endsWith("theme-eclipse.js"))
  {
    DEBUG_WM(F("serving theme-eclipse.js"));
  }

  if(path.endsWith("worker-html.js"))
  {
    DEBUG_WM(F("serving worker-html.js"));
  }
  
  String contentType = getContentType(path);
  String pathWithGz = path + ".gz";
  if(SPIFFS.exists(pathWithGz) || SPIFFS.exists(path))
  {
    if(SPIFFS.exists(pathWithGz))
    {
      path += ".gz";
    }
    File file = SPIFFS.open(path, "r");
    size_t sent = server->streamFile(file, contentType);
    file.close();
  }
  DEBUG_WM(F("SPIFFS editor page sent"));
}

void WiFiManager::handleFileCreate()
{
  if(server->args() == 0)
  {
    return server->send(500, "text/plain", "BAD ARGS");
  }
  String path = server->arg(0);
  DEBUG_WM(F("Handle file create: "));
  DEBUG_WM(path);
  if(path == "/")
  {
    return server->send(500, "text/plain", "BAD PATH");
  }
  
  if(SPIFFS.exists(path))
  {
    return server->send(500, "text/plain", "FILE EXISTS");
  }
  File file = SPIFFS.open(path, "w");
  if(file)
  {
    file.close();
  }
  else
  {
    return server->send(500, "text/plain", "FILE CREATE FAILED");
  }
  server->send(200, "text/plain", "");
  path = String();
}

void WiFiManager::handleFileList()
{
  if(!server->hasArg("dir")) 
  {
    server->send(500, "text/plain", "BAD ARGS"); 
    return;
  }
  String path = server->arg("dir");
  DEBUG_WM(F("Handle file list: "));
  DEBUG_WM(path);
  Dir dir = SPIFFS.openDir(path);
  path = String();
  String output = "[";
  while(dir.next())
  {
    File entry = dir.openFile("r");
    if (output != "[") output += ',';
    bool isDir = false;
    output += "{\"type\":\"";
    output += (isDir)?"dir":"file";
    output += "\",\"name\":\"";
    output += String(entry.name()).substring(1);
    output += "\"}";
    entry.close();
  }
  output += "]";
  server->send(200, "text/json", output);
}

void WiFiManager::handleFileRead()
{
  String param = server->arg("filename");
  param = "/" + param; // file send from client in the form "/filename.extension" then saved in spiffs system, without "/" before filename, it wont work
  DEBUG_WM(F("Handle file: "));
  DEBUG_WM(param);
  String contentType = getContentType(param);
  String paramWithGz = param + ".gz";
  if(SPIFFS.exists(paramWithGz) || SPIFFS.exists(param))
  {
    if(SPIFFS.exists(paramWithGz))
    {
      param += ".gz";
    }
    File file = SPIFFS.open(param, "r");
    size_t sent = server->streamFile(file, contentType);
    file.close();
  }
}

void WiFiManager::handleFileDownload()
{
  String file_ = server->arg("file");
  file_= "/" + file_;
  DEBUG_WM(F("Handle file download: "));
  DEBUG_WM(file_);
  String contentType = getContentType(file_);
  DEBUG_WM(F("File type: "));
  DEBUG_WM(contentType);
  String fileWithGz = file_ + ".gz";
  if(SPIFFS.exists(fileWithGz) || SPIFFS.exists(file_))
  {
    if(SPIFFS.exists(fileWithGz))
    {
      file_ += ".gz";
    }
    File file = SPIFFS.open(file_, "r");
    // tell browser what file type it received and what file name it should display
    // without this, browser will not know how to open the file
    server->sendHeader("Content-Disposition","attachment;filename=" + file_.substring(1));
    size_t sent = server->streamFile(file, contentType);
    file.close();
  }
}

void WiFiManager::handleUploadHeader()
{
  server->send(200, "text/plain","");
}

void WiFiManager::handleFileUpload()
{
  if(server->uri() != "/edit") 
  {
    return;
  }
  DEBUG_WM(F("Handle file upload..."));
  HTTPUpload& upload = server->upload();
  if(upload.status == UPLOAD_FILE_START)
  {
    String filename = upload.filename;
    if(!filename.startsWith("/")) 
    {
      filename = "/" + filename;
    }
    DEBUG_WM(F("Upload file: "));
	DEBUG_WM(filename);
    fsUploadFile = SPIFFS.open(filename, "w");
    filename = String();
  } 
  else if(upload.status == UPLOAD_FILE_WRITE)
  {
    DEBUG_WM(F("Uploading size: "));
	DEBUG_WM(upload.currentSize);
    if(fsUploadFile)
    {
      fsUploadFile.write(upload.buf, upload.currentSize);
    }
  } 
  else if(upload.status == UPLOAD_FILE_END)
  {
    if(fsUploadFile)
    {
      fsUploadFile.close();
    }
    DEBUG_WM(F("Total upload size: "));
	DEBUG_WM(upload.totalSize);
  }
}

void WiFiManager::handleFileDelete()
{
  if(server->args() == 0) 
  {
    return server->send(500, "text/plain", "BAD ARGS");
  }
  String path = server->arg(0);
  DEBUG_WM(F("Handle delete file: "));
  DEBUG_WM(path);
  if(path == "/")
  {
    return server->send(500, "text/plain", "BAD PATH");
  }
  if(!SPIFFS.exists(path))
  {
    return server->send(404, "text/plain", "FileNotFound");
  }
  SPIFFS.remove(path);
  server->send(200, "text/plain", "");
  path = String();
}

void WiFiManager::handleNotFound() {
  
  if (captivePortal()) { // If caprive portal redirect instead of displaying the error page.
      return;
  }
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server->uri();
  message += "\nMethod: ";
  message += ( server->method() == HTTP_GET ) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server->args();
  message += "\n";

  for ( uint8_t i = 0; i < server->args(); i++ ) {
    message += " " + server->argName ( i ) + ": " + server->arg ( i ) + "\n";
  }
  server->sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server->sendHeader("Pragma", "no-cache");
  server->sendHeader("Expires", "-1");
  server->send ( 404, "text/plain", message );
}

/** Redirect to captive portal if we got a request for another domain. Return true in
that case so the page handler do not try to handle the request again. */
boolean WiFiManager::captivePortal() {
  if (!isIp(server->hostHeader()) && server->hostHeader() != (String(myHostname))) {
    DEBUG_WM(F("Request redirected to captive portal"));
    server->sendHeader("Location", ("http://") +String(myHostname), true);
    server->setContentLength(0);
    server->send ( 302, "text/plain", ""); // Empty content inhibits Content-length header so we have to close the socket ourselves.
//    server->client().stop(); // Stop is needed because we sent no content length
    return true;
  }
  return false;
}

//start up config portal callback
void WiFiManager::setAPCallback( void (*func)(WiFiManager* myWiFiManager) ) {
  _apcallback = func;
}

//start up save config callback
void WiFiManager::setSaveConfigCallback( void (*func)(void) ) {
  _savecallback = func;
}

//sets a custom element to add to head, like a new style tag
void WiFiManager::setCustomHeadElement(const char* element) {
  _customHeadElement = element;
}

//if this is true, remove duplicated Access Points - defaut true
void WiFiManager::setRemoveDuplicateAPs(boolean removeDuplicates) {
  _removeDuplicateAPs = removeDuplicates;
}

//Scan for WiFiNetworks in range and sort by signal strength
//space for indices array allocated on the heap and should be freed when no longer required
int WiFiManager::scanWifiNetworks(int **indicesptr) {
    int n = WiFi.scanNetworks();
    DEBUG_WM(F("Scan done"));
    if (n == 0) {
      DEBUG_WM(F("No networks found"));
      return(0);
    } else {
	  // Allocate space off the heap for indices array.
	  // This space should be freed when no longer required.
 	  int* indices = (int *)malloc(n*sizeof(int));
					if (indices == NULL){
						DEBUG_WM(F("ERROR: Out of memory"));
						return(0);
						}
	  *indicesptr = indices;
      //sort networks
      for (int i = 0; i < n; i++) {
        indices[i] = i;
      }

      std::sort(indices, indices + n, [](const int & a, const int & b) -> bool
      {
        return WiFi.RSSI(a) > WiFi.RSSI(b);
      });
      // remove duplicates ( must be RSSI sorted )
      if(_removeDuplicateAPs) {
        String cssid;
        for (int i = 0; i < n; i++) {
          if(indices[i] == -1) continue;
          cssid = WiFi.SSID(indices[i]);
          for (int j = i + 1; j < n; j++) {
            if(cssid == WiFi.SSID(indices[j])){
              DEBUG_WM("DUP AP: " + WiFi.SSID(indices[j]));
              indices[j] = -1; // set dup aps to index -1
            }
          }
        }
      }

      for (int i = 0; i < n; i++) {
        if(indices[i] == -1) continue; // skip dups

        int quality = getRSSIasQuality(WiFi.RSSI(indices[i]));
        if (!(_minimumQuality == -1 || _minimumQuality < quality)) {
          indices[i] == -1;
          DEBUG_WM(F("Skipping due to quality"));
        }
      }

      return (n);
    }
}




template <typename Generic>
void WiFiManager::DEBUG_WM(Generic text) {
  if (_debug) {
    Serial.print("*WM: ");
    Serial.println(text);
  }
}

int WiFiManager::getRSSIasQuality(int RSSI) {
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

/** Is this an IP? */
boolean WiFiManager::isIp(String str) {
  for (int i = 0; i < str.length(); i++) {
    int c = str.charAt(i);
    if (c != '.' && (c < '0' || c > '9')) {
      return false;
    }
  }
  return true;
}

/** IP to String? */
String WiFiManager::toStringIP(IPAddress ip) {
  String res = "";
  for (int i = 0; i < 3; i++) {
    res += String((ip >> (8 * i)) & 0xFF) + ".";
  }
  res += String(((ip >> 8 * 3)) & 0xFF);
  return res;
}

// take ip string as input parameter and return IPAddress object
IPAddress WiFiManager::stringToIP(String ip_string)
{
   String temp;
   uint8_t a, b, c , d;
   a = atoi(ip_string.substring(0,ip_string.indexOf('.')).c_str());temp = ip_string.substring(ip_string.indexOf('.')+1);
   b = atoi(temp.substring(0,temp.indexOf('.')).c_str());temp = ip_string.substring(ip_string.indexOf('.')+1).substring(ip_string.indexOf('.')+1);
   c = atoi(temp.substring(0,temp.indexOf('.')).c_str());temp = ip_string.substring(ip_string.indexOf('.')+1).substring(ip_string.indexOf('.')+1);
   d = atoi(temp.substring(temp.indexOf('.')+1).c_str());
   return IPAddress(a, b, c, d);
}

//format bytes
String formatBytes(size_t bytes){
  if (bytes < 1024){
    return String(bytes)+ F("B");
  } else if(bytes < (1024 * 1024)){
    return String(bytes/1024.0)+ F("KB");
  } else if(bytes < (1024 * 1024 * 1024)){
    return String(bytes/1024.0/1024.0)+ F("MB");
  } else {
    return String(bytes/1024.0/1024.0/1024.0)+ F("GB");
  }
}

void WiFiManager::SPIFFS_List()
{
  SPIFFS.begin();
  Dir dir = SPIFFS.openDir(F("/"));
  DEBUG_WM(F("List of file(s) on SPIFFS"));
  while (dir.next()) {    
   String fileName = dir.fileName();
   size_t fileSize = dir.fileSize();
   DEBUG_WM(fileName.c_str());
   DEBUG_WM(formatBytes(fileSize).c_str());
  }
  DEBUG_WM(F(""));
}

//indicator led blinking if running in AP mode otherwise off
void WiFiManager::AP_Led_Indicator(bool ap_activated)
{
  if(ap_activated)
  {
    analogWrite(indicator, 3);
    delay(1000);
    analogWrite(indicator, 0);
    delay(1000);
  }
}

void WiFiManager::SPIFFS_Credentials(String input_ssid, String input_pass)
{
  SPIFFS.begin();
  String spiffs_ssid, spiffs_pass, line;
  File f = SPIFFS.open("/wifi.txt", "r");
  
  if (!f) {
    DEBUG_WM(F("File not existed. Creating file..."));
    File f = SPIFFS.open("/wifi.txt", "w+");
    if (!f) {
      DEBUG_WM(F("File creation failed"));
    }
    else
    {
      DEBUG_WM(F("File created."));
    }
  } 
  
  //file ready.
  //write to file if wifi credentials fed.
  if(input_ssid!="")
  {
    File f = SPIFFS.open("/wifi.txt", "w+");
    DEBUG_WM(F("\nWriting to file..."));
    f.print(input_ssid);
	f.print(";");
    f.print(input_pass);
	f.print(";");
  }
  else // read file if no wifi credentials fed.
  {  
    DEBUG_WM(F("\nReading file..."));
    while(f.available()) {
      line = f.readStringUntil('\n');
    }
	DEBUG_WM(F("Reading file: "));
	DEBUG_WM(f);
	DEBUG_WM(line);
    spiffs_ssid = line.substring(0,line.indexOf(";"));
    spiffs_pass = line.substring(line.indexOf(";") + 1);
    spiffs_pass = spiffs_pass.substring(0,spiffs_pass.indexOf(";"));
    DEBUG_WM(F("SSID: "));
	DEBUG_WM(spiffs_ssid);
    DEBUG_WM(F("Password: "));
	DEBUG_WM(spiffs_pass);
	if(spiffs_ssid!="")
	{
		ssid_ = spiffs_ssid;
		pass_ = spiffs_pass;
		// if true means credentials exist in SPIFFS, 
		// then load it and instruct server connect to network. 
		// If false means new credentials, then after connect successfull to network, save it to SPIFFS. 
		wifi_credentials_ok = true; 
	}
  }
  f.close();
  DEBUG_WM("Credentials check done.");
}

void WiFiManager::sendNTPpacket(IPAddress &address)
{
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}

time_t WiFiManager::getNTPTime()
{
  IPAddress ntpServerIP; // NTP server's ip address
  while (Udp.parsePacket() > 0) ; // discard any previously received packets
  DEBUG_WM(F("Transmit NTP Request"));
  // get a random server from the pool
  WiFi.hostByName(ntpServerName, ntpServerIP);
  DEBUG_WM(ntpServerName);
  DEBUG_WM(ntpServerIP);
  sendNTPpacket(ntpServerIP);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      DEBUG_WM(F("Receive NTP Response"));
      Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
  }
  DEBUG_WM(F("No NTP Response :-("));
  return 0; // return 0 if unable to get the time
}

void WiFiManager::ntpInnitialize()
{
  DEBUG_WM(F("Starting UDP"));
  Udp.begin(localPort);
  //DEBUG_WM(F("Local port: "));
  //DEBUG_WM(Udp.localPort());
  DEBUG_WM(F("waiting for sync"));
  setSyncProvider(getNTPTime());
  //setSyncProvider(getExternalTime(getNTPTime()));
  setSyncInterval(10000);
}

void WiFiManager::SPIFFS_IP_Configure(String static_ip)
{

  DEBUG_WM("SPIFFS ip configure start");
  SPIFFS.begin();
  File f = SPIFFS.open("/ip.txt", "r");
  if (!f) 
  {
	DEBUG_WM(F("IP text file not existed. Creating file..."));
	File f = SPIFFS.open("/ip.txt", "w+");
	if (!f) {
		DEBUG_WM(F("IP text file creation failed"));
	}
	else
	{
		DEBUG_WM(F("IP text file created."));
	}
  }
  //if static ip fed (static ip mode), write ip details to spiff file (ip.txt)
  if(static_ip!="")
  {
	DEBUG_WM(F("\nStatic ip mode"));	
	File f = SPIFFS.open("/ip.txt", "w+");
	//file ready. Start to write ip details
    DEBUG_WM(F("\nWriting ip details to file..."));
	DEBUG_WM("Static IP:");
	DEBUG_WM(static_ip);
    f.print(static_ip);
	f.print(";");
	DEBUG_WM("Subnet:");
	DEBUG_WM(WiFi.subnetMask());
	f.print(WiFi.subnetMask());
	f.print(";");
	DEBUG_WM("Gateway:");
	DEBUG_WM(WiFi.gatewayIP());
	f.print(WiFi.gatewayIP());
	f.print(";");
	f.close();
	is_Static_IP = true;
  }
  else// no static ip fed (dhcp mode chosen),clear ip details in spiff file (ip.txt)
  {
	DEBUG_WM(F("\nDHCP mode, clearing ip text file..."));
	SPIFFS_Clear_IP();
	is_Static_IP = false;
  }
  server->sendHeader("Connection", "close");
  server->sendHeader("Access-Control-Allow-Origin", "*");
  server->send(200, "text/plain","IP Configuration Done.");
  DEBUG_WM("SPIFFS ip configuration done.Reseting...");
  f.close();
  ESP.reset();
  delay(2000);
}

// if static ip found in spiffs, load and configure for esp module
// if no ip details found in spiffs, dhcp mode used.
void WiFiManager::SPIFFS_IP_Innitialize()
{
  SPIFFS.begin();
  File f = SPIFFS.open("/ip.txt", "r");
  
  if (!f) 
  {
	DEBUG_WM(F("IP text file not existed. Creating file..."));
	File f = SPIFFS.open("/ip.txt", "w+");
	if (!f) 
	{
		DEBUG_WM(F("IP text file creation failed"));
	}
	else
	{
		DEBUG_WM(F("IP text file created."));
	}
  }
  
  //file ready, start reading...
  String line = "";
  DEBUG_WM(F("\nReading file..."));
  while(f.available()) 
  {
    line = f.readStringUntil('\n');
  }
  //DHCP mode
  if(line == "")
  {
	DEBUG_WM(F("No static ip found, activate dhcp mode."));
	is_Static_IP = false;
  }
  else// Static ip mode
  {
	DEBUG_WM(F("Static ip found, activate static ip mode"));
	is_Static_IP = true;
	ip_info.static_ip = line.substring(0,line.indexOf(";"));
	ip_info.netmask = line.substring(line.indexOf(";") + 1);
	ip_info.netmask = ip_info.netmask.substring(0,ip_info.netmask.indexOf(";"));
	ip_info.gateway = line.substring(line.indexOf(";")+1);
	ip_info.gateway = ip_info.gateway.substring(ip_info.gateway.indexOf(";")+1);
	ip_info.gateway = ip_info.gateway.substring(0,ip_info.gateway.indexOf(";"));
	DEBUG_WM(F("Static IP: "));
	DEBUG_WM(ip_info.static_ip);
    DEBUG_WM(F("Netmask: "));
	DEBUG_WM(ip_info.netmask);
	DEBUG_WM(F("Gateway: "));
	DEBUG_WM(ip_info.gateway);
  }
  f.close();
  DEBUG_WM("IP innitialized.");
}

void WiFiManager::SPIFFS_Clear_IP()
{
	SPIFFS.begin();
	File f = SPIFFS.open("/ip.txt", "w+");  
	f.print("");
	DEBUG_WM("ip.txt contents cleared.");
	f.close();
}
