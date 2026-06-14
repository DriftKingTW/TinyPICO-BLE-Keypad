#pragma once

#include <Arduino.h>

// Thread-safe holder for the OLED screen state. The status lines, icon and the
// "last pressed key" label are written from several tasks across both cores
// (loop, generalTask, the encoder tasks). A mutex guards every access so the
// String members are never read while another core is mutating them.
namespace Display {

// Create the guarding mutex. Call once in setup() before any task starts.
void begin();

void setTop(const String &text);
void setBottom(const String &text);
void setIcon(int icon);

// Record the most recently activated key/macro name to show on the next render.
void setKeyInfo(const String &text);
// If a new key info has been set since the last call, copy it into `out` and
// return true (clearing the pending flag); otherwise return false.
bool takeKeyInfo(String &out);

// Copy the current state out for rendering.
void snapshot(String &top, String &bottom, int &icon);

}  // namespace Display
