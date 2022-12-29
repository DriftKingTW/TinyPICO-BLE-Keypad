#include <Arduino.h>
#include <ArduinoJson.h>
#include <BleKeyboard.h>
#include <EEPROM.h>
#include <ESPmDNS.h>
#include <SPIFFS.h>
#include <TinyPICO.h>
#include <U8g2lib.h>
#include <WebServer.h>
#include <WiFi.h>

#include <algorithm>
#include <animation.hpp>
#include <cstring>
#include <fstream>
#include <helper.hpp>
#include <iterator>
#include <string>

using namespace std;

#define BAUD_RATE 115200
#define EEPROM_SIZE 1
#define EEPROM_ADDR_LAYOUT 0

#define BLE_NAME "TinyPICO BLE"
#define AUTHOR "DriftKingTW"

#define ACTIVE LOW
#define WAKEUP_KEY_BITMAP 0x8000

#define AP_SSID "TinyPICO Keypad WLAN"
#define MDNS_NAME "tp-keypad"

#define ROWS 5
#define COLS 7

struct Key {
    uint8_t keyStroke;
    bool state;
    String keyInfo;
};

struct Macro {
    unsigned short type;
    // 6 key roll over using BLE keyboard
    uint8_t keyStrokes[6];
    String macroInfo;
    String stringContent;
};

// Tasks
void ledTask(void *);
void generalTask(void *);
void networkTask(void *);
void screenTask(void *);

// Keyboard
void initKeys();
void initMacros();
void updateKeymaps();
void keyPress(Key &key);
void keyRelease(Key &key);
void macroPress(Macro &macro);
void switchLayout();
void switchLayout(int layoutIndex);
int findLayoutIndex(String layoutName);
void switchDevice();

// OLED Control
void renderScreen();

// Power Management
void switchBootMode();
void checkBattery();
void checkIdle();
void resetIdle();
void goSleeping();
int getBatteryPercentage();
void breathLEDAnimation();
void showLowBatteryWarning();
void setCPUFrequency(int freq);

// File Management
String loadJSONFileAsString(String filename);

// Web Server
void initWebServer();
void handleRoot();
void handleNotFound();
void sendCrossOriginHeader();
bool handleFileRead(String);
String getContentType(String);