#include "display_state.h"

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace {
SemaphoreHandle_t gMutex = nullptr;
String gTop;
String gBottom;
int gIcon = 0;
String gKeyInfo;
bool gKeyInfoPending = false;

// RAII lock; a no-op until Display::begin() has created the mutex.
struct Guard {
    Guard() {
        if (gMutex) xSemaphoreTake(gMutex, portMAX_DELAY);
    }
    ~Guard() {
        if (gMutex) xSemaphoreGive(gMutex);
    }
};
}  // namespace

namespace Display {

void begin() {
    if (!gMutex) gMutex = xSemaphoreCreateMutex();
}

void setTop(const String &text) {
    Guard g;
    gTop = text;
}

void setBottom(const String &text) {
    Guard g;
    gBottom = text;
}

void setIcon(int icon) {
    Guard g;
    gIcon = icon;
}

void setKeyInfo(const String &text) {
    Guard g;
    gKeyInfo = text;
    gKeyInfoPending = true;
}

bool takeKeyInfo(String &out) {
    Guard g;
    if (!gKeyInfoPending) return false;
    out = gKeyInfo;
    gKeyInfoPending = false;
    return true;
}

void snapshot(String &top, String &bottom, int &icon) {
    Guard g;
    top = gTop;
    bottom = gBottom;
    icon = gIcon;
}

}  // namespace Display
