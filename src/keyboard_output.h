#pragma once

#include <Arduino.h>

#include "blehid.h"
#include "usbhid.h"

// Common interface over the two HID transports so the rest of the firmware can
// emit key events without branching on USB vs BLE mode. The concrete classes
// only delegate to the UsbHid / BleHid wrappers, so neither HID library header
// leaks into callers.
class KeyboardOutput {
   public:
    virtual ~KeyboardOutput() {}
    virtual void press(uint8_t keyStroke) = 0;
    virtual void release(uint8_t keyStroke) = 0;
    virtual void write(uint8_t keyStroke) = 0;
    virtual void releaseAll() = 0;
    virtual void print(const String &text) = 0;
    virtual void println(const String &text) = 0;
};

class UsbKeyboardOutput : public KeyboardOutput {
   public:
    // press/release remap modifier key codes to raw HID usages (see .cpp).
    void press(uint8_t keyStroke) override;
    void release(uint8_t keyStroke) override;
    void write(uint8_t keyStroke) override { UsbHid::write(keyStroke); }
    void releaseAll() override { UsbHid::releaseAll(); }
    void print(const String &text) override { UsbHid::print(text); }
    void println(const String &text) override { UsbHid::println(text); }
};

class BleKeyboardOutput : public KeyboardOutput {
   public:
    void press(uint8_t keyStroke) override { BleHid::press(keyStroke); }
    void release(uint8_t keyStroke) override { BleHid::release(keyStroke); }
    void write(uint8_t keyStroke) override { BleHid::write(keyStroke); }
    void releaseAll() override { BleHid::releaseAll(); }
    void print(const String &text) override { BleHid::print(text); }
    void println(const String &text) override { BleHid::println(text); }
};
