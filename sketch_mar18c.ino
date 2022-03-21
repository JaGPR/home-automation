#include "EEPROM.h"
#include "HTML.h"

#include <WebServer.h>
//#define ENABLE_DEBUG
#ifdef ENABLE_DEBUG
#define DEBUG_ESP_PORT Serial
#define NODEBUG_WEBSOCKETS
#define NDEBUG
#endif

WebServer server(80);

#include "WiFiManager.h"
#include <Arduino.h>
#include <WiFi.h>
#include "SinricPro.h"
#include "SinricProSwitch.h"

#include <map>


#define APP_KEY           "3481b742-aa0f-43ee-9f0d-f9502626609a"      // Should look like "de0bxxxx-1x3x-4x3x-ax2x-5dabxxxxxxxx"
#define APP_SECRET        "f7fa6df3-419b-4e54-824c-7ab8fe526ed1-19b96feb-32e8-4b91-9fc6-43458bcb4349"   // Should look like "5f36xxxx-x3x7-4x3x-xexe-e86724a9xxxx-4c4axxxx-3x3x-x5xe-x9x3-333d65xxxxxx"
#define device_ID_1       "6234f67f753dc5aab493a2d1"
#define device_ID_2       "6234f6dcd0fd258c52fb0968"


// define the GPIO connected with Relays and switches
#define RelayPin1 23  //D23
#define RelayPin2 22  //D22


#define SwitchPin1 13  //D13
#define SwitchPin2 12  //D12


#define wifiLed   2   //D2

// comment the following line if you use a toggle switches instead of tactile buttons
//#define TACTILE_BUTTON 1

#define BAUD_RATE   9600

#define DEBOUNCE_TIME 250

typedef struct {      // struct for the std::map below
  int relayPIN;
  int flipSwitchPIN;
} deviceConfig_t;


// this is the main configuration
// please put in your deviceId, the PIN for Relay and PIN for flipSwitch
// this can be up to N devices...depending on how much pin's available on your device ;)
// right now we have 4 devicesIds going to 4 relays and 4 flip switches to switch the relay manually
std::map<String, deviceConfig_t> devices = {
  //{deviceId, {relayPIN,  flipSwitchPIN}}
  {device_ID_1, {  RelayPin1, SwitchPin1 }},
  {device_ID_2, {  RelayPin2, SwitchPin2 }}
};

typedef struct {      // struct for the std::map below
  String deviceId;
  bool lastFlipSwitchState;
  unsigned long lastFlipSwitchChange;
} flipSwitchConfig_t;

std::map<int, flipSwitchConfig_t> flipSwitches;    // this map is used to map flipSwitch PINs to deviceId and handling debounce and last flipSwitch state checks
// it will be setup in "setupFlipSwitches" function, using informations from devices map

void setupRelays() {
  for (auto &device : devices) {           // for each device (relay, flipSwitch combination)
    int relayPIN = device.second.relayPIN; // get the relay pin
    pinMode(relayPIN, OUTPUT);             // set relay pin to OUTPUT
    digitalWrite(relayPIN, HIGH);
  }
}

void setupFlipSwitches() {
  for (auto &device : devices)  {                     // for each device (relay / flipSwitch combination)
    flipSwitchConfig_t flipSwitchConfig;              // create a new flipSwitch configuration

    flipSwitchConfig.deviceId = device.first;         // set the deviceId
    flipSwitchConfig.lastFlipSwitchChange = 0;        // set debounce time
    flipSwitchConfig.lastFlipSwitchState = true;     // set lastFlipSwitchState to false (LOW)--

    int flipSwitchPIN = device.second.flipSwitchPIN;  // get the flipSwitchPIN

    flipSwitches[flipSwitchPIN] = flipSwitchConfig;   // save the flipSwitch config to flipSwitches map
    pinMode(flipSwitchPIN, INPUT_PULLUP);                   // set the flipSwitch pin to INPUT
  }
}

bool onPowerState(String deviceId, bool &state)
{
  Serial.printf("%s: %s\r\n", deviceId.c_str(), state ? "on" : "off");
  int relayPIN = devices[deviceId].relayPIN; // get the relay pin for corresponding device
  digitalWrite(relayPIN, !state);             // set the new relay state
  return true;
}

