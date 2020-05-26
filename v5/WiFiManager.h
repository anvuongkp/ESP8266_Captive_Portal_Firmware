/**************************************************************
   WiFiManager is a library for the ESP8266/Arduino platform
   (https://github.com/esp8266/Arduino) to enable easy
   configuration and reconfiguration of WiFi credentials using a Captive Portal
   Original version from tzapu: https://github.com/tzapu/WiFiManager
   This version is modified by An VUONG from Ken Taylor's version: https://github.com/kentaylor
   Licensed under MIT license
 **************************************************************/

#ifndef WiFiManager_h
#define WiFiManager_h
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <FS.h>
#include <DNSServer.h>
#include <Time.h>
#include <WiFiUdp.h>
#include <memory>
#undef min
#undef max
#include <algorithm>
extern "C" {
  #include "user_interface.h"
}

#define WFM_LABEL_BEFORE 1
#define WFM_LABEL_AFTER 2
#define WFM_NO_LABEL 0

const char HTTP_200[] PROGMEM             = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n";
const char HTTP_HEAD[] PROGMEM            = "<!DOCTYPE html><html lang=\"en\"><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1, user-scalable=no\"/><title>{v}</title>";
const char HTTP_STYLE[] PROGMEM           = "<style>body,textarea,input,select{background: 0;border-radius: 0;font: 16px sans-serif;margin: 0}textarea,input,select{outline: 0;font-size: 14px;border: 1px solid #ccc;padding: 8px;width: 90%}.btn a{text-decoration: none}.container{margin: auto;width: 90%}@media(min-width:1200px){.container{margin: auto;width: 30%}}@media(min-width:768px) and (max-width:1200px){.container{margin: auto;width: 50%}}.btn,h2{font-size: 2em}h1{font-size: 3em}.btn{background: #286090;border-radius: 4px;border: 0;color: #fff;cursor: pointer;display: inline-block;margin: 2px 0;padding: 10px 14px 11px;width: 100%}.btn:hover{background: #337AB7}.btn:active,.btn:focus{background: #2E6DA4}label>*{display: inline}form>*{display: block;margin-bottom: 10px}textarea:focus,input:focus,select:focus{border-color: #5ab}.msg{background: #def;border-left: 5px solid #59d;padding: 1.5em}.q{float: right;width: 64px;text-align: right}.l{background: url('data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAMAAABEpIrGAAAALVBMVEX///8EBwfBwsLw8PAzNjaCg4NTVVUjJiZDRUUUFxdiZGSho6OSk5Pg4eFydHTCjaf3AAAAZElEQVQ4je2NSw7AIAhEBamKn97/uMXEGBvozkWb9C2Zx4xzWykBhFAeYp9gkLyZE0zIMno9n4g19hmdY39scwqVkOXaxph0ZCXQcqxSpgQpONa59wkRDOL93eAXvimwlbPbwwVAegLS1HGfZAAAAABJRU5ErkJggg==') no-repeat left center;background-size: 1em}input[type='checkbox']{float: left;width: 20px}.table td{padding:.5em;text-align:left}.table tbody>:nth-child(2n-1){background:#ddd}</style>";
const char HTTP_CUSTOM_STYLE[] PROGMEM	  = "<style>gpio_table {border-collapse: collapse;width: 100%;}th, td {text-align: left;padding: 8px;}tr:nth-child(even){background-color: #f2f2f2}th {background-color: #286090;color: white;}input[type=\"text\"]{border: 1px solid #cccccc;}ul.breadcrumb{padding: 10px 16px;list-style: none;background-color: #eee;font-size: 17px;}ul.breadcrumb li {display: inline;}ul.breadcrumb li+li:before {padding: 8px;color: black;content: ' / ';} ul.breadcrumb li a {color: #0275d8;text-decoration: none;}ul.breadcrumb li a:hover {color: #01447e;text-decoration: underline;}</style>";
const char HTTP_SCRIPT[] PROGMEM          = "<script>function c(l){document.getElementById('s').value=l.innerText||l.textContent;document.getElementById('p').focus();}</script>";
const char HTTP_HEAD_END[] PROGMEM        = "</head><body><div class=\"container\">";
const char HTTP_PORTAL_OPTIONS[] PROGMEM  = "<form action=\"/wifi_configuration\" method=\"get\"><button class=\"btn\">WiFi Configuration</button></form><br/><form action=\"/ip_configuration\" method=\"get\"><button class=\"btn\">IP Configuration</button></form><br/><form action=\"/gpio_control\" method=\"get\"><button class=\"btn\">WiFi Switches Control</button></form><br/><form action=\"/firmware_update\" method=\"get\"><button class=\"btn\">Firmware Update</button></form><br/><form action=\"/editor_page\" method=\"get\"><button class=\"btn\">System File Editor</button></form><br/><form action=\"/extra_functions\" method=\"get\"><button class=\"btn\">Extra Funtions</button></form><br/>";
const char HTTP_ITEM[] PROGMEM            = "<div><a href=\"#p\" onclick=\"c(this)\">{v}</a>&nbsp;<span class=\"q {i}\">{r}%</span></div>";
const char JSON_ITEM[] PROGMEM            = "{\"SSID\":\"{v}\", \"Encryption\":{i}, \"Quality\":\"{r}\"}";
const char HTTP_FORM_START[] PROGMEM      = "<form method=\"get\" action=\"wifi_save\"><label>SSID</label><input id=\"s\" name=\"s\" length=32 placeholder=\"SSID\"><label>Password</label><input id=\"p\" name=\"p\" length=64 placeholder=\"password\">";
const char HTTP_FORM_LABEL[] PROGMEM      = "<label for=\"{i}\">{p}</label>";
const char HTTP_FORM_PARAM[] PROGMEM      = "<input id=\"{i}\" name=\"{n}\" length={l} placeholder=\"{p}\" value=\"{v}\" {c}>";
const char HTTP_FORM_END[] PROGMEM        = "<button class=\"btn\" type=\"submit\">save</button></form>";
const char HTTP_SAVED[] PROGMEM           = "<div class=\"msg\"><strong>Credentials Saved</strong><br>Trying to connect ESP to the {x} network.<br>Give it 10 seconds or so and check <a href=\"/\">how it went.</a> <p/>The {v} network you are connected to will be restarted on the radio channel of the {x} network. You may have to manually reconnect to the {v} network.</div>";
const char HTTP_END[] PROGMEM             = "</div></body></html>";

