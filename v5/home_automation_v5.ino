//Version 5.0
// System File Editor (SPIFFS Editor) Added (21 July 2017 @14:29)
// Interface Renovation Added (21 July 2017 @14:29)

///######################################################################### Libraries Setup ################################################################################################################
#include <WiFiManager.h>
///######################################################################### Factory Reset Setup ############################################################################################################
const byte reset_pin = 13;
unsigned int factory_reset_interval = 2000;
bool reset_activated = false;
unsigned long pressed_time;
unsigned long released_time;
bool button_down = true;
bool button_up = false;
///######################################################################### Global Variables ################################################################################################################
const byte indicator = 4;
const char* ap_ssid = "Captive-Portal-V5.0"; // ssid esp8266 uses when first time start
const char* ap_password = "password"; //password to login when esp8266 start in network configuration mode
///####################################################################### Functions #########################################################################################################################
void GPIO_Innitialize(){pinMode(5, OUTPUT);pinMode(12, OUTPUT);pinMode(14, OUTPUT);pinMode(indicator, INPUT);  digitalWrite(5, LOW);digitalWrite(12, LOW);digitalWrite(14, LOW);digitalWrite(indicator, LOW);}
void SPIFFS_Clear_Credentials(){SPIFFS.begin();File f = SPIFFS.open("/wifi.txt", "w+");  f.print("");f.close();}
void Reset_Handle(bool activated)
{
  if(activated)
  {
    if(button_down && button_up)
    {
      unsigned long interval = released_time - pressed_time;
      if(interval >= factory_reset_interval)
      {
        detachInterrupt(digitalPinToInterrupt(reset_pin));              
        SPIFFS_Clear_Credentials();//Factory reset and wipe out wifi credentials
        WiFi.disconnect(true); // Wipe out WiFi credentials.
        ESP.restart();
        delay(2000);
      }
      else
      {
        ESP.reset();// soft reset module, reset still leave registers in old state, restart is clean way.
        delay(2000);}
      button_down = true;button_up = false;reset_activated = false;
    }
  }
}
void Reset_Activated()
{
  noInterrupts();
  if(button_up)
  {
    released_time = millis();
    button_down = true;
    reset_activated = true;
    Reset_Handle(reset_activated);
  }
  else if(button_down)
  {
    pressed_time = millis();
    button_up = true;
    button_down = false;
  } 
  interrupts();
}
void configModeCallback (WiFiManager *myWiFiManager) {attachInterrupt(digitalPinToInterrupt(reset_pin), Reset_Activated, CHANGE);}
///########################################################################## Setup ##########################################################################################################################
void setup() {
  Serial.begin(115200);
  GPIO_Innitialize();
  WiFiManager wifi_manager;  
  wifi_manager.setAPCallback(configModeCallback);  
  wifi_manager.startConfigPortal(ap_ssid, ap_password);
}
///###################################################################### Loop ###############################################################################################################################
void loop() {}