void handleFlipSwitches() {
  unsigned long actualMillis = millis();                                          // get actual millis
  for (auto &flipSwitch : flipSwitches) {                                         // for each flipSwitch in flipSwitches map
    unsigned long lastFlipSwitchChange = flipSwitch.second.lastFlipSwitchChange;  // get the timestamp when flipSwitch was pressed last time (used to debounce / limit events)

    if (actualMillis - lastFlipSwitchChange > DEBOUNCE_TIME) {                    // if time is > debounce time...

      int flipSwitchPIN = flipSwitch.first;                                       // get the flipSwitch pin from configuration
      bool lastFlipSwitchState = flipSwitch.second.lastFlipSwitchState;           // get the lastFlipSwitchState
      bool flipSwitchState = digitalRead(flipSwitchPIN);                          // read the current flipSwitch state
      if (flipSwitchState != lastFlipSwitchState) {                               // if the flipSwitchState has changed...
#ifdef TACTILE_BUTTON
        if (flipSwitchState) {                                                    // if the tactile button is pressed
#endif
          flipSwitch.second.lastFlipSwitchChange = actualMillis;                  // update lastFlipSwitchChange time
          String deviceId = flipSwitch.second.deviceId;                           // get the deviceId from config
          int relayPIN = devices[deviceId].relayPIN;                              // get the relayPIN from config
          bool newRelayState = !digitalRead(relayPIN);                            // set the new relay State
          digitalWrite(relayPIN, newRelayState);                                  // set the trelay to the new state

          SinricProSwitch &mySwitch = SinricPro[deviceId];                        // get Switch device from SinricPro
          mySwitch.sendPowerStateEvent(!newRelayState);                            // send the event
#ifdef TACTILE_BUTTON
        }
#endif
        flipSwitch.second.lastFlipSwitchState = flipSwitchState;                  // update lastFlipSwitchState
      }
    }
  }
}
void setupWiFi()
{
  Serial.printf("\r\n[Wifi]: Connecting");
  String ssid, pass;
  ssid = EEPROM.readString(100);
  pass = EEPROM.readString(200);
  Serial.print(pass.c_str());
  Serial.print(ssid.c_str());

  char s[67];
  char p[67];
  ssid.toCharArray(s, 67);
  pass.toCharArray(p, 67);
  WiFi.begin(s, p);
  delay(10000);
  if ((WiFi.status() == WL_CONNECTED))
  {
    digitalWrite(wifiLed, HIGH);
    Serial.printf("connected!\r\n[WiFi]: IP-Address isṀ %s\r\n", WiFi.localIP().toString().c_str());
  }
}

void setupSinricPro()
{
  for (auto &device : devices)
  {
    const char *deviceId = device.first.c_str();
    SinricProSwitch &mySwitch = SinricPro[deviceId];
    mySwitch.onPowerState(onPowerState);
  }

  SinricPro.begin(APP_KEY, APP_SECRET);
  SinricPro.restoreDeviceStates(true);
}

void setup() {
  EEPROM.begin(400);
  Serial.begin(115200);
  if (!CheckWIFICreds()) {
    Serial.println("No WIFI credentials stored in memory. Loading form...");
    digitalWrite(2, LOW);
    while (loadWIFICredsForm());
  }
  pinMode(wifiLed, OUTPUT);
  digitalWrite(wifiLed, LOW);
  setupWiFi();
  setupFlipSwitches();
  setupRelays();
  setupSinricPro();
  pinMode(2, OUTPUT);
  pinMode(34,INPUT);

}
void loop()
{
int buttonstate=0;
buttonstate=digitalRead(34);
  if ( buttonstate == HIGH || (WiFi.status() != WL_CONNECTED))
  {
    Serial.println("WIPING THE CREDENTIALS");
    digitalWrite(2, LOW);
    
    wipeEEPROM();
    loadWIFICredsForm();
  }
  SinricPro.handle();
  handleFlipSwitches();
}

void wipeEEPROM() {
  for (int i = 0; i < 400; i++) {
    EEPROM.writeByte(i, 0);
  }
  EEPROM.commit();
}