#define WIFI_MANAGER_MAX_PARAMS 10

class WiFiManagerParameter {
  public:
    WiFiManagerParameter(const char *custom);
    WiFiManagerParameter(const char *id, const char *placeholder, const char *defaultValue, int length);
    WiFiManagerParameter(const char *id, const char *placeholder, const char *defaultValue, int length, const char *custom);
    WiFiManagerParameter(const char *id, const char *placeholder, const char *defaultValue, int length, const char *custom, int labelPlacement);

    const char *getID();
    const char *getValue();
    const char *getPlaceholder();
    int         getValueLength();
    int         getLabelPlacement();
    const char *getCustomHTML();
  private:
    const char *_id;
    const char *_placeholder;
    char       *_value;
    int         _length;
	int         _labelPlacement;
    const char *_customHTML;

    void init(const char *id, const char *placeholder, const char *defaultValue, int length, const char *custom, int labelPlacement);

    friend class WiFiManager;
};


class WiFiManager
{
  public:
    WiFiManager();
    ~WiFiManager();

    boolean       autoConnect(); //Deprecated. Do not use.
    boolean       autoConnect(char const *apName, char const *apPassword = NULL); //Deprecated. Do not use.

    //if you want to start the config portal
    boolean       startConfigPortal();
    boolean       startConfigPortal(char const *apName, char const *apPassword = NULL);

    // get the AP name of the config portal, so it can be used in the callback
    String        getConfigPortalSSID();

    void          resetSettings();

    //sets timeout before webserver loop ends and exits even if there has been no setup.
    //usefully for devices that failed to connect at some point and got stuck in a webserver loop
    //in seconds setConfigPortalTimeout is a new name for setTimeout
    void          setConfigPortalTimeout(unsigned long seconds);
    void          setTimeout(unsigned long seconds);

    //sets timeout for which to attempt connecting, usefull if you get a lot of failed connects
    void          setConnectTimeout(unsigned long seconds);


    void          setDebugOutput(boolean debug);
    //defaults to not showing anything under 8% signal quality if called
    void          setMinimumSignalQuality(int quality = 8);
    //sets a custom ip /gateway /subnet configuration
    void          setAPStaticIPConfig(IPAddress ip, IPAddress gw, IPAddress sn);
    //sets config for a static IP
    void          setSTAStaticIPConfig(IPAddress ip, IPAddress gw, IPAddress sn);
    //called when AP mode and config portal is started
    void          setAPCallback( void (*func)(WiFiManager*) );
    //called when settings have been changed and connection was successful
    void          setSaveConfigCallback( void (*func)(void) );
    //adds a custom parameter
    void          addParameter(WiFiManagerParameter *p);
    //if this is set, it will exit after config, even if connection is unsucessful.
    void          setBreakAfterConfig(boolean shouldBreak);
    //if this is set, try WPS setup when starting (this will delay config portal for up to 2 mins)
    //TODO
    //if this is set, customise style
    void          setCustomHeadElement(const char* element);
    //if this is true, remove duplicated Access Points - defaut true
    void          setRemoveDuplicateAPs(boolean removeDuplicates);
    //Scan for WiFiNetworks in range and sort by signal strength
    //space for indices array allocated on the heap and should be freed when no longer required
    int           scanWifiNetworks(int **indicesptr);

  private:
    std::unique_ptr<DNSServer>        dnsServer;
    std::unique_ptr<ESP8266WebServer> server;

    //const int     WM_DONE                 = 0;
    //const int     WM_WAIT                 = 10;

    //const String  HTTP_HEAD = "<!DOCTYPE html><html lang=\"en\"><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"/><title>{v}</title>";

