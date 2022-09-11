#include <main.hpp>

RTC_DATA_ATTR unsigned int timeSinceBoot = 0;
RTC_DATA_ATTR bool bootConfigMode = false;

U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C u8g2(U8G2_R0,
                                            /* reset=*/U8X8_PIN_NONE);
BleKeyboard bleKeyboard(BLE_NAME, AUTHOR);
TinyPICO tp = TinyPICO();

TaskHandle_t TaskGeneralStatusCheck;
TaskHandle_t TaskLED;
TaskHandle_t TaskNetwork;
TaskHandle_t TaskScreen;

// Stucture for key stroke
Key key1, key2, key3, key4, key5, key6, key7, key8, key9, key10, key11, key12,
    key13, key14, key15, key16, key17, key18, key19, key20, key21, key22, key23,
    key24, key25, key26, key27, key28, key29, key30, key31, dummy;

Macro macro1, macro2, macro3, macro4, macro5, macro6, macro7, macro8, macro9,
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

String keyMapJSON = "", macroMapJSON = "";
String currentKeyInfo = "";
bool updateKeyInfo = false;
bool isFnKeyPressed = false;
RTC_DATA_ATTR byte currentLayoutIndex = 0;
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
const long BATTERY_INTERVAL = 10 * 1000;

// Low battery LED blink timer
unsigned long ledPreviousMillis = 0;
const long LED_INTERVAL = 5 * 1000;

// IP/mDNS switch timer
unsigned long networkInfoPreviousMillis = 0;
const long NETWORK_INFO_INTERVAL = 5 * 1000;

unsigned long currentMillis = 0;

bool isLowBattery = false;
int batteryPercentage = 101;

bool keymapsNeedsUpdate = false;
bool configUpdated = false;
bool showCompleteAnimation = false;
bool isSoftAPEnabled = false;
bool isGoingToSleep = false;
bool clearDisplay = false;

// OLED Screen Content
String contentTop = "";
String contentBottom = "";
// loading: 0, ble: 1, wifi: 2, ap: 3, charging: 4, plugged in: 5,
// low battery: 6, sleep: 7
int contentIcon = 0;

// Set web server port number to 80
WebServer server(80);

void setup() {
    Serial.begin(BAUD_RATE);

    printSpacer();

    setCPUFrequency(240);

    printSpacer();

    Serial.println("Starting BLE work...");
    bleKeyboard.begin();

    Serial.println("Starting u8g2...");
    u8g2.begin();

    printSpacer();

    Serial.println("Configuring ext1 wakeup source...");
    esp_sleep_enable_ext1_wakeup(WAKEUP_KEY_BITMAP, ESP_EXT1_WAKEUP_ANY_HIGH);
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);

    Serial.println("Configuring General Status Check Task on CPU core 0...");
    xTaskCreatePinnedToCore(
        generalTask,             /* Task function. */
        "GeneralTask",           /* name of task. */
        5000,                    /* Stack size of task */
        NULL,                    /* parameter of the task */
        1,                       /* priority of the task */
        &TaskGeneralStatusCheck, /* Task handle to keep track of created task */
        0);                      /* pin task to core 0 */

    xTaskCreatePinnedToCore(
        ledTask,    /* Task function. */
        "LED Task", /* name of task. */
        1000,       /* Stack size of task */
        NULL,       /* parameter of the task */
        1,          /* priority of the task */
        &TaskLED,   /* Task handle to keep track of created task */
        0);         /* pin task to core 0 */

    xTaskCreatePinnedToCore(
        screenTask,    /* Task function. */
        "Screen Task", /* name of task. */
        5000,          /* Stack size of task */
        NULL,          /* parameter of the task */
        1,             /* priority of the task */
        &TaskScreen,   /* Task handle to keep track of created task */
        0);            /* pin task to core 0 */

    printSpacer();

    Serial.println("Checking if ESP is wakeup from sleep...");
    esp_sleep_wakeup_cause_t wakeup_reason;

    wakeup_reason = esp_sleep_get_wakeup_cause();

    if (wakeup_reason != ESP_SLEEP_WAKEUP_EXT1) {
        Serial.println("Strating EEPROM...");
        EEPROM.begin(EEPROM_SIZE);
        byte savedLayoutIndex = EEPROM.read(0);
        if (savedLayoutIndex == 255) {
            Serial.println("EEPROM is empty, set default layout to 0");
            EEPROM.write(0, currentLayoutIndex);
            EEPROM.commit();
        } else {
            Serial.println("EEPROM saved layout index: " + savedLayoutIndex);
            currentLayoutIndex = savedLayoutIndex;
        }
    }

    printSpacer();

    Serial.println("Loading SPIFFS...");
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

    Serial.println("Loading config files from SPIFFS...");
    keyMapJSON = loadJSONFileAsString("keyconfig");
    macroMapJSON = loadJSONFileAsString("macros");

    printSpacer();

    Serial.println("Configuring input pin and keys...");
    initKeys();
    initMacros();

    printSpacer();

    if (bootConfigMode) {
        initWebServer();
    } else {
        setCPUFrequency(80);
    }

    printSpacer();

    Serial.println("Setup finished!");

    printSpacer();
}

