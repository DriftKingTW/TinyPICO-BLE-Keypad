#include "usbhid.h"

#include "USB.h"
#include "USBHIDKeyboard.h"

static USBHIDKeyboard usbKeyboard;

namespace UsbHid {

void begin() {
    usbKeyboard.begin();
    USB.begin();
}

void press(uint8_t keyStroke) { usbKeyboard.press(keyStroke); }

void release(uint8_t keyStroke) { usbKeyboard.release(keyStroke); }

void pressRaw(uint8_t keyStroke) { usbKeyboard.pressRaw(keyStroke); }

void releaseRaw(uint8_t keyStroke) { usbKeyboard.releaseRaw(keyStroke); }

void write(uint8_t keyStroke) { usbKeyboard.write(keyStroke); }

void releaseAll() { usbKeyboard.releaseAll(); }

void print(const String &text) { usbKeyboard.print(text); }

void println(const String &text) { usbKeyboard.println(text); }

}  // namespace UsbHid
