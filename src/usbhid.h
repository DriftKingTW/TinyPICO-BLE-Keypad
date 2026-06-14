#pragma once

#include <Arduino.h>

// Thin wrapper around USBHIDKeyboard, kept in its own translation unit so that
// USBHIDKeyboard.h (TinyUSB) and BleKeyboard.h (NimBLE) are never included into
// the same file. Their headers define conflicting symbols -- e.g. KEY_LEFT_CTRL
// (a #define in one, a const in the other) and HID_SUBCLASS_NONE -- which makes
// them impossible to compile together.
namespace UsbHid {
void begin();
void press(uint8_t keyStroke);
void release(uint8_t keyStroke);
void pressRaw(uint8_t keyStroke);
void releaseRaw(uint8_t keyStroke);
void write(uint8_t keyStroke);
void releaseAll();
void print(const String &text);
void println(const String &text);
}  // namespace UsbHid
