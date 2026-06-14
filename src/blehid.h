#pragma once

#include <Arduino.h>

// Thin wrapper around BleKeyboard, kept in its own translation unit so that
// BleKeyboard.h (NimBLE) and USBHIDKeyboard.h (TinyUSB) are never included into
// the same file -- their headers define conflicting symbols (KEY_*,
// HID_SUBCLASS_*) and cannot be compiled together. Mirrors src/usbhid.h.
namespace BleHid {
void begin();
void press(uint8_t keyStroke);
void release(uint8_t keyStroke);
void write(uint8_t keyStroke);
void releaseAll();
void print(const String &text);
void println(const String &text);
bool isConnected();
void setBatteryLevel(uint8_t level);
}  // namespace BleHid
