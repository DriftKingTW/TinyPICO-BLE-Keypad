#include <Arduino.h>
#include <ArduinoJson.h>
#include <BleKeyboard.h>
#include <ESPmDNS.h>
#include <SPIFFS.h>
#include <TinyPICO.h>
#include <U8g2lib.h>
#include <WebServer.h>
#include <WiFi.h>

#include <algorithm>
#include <cstring>
#include <fstream>
#include <helper.hpp>
#include <iterator>

#define BLE_NAME "TinyPICO BLE"
#define AUTHOR "DriftKingTW"
#define ACTIVE LOW

#define AP_SSID "TinyPICO Keypad WLAN"
#define MDNS_NAME "tp-keypad"

#define ROWS 5
#define COLS 7

String keyMapJSON = "", macroMapJSON = "";

U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C u8g2(U8G2_R0,
                                            /* reset=*/U8X8_PIN_NONE);
BleKeyboard bleKeyboard(BLE_NAME, AUTHOR);
TinyPICO tp = TinyPICO();

TaskHandle_t TaskGeneralStatusCheck;
TaskHandle_t TaskLED;
TaskHandle_t TaskNetwork;

RTC_DATA_ATTR unsigned int timeSinceBoot = 0;
RTC_DATA_ATTR unsigned int savedLayoutIndex = 0;
RTC_DATA_ATTR bool bootConfigMode = false;

// Stucture for key stroke
struct Key {
    uint8_t keyStroke;
    bool state;
    String keyInfo;
} key1, key2, key3, key4, key5, key6, key7, key8, key9, key10, key11, key12,
    key13, key14, key15, key16, key17, key18, key19, key20, key21, key22, key23,
    key24, key25, key26, key27, key28, key29, key30, key31, dummy;
struct Macro {
    unsigned short type;
    // 6 key roll over using BLE keyboard
    uint8_t keyStrokes[6];
    String macroInfo;
    String stringContent;
} macro1, macro2, macro3, macro4, macro5, macro6, macro7, macro8, macro9,
    macro10, macro11, macro12, macro13, macro14, macro15, macro16, macro17,
    macro18, macro19, macro20;

Macro macroMap[20] = {macro1,  macro2,  macro3,  macro4,  macro5,
                      macro6,  macro7,  macro8,  macro9,  macro10,
                      macro11, macro12, macro13, macro14, macro15,
                      macro16, macro17, macro18, macro19, macro20};

Key keyMap[ROWS][COLS] = {{key1, key2, key3, key4, key5, key6, dummy},
                          {key7, key8, key9, key10, key11, key12, key13},
                          {key14, key15, key16, key17, key18, key19, dummy},
                          {key20, key21, key22, key23, key24, key25, key26},
                          {key27, key28, key29, dummy, key30, dummy, key31}};

String currentKeyInfo = "";
bool updateKeyInfo = "";
byte currentLayoutIndex = 0;
byte layoutLength = 0;
String currentLayout = "";
// For maximum 10 layers
const short jsonDocSize = 16384;

byte inputs[] = {23, 19, 18, 5, 32, 33, 25};  // declaring inputs and outputs
const int inputCount = sizeof(inputs) / sizeof(inputs[0]);
byte outputs[] = {4, 14, 15, 27, 26};  // row
const int outputCount = sizeof(outputs) / sizeof(outputs[0]);

// Auto sleep timer
unsigned long sleepPreviousMillis = 0;
const long SLEEP_INTERVAL = 10 * 60 * 1000;

// Battery timer
unsigned long batteryPreviousMillis = 0;
const long BATTERY_INTERVAL = 5 * 1000;

// Low battery LED blink timer
unsigned long ledPreviousMillis = 0;
const long LED_INTERVAL = 5 * 1000;

// IP/mDNS switch timer
unsigned long networkInfoPreviousMillis = 0;
const long NETWORK_INFO_INTERVAL = 5 * 1000;

unsigned long currentMillis = 0;

bool isLowBattery = false;
int batteryPercentage = 101;

bool updateKeyMaps = false;
bool configUpdated = false;

