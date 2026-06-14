#include "keyboard_output.h"

// USB HID needs modifier keys (codes 128-135, i.e. the BLE keyboard's
// KEY_LEFT_CTRL .. KEY_RIGHT_GUI) sent as raw usages 0xe0-0xe7 rather than as
// regular key presses. BLE handles those codes directly, so only the USB output
// remaps them.
void UsbKeyboardOutput::press(uint8_t keyStroke) {
    switch (keyStroke) {
        case 128:  // KEY_LEFT_CTRL
            UsbHid::pressRaw(0xe0);
            break;
        case 129:  // KEY_LEFT_SHIFT
            UsbHid::pressRaw(0xe1);
            break;
        case 130:  // KEY_LEFT_ALT
            UsbHid::pressRaw(0xe2);
            break;
        case 131:  // KEY_LEFT_GUI
            UsbHid::pressRaw(0xe3);
            break;
        case 132:  // KEY_RIGHT_CTRL
            UsbHid::pressRaw(0xe4);
            break;
        case 133:  // KEY_RIGHT_SHIFT
            UsbHid::pressRaw(0xe5);
            break;
        case 134:  // KEY_RIGHT_ALT
            UsbHid::pressRaw(0xe6);
            break;
        case 135:  // KEY_RIGHT_GUI
            UsbHid::pressRaw(0xe7);
            break;
        default:
            UsbHid::press(keyStroke);
    }
}

void UsbKeyboardOutput::release(uint8_t keyStroke) {
    switch (keyStroke) {
        case 128:  // KEY_LEFT_CTRL
            UsbHid::releaseRaw(0xe0);
            break;
        case 129:  // KEY_LEFT_SHIFT
            UsbHid::releaseRaw(0xe1);
            break;
        case 130:  // KEY_LEFT_ALT
            UsbHid::releaseRaw(0xe2);
            break;
        case 131:  // KEY_LEFT_GUI
            UsbHid::releaseRaw(0xe3);
            break;
        case 132:  // KEY_RIGHT_CTRL
            UsbHid::releaseRaw(0xe4);
            break;
        case 133:  // KEY_RIGHT_SHIFT
            UsbHid::releaseRaw(0xe5);
            break;
        case 134:  // KEY_RIGHT_ALT
            UsbHid::releaseRaw(0xe6);
            break;
        case 135:  // KEY_RIGHT_GUI
            UsbHid::releaseRaw(0xe7);
            break;
        default:
            UsbHid::release(keyStroke);
    }
}
