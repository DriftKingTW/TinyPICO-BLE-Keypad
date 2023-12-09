#include <Arduino.h>
#include <ArduinoJson.h>
#include <BleKeyboard.h>
#include <EEPROM.h>
#include <ESP32Encoder.h>
#include <ESPmDNS.h>
#include <FastLED.h>
#include <ImprovWiFiLibrary.h>
#include <PCF8574.h>
#include <SPIFFS.h>

#include "USB.h"
#include "USBHIDKeyboard.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include <U8g2lib.h>
#include <WebServer.h>
#include <WiFi.h>
#include <Wire.h>
#include <driver/rtc_io.h>

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

#define BLE_NAME "Schnell Keypad"
#define AUTHOR "DriftKingTW"

#define ACTIVE LOW
#define WAKEUP_KEY_BITMAP 0x1000  // Pin 12

#define AP_SSID "Schnell Keypad WLAN"
#define MDNS_NAME "Schnell"

#define ROWS 5
#define COLS 7

#define SCL 15
#define SDA 16

#define VOLTAGE_DIVIDER_RATIO 2.0  // For 100K/100K divider
#define V_REF 3.3                  // Reference voltage for ADC
#define BATT_PIN 6

#define BD_SW_CW 47
#define BD_SW_CCW 48
#define BD_SW_PUSH 21

#define EC_PIN_A 45
#define EC_PIN_B 46

#define LED_PIN_DIN 38
#define NUM_LEDS 1

#define CFG_BTN_PIN_1 2
#define CFG_BTN_PIN_2 1

// ====== Extension Board Pin Definition ======

// Rotary Encoder Extension Board
#define encoderPinA P5
#define encoderPinB P4
#define encoderSW P6
#define extensionBtn1 P0
#define extensionBtn2 P1
#define extensionBtn3 P2
#define ENCODER_EXTENSION_ADDR 0x38

// ====== End Extension Board Pin Definition ======

struct Key {
    uint8_t keyStroke;
    bool state;
    String keyInfo;
};

struct RotaryEncoderConfig {
    uint8_t rotaryButton;
    String rotaryButtonInfo;
    bool rotaryButtonState;
    uint8_t rotaryCW;
    uint8_t rotaryCCW;
    String rotaryCWInfo;
    String rotaryCCWInfo;
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
void ICACHE_RAM_ATTR encoderTask(void *);
void ICACHE_RAM_ATTR encoderExtBoardTask(void *);
void i2cTask(void *);

// Keyboard
void initKeys();
void initMacros();
void updateKeymaps();
void keyPress(Key &key);
bool keyPress(uint8_t keyStroke, String keyInfo, bool keyState);
void keyRelease(Key &key);
bool keyRelease(uint8_t keyStroke, String keyInfo, bool keyState);
void macroPress(Macro &macro);
void tapToggleActive(size_t index);
void tapToggleRelease(size_t orginalLayerIndex);
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
void setCPUFrequency(int freq);
bool getUSBPowerState();

// File Management
String loadJSONFileAsString(String filename);

// Web Server
void initWebServer();
void handleRoot();
void handleNotFound();
void sendCrossOriginHeader();
bool handleFileRead(String);
String getContentType(String);