// Function declaration
void ledTask(void *);
void generalTask(void *);
void networkTask(void *);
void initKeys();
void initMacros();
void goSleeping();
void switchBootMode();
void checkIdle();
void resetIdle();
void renderScreen(String msg);
void keyPress(Key &key);
void keyRelease(Key &key);
void macroPress(Macro &macro);
void breathLEDAnimation();
int getBatteryPercentage();
void showLowBatteryWarning();
void checkBattery();
void initWebServer();
void handleRoot();
void handleNotFound();
void sendCrossOriginHeader();
bool handleFileRead(String);
String getContentType(String);

// Set web server port number to 80
WebServer server(80);

void setup() {
    Serial.begin(115200);

    if (bootConfigMode) {
        Serial.println("\nBooting in update config mode...\n");
    }

    Serial.println("Starting BLE work...");
    bleKeyboard.begin();

    Serial.println("Starting u8g2...");
    u8g2.begin();

    Serial.println("Configuring ext1 wakeup source...");
    esp_sleep_enable_ext1_wakeup(0x8000, ESP_EXT1_WAKEUP_ANY_HIGH);

    Serial.println("Configuring General Status Check Task on CPU core 0...");
    xTaskCreatePinnedToCore(
        generalTask,             /* Task function. */
        "GeneralTask",           /* name of task. */
        5000,                    /* Stack size of task */
        NULL,                    /* parameter of the task */
        1,                       /* priority of the task */
        &TaskGeneralStatusCheck, /* Task handle to keep track of created task */
        0);                      /* pin task to core 0 */
    Serial.println("General Status Check Task started");

    xTaskCreatePinnedToCore(
        ledTask,    /* Task function. */
        "LED Task", /* name of task. */
        1000,       /* Stack size of task */
        NULL,       /* parameter of the task */
        1,          /* priority of the task */
        &TaskLED,   /* Task handle to keep track of created task */
        0);         /* pin task to core 0 */
    Serial.println("LED Task started");

    Serial.println("\nLoading SPIFFS...");
    if (!SPIFFS.begin(true)) {
        Serial.println("An Error has occurred while mounting SPIFFS");
        return;
    }

    Serial.print("SPIFFS Free: ");
    Serial.println(
        humanReadableSize((SPIFFS.totalBytes() - SPIFFS.usedBytes())));
    Serial.print("SPIFFS Used: ");
    Serial.println(humanReadableSize(SPIFFS.usedBytes()));
    Serial.print("SPIFFS Total: ");
    Serial.println(humanReadableSize(SPIFFS.totalBytes()));

    Serial.println(listFiles());

    Serial.println("Loading \"keyconfig.json\" from SPIFFS...");
    File file = SPIFFS.open("/keyconfig.json");
    if (!file) {
        Serial.println("Failed to open file for reading");
        return;
    }
    while (file.available()) {
        keyMapJSON += (char)file.read();
    }
    file.close();

    file = SPIFFS.open("/macros.json");
    if (!file) {
        Serial.println("Failed to open file for reading");
        return;
    }
    while (file.available()) {
        macroMapJSON += (char)file.read();
    }
    file.close();

    Serial.println("Configuring input pin...");
    currentLayoutIndex = savedLayoutIndex;
    initKeys();
    initMacros();

    Serial.println("Setup finished!");

    if (bootConfigMode) {
        initWebServer();
    }
}

/**
 * General tasks
 *
 */