/**
 * General tasks
 *
 */
void generalTask(void *pvParameters) {
    int previousMillis = 0;

    // if (bootConfigMode) {
    //     networkAnimation(u8g2);
    // } else {
    //     loadingAnimation(u8g2);
    // }

    while (true) {
        checkBattery();

        // Update screen info
        if (isGoingToSleep) {
            contentBottom = "Going to sleep";
            contentIcon = 7;
        } else if (bootConfigMode) {
            String networkInfo = "";
            if (currentMillis - networkInfoPreviousMillis <
                NETWORK_INFO_INTERVAL) {
                networkInfo = (String)WiFi.localIP().toString().c_str();
                contentIcon = 2;
            } else if ((currentMillis - networkInfoPreviousMillis) <
                       (NETWORK_INFO_INTERVAL * 2)) {
                networkInfo = (String)MDNS_NAME + ".local";
                contentIcon = 2;
            } else {
                networkInfo = (String)MDNS_NAME + ".local";
                networkInfoPreviousMillis = currentMillis;
                contentIcon = 2;
            }
            if (isSoftAPEnabled) {
                contentIcon = 3;
            } else if (WiFi.localIP().toString() == "0.0.0.0") {
                networkInfo = "Connecting to...";
                contentIcon = 2;
            }
            contentTop = networkInfo;
        } else {
            String result = "";
            bool plugged = digitalRead(9);
            bool charging = tp.IsChargingBattery();
            if (plugged && charging) {
                result = "Charging";
                contentIcon = 4;
            } else if (plugged) {
                result = "Plugged in";
                contentIcon = 5;
            } else if (batteryPercentage > 100) {
                result = "Reading battery...";
            } else {
                result = "Bat. " + (String)batteryPercentage + "%";
                contentIcon = 1;
            }

            contentTop = result;
        }

        // Show connecting message when BLE is disconnected
        while (!bleKeyboard.isConnected()) {
            contentBottom = "Connecting BLE..";
            breathLEDAnimation();
            checkIdle();
            delay(100);
        }

        // Show config updated message after keyconfig updated
        if (configUpdated) {
            contentBottom = "Config Updated!";
            configUpdated = false;
            delay(1000);
        }

        if (showCompleteAnimation) {
            vTaskSuspend(TaskScreen);
            finishAnimation(u8g2);
            showCompleteAnimation = false;
            contentBottom = "Layout: " + currentLayout;
            vTaskResume(TaskScreen);
        }

        checkIdle();

        // Show current pressed key info
        if (updateKeyInfo) {
            contentBottom = currentKeyInfo;
            updateKeyInfo = false;
        }

        // Idle message
        if (currentMillis - sleepPreviousMillis > 5000) {
            contentBottom = "Layout: " + currentLayout;
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

/**
 * OLED screen related tasks
 *
 */
void screenTask(void *pvParameters) {
    while (true) {
        renderScreen();
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

void loop() {
    // Check every keystroke is pressed or not when connected
    while (bleKeyboard.isConnected()) {
        if (keymapsNeedsUpdate) {
            updateKeymaps();
            showCompleteAnimation = true;
        }
        for (int r = 0; r < ROWS; r++) {
            digitalWrite(outputs[r], LOW);  // Setting one row low
            for (int c = 0; c < COLS; c++) {
                if (digitalRead(inputs[c]) == ACTIVE) {
                    resetIdle();
                    if (isFnKeyPressed) {
                        if (r == 0 && c == 0) {
                            goSleeping();
                        } else if (r == 1 && c == 0) {
                            switchBootMode();
                        } else if (r == 4 && c == 4) {
                            // Switch layout
                            currentLayoutIndex =
                                currentLayoutIndex < layoutLength - 1
                                    ? currentLayoutIndex + 1
                                    : 0;
                            initKeys();
                            delay(300);
                        }
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
    contentBottom = "Layout: " + currentLayout;

    EEPROM.write(0, currentLayoutIndex);
    EEPROM.commit();
    Serial.println("Key layout loaded: " + currentLayout);
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
 * Update keymaps
 *
 */
void updateKeymaps() {
    resetIdle();

    keyMapJSON = "";
    macroMapJSON = "";

    Serial.println("Loading config files from SPIFFS...");
    keyMapJSON = loadJSONFileAsString("keyconfig");
    macroMapJSON = loadJSONFileAsString("macros");

    initKeys();
    initMacros();

    keymapsNeedsUpdate = false;
    configUpdated = true;
}

/**
 * Press key
 *
 * @param {Key} key the key to be pressed
 */
void keyPress(Key &key) {
    if (key.keyInfo == "FN") {
        isFnKeyPressed = true;
    }
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
    if (key.keyInfo == "FN") {
        isFnKeyPressed = false;
    }
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
 * Load JSON file as string from SPIFFS
 *
 * @param {filename} JSON file name (w/o extension)
 * @return {filestring} file content as string
 */
String loadJSONFileAsString(String filename) {
    File file = SPIFFS.open("/" + filename + ".json");
    String buffer;
    if (!file) {
        Serial.println("Failed to open file for reading");
    }
    while (file.available()) {
        buffer += (char)file.read();
    }
    file.close();
    return buffer;
}

/**
 * Set CPU Frequency by Mhz and print it on serial
 *
 */
void setCPUFrequency(int freq) {
    Serial.begin(BAUD_RATE);
    setCpuFrequencyMhz(freq);
    Serial.println("CPU clock speed set to " + String(freq) + "Mhz");
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
void renderScreen() {
    // string to char array
    int nTop = contentBottom.length();
    char charArrayTop[nTop];
    strcpy(charArrayTop, contentBottom.c_str());

    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB08_tr);
    u8g2.setFontPosCenter();
    u8g2.drawStr(16 + 4, 24, charArrayTop);

    int nBottom = contentTop.length();
    char charArrayBottom[nBottom];
    strcpy(charArrayBottom, contentTop.c_str());
    u8g2.setFont(u8g2_font_ncenB08_tr);
    u8g2.setFontPosCenter();
    u8g2.drawStr(16 + 4, 10, charArrayBottom);

    u8g2.setFont(u8g2_font_open_iconic_all_2x_t);
    switch (contentIcon) {
        case 0:
            u8g2.drawGlyph(0, 16, 0xCD);
            break;
        case 1:
            u8g2.drawGlyph(0, 16, 0x5E);
            break;
        case 2:
            u8g2.drawGlyph(0, 16, 0xF7);
            break;
        case 3:
            u8g2.drawGlyph(0, 16, 0x54);
            break;
        case 4:
            u8g2.drawGlyph(0, 16, 0x60);
            break;
        case 5:
            u8g2.drawGlyph(0, 16, 0xAA);
            break;
        case 6:
            u8g2.drawGlyph(0, 16, 0x50);
            break;
        case 7:
            u8g2.drawGlyph(0, 16, 0xDF);
            break;
        default:
            u8g2.drawGlyph(0, 16, 0x00);
    }

    while (clearDisplay) {
        u8g2.clearBuffer();
        u8g2.sendBuffer();
        delay(100);
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
    isGoingToSleep = true;
    gpio_pulldown_en(GPIO_NUM_15);
    // gpio_pulldown_en(GPIO_NUM_27);
    // gpio_pulldown_en(GPIO_NUM_26);
    delay(1000);
    clearDisplay = true;
    delay(500);
    esp_deep_sleep_start();
}

/**
 * Switching between different boot modes
 *
 */
void switchBootMode() {
    Serial.println("Resetting...");
    if (!bootConfigMode) {
        contentBottom = "Layout: " + currentLayout;
    } else {
        contentBottom = "=> Normal Mode <=";
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
    if (currentMillis - batteryPreviousMillis > BATTERY_INTERVAL ||
        batteryPercentage > 100) {
        batteryPercentage = getBatteryPercentage();
        batteryPreviousMillis = currentMillis;

        if (batteryPercentage <= 20) {
            isLowBattery = true;
        } else {
            isLowBattery = false;
        }
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
    contentBottom = (String)ssid;
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
            keymapsNeedsUpdate = true;
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