    void          setupConfigPortal();
    void          startWPS();
    const char*         getStatus(int status);

    const char*   _apName                 = "Captive-Portal-V5.0";
    const char*   _apPassword             = NULL;
    String        _ssid                   = "";
    String        _pass                   = "";
    unsigned long _configPortalTimeout    = 0;
    unsigned long _connectTimeout         = 0;
    unsigned long _configPortalStart      = 0;
    /* hostname for mDNS. Set to a valid internet address so that user
    will see an information page if they are connected to the wrong network */
	const char *myHostname = "iot8701.16mb.com/home-automation.html";
	int numberOfNetworks;
	int *networkIndices;
    int **networkIndicesptr = &networkIndices;

    IPAddress     _ap_static_ip;
    IPAddress     _ap_static_gw;
    IPAddress     _ap_static_sn;
    IPAddress     _sta_static_ip;
    IPAddress     _sta_static_gw;
    IPAddress     _sta_static_sn;

    int           _paramsCount            = 0;
    int           _minimumQuality         = -1;
    boolean       _removeDuplicateAPs     = true;
    boolean       _shouldBreakAfterConfig = false;
    boolean       _tryWPS                 = false;

    const char*   _customHeadElement      = "";
    //String        getEEPROMString(int start, int len);
    //void          setEEPROMString(int start, int len, String string);

    int           status = WL_IDLE_STATUS;
    int           connectWifi(String ssid, String pass);
    uint8_t       waitForConnectResult();

    void          handleRoot();
    void          handleWifi();
    void          handleWifiSave();
    void          handleServerClose();
    void          handleInfo();
    void          handleState();
    void          handleScan();
    void          handleReset();
	void 		  handleGPIOControl();
	void		  handleGPIOToggle();
	void		  handleGPIOStatus();
	void		  handleFirmwareUpdatePage();
	void		  handleUpdateHeader();
	void		  handleUpdate();
	void		  handleTime();
	void		  handleRestart();
	void		  handleIPConfigurationPage();
	void		  handleIPStatus();
	void		  handleIPChange();
	void 		  handleEditorPage();
	void 		  handleFileCreate();
	void		  handleFileList();
	void		  handleFileRead();
	void		  handleFileDownload();
	void		  handleUploadHeader();
	void 		  handleFileUpload();
	void 		  handleFileDelete();
    void          handleNotFound();
    boolean       captivePortal();
    void          reportStatus(String &page);
	
    // DNS server
    const byte    DNS_PORT = 53;

	// SPIFFS
	void SPIFFS_List();
	String getContentType(String filename);
	void SPIFFS_Credentials(String input_ssid="", String input_pass="");
	bool wifi_credentials_ok = false;
	String ssid_;
	String pass_;
	
	// SPIFFS Editor
	File fsUploadFile;
	
	//NTP Synchronization
	const char* ntpServerName = "asia.pool.ntp.org";
	const int timeZone = +7;  // (GMT+07:00) Bangkok, Hanoi, Jakarta
	WiFiUDP Udp;
	unsigned int localPort = 8888;  // local port to listen for UDP packets
	static const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
	byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets
	void ntpInnitialize();
	void sendNTPpacket(IPAddress &address);
	time_t getNTPTime();
	
	// IP Configuration
	void SPIFFS_IP_Configure(String static_ip);
	void SPIFFS_IP_Innitialize();
	void SPIFFS_Clear_IP();
	bool is_Static_IP = false;
	
    //helpers
    int           getRSSIasQuality(int RSSI);
    boolean       isIp(String str);
    String        toStringIP(IPAddress ip);
	IPAddress	  stringToIP(String ip_string);

    boolean       connect;
    boolean       stopConfigPortal = false;
    boolean       _debug = true;
	
	//indicator on when running in ap mode
	const byte indicator = 4;
	bool ap_mode = false;
	void AP_Led_Indicator(bool activate);
	
	//GPIO Struct
	//dont use gpio4 (indicator purpose) and gpio13(factory reset purpose)
	struct GPIOP{
		String status = "Off";
		String alias ="";
	} gpio0, gpio2, gpio5, gpio12, gpio14, gpio15, gpio16;
	
	// IP details struct. loaded from spiffs file (ip.txt) when module started.
	// empty string when module run in dhcp otherwise static mode.
	struct IP_INFO{
		String static_ip = "";
		String netmask = "";
		String gateway = "";
	} ip_info;
	
    void (*_apcallback)(WiFiManager*) = NULL;
    void (*_savecallback)(void) = NULL;

    WiFiManagerParameter* _params[WIFI_MANAGER_MAX_PARAMS];

    template <typename Generic>
    void          DEBUG_WM(Generic text);

    template <class T>
    auto optionalIPFromString(T *obj, const char *s) -> decltype(  obj->fromString(s)  ) {
      return  obj->fromString(s);
    }
    auto optionalIPFromString(...) -> bool {
      DEBUG_WM("NO fromString METHOD ON IPAddress, you need ESP8266 core 2.1.0 or newer for Custom IP configuration to work.");
      return false;
    }
};
#endif
