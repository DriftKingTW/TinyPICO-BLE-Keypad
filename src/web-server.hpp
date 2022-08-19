/*
 * Modify from:
 * https://github.com/smford/esp32-asyncwebserver-fileupload-example
 */

#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <WiFi.h>

const String SSID = "TinyPICO BLE Keypad WLAN";
const String WIFI_PASSWD = "12345678";
const int HTTP_PORT = 80;

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html lang="en">
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <meta charset="UTF-8">
</head>
<body>
  <p><h1>File Upload</h1></p>
  <p>Free Storage: %FREESPIFFS% | Used Storage: %USEDSPIFFS% | Total Storage: %TOTALSPIFFS%</p>
  <form method="POST" action="/upload" enctype="multipart/form-data"><input type="file" name="data"/><input type="submit" name="upload" value="Upload" title="Upload File"></form>
  <p>After clicking upload it will take some time for the file to firstly upload and then be written to SPIFFS, there is no indicator that the upload began.  Please be patient.</p>
  <p>Once uploaded the page will refresh and the newly uploaded file will appear in the file list.</p>
  <p>If a file does not appear, it will be because the file was too big, or had unusual characters in the file name (like spaces).</p>
  <p>You can see the progress of the upload by watching the serial output.</p>
  <p>%FILELIST%</p>
</body>
</html>
)rawliteral";

void configureWebServer();
String listFiles(bool ishtml = false);
void rebootESP(String);
String humanReadableSize(const size_t);
String processor(const String &var);
void handleUpload(AsyncWebServerRequest *, String, size_t, uint8_t *, size_t,
                  bool);

// configuration structure
struct Config {
    String ssid;            // wifi ssid
    String wifipassword;    // wifi password
    int webserverporthttp;  // http port number for web admin
};

// variables
Config config;           // configuration
AsyncWebServer *server;  // initialise webserver

void initWebServer() {
    Serial.begin(115200);

    Serial.println("Mounting SPIFFS ...");
    if (!SPIFFS.begin(true)) {
        // if you have not used SPIFFS before on a ESP32, it will show this
        // error. after a reboot SPIFFS will be configured and will happily
        // work.
        Serial.println("ERROR: Cannot mount SPIFFS, Rebooting");
        rebootESP("ERROR: Cannot mount SPIFFS, Rebooting");
    }

    Serial.print("SPIFFS Free: ");
    Serial.println(
        humanReadableSize((SPIFFS.totalBytes() - SPIFFS.usedBytes())));
    Serial.print("SPIFFS Used: ");
    Serial.println(humanReadableSize(SPIFFS.usedBytes()));
    Serial.print("SPIFFS Total: ");
    Serial.println(humanReadableSize(SPIFFS.totalBytes()));

    Serial.println(listFiles());

    Serial.println("\nLoading Configuration ...");

    config.ssid = SSID;
    config.wifipassword = WIFI_PASSWD;
    config.webserverporthttp = HTTP_PORT;

    // Serial.print("\nConnecting to Wifi: ");
    // WiFi.begin(config.ssid.c_str(), config.wifipassword.c_str());
    // while (WiFi.status() != WL_CONNECTED) {
    //     delay(500);
    //     Serial.print(".");
    // }

    // Configure soft AP
    WiFi.softAP(config.ssid.c_str(), config.wifipassword.c_str());

    // Serial.println("\n\nNetwork Configuration:");
    // Serial.println("----------------------");
    // Serial.print("         SSID: ");
    // Serial.println(WiFi.SSID());
    // Serial.print("  Wifi Status: ");
    // Serial.println(WiFi.status());
    // Serial.print("Wifi Strength: ");
    // Serial.print(WiFi.RSSI());
    // Serial.println(" dBm");
    // Serial.print("          MAC: ");
    // Serial.println(WiFi.macAddress());
    // Serial.print("           IP: ");
    // Serial.println(WiFi.localIP());
    // Serial.print("       Subnet: ");
    // Serial.println(WiFi.subnetMask());
    // Serial.print("      Gateway: ");
    // Serial.println(WiFi.gatewayIP());
    // Serial.print("        DNS 1: ");
    // Serial.println(WiFi.dnsIP(0));
    // Serial.print("        DNS 2: ");
    // Serial.println(WiFi.dnsIP(1));
    // Serial.print("        DNS 3: ");
    // Serial.println(WiFi.dnsIP(2));

    Serial.println("\n\nNetwork Configuration:");
    Serial.println("----------------------");
    Serial.print("         SSID: ");
    Serial.println(config.ssid.c_str());
    Serial.print("           IP: ");
    Serial.println(WiFi.softAPIP());
    Serial.print("          MAC: ");
    Serial.println(WiFi.macAddress());
    Serial.print("\n\n");

    // configure web server
    Serial.println("Configuring Webserver ...");
    server = new AsyncWebServer(config.webserverporthttp);
    configureWebServer();

    // startup web server
    Serial.println("Starting Webserver ...");
    server->begin();
}
void rebootESP(String message) {
    Serial.print("Rebooting ESP32: ");
    Serial.println(message);
    ESP.restart();
}

