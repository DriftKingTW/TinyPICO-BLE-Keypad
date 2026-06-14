#include "web_server.h"

#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <ImprovWiFiLibrary.h>
#include <SPIFFS.h>
#include <WebServer.h>
#include <WiFi.h>

#include "display_state.h"

#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "dev"
#endif

// Sized for up to ~10 layers (matches the project-wide config document size).
static const int kJsonDocSize = 16384;

// Defined in main.cpp.
extern void switchLayout(int layoutIndex);
extern int findLayoutIndex(String layoutName);
extern volatile bool keymapsNeedsUpdate;
extern volatile bool isSoftAPEnabled;
extern String humanReadableSize(const size_t bytes);

WebServer server(80);
ImprovWiFi improvSerial(&Serial);
TaskHandle_t TaskNetwork;

static void onImprovWiFiErrorCb(ImprovTypes::Error err) {
    Serial.println("Error: " + String(err));
}

static void onImprovWiFiConnectedCb(const char *ssid, const char *password) {
    // Save ssid and password to config.json
    DynamicJsonDocument doc(256);

    doc["ssid"] = ssid;
    doc["password"] = password;

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
        Serial.println("Failed to open config file for writing");
    }
    if (serializeJson(doc, configFile) == 0) {
        Serial.println("Failed to write to config file");
    }
    configFile.close();
}

void setupImprov() {
    improvSerial.setDeviceInfo(ImprovTypes::ChipFamily::CF_ESP32,
                               "Schnell Firmware", FIRMWARE_VERSION,
                               "Schnell Keypad");
    improvSerial.onImprovError(onImprovWiFiErrorCb);
    improvSerial.onImprovConnected(onImprovWiFiConnectedCb);
}

/**
 * Network related tasks
 *
 */
