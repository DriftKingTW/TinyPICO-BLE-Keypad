#include "blehid.h"

#include <BleKeyboard.h>

namespace {
const char *kDeviceName = "Schnell Keypad";
const char *kManufacturer = "DriftKingTW";
BleKeyboard bleKeyboard(kDeviceName, kManufacturer);
}  // namespace

namespace BleHid {

void begin() { bleKeyboard.begin(); }

void press(uint8_t keyStroke) { bleKeyboard.press(keyStroke); }

void release(uint8_t keyStroke) { bleKeyboard.release(keyStroke); }

void write(uint8_t keyStroke) { bleKeyboard.write(keyStroke); }

void releaseAll() { bleKeyboard.releaseAll(); }

void print(const String &text) { bleKeyboard.print(text); }

void println(const String &text) { bleKeyboard.println(text); }

bool isConnected() { return bleKeyboard.isConnected(); }

void setBatteryLevel(uint8_t level) { bleKeyboard.setBatteryLevel(level); }

}  // namespace BleHid