// list all of the files, if ishtml=true, return html rather than simple text
String listFiles(bool ishtml) {
    String returnText = "";
    Serial.println("Listing files stored on SPIFFS");
    File root = SPIFFS.open("/");
    File foundfile = root.openNextFile();
    if (ishtml) {
        returnText +=
            "<table><tr><th align='left'>Name</th><th "
            "align='left'>Size</th></tr>";
    }
    while (foundfile) {
        if (ishtml) {
            returnText += "<tr align='left'><td>" + String(foundfile.name()) +
                          "</td><td>" + humanReadableSize(foundfile.size()) +
                          "</td></tr>";
        } else {
            returnText += "File: " + String(foundfile.name()) + "\n";
        }
        foundfile = root.openNextFile();
    }
    if (ishtml) {
        returnText += "</table>";
    }
    root.close();
    foundfile.close();
    return returnText;
}

// Make size of files human readable
// source: https://github.com/CelliesProjects/minimalUploadAuthESP32
String humanReadableSize(const size_t bytes) {
    if (bytes < 1024)
        return String(bytes) + " B";
    else if (bytes < (1024 * 1024))
        return String(bytes / 1024.0) + " KB";
    else if (bytes < (1024 * 1024 * 1024))
        return String(bytes / 1024.0 / 1024.0) + " MB";
    else
        return String(bytes / 1024.0 / 1024.0 / 1024.0) + " GB";
}

void configureWebServer() {
    server->on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        String logmessage =
            "Client:" + request->client()->remoteIP().toString() + +" " +
            request->url();
        Serial.println(logmessage);
        request->send_P(200, "text/html", index_html, processor);
    });

    // run handleUpload function when any file is uploaded
    server->on(
        "/upload", HTTP_POST,
        [](AsyncWebServerRequest *request) { request->send(200); },
        handleUpload);
}

// handles uploads
void handleUpload(AsyncWebServerRequest *request, String filename, size_t index,
                  uint8_t *data, size_t len, bool final) {
    String logmessage = "Client:" + request->client()->remoteIP().toString() +
                        " " + request->url();
    Serial.println(logmessage);

    if (!index) {
        logmessage = "Upload Start: " + String(filename);
        // open the file on first call and store the file handle in the request
        // object
        request->_tempFile = SPIFFS.open("/" + filename, "w");
        Serial.println(logmessage);
    }

    if (len) {
        // stream the incoming chunk to the opened file
        request->_tempFile.write(data, len);
        logmessage = "Writing file: " + String(filename) +
                     " index=" + String(index) + " len=" + String(len);
        Serial.println(logmessage);
    }

    if (final) {
        logmessage = "Upload Complete: " + String(filename) +
                     ",size: " + String(index + len);
        // close the file handle as the upload is now done
        request->_tempFile.close();
        Serial.println(logmessage);
        request->redirect("/");
    }
}

String processor(const String &var) {
    if (var == "FILELIST") {
        return listFiles(true);
    }
    if (var == "FREESPIFFS") {
        return humanReadableSize((SPIFFS.totalBytes() - SPIFFS.usedBytes()));
    }

    if (var == "USEDSPIFFS") {
        return humanReadableSize(SPIFFS.usedBytes());
    }

    if (var == "TOTALSPIFFS") {
        return humanReadableSize(SPIFFS.totalBytes());
    }

    return String();
}