static void networkTask(void *pvParameters) {
    while (true) {
        server.handleClient();
        improvSerial.handleSerial();
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
}

static void handleRoot();
static void handleNotFound();
static void sendCrossOriginHeader() { server.send(204); }

static String getContentType(String filename) {
    if (filename.endsWith(".htm"))
        return "text/html";
    else if (filename.endsWith(".html"))
        return "text/html";
    else if (filename.endsWith(".css"))
        return "text/css";
    else if (filename.endsWith(".js"))
        return "text/javascript";
    else if (filename.endsWith(".jpg"))
        return "image/jpeg";
    else if (filename.endsWith(".ico"))
        return "image/x-icon";
    return "text/plain";
}

static bool handleFileRead(String path) {
    Serial.println("handleFileRead: " + path);
    String contentType = getContentType(path);
    if (SPIFFS.exists(path)) {
        File file = SPIFFS.open(path, "r");
        size_t sent = server.streamFile(file, contentType);
        file.close();
        return true;
    }
    Serial.println("\tFile Not Found");
    return false;
}

static void handleNotFound() {
    if (!handleFileRead(server.uri())) {
        server.send(404, "text/plain", "Not found");
    }
}

static void handleRoot() {
    String pathWithExtension = "/index.html";
    if (SPIFFS.exists(pathWithExtension)) {
        Serial.println(F("index found on SPIFFS"));
        File file = SPIFFS.open(pathWithExtension, "r");
        size_t sent = server.streamFile(file, "text/html");
        file.close();
    } else {
        Serial.println(F("index not found on SPIFFS"));
        handleNotFound();
    }
}

/**
 * Activate WiFi and start the server
 *
 */
void initWebServer(const char *apSsid, const char *mdnsName) {
    Serial.println("Loading \"config.json\" from SPIFFS...");
    File file = SPIFFS.open("/config.json");
    if (!file) {
        Serial.println("Failed to open file for reading");
        return;
    }

    Serial.println("Reading WIFI configuration from \"config.json\"...");
    String wifiConfigJSON = "";
    while (file.available()) {
        wifiConfigJSON += (char)file.read();
    }
    file.close();

    DynamicJsonDocument doc(kJsonDocSize);
    DeserializationError err = deserializeJson(
        doc, wifiConfigJSON, DeserializationOption::NestingLimit(5));
    if (err) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(err.c_str());
    }

    const char *ssid = doc["ssid"];
    const char *password = doc["password"];

    // Connect to Wi-Fi network with SSID and password
    Display::setBottom((String)ssid);
    Serial.print("Connecting to ");
    Serial.println(ssid);
    WiFi.mode(WIFI_AP_STA);
    WiFi.begin(ssid, password);

    if (MDNS.begin(mdnsName)) {
        Serial.println((String) "MDNS responder started: " + mdnsName +
                       ".local");
    }

    short count = 0;
    while (WiFi.status() != WL_CONNECTED && count < 20) {
        delay(500);
        Serial.print(".");
        count++;
    }

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("\nWiFi connect failed! Activating soft AP...");
        WiFi.disconnect();
        WiFi.softAP(apSsid, password);
        isSoftAPEnabled = true;
    } else {
        Serial.println("\nWiFi connected!");
    }

    Serial.println((String) "IP: " + WiFi.localIP().toString().c_str());
    Serial.println((String) "Soft AP IP: " +
                   WiFi.softAPIP().toString().c_str());
    server.enableCORS();

    server.on("/", handleRoot);

    server.on("/api/spiffs", HTTP_GET, []() {
        DynamicJsonDocument res(2048 + 128);
        String buffer;
        DynamicJsonDocument doc(2048);

        // create an empty array
        JsonArray array = doc.to<JsonArray>();
        StaticJsonDocument<256> item;

        File root = SPIFFS.open("/");
        File foundfile = root.openNextFile();
        while (foundfile) {
            item["name"] = String(foundfile.name());
            item["size"] = humanReadableSize(foundfile.size());
            array.add(item);
            foundfile = root.openNextFile();
        }

        res["message"] = "success";
        res["total"] = humanReadableSize(SPIFFS.totalBytes());
        res["used"] = humanReadableSize(SPIFFS.usedBytes());
        res["free"] =
            humanReadableSize((SPIFFS.totalBytes() - SPIFFS.usedBytes()));
        res["files"] = array;
        serializeJson(res, buffer);
        server.send(200, "application/json", buffer);
        return;
    });

    server.on("/api/config", HTTP_GET, []() {
        if (server.hasArg("plain") == false) {
            // Handle error here
            Serial.println("Arg Error");
        }
        String type = server.arg("type");
        DynamicJsonDocument res(kJsonDocSize + 128);
        String buffer;
        DynamicJsonDocument doc(kJsonDocSize);

        String filename = "";

        if (type == "keyconfig" || type == "macros") {
            filename = type;
        } else {
            filename = "keyconfig";
        }

        Serial.println("Loading \"" + filename + ".json\" from SPIFFS...");
        File file = SPIFFS.open("/" + filename + ".json");
        if (!file) {
            Serial.println("Failed to open file for reading");
            return;
        }

        Serial.println("Reading key configuration from \"" + filename +
                       ".json\"...");
        String keyConfigJSON = "";
        while (file.available()) {
            keyConfigJSON += (char)file.read();
        }
        file.close();

        deserializeJson(doc, keyConfigJSON);
        res["message"] = "success";
        res["config"] = doc;
        serializeJson(res, buffer);
        server.send(200, "application/json", buffer);
        return;
    });

    server.on("/api/config", HTTP_PUT, []() {
        if (server.hasArg("plain") == false) {
            // Handle error here
            Serial.println("Arg Error");
        }
        String type = server.arg("type");
        String body = server.arg("plain");
        DynamicJsonDocument res(512);
        String buffer;
        // Handle incoming JSON data
        DynamicJsonDocument doc(kJsonDocSize);
        deserializeJson(doc, body);

        // Return error if config is overflowed
        if (doc.overflowed()) {
            res["message"] = "overflowed";
            serializeJson(res, buffer);
            server.send(400, "application/json", buffer);
            return;
        }

        String filename = "";

        if (type == "keyconfig" || type == "macros") {
            filename = type;
        }

        File config = SPIFFS.open("/" + filename + ".json", "w");
        if (!config) {
            res["message"] = "failed to create file";
            serializeJson(res, buffer);
            server.send(400, "application/json", buffer);
            return;
        }

        // Serialize JSON to file
        if (!serializeJson(doc, config)) {
            res["message"] = "failed to write file";
            serializeJson(res, buffer);
            server.send(400, "application/json", buffer);
            return;
        }

        // Writing JSON to file
        else {
            res["message"] = "success";
            serializeJson(res, buffer);
            server.send(200, "application/json", buffer);
            keymapsNeedsUpdate = true;
            return;
        }
    });

    server.on("/api/layout", HTTP_POST, []() {
        if (server.hasArg("plain") == false) {
            // Handle error here
            Serial.println("Arg Error");
        }
        String type = server.arg("type");
        String body = server.arg("plain");
        DynamicJsonDocument res(4096);
        String buffer;
        // Handle incoming JSON data
        DynamicJsonDocument doc(4096);
        deserializeJson(doc, body);

        // Return error if config is overflowed
        if (doc.overflowed()) {
            res["message"] = "overflowed";
            serializeJson(res, buffer);
            server.send(400, "application/json", buffer);
            return;
        }

        // Change current layout by layout title
        int index = findLayoutIndex(doc["layout"].as<String>());
        if (index == -1) {
            res["message"] = "layout not found";
            serializeJson(res, buffer);
            server.send(400, "application/json", buffer);
            return;
        }
        switchLayout(index);

        res["message"] = doc["layout"].as<String>();
        serializeJson(res, buffer);
        server.send(200, "application/json", buffer);
        keymapsNeedsUpdate = true;
        return;
    });

    server.on("/api/config", HTTP_OPTIONS, sendCrossOriginHeader);

    server.on("/api/network", HTTP_GET, []() {
        DynamicJsonDocument res(512 + 128);
        String buffer;
        DynamicJsonDocument doc(512);

        Serial.println("Loading \"config.json\" from SPIFFS...");
        File file = SPIFFS.open("/config.json");
        if (!file) {
            Serial.println("Failed to open file for reading");
            return;
        }

        Serial.println("Reading WIFI configuration from \"config.json\"...");
        String wifiConfigJSON = "";
        while (file.available()) {
            wifiConfigJSON += (char)file.read();
        }
        file.close();

        deserializeJson(doc, wifiConfigJSON);
        doc["rssi"] = WiFi.RSSI();
        doc["mac"] = WiFi.macAddress();
        doc["ip"] = WiFi.localIP();
        doc["apIp"] = WiFi.softAPIP();
        doc["subnetMask"] = WiFi.subnetMask();
        doc["gatewayIP"] = WiFi.gatewayIP();
        res["message"] = "success";
        res["wifi"] = doc;
        serializeJson(res, buffer);
        server.send(200, "application/json", buffer);
        return;
    });

    server.on("/api/network", HTTP_PUT, []() {
        if (server.hasArg("plain") == false) {
            // Handle error here
            Serial.println("Arg Error");
        }
        String body = server.arg("plain");
        DynamicJsonDocument res(512);
        String buffer;
        // Handle incoming JSON data
        DynamicJsonDocument doc(256);
        deserializeJson(doc, body);

        // Return error if config is overflowed
        if (doc.overflowed()) {
            res["message"] = "overflowed";
            serializeJson(res, buffer);
            server.send(400, "application/json", buffer);
            return;
        }

        const String filename = "config.json";

        File config = SPIFFS.open("/" + filename, "w");
        if (!config) {
            res["message"] = "failed to create file";
            serializeJson(res, buffer);
            server.send(400, "application/json", buffer);
            config.close();
            return;
        }

        // Serialize JSON to file
        if (!serializeJson(doc, config)) {
            res["message"] = "failed to write file";
            serializeJson(res, buffer);
            server.send(400, "application/json", buffer);
            return;
        }

        // Writing JSON to file
        else {
            res["message"] = "success";
            serializeJson(res, buffer);
            server.send(200, "application/json", buffer);
            return;
        }
    });

    server.on("/api/network", HTTP_OPTIONS, sendCrossOriginHeader);

    server.onNotFound(handleNotFound);

    server.begin();
    Serial.println("HTTP server started");

    xTaskCreate(networkTask,    /* Task function. */
                "Network Task", /* name of task. */
                10000,          /* Stack size of task */
                NULL,           /* parameter of the task */
                1,              /* priority of the task */
                &TaskNetwork    /* Task handle to keep track of created task */
    );                          /* pin task to core 0 */
    Serial.println("Network service started");
}
