#pragma once

#include <Arduino.h>

// HTTP configuration server + Improv-over-serial provisioning. The WebServer /
// ImprovWiFi objects and every request handler live in web_server.cpp so that
// main.cpp no longer carries the networking surface.

// Configure the Improv serial device info / callbacks. Call once in setup().
void setupImprov();

// Connect WiFi (or fall back to soft AP), start mDNS + the HTTP server and the
// network task. Call only in WiFi boot mode.
void initWebServer(const char *apSsid, const char *mdnsName);