void generalTask(void *pvParameters) {
    int previousMillis = 0;

    while (true) {
        // Show connecting message when BLE is disconnected
        while (!bleKeyboard.isConnected()) {
            renderScreen("Connecting BLE..");
            breathLEDAnimation();
            delay(100);
        }

        // Show config updated message after keyconfig updated
        if (configUpdated) {
            renderScreen("Config Updated!");
            configUpdated = false;
            delay(1000);
        }

        checkIdle();
        checkBattery();

        // Show current pressed key info
        if (updateKeyInfo) {
            renderScreen(currentKeyInfo);
            updateKeyInfo = false;
        }
        // Idle message
        if (currentMillis - sleepPreviousMillis > 5000) {
            renderScreen("Layout: " + currentLayout);
        }

        // Record boot time every 5 seconds
        if (currentMillis - previousMillis > 5000) {
            timeSinceBoot += (currentMillis - previousMillis) / 1000;
            previousMillis = currentMillis;
            Serial.println((String) "Time since boot: " + timeSinceBoot +
                           " seconds");
        }

        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

/**
 * General tasks for LED related tasks
 *
 */
void ledTask(void *pvParameters) {
    while (true) {
        currentMillis = millis();
        showLowBatteryWarning();
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

/**
 * Network related tasks
 *
 */
void networkTask(void *pvParameters) {
    while (true) {
        server.handleClient();
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

void loop() {
    // Check every keystroke is pressed or not when connected
    while (bleKeyboard.isConnected()) {
        if (updateKeyMaps) {
            resetIdle();

            keyMapJSON = "";
            macroMapJSON = "";

            Serial.println("Loading \"keyconfig.json\" from SPIFFS...");
            File file = SPIFFS.open("/keyconfig.json");
            if (!file) {
                Serial.println("Failed to open file for reading");
                return;
            }
            while (file.available()) {
                keyMapJSON += (char)file.read();
            }
            file.close();

            file = SPIFFS.open("/macros.json");
            if (!file) {
                Serial.println("Failed to open file for reading");
                return;
            }
            while (file.available()) {
                macroMapJSON += (char)file.read();
            }
            file.close();

            initKeys();
            initMacros();

            updateKeyMaps = false;
            configUpdated = true;
        }
        for (int r = 0; r < ROWS; r++) {
            digitalWrite(outputs[r], LOW);  // Setting one row low
            for (int c = 0; c < COLS; c++) {
                if (digitalRead(inputs[c]) == ACTIVE) {
                    resetIdle();
                    if (r == 1 && c == 0) {
                        // Enter deep sleep mode
                        goSleeping();
                    } else if (r == 0 && c == 0) {
                        switchBootMode();
                    } else if (keyMap[r][c].keyInfo == "FN") {
                        // Switch layout
                        if (currentLayoutIndex < layoutLength - 1) {
                            currentLayoutIndex++;
                        } else {
                            currentLayoutIndex = 0;
                        }
                        savedLayoutIndex = currentLayoutIndex;
                        initKeys();
                        delay(300);
                    } else if (keyMap[r][c].keyInfo.startsWith("MACRO_")) {
                        // Macro press
                        size_t index =
                            keyMap[r][c]
                                .keyInfo
                                .substring(6, sizeof(keyMap[r][c].keyInfo) - 1)
                                .toInt();
                        macroPress(macroMap[index]);
                    } else {
                        // Standard key press
                        keyPress(keyMap[r][c]);
                    }
                } else {
                    keyRelease(keyMap[r][c]);
                }
                delayMicroseconds(10);
            }
            digitalWrite(outputs[r], HIGH);  // Setting the row back to high
            delayMicroseconds(10);
        }
        delay(10);
    }
    delay(100);
}

/**
 * Initialize every Key instance that used in this program
 *
 */
void initKeys() {
    Serial.begin(115200);
    setCpuFrequencyMhz(240);

    Serial.println("CPU clock speed set to 240Mhz");
    Serial.println("Reading JSON keymap configuration...");

    DynamicJsonDocument doc(jsonDocSize);
    DeserializationError err = deserializeJson(
        doc, keyMapJSON, DeserializationOption::NestingLimit(5));
    if (err) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(err.c_str());
    }

    uint8_t keyLayout[ROWS][COLS];
    String keyInfo[ROWS][COLS];

    copyArray(doc[currentLayoutIndex]["keymap"], keyLayout);
    copyArray(doc[currentLayoutIndex]["keyInfo"], keyInfo);

    layoutLength = doc.size();

    // GPIO configuration
    for (int i = 0; i < outputCount; i++) {
        pinMode(outputs[i], OUTPUT);
        digitalWrite(outputs[i], HIGH);
    }

    for (int i = 0; i < inputCount; i++) {
        pinMode(inputs[i], INPUT_PULLUP);
    }

    // Assign keymap data
    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++) {
            keyMap[r][c].keyStroke = keyLayout[r][c];
            keyMap[r][c].keyInfo = keyInfo[r][c];
            keyMap[r][c].state = false;
        }
    }

    // Show layout title on screen
    String str = doc[currentLayoutIndex]["title"];
    currentLayout = str;
    renderScreen("Layout: " + currentLayout);
    Serial.println("Key layout loaded: " + currentLayout);

    if (!bootConfigMode) {
        Serial.begin(115200);
        setCpuFrequencyMhz(80);
        Serial.println("CPU clock speed set to 80Mhz");
    }
}

/**
 * Initialize every Macro instance that used in this program
 *
 */
void initMacros() {
    DynamicJsonDocument doc(jsonDocSize);
    DeserializationError err = deserializeJson(
        doc, macroMapJSON, DeserializationOption::NestingLimit(5));
    if (err) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(err.c_str());
    }
    size_t macrosLength = doc.size();
    for (size_t i = 0; i < macrosLength; i++) {
        uint8_t macroLayout[] = {0, 0, 0, 0, 0, 0};
        String macroNameStr = doc[i]["name"];
        String macroStringContent = doc[i]["stringContent"];
        macroMap[i].type = doc[i]["type"];
        macroMap[i].macroInfo = macroNameStr;
        macroMap[i].stringContent = macroStringContent;
        copyArray(doc[i]["keyStrokes"], macroLayout);
        std::copy(std::begin(macroLayout), std::end(macroLayout),
                  std::begin(macroMap[i].keyStrokes));
    }
}

/**
 * Press key
 *
 * @param {Key} key the key to be pressed
 */
void keyPress(Key &key) {
    if (key.state == false) {
        bleKeyboard.press(key.keyStroke);
    }
    key.state = true;
    updateKeyInfo = true;
    currentKeyInfo = key.keyInfo;
}

/**
 * Release key
 *
 * @param {Key} key the key to be released
 */
void keyRelease(Key &key) {
    if (key.state == true) {
        bleKeyboard.release(key.keyStroke);
    }
    key.state = false;
    return;
}

/**
 * Press macro keys
 *
 * @param {Macro} macro to be pressed
 * type 0: for key strokes
 * type 1: for string content
 * type 2: for string content w/ enter key
 */
void macroPress(Macro &macro) {
    updateKeyInfo = true;
    currentKeyInfo = macro.macroInfo;
    if (macro.type == 0) {
        size_t length = sizeof(macro.keyStrokes);
        for (size_t i = 0; i < length; i++) {
            bleKeyboard.press(macro.keyStrokes[i]);
            delayMicroseconds(10);
        }
        delay(50);
        bleKeyboard.releaseAll();
    } else if (macro.type == 1) {
        bleKeyboard.print(macro.stringContent);
    } else if (macro.type == 2) {
        bleKeyboard.println(macro.stringContent);
    }
    delay(100);
}

/**
 * Breath LED Animation
 *
 */
void breathLEDAnimation() {
    int brightness = 0;
    tp.DotStar_SetPower(true);
    // Brighten LED step by step
    for (; brightness <= 50; brightness++) {
        tp.DotStar_SetPixelColor(0, brightness, 0);
        delayMicroseconds(5);
    }
    // Dimming LED step by step
    for (; brightness >= 0; brightness--) {
        delayMicroseconds(5);
        tp.DotStar_SetPixelColor(0, brightness, 0);
    }
}

/**
 * Print message on oled screen.
 *
 * @param {char} array to print on oled screen
 */
void renderScreen(String msg) {
    // string to char array
    int n = msg.length();
    char char_array[n + 1];
    strcpy(char_array, msg.c_str());

    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB08_tr);
    u8g2.setFontPosCenter();
    u8g2.drawStr(64 - u8g2.getStrWidth(char_array) / 2, 24, char_array);
    if (bootConfigMode) {
        String ip_str = "";
        if (currentMillis - networkInfoPreviousMillis < NETWORK_INFO_INTERVAL) {
            ip_str = (String)WiFi.localIP().toString().c_str();
        } else if ((currentMillis - networkInfoPreviousMillis) <
                   (NETWORK_INFO_INTERVAL * 2)) {
            ip_str = (String)MDNS_NAME + ".local";
        } else {
            ip_str = (String)MDNS_NAME + ".local";
            networkInfoPreviousMillis = currentMillis;
        }
        if (WiFi.localIP().toString() == "0.0.0.0") {
            ip_str = "Connecting to...";
        }
        int n = ip_str.length();
        char ip_char_array[n + 1];
        strcpy(ip_char_array, ip_str.c_str());
        u8g2.drawStr(64 - u8g2.getStrWidth(ip_char_array) / 2, 10,
                     ip_char_array);
    } else {
        String result = "";
        bool plugged = digitalRead(9);
        bool charging = tp.IsChargingBattery();
        u8g2.setFontPosCenter();
        u8g2.setFont(u8g2_font_open_iconic_all_2x_t);
        if (plugged && charging) {
            result = "Charging";
            u8g2.drawStr((64 - u8g2.getStrWidth("\u0060") / 2) - 32, 10,
                         "\u0060");
        } else if (plugged) {
            result = "Plugged in";
            u8g2.drawStr((64 - u8g2.getStrWidth("\u0060") / 2) - 32, 10,
                         "\u0060");
        } else if (batteryPercentage > 100) {
            result = "Reading battery...";
        } else {
            result = (String)batteryPercentage + "%";
            if (batteryPercentage > 75) {
                u8g2.drawStr((64 - u8g2.getStrWidth("\u005B") / 2) - 32, 10,
                             "\u005B");
            } else if (batteryPercentage > 50) {
                u8g2.drawStr((64 - u8g2.getStrWidth("\u005A") / 2) - 32, 10,
                             "\u005A");
            } else if (batteryPercentage > 25) {
                u8g2.drawStr((64 - u8g2.getStrWidth("\u005A") / 2) - 32, 10,
                             "\u005A");
            }
        }

        int n = result.length();
        char char_array[n];
        strcpy(char_array, result.c_str());
        u8g2.setFont(u8g2_font_ncenB08_tr);
        u8g2.setFontPosCenter();
        u8g2.drawStr((64 - u8g2.getStrWidth(char_array) / 2) + 8, 10,
                     char_array);
        u8g2.sendBuffer();

        if (batteryPercentage <= 20) {
            isLowBattery = true;
        } else {
            isLowBattery = false;
        }
    }
    u8g2.sendBuffer();
}

/**
 * Return Battery Current Percentage
 *
 */
int getBatteryPercentage() {
    const float minVoltage = 3.4, fullVolatge = 4.0;
    float batteryVoltage = tp.GetBatteryVoltage();

    Serial.println((String) "Battery Voltage: " + batteryVoltage);

    // Convert to percentage
    float percentage =
        (batteryVoltage - minVoltage) / (fullVolatge - minVoltage) * 100;

    // Update device's battery level
    bleKeyboard.setBatteryLevel(percentage);

    return percentage > 100 ? 100 : percentage;
}

/**
 * Enter deep sleep mode
 *
 */
void goSleeping() {
    u8g2.clearBuffer();
    u8g2.sendBuffer();
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
    gpio_pulldown_en(GPIO_NUM_15);
    // gpio_pulldown_en(GPIO_NUM_27);
    // gpio_pulldown_en(GPIO_NUM_26);
    esp_deep_sleep_start();
}

/**
 * Switching between different boot modes
 *
 */
void switchBootMode() {
    Serial.println("Resetting...");
    if (!bootConfigMode) {
        renderScreen("=> Config Mode <=");
    } else {
        renderScreen("=> Normal Mode <=");
        WiFi.softAPdisconnect(true);
    }
    bootConfigMode = !bootConfigMode;
    esp_sleep_enable_timer_wakeup(100);
    esp_deep_sleep_start();
}

/**
 * Check if device is idle for a specified period to determine if it should
 * go to sleep or not.
 *
 */
void checkIdle() {
    if (currentMillis - sleepPreviousMillis > SLEEP_INTERVAL) {
        goSleeping();
    }
}

/**
 * Check battery status for a specified period
 *
 */
void checkBattery() {
    if (currentMillis - batteryPreviousMillis > BATTERY_INTERVAL) {
        batteryPercentage = getBatteryPercentage();
        batteryPreviousMillis = currentMillis;
    }
    return;
}

/**
 * Blink LED to indicate that battery is low
 *
 */
void showLowBatteryWarning() {
    if (!isLowBattery) {
        if (bleKeyboard.isConnected()) {
            tp.DotStar_SetPower(false);
        }
        return;
    }
    tp.DotStar_SetPower(true);
    if (currentMillis - ledPreviousMillis > 1000 &&
        currentMillis - ledPreviousMillis <= 1200) {
        tp.DotStar_SetBrightness(5);
        tp.DotStar_SetPixelColor(255, 0, 0);
    } else if (currentMillis - ledPreviousMillis > 1200 &&
               currentMillis - ledPreviousMillis <= 1300) {
        tp.DotStar_SetPixelColor(0, 0, 0);
    } else if (currentMillis - ledPreviousMillis > 1300 &&
               currentMillis - ledPreviousMillis <= 1500) {
        tp.DotStar_SetPixelColor(255, 0, 0);
    } else if (currentMillis - ledPreviousMillis > 1700) {
        tp.DotStar_SetPixelColor(0, 0, 0);
        ledPreviousMillis = currentMillis;
    }
}

/**
 * Update sleepPreviousMillis' value to reset idle timer
 *
 */
void resetIdle() { sleepPreviousMillis = currentMillis; }

/**
 * Activate WiFi and start the server
 *
 */
void initWebServer() {
    Serial.println("Loading \"wifi.json\" from SPIFFS...");
    File file = SPIFFS.open("/wifi.json");
    if (!file) {
        Serial.println("Failed to open file for reading");
        return;
    }

    Serial.println("Reading WIFI configuration from \"wifi.json\"...");
    String wifiConfigJSON = "";
    while (file.available()) {
        wifiConfigJSON += (char)file.read();
    }
    file.close();

    DynamicJsonDocument doc(jsonDocSize);
    DeserializationError err = deserializeJson(
        doc, wifiConfigJSON, DeserializationOption::NestingLimit(5));
    if (err) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(err.c_str());
    }

    const char *ssid = doc["ssid"];
    const char *password = doc["password"];

    // Connect to Wi-Fi network with SSID and password
    renderScreen((String)ssid);
    Serial.print("Connecting to ");
    Serial.println(ssid);
    WiFi.mode(WIFI_AP_STA);
    WiFi.begin(ssid, password);

    if (MDNS.begin(MDNS_NAME)) {
        Serial.println((String) "MDNS responder started: " + MDNS_NAME +
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
        WiFi.softAP(AP_SSID, password);
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
        DynamicJsonDocument res(jsonDocSize + 128);
        String buffer;
        DynamicJsonDocument doc(jsonDocSize);

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
        String keyconfigJSON = "";
        while (file.available()) {
            keyconfigJSON += (char)file.read();
        }
        file.close();

        deserializeJson(doc, keyconfigJSON);
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
        DynamicJsonDocument doc(jsonDocSize);
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
            updateKeyMaps = true;
            return;
        }
    });

    server.on("/api/config", HTTP_OPTIONS, sendCrossOriginHeader);

    server.on("/api/network", HTTP_GET, []() {
        DynamicJsonDocument res(512 + 128);
        String buffer;
        DynamicJsonDocument doc(512);

        Serial.println("Loading \"wifi.json\" from SPIFFS...");
        File file = SPIFFS.open("/wifi.json");
        if (!file) {
            Serial.println("Failed to open file for reading");
            return;
        }

        Serial.println("Reading WIFI configuration from \"wifi.json\"...");
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

        const String filename = "wifi.json";

        File config = SPIFFS.open("/" + filename, "w");
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

void handleRoot() {
    String pathWithExtension = "/index.html";
    if (SPIFFS.exists(pathWithExtension)) {
        Serial.println(F("index found on SPIFFS"));
        File file = SPIFFS.open(pathWithExtension, "r");
        size_t sent = server.streamFile(file, "text/html");
        file.close();
    } else {
        Serial.println(F("index not found on SPIFFS"));
        handleNotFound;
    }
}

void handleNotFound() {
    if (!handleFileRead(server.uri())) {
        server.send(404, "text/plain", "Not found");
    }
}

void sendCrossOriginHeader() { server.send(204); }

bool handleFileRead(String path) {
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

String getContentType(String filename) {
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