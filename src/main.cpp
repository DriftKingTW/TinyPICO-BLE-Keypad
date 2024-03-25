#include <main.hpp>

RTC_DATA_ATTR unsigned int timeSinceBoot = 0;
RTC_DATA_ATTR bool bootWiFiMode = false;

U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C u8g2(U8G2_R0);
BleKeyboard bleKeyboard(BLE_NAME, AUTHOR);
USBHIDKeyboard usbKeyboard;

PCF8574 pcf8574RotaryExtension(ENCODER_EXTENSION_ADDR);
bool isRotaryExtensionConnected = false;

TaskHandle_t TaskGeneralStatusCheck;
TaskHandle_t TaskLED;
TaskHandle_t TaskNetwork;
TaskHandle_t TaskScreen;
TaskHandle_t TaskEncoderExtension;
TaskHandle_t TaskEncoder;
TaskHandle_t TaskI2C;

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
                          {key27, key28, key29, dummy, key30, key31, dummy}};

// Rotray Extnesion ( 3 keys + 1 rotary encoder)
Key rotaryExtKeyMap[3] = {Key(), Key(), Key()};
RotaryEncoderConfig rotaryExtRotaryEncoders[1] = {RotaryEncoderConfig()};

// Onboard Rotary Encoder
ESP32Encoder onboardEncoders[1] = {ESP32Encoder()};
RotaryEncoderConfig onboardRotaryEncoders[1] = {RotaryEncoderConfig()};

String keyConfigJSON = "", macroMapJSON = "";
String currentKeyInfo = "";
bool updateKeyInfo = false;
bool isFnKeyPressed = false;
bool isDetectingLastConnectedDevice = true;
RTC_DATA_ATTR byte currentLayoutIndex = 0;
RTC_DATA_ATTR byte currentActiveDevice = 0;
RTC_DATA_ATTR String currentActiveDeviceAddress = "";
RTC_DATA_ATTR bool isUsbMode = true;
byte layoutLength = 0;
String currentLayout = "";
// For maximum 10 layers
const short jsonDocSize = 16384;
size_t tapToggleOrginalLayerIndex = 0;
bool isTemporaryToggled = false;

byte inputs[] = {9, 3, 8, 5, 4, 18, 17};  // Column
const int inputCount = sizeof(inputs) / sizeof(inputs[0]);
byte outputs[] = {14, 13, 12, 11, 10};  // Row
const int outputCount = sizeof(outputs) / sizeof(outputs[0]);

// Auto sleep timer
unsigned long sleepPreviousMillis = 0;
const long SLEEP_INTERVAL = 30 * 60 * 1000;
const long SCREEN_SLEEP_INTERVAL = 3 * 60 * 1000;

// Battery timer
unsigned long batteryPreviousMillis = 0;
const long BATTERY_INTERVAL = 10 * 1000;

// Low battery LED blink timer
unsigned long ledPreviousMillis = 0;
const long LED_INTERVAL = 5 * 1000;

// IP/mDNS switch timer
unsigned long networkInfoPreviousMillis = 0;
const long NETWORK_INFO_INTERVAL = 5 * 1000;

// Tap-Toggle timer
unsigned long tapTogglePreviousMillis = 0;
uint8_t tapToggleCount = 0;

unsigned long currentMillis = 0;

bool isLowBattery = false;
int batteryPercentage = 101;

bool keymapsNeedsUpdate = false;
bool configUpdated = false;
bool isSoftAPEnabled = false;
bool isGoingToSleep = false;
bool clearDisplay = false;
bool isSwitchingBootMode = false;
bool isCaffeinated = false;
bool isOutputLocked = false;
bool isScreenInverted = false;
bool isScreenDisabled = false;
bool isScreenSleeping = false;

// OLED Screen Content
String contentTop = "";
String contentBottom = "";
// loading: 0, ble: 1, wifi: 2, ap: 3, charging: 4, plugged in: 5,
// low battery: 6, sleep: 7, config: 8, caffeinated: 9, locked: 10,
// usb connected: 11
int contentIcon = 0;

// Set web server port number to 80
WebServer server(80);
ImprovWiFi improvSerial(&Serial);

CRGB leds[NUM_LEDS];

void onImprovWiFiErrorCb(ImprovTypes::Error err) {
    Serial.println("Error: " + String(err));
}

void onImprovWiFiConnectedCb(const char *ssid, const char *password) {
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

void setup() {
    Serial.begin(BAUD_RATE);

    delay(10);

    printSpacer();

    setCPUFrequency(240);

    printSpacer();

    // Config Voltage ADC Input pin6, pin7
    adcAttachPin(6);
    adcAttachPin(7);

    Serial.println("Starting Wire...");
    Wire.begin(SDA, SCL, 400000);

    printSpacer();

    Serial.println("Starting BLE work...");
    bleKeyboard.begin();
    // bleKeyboard.set_current_active_device(currentActiveDevice);
    usbKeyboard.begin();
    USB.begin();

    Serial.println("Starting u8g2...");
    u8g2.begin();

    printSpacer();

    Serial.println("Starting improv serial work...");
    improvSerial.setDeviceInfo(ImprovTypes::ChipFamily::CF_ESP32,
                               "Schnell Firmware", "1.0.0", "Schnell Keypad");
    improvSerial.onImprovError(onImprovWiFiErrorCb);
    improvSerial.onImprovConnected(onImprovWiFiConnectedCb);

    printSpacer();

    Serial.println("Configuring LEDs...");

    FastLED.addLeds<WS2812, LED_PIN_DIN, GRB>(leds, NUM_LEDS);

    FastLED.setBrightness(5);

    printSpacer();

    Serial.println("Configuring Configuration Buttons...");

    pinMode(CFG_BTN_PIN_0, INPUT_PULLUP);
    pinMode(CFG_BTN_PIN_1, INPUT_PULLUP);
    pinMode(CFG_BTN_PIN_2, INPUT_PULLUP);

    printSpacer();

    pcf8574RotaryExtension.encoder(encoderPinA, encoderPinB);
    pcf8574RotaryExtension.pinMode(encoderSW, INPUT_PULLUP);
    pcf8574RotaryExtension.pinMode(extensionBtn1, INPUT_PULLUP);
    pcf8574RotaryExtension.pinMode(extensionBtn2, INPUT_PULLUP);
    pcf8574RotaryExtension.pinMode(extensionBtn3, INPUT_PULLUP);
    pcf8574RotaryExtension.setLatency(0);

    if (pcf8574RotaryExtension.begin()) {
        Serial.println("Rotary Extension Board initialized");
        isRotaryExtensionConnected = true;
    } else {
        Serial.println("Rotary Extension Board not found");
        isRotaryExtensionConnected = false;
    }

    xTaskCreate(
        encoderExtBoardTask,      /* Task function. */
        "Encoder Ext Board Task", /* name of task. */
        5000,                     /* Stack size of task */
        NULL,                     /* parameter of the task */
        2,                        /* priority of the task */
        &TaskEncoderExtension /* Task handle to keep track of created task */
    );

    ESP32Encoder::useInternalWeakPullResistors = UP;

    onboardEncoders[0].attachHalfQuad(EC_PIN_A, EC_PIN_B);

    xTaskCreate(encoderTask,    /* Task function. */
                "Encoder Task", /* name of task. */
                5000,           /* Stack size of task */
                NULL,           /* parameter of the task */
                2,              /* priority of the task */
                &TaskEncoder    /* Task handle to keep track of created task */
    );

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
    keyConfigJSON = loadJSONFileAsString("keyconfig");

    StaticJsonDocument<256> doc;
    String configJSON = loadJSONFileAsString("system");
    deserializeJson(doc, configJSON);
    // String address = doc["currentActiveDeviceAddress"];
    // currentActiveDeviceAddress = address;

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
        5000,       /* Stack size of task */
        NULL,       /* parameter of the task */
        1,          /* priority of the task */
        &TaskLED,   /* Task handle to keep track of created task */
        0);         /* pin task to core 0 */

    xTaskCreatePinnedToCore(
        i2cTask,             /* Task function. */
        "I2C Related Tasks", /* name of task. */
        5000,                /* Stack size of task */
        NULL,                /* parameter of the task */
        1,                   /* priority of the task */
        &TaskI2C,            /* Task handle to keep track of created task */
        0);                  /* pin task to core 0 */

    printSpacer();

    Serial.println("Checking if ESP is wakeup from sleep...");
    esp_sleep_wakeup_cause_t wakeup_reason;

    wakeup_reason = esp_sleep_get_wakeup_cause();

    if (wakeup_reason != ESP_SLEEP_WAKEUP_EXT1) {
        Serial.println("Strating EEPROM...");
        EEPROM.begin(EEPROM_SIZE);
        byte savedLayoutIndex = EEPROM.read(EEPROM_ADDR_LAYOUT);
        if (savedLayoutIndex == 255) {
            Serial.println("EEPROM is empty, set default layout to 0");
            EEPROM.write(EEPROM_ADDR_LAYOUT, currentLayoutIndex);
            EEPROM.commit();
        } else {
            Serial.println("EEPROM saved layout index: " + savedLayoutIndex);
            currentLayoutIndex = savedLayoutIndex;
        }
    }

    printSpacer();

    Serial.println("Configuring input pin and keys...");
    initKeys();
    initMacros();

    printSpacer();

    if (bootWiFiMode) {
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

    // if (bootWiFiMode) {
    //     networkAnimation(u8g2);
    // } else {
    //     loadingAnimation(u8g2);
    // }

    while (true) {
        checkBattery();

        // if (isDetectingLastConnectedDevice && bleKeyboard.isConnected() &&
        //     bleKeyboard.getCounnectedCount() > 1) {
        // isDetectingLastConnectedDevice = false;
        // std::array<std::string, 2> addresses =
        //     bleKeyboard.getDevicesAddress();
        // for (int i = 0; i < addresses.size(); i++) {
        //     if (!currentActiveDeviceAddress.length()) {
        //         currentActiveDeviceAddress = String(addresses[i].c_str());
        //         StaticJsonDocument<128> doc;
        //         if (SPIFFS.begin()) {
        //             File config = SPIFFS.open("/system.json", "w");
        //             doc["currentActiveDeviceAddress"] =
        //                 currentActiveDeviceAddress;
        //             serializeJson(doc, config);
        //             config.close();
        //         }
        //     }
        //     if (currentActiveDeviceAddress.equals(
        //             String(addresses[i].c_str()))) {
        //         currentActiveDevice = i;
        //     }
        //     bleKeyboard.set_current_active_device(currentActiveDevice);
        // }
        // }

        // Update screen info
        if (isGoingToSleep) {
            contentBottom = "Going to sleep";
            contentIcon = 7;
        } else if (isOutputLocked) {
            contentIcon = 10;
        } else if (isCaffeinated) {
            contentIcon = 9;
        } else if (bootWiFiMode) {
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
            bool plugged = getUSBPowerState();
            if (!plugged) {
                isUsbMode = false;
            }
            // TODO: .IsChargingBattery();
            bool charging = false;
            if (plugged && charging) {
                result = "Charging";
                contentIcon = 4;
            } else if (plugged) {
                if (isUsbMode) {
                    result = "Plugged in [USB]";
                    contentIcon = 11;
                } else {
                    result = "Plugged in [BT]";
                    contentIcon = 5;
                }
                contentIcon = 5;
            } else if (batteryPercentage > 100) {
                result = "Reading battery...";
            } else {
                result = "Bat. " + (String)batteryPercentage + "%";
                if (isUsbMode) {
                    contentIcon = 11;
                } else {
                    contentIcon = 1;
                }
            }

            contentTop = result;
        }

        if (isSwitchingBootMode) {
            if (!bootWiFiMode) {
                contentIcon = 8;
                contentBottom = "> WiFi Mode <  ";
            } else {
                contentBottom = "> Standard Mode <";
            }
        }

        // Show connecting message when BLE is disconnected
        while (!isUsbMode && !bleKeyboard.isConnected()) {
            contentBottom = "Connecting BLE..";
            checkIdle();
            delay(100);
        }

        // Show config updated message after keyconfig updated
        if (configUpdated) {
            contentBottom = "Config Updated!";
            configUpdated = false;
            delay(1000);
        }

        checkIdle();

        // Show current pressed key info
        if (updateKeyInfo) {
            contentBottom = currentKeyInfo;
            updateKeyInfo = false;
        }

        // Idle message
        if (currentMillis - sleepPreviousMillis > 5000) {
            contentBottom = "@" + currentLayout;
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

        if (isGoingToSleep) {
            vTaskDelay(100 / portTICK_PERIOD_MS);
            leds[0] = CRGB::Black;
            FastLED.show();
            continue;
        }

        // Low battery LED blink
        if (isLowBattery) {
            if (currentMillis - ledPreviousMillis > 1000 &&
                currentMillis - ledPreviousMillis <= 1200) {
                leds[0] = CRGB::Black;
                FastLED.show();
            } else if (currentMillis - ledPreviousMillis > 1200 &&
                       currentMillis - ledPreviousMillis <= 1300) {
                leds[0] = CRGB::Red;
                FastLED.show();
            } else if (currentMillis - ledPreviousMillis > 1300 &&
                       currentMillis - ledPreviousMillis <= 1500) {
                leds[0] = CRGB::Black;
                FastLED.show();
            } else if (currentMillis - ledPreviousMillis > 1500) {
                leds[0] = CRGB::Red;
                FastLED.show();
                ledPreviousMillis = currentMillis;
            }
        } else {
            if (isScreenDisabled || isScreenSleeping) {
                leds[0] = CRGB::Green;
            } else if (!bleKeyboard.isConnected() && !isUsbMode) {
                leds[0] = CRGB::Blue;
                FastLED.show();
                delay(300);
                leds[0] = CRGB::Black;
                FastLED.show();
                delay(300);
            } else if (getUSBPowerState()) {
                leds[0] = CRGB::Green;
            } else {
                leds[0] = CRGB::Black;
            }
            FastLED.show();
        }

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
        improvSerial.handleSerial();
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
}

/**
 * Onboard rotary encoder scanning
 *
 */
void encoderTask(void *pvParameters) {
    long int value = onboardEncoders[0].getCount();
    long int lastValue = value;
    bool trigger = false;
    String direction = "";

    while (true) {
        value = onboardEncoders[0].getCount();

        if (value != lastValue) {
            if (value > lastValue) {  // CCW
                direction = "CCW";
            } else if (value < lastValue) {  // CW
                direction = "CW";
            }

            lastValue = value;

            if (trigger) {
                trigger = false;
            } else {
                trigger = true;
                vTaskDelay(10 / portTICK_PERIOD_MS);
                continue;
            }

            if (direction.equals("CCW")) {
                resetIdle();
                if (isUsbMode && !isOutputLocked) {
                    usbKeyboard.release(onboardRotaryEncoders[0].rotaryCCW);
                    usbKeyboard.write(onboardRotaryEncoders[0].rotaryCCW);
                } else if (!isOutputLocked) {
                    bleKeyboard.release(onboardRotaryEncoders[0].rotaryCCW);
                    bleKeyboard.write(onboardRotaryEncoders[0].rotaryCCW);
                }
                updateKeyInfo = true;
                currentKeyInfo = onboardRotaryEncoders[0].rotaryCCWInfo;
            } else if (direction.equals("CW")) {
                resetIdle();
                if (isUsbMode && !isOutputLocked) {
                    usbKeyboard.release(onboardRotaryEncoders[0].rotaryCW);
                    usbKeyboard.write(onboardRotaryEncoders[0].rotaryCW);
                } else if (!isOutputLocked) {
                    bleKeyboard.release(onboardRotaryEncoders[0].rotaryCW);
                    bleKeyboard.write(onboardRotaryEncoders[0].rotaryCW);
                }
                updateKeyInfo = true;
                currentKeyInfo = onboardRotaryEncoders[0].rotaryCWInfo;
            }
        }

        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

/**
 * Rotary encoder related tasks
 *
 */
void encoderExtBoardTask(void *pvParameters) {
    int value = 0;
    bool btnState = false;
    bool pinAState = false;
    bool pinBState = false;
    bool lastPinAState = false;
    bool lastPinBState = false;
    bool trigger = false;
    unsigned long rotaryEncoderLastButtonPress = 0;
    String direction = "";
    byte btnArray[] = {encoderSW, extensionBtn1, extensionBtn2, extensionBtn3};

    while (true) {
        if (isRotaryExtensionConnected) {
            // Scan for rotary encoder
            pinAState = pcf8574RotaryExtension.digitalRead(encoderPinA);
            pinBState = pcf8574RotaryExtension.digitalRead(encoderPinB);
            if (pinAState != lastPinAState || pinBState != lastPinBState) {
                if (pinAState == true && pinBState == true) {
                    if (lastPinAState == false && lastPinBState == true) {
                        direction = "CCW";
                    } else if (lastPinAState == true &&
                               lastPinBState == false) {
                        direction = "CW";
                    }
                    trigger = true;
                } else if (pinAState == false && pinBState == false) {
                    if (lastPinAState == false && lastPinBState == true) {
                        direction = "CW";
                    } else if (lastPinAState == true &&
                               lastPinBState == false) {
                        direction = "CCW";
                    }
                    trigger = true;
                }
                lastPinAState = pinAState;
                lastPinBState = pinBState;

                if (trigger) {
                    resetIdle();
                    if (direction.equals("CW")) {
                        if (isUsbMode && !isOutputLocked) {
                            usbKeyboard.write(
                                rotaryExtRotaryEncoders[0].rotaryCW);
                        } else if (!isOutputLocked) {
                            bleKeyboard.write(
                                rotaryExtRotaryEncoders[0].rotaryCW);
                        }
                        updateKeyInfo = true;
                        currentKeyInfo =
                            rotaryExtRotaryEncoders[0].rotaryCWInfo;
                    } else if (direction.equals("CCW")) {
                        if (isUsbMode && !isOutputLocked) {
                            usbKeyboard.write(
                                rotaryExtRotaryEncoders[0].rotaryCCW);
                        } else if (!isOutputLocked) {
                            bleKeyboard.write(
                                rotaryExtRotaryEncoders[0].rotaryCCW);
                        }
                        updateKeyInfo = true;
                        currentKeyInfo =
                            rotaryExtRotaryEncoders[0].rotaryCCWInfo;
                    }
                    trigger = false;
                }
            }

            // Scan for button press
            for (int i = 0; i < 4; i++) {
                btnState = pcf8574RotaryExtension.digitalRead(btnArray[i]);
                if (btnState == LOW) {
                    resetIdle();
                    switch (btnArray[i]) {
                        case encoderSW:
                            // when using keyboard.write
                            // if (millis() - rotaryEncoderLastButtonPress >
                            // 200) {
                            //     bleKeyboard.write(
                            //         rotaryExtRotaryEncoders[0].rotaryButton);
                            //     updateKeyInfo = true;
                            //     currentKeyInfo = rotaryExtRotaryEncoders[0]
                            //                          .rotaryButtonInfo;
                            // }
                            rotaryExtRotaryEncoders[0]
                                .rotaryButtonState = keyPress(
                                rotaryExtRotaryEncoders[0].rotaryButton,
                                rotaryExtRotaryEncoders[0].rotaryButtonInfo,
                                rotaryExtRotaryEncoders[0].rotaryButtonState);
                            break;
                            break;
                        case extensionBtn1:
                            rotaryExtKeyMap[0].state =
                                keyPress(rotaryExtKeyMap[0].keyStroke,
                                         rotaryExtKeyMap[0].keyInfo,
                                         rotaryExtKeyMap[0].state);
                            break;
                        case extensionBtn2:
                            rotaryExtKeyMap[1].state =
                                keyPress(rotaryExtKeyMap[1].keyStroke,
                                         rotaryExtKeyMap[1].keyInfo,
                                         rotaryExtKeyMap[1].state);
                            break;
                        case extensionBtn3:
                            rotaryExtKeyMap[2].state =
                                keyPress(rotaryExtKeyMap[2].keyStroke,
                                         rotaryExtKeyMap[2].keyInfo,
                                         rotaryExtKeyMap[2].state);
                            break;
                    }
                    rotaryEncoderLastButtonPress = millis();
                } else if (btnState == HIGH) {
                    switch (btnArray[i]) {
                        case encoderSW:
                            if (rotaryExtRotaryEncoders[0].rotaryButtonState) {
                                rotaryExtRotaryEncoders[0]
                                    .rotaryButtonState = keyRelease(
                                    rotaryExtRotaryEncoders[0].rotaryButton,
                                    rotaryExtRotaryEncoders[0].rotaryButtonInfo,
                                    rotaryExtRotaryEncoders[0]
                                        .rotaryButtonState);
                            }
                            break;
                        case extensionBtn1:
                            if (rotaryExtKeyMap[0].state) {
                                rotaryExtKeyMap[0].state =
                                    keyRelease(rotaryExtKeyMap[0].keyStroke,
                                               rotaryExtKeyMap[0].keyInfo,
                                               rotaryExtKeyMap[0].state);
                            }
                            break;
                        case extensionBtn2:
                            if (rotaryExtKeyMap[1].state) {
                                rotaryExtKeyMap[1].state =
                                    keyRelease(rotaryExtKeyMap[1].keyStroke,
                                               rotaryExtKeyMap[1].keyInfo,
                                               rotaryExtKeyMap[1].state);
                            }
                            break;
                        case extensionBtn3:
                            if (rotaryExtKeyMap[2].state) {
                                rotaryExtKeyMap[2].state =
                                    keyRelease(rotaryExtKeyMap[2].keyStroke,
                                               rotaryExtKeyMap[2].keyInfo,
                                               rotaryExtKeyMap[2].state);
                            }
                            break;
                    }
                }
            }
        }

        vTaskDelay(3 / portTICK_PERIOD_MS);
    }
}

void i2cTask(void *pvParameters) {
    byte error;
    byte devices[1] = {ENCODER_EXTENSION_ADDR};
    while (true) {
        renderScreen();

        for (byte i : devices) {
            Wire.beginTransmission(devices[i]);
            error = Wire.endTransmission();

            if (devices[i] == ENCODER_EXTENSION_ADDR) {
                isRotaryExtensionConnected = (error == 0);
            }
        }

        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
}

/**
 * Main loop for keyboard matrix scan
 *
 */
void loop() {
    // Check every keystroke is pressed or not when connected
    if (keymapsNeedsUpdate) {
        updateKeymaps();
    }

    if (isGoingToSleep) {
        return;
    }

    // Keypad scan
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
                    } else if (r == 3 && c == 0) {
                        isUsbMode = !isUsbMode;
                        usbKeyboard.releaseAll();
                        bleKeyboard.releaseAll();
                        delay(300);
                    } else if (r == 4 && c == 4) {
                        switchLayout();
                    } else if (r == 3 && c == 3) {
                        isCaffeinated = !isCaffeinated;
                        delay(300);
                    } else if (r == 3 && c == 4) {
                        isOutputLocked = !isOutputLocked;
                        delay(300);
                    } else if (r == 1 && c == 6) {
                        isScreenDisabled = !isScreenDisabled;
                        delay(300);
                    } else if (r == 3 && c == 6) {
                        isScreenInverted = !isScreenInverted;
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
                } else if (keyMap[r][c].keyInfo.startsWith("TT_")) {
                    // Tap-Toggle press
                    if (!isTemporaryToggled) {
                        size_t index =
                            keyMap[r][c]
                                .keyInfo
                                .substring(3, sizeof(keyMap[r][c].keyInfo) - 1)
                                .toInt();
                        tapToggleActive(index);
                    }
                } else {
                    // Standard key press
                    keyPress(keyMap[r][c]);
                }
            } else {
                if (keyMap[r][c].keyInfo.startsWith("TT_") &&
                    isTemporaryToggled) {
                    tapToggleRelease(tapToggleOrginalLayerIndex);
                }
                keyRelease(keyMap[r][c]);
            }
            delayMicroseconds(10);
        }
        digitalWrite(outputs[r], HIGH);  // Setting the row back to high
        delayMicroseconds(10);
    }

    // Read Bi-Directional Switch input
    if (digitalRead(BD_SW_CW) == ACTIVE) {
        resetIdle();
        switchLayout(currentLayoutIndex + 1);
        while (digitalRead(BD_SW_CW) == ACTIVE) {
            delay(10);
        }
    } else if (digitalRead(BD_SW_CCW) == ACTIVE) {
        resetIdle();
        switchLayout(currentLayoutIndex - 1);
        while (digitalRead(BD_SW_CCW) == ACTIVE) {
            delay(10);
        }
    } else if (digitalRead(BD_SW_PUSH) == ACTIVE) {
        resetIdle();
        switchLayout(0);
        while (digitalRead(BD_SW_PUSH) == ACTIVE) {
            delay(10);
        }
    }

    readConfigButtons();
}

/**
 * Initialize every Key instance that used in this program
 *
 */
void initKeys() {
    Serial.println("Reading JSON keymap configuration...");

    DynamicJsonDocument doc(jsonDocSize);
    DeserializationError err = deserializeJson(
        doc, keyConfigJSON, DeserializationOption::NestingLimit(5));
    if (err) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(err.c_str());
    }

    uint8_t keyLayout[ROWS][COLS];
    String keyInfo[ROWS][COLS];

    copyArray(doc["keyConfig"][currentLayoutIndex]["keymap"], keyLayout);
    copyArray(doc["keyConfig"][currentLayoutIndex]["keyInfo"], keyInfo);

    layoutLength = doc["keyConfig"].size();

    // GPIO configuration
    for (int i = 0; i < outputCount; i++) {
        pinMode(outputs[i], OUTPUT);
        digitalWrite(outputs[i], HIGH);
    }

    for (int i = 0; i < inputCount; i++) {
        pinMode(inputs[i], INPUT_PULLUP);
    }

    // Bi-Direction (/w Push) Switch
    pinMode(BD_SW_CW, INPUT_PULLUP);
    pinMode(BD_SW_CCW, INPUT_PULLUP);
    pinMode(BD_SW_PUSH, INPUT_PULLUP);

    // Assign keymap data
    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++) {
            keyMap[r][c].keyStroke = keyLayout[r][c];
            keyMap[r][c].keyInfo = keyInfo[r][c];
            keyMap[r][c].state = false;
        }
    }

    // Load Onboard Rotary Encoder config from doc["onBoardRotaryEncoder"]
    uint8_t onboardRotaryEncoderRotaryMap[3];
    String onboardRotaryEncoderInfo[3];
    if (doc["onBoardRotaryEncoder"].isNull()) {
        Serial.println("No onboard rotary encoder config found");
    } else {
        copyArray(doc["onBoardRotaryEncoder"][currentLayoutIndex]["rotaryMap"],
                  onboardRotaryEncoderRotaryMap);
        copyArray(doc["onBoardRotaryEncoder"][currentLayoutIndex]["rotaryInfo"],
                  onboardRotaryEncoderInfo);

        // Assign rotary encoder data
        for (int i = 0; i < sizeof(onboardRotaryEncoders) /
                                sizeof(onboardRotaryEncoders[0]);
             i++) {
            onboardRotaryEncoders[0].rotaryButton =
                onboardRotaryEncoderRotaryMap[0];
            onboardRotaryEncoders[0].rotaryButtonInfo =
                onboardRotaryEncoderInfo[0];
            onboardRotaryEncoders[0].rotaryButtonState = false;
            onboardRotaryEncoders[0].rotaryCCW =
                onboardRotaryEncoderRotaryMap[1];
            onboardRotaryEncoders[0].rotaryCW =
                onboardRotaryEncoderRotaryMap[2];
            onboardRotaryEncoders[0].rotaryCCWInfo =
                onboardRotaryEncoderInfo[1];
            onboardRotaryEncoders[0].rotaryCWInfo = onboardRotaryEncoderInfo[2];
        }
    }

    // Load Rotary Extension config from doc["rotaryExtension"]
    uint8_t rotaryExtKeyLayout[3];
    String rotaryExtKeyInfo[3];
    uint8_t rotaryExtRotaryMap[3];
    String rotaryExtRotaryInfo[3];
    if (doc["rotaryExtension"].isNull()) {
        Serial.println("No rotary extension config found");
    } else {
        copyArray(doc["rotaryExtension"][currentLayoutIndex]["keymap"],
                  rotaryExtKeyLayout);
        copyArray(doc["rotaryExtension"][currentLayoutIndex]["keyInfo"],
                  rotaryExtKeyInfo);
        copyArray(doc["rotaryExtension"][currentLayoutIndex]["rotaryMap"],
                  rotaryExtRotaryMap);
        copyArray(doc["rotaryExtension"][currentLayoutIndex]["rotaryInfo"],
                  rotaryExtRotaryInfo);

        // Assign keymap data
        for (int i = 0; i < 3; i++) {
            rotaryExtKeyMap[i].keyStroke = rotaryExtKeyLayout[i];
            rotaryExtKeyMap[i].keyInfo = rotaryExtKeyInfo[i];
            rotaryExtKeyMap[i].state = false;
        }
        // Assign rotary encoder data
        rotaryExtRotaryEncoders[0].rotaryButton = rotaryExtRotaryMap[0];
        rotaryExtRotaryEncoders[0].rotaryButtonInfo = rotaryExtRotaryInfo[0];
        rotaryExtRotaryEncoders[0].rotaryButtonState = false;
        rotaryExtRotaryEncoders[0].rotaryCCW = rotaryExtRotaryMap[1];
        rotaryExtRotaryEncoders[0].rotaryCW = rotaryExtRotaryMap[2];
        rotaryExtRotaryEncoders[0].rotaryCCWInfo = rotaryExtRotaryInfo[1];
        rotaryExtRotaryEncoders[0].rotaryCWInfo = rotaryExtRotaryInfo[2];
    }

    // Show layout title on screen
    String str = doc["keyConfig"][currentLayoutIndex]["title"];
    currentLayout = str;
    contentBottom = "@" + currentLayout;

    EEPROM.write(EEPROM_ADDR_LAYOUT, currentLayoutIndex);
    Serial.println("Key layout loaded: " + currentLayout);
}

/**
 * Initialize every Macro instance that used in this program
 *
 */
void initMacros() {
    DynamicJsonDocument doc(jsonDocSize);
    DeserializationError err = deserializeJson(
        doc, keyConfigJSON, DeserializationOption::NestingLimit(5));
    if (err) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(err.c_str());
    }
    size_t macrosLength = doc["macros"].size();
    for (size_t i = 0; i < macrosLength; i++) {
        uint8_t macroLayout[] = {0, 0, 0, 0, 0, 0};
        String macroNameStr = doc["macros"][i]["name"];
        String macroStringContent = doc["macros"][i]["stringContent"];
        macroMap[i].type = doc["macros"][i]["type"];
        macroMap[i].macroInfo = macroNameStr;
        macroMap[i].stringContent = macroStringContent;
        copyArray(doc["macros"][i]["keyStrokes"], macroLayout);
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

    keyConfigJSON = "";

    Serial.println("Loading config files from SPIFFS...");
    keyConfigJSON = loadJSONFileAsString("keyconfig");

    initKeys();
    initMacros();

    keymapsNeedsUpdate = false;
    configUpdated = true;
}

void readConfigButtons() {
    int longPressCounter = 0;
    if (digitalRead(CFG_BTN_PIN_1) == ACTIVE) {
        resetIdle();
        while (digitalRead(CFG_BTN_PIN_1) == ACTIVE) {
            if (digitalRead(CFG_BTN_PIN_2) == ACTIVE) {
                resetConfigFiles();
                return;
            }
            delay(10);
            if (longPressCounter > 100) {
                goSleeping();
            }
            longPressCounter++;
        }
        isOutputLocked = !isOutputLocked;
    } else if (digitalRead(CFG_BTN_PIN_2) == ACTIVE) {
        resetIdle();
        while (digitalRead(CFG_BTN_PIN_2) == ACTIVE) {
            if (digitalRead(CFG_BTN_PIN_1) == ACTIVE) {
                resetConfigFiles();
                return;
            }
            delay(10);
            if (longPressCounter > 100) {
                isCaffeinated = !isCaffeinated;
                while (digitalRead(CFG_BTN_PIN_2) == ACTIVE) {
                    delay(10);
                }
                return;
            }
            longPressCounter++;
        }
        if (!isScreenSleeping) {
            isScreenDisabled = !isScreenDisabled;
        }
    } else if (digitalRead(CFG_BTN_PIN_0) == ACTIVE) {
        resetIdle();
        while (digitalRead(CFG_BTN_PIN_0) == ACTIVE) {
            delay(10);
            if (longPressCounter > 100) {
                switchBootMode();
            }
            longPressCounter++;
        }
        isUsbMode = !isUsbMode;
        usbKeyboard.releaseAll();
        bleKeyboard.releaseAll();
    }
}

void resetConfigFiles() {
    resetIdle();
    for (byte countDown = 5; 0 < countDown; countDown--) {
        updateKeyInfo = true;
        currentKeyInfo = "Reset config in " + (String)countDown;

        if (digitalRead(CFG_BTN_PIN_1) != ACTIVE ||
            digitalRead(CFG_BTN_PIN_2) != ACTIVE) {
            updateKeyInfo = true;
            currentKeyInfo = "Reset canceled";
            delay(1000);
            return;
        }
        delay(1000);
    }
    updateKeyInfo = true;
    currentKeyInfo = "Resetting config...";
    delay(500);
    if (SPIFFS.begin()) {
        File configFile = SPIFFS.open("/config.default.json", "r");
        File keyConfigFile = SPIFFS.open("/keyconfig.default.json", "r");
        File configFileWrite = SPIFFS.open("/config.json", "w");
        File keyConfigFileWrite = SPIFFS.open("/keyconfig.json", "w");
        if (configFile && configFileWrite) {
            while (configFile.available()) {
                configFileWrite.write(configFile.read());
            }
            configFile.close();
            configFileWrite.close();
        }
        if (keyConfigFile && keyConfigFileWrite) {
            while (keyConfigFile.available()) {
                keyConfigFileWrite.write(keyConfigFile.read());
            }
            keyConfigFile.close();
            keyConfigFileWrite.close();
        }
        currentLayoutIndex = 0;
        keymapsNeedsUpdate = true;
    }
    return;
}

/**
 * Press USB HID keys. (Fix for modifier keys)
 *
 * @param {uint8_t} keyStroke the key stroke to be pressed
 */
void usbKeyboardPress(uint8_t keyStroke) {
    switch (keyStroke) {
        case 128:  // KEY_LEFT_CTRL
            usbKeyboard.pressRaw(0xe0);
            break;
        case 129:  // KEY_LEFT_SHIFT
            usbKeyboard.pressRaw(0xe1);
            break;
        case 130:  // KEY_LEFT_ALT
            usbKeyboard.pressRaw(0xe2);
            break;
        case 131:  // KEY_LEFT_GUI
            usbKeyboard.pressRaw(0xe3);
            break;
        case 132:  // KEY_RIGHT_CTRL
            usbKeyboard.pressRaw(0xe4);
            break;
        case 133:  // KEY_RIGHT_SHIFT
            usbKeyboard.pressRaw(0xe5);
            break;
        case 134:  // KEY_RIGHT_ALT
            usbKeyboard.pressRaw(0xe6);
            break;
        case 135:  // KEY_RIGHT_GUI
            usbKeyboard.pressRaw(0xe7);
            break;
        default:
            usbKeyboard.press(keyStroke);
    }
}

/**
 * Release USB HID keys. (Fix for modifier keys)
 *
 * @param {uint8_t} keyStroke the key stroke to be pressed
 */
void usbKeyboardRelease(uint8_t keyStroke) {
    switch (keyStroke) {
        case 128:  // KEY_LEFT_CTRL
            usbKeyboard.releaseRaw(0xe0);
            break;
        case 129:  // KEY_LEFT_SHIFT
            usbKeyboard.releaseRaw(0xe1);
            break;
        case 130:  // KEY_LEFT_ALT
            usbKeyboard.releaseRaw(0xe2);
            break;
        case 131:  // KEY_LEFT_GUI
            usbKeyboard.releaseRaw(0xe3);
            break;
        case 132:  // KEY_RIGHT_CTRL
            usbKeyboard.releaseRaw(0xe4);
            break;
        case 133:  // KEY_RIGHT_SHIFT
            usbKeyboard.releaseRaw(0xe5);
            break;
        case 134:  // KEY_RIGHT_ALT
            usbKeyboard.releaseRaw(0xe6);
            break;
        case 135:  // KEY_RIGHT_GUI
            usbKeyboard.releaseRaw(0xe7);
            break;
        default:
            usbKeyboard.release(keyStroke);
    }
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
    if (key.state == false && !isOutputLocked) {
        if (isUsbMode) {
            usbKeyboardPress(key.keyStroke);
        } else {
            bleKeyboard.press(key.keyStroke);
        }
    }
    key.state = true;
    updateKeyInfo = true;
    currentKeyInfo = key.keyInfo;
}

bool keyPress(uint8_t keyStroke, String keyInfo, bool keyState) {
    if (keyInfo == "FN") {
        isFnKeyPressed = true;
    }
    if (keyState == false && !isOutputLocked) {
        if (isUsbMode) {
            usbKeyboardPress(keyStroke);
        } else {
            bleKeyboard.press(keyStroke);
        }
    }
    keyState = true;
    updateKeyInfo = true;
    currentKeyInfo = keyInfo;
    return keyState;
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
    if (key.state == true && !isOutputLocked) {
        if (isUsbMode) {
            usbKeyboardRelease(key.keyStroke);
        } else {
            bleKeyboard.release(key.keyStroke);
        }
    }
    key.state = false;
    return;
}

bool keyRelease(uint8_t keyStroke, String keyInfo, bool keyState) {
    if (keyInfo == "FN") {
        isFnKeyPressed = false;
    }
    if (keyState == true && !isOutputLocked) {
        if (isUsbMode) {
            usbKeyboardRelease(keyStroke);
        } else {
            bleKeyboard.release(keyStroke);
        }
    }
    keyState = false;
    return keyState;
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
            if (isUsbMode) {
                usbKeyboard.press(macro.keyStrokes[i]);
            } else {
                bleKeyboard.press(macro.keyStrokes[i]);
            }
            delayMicroseconds(10);
        }
        delay(50);
        if (isUsbMode) {
            usbKeyboard.releaseAll();
        } else {
            bleKeyboard.releaseAll();
        }
    } else if (macro.type == 1) {
        if (isUsbMode) {
            usbKeyboard.print(macro.stringContent);
        } else {
            bleKeyboard.print(macro.stringContent);
        }
    } else if (macro.type == 2) {
        if (isUsbMode) {
            usbKeyboard.println(macro.stringContent);
        } else {
            bleKeyboard.println(macro.stringContent);
        }
    }
    delay(100);
}

/**
 * Tap toggle layer
 *
 * @param {size_t} index of the layer to be toggled
 */
void tapToggleActive(size_t index) {
    isTemporaryToggled = true;
    tapToggleOrginalLayerIndex = currentLayoutIndex;
    currentLayoutIndex = index;
    initKeys();
}

/**
 * Tap toggle layer release
 *
 * @param {size_t} original layer index of the layer to be restored
 */
void tapToggleRelease(size_t orginalLayerIndex) {
    isTemporaryToggled = false;
    tapToggleCount++;
    if (currentMillis - tapTogglePreviousMillis < 300) {
        if (tapToggleCount > 1) {
            tapToggleCount = 0;
            currentLayoutIndex = orginalLayerIndex;
        }
    } else {
        tapToggleCount = 0;
        currentLayoutIndex = orginalLayerIndex;
    }
    tapTogglePreviousMillis = currentMillis;
    initKeys();
}

/**
 * Switch keymap layout
 *
 */
void switchLayout() {
    currentLayoutIndex =
        currentLayoutIndex < layoutLength - 1 ? currentLayoutIndex + 1 : 0;
    initKeys();
    delay(300);
}

// overload switchLayout() to accept layout index as parameter
void switchLayout(int layoutIndex) {
    if (layoutIndex > layoutLength - 1) {
        layoutIndex = 0;
    } else if (layoutIndex < 0) {
        layoutIndex = layoutLength - 1;
    }
    currentLayoutIndex = layoutIndex;
    initKeys();
}

// input layout name as string and find the index of that layout
int findLayoutIndex(String layoutName) {
    DynamicJsonDocument doc(jsonDocSize);
    DeserializationError err = deserializeJson(
        doc, keyConfigJSON, DeserializationOption::NestingLimit(5));
    if (err) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(err.c_str());
    }
    size_t layoutLength = doc.size();
    for (size_t i = 0; i < layoutLength; i++) {
        String str = doc[i]["title"];
        if (str.equalsIgnoreCase(layoutName)) {
            return i;
        }
    }
    return -1;
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
        case 8:
            u8g2.drawGlyph(0, 16, 0x11A);
            break;
        case 9:
            u8g2.drawGlyph(0, 16, 0xCA);
            break;
        case 10:
            u8g2.drawGlyph(0, 16, 0xA5);
            break;
        case 11:
            u8g2.drawGlyph(0, 16, 0xDE);
            break;
        default:
            u8g2.drawGlyph(0, 16, 0x00);
    }

    while (clearDisplay || isScreenDisabled || isScreenSleeping) {
        u8g2.clearBuffer();
        u8g2.sendBuffer();
        delay(100);
    }

    if (isScreenInverted) {
        u8g2.drawBox(0, 0, 192, 64);
        u8g2.setDrawColor(2);
    }

    u8g2.sendBuffer();
}

/**
 * Return Battery Current Percentage
 *
 */
int getBatteryPercentage() {
    const float minVoltage = 3, fullVolatge = 3.8;

    int raw = analogRead(6);
    float batteryVoltage = raw * V_REF / 4096.0 * VOLTAGE_DIVIDER_RATIO;

    Serial.println((String) "Battery Voltage: " + batteryVoltage);

    // Convert to percentage
    float percentage =
        (batteryVoltage - minVoltage) / (fullVolatge - minVoltage) * 100;

    // Percentage step by 5
    percentage = round(percentage / 5) * 5;

    // Update device's battery level
    bleKeyboard.setBatteryLevel(percentage);

    return percentage > 100 ? 100 : percentage;
}

/**
 * Return true if USB bus power is detected
 *
 */
bool getUSBPowerState() {
    int rawUsb = analogRead(7);
    float voltageUsb = rawUsb * V_REF / 4096.0 * VOLTAGE_DIVIDER_RATIO;
    if (voltageUsb > 4) {
        return true;
    } else {
        return false;
    }
}

/**
 * Enter deep sleep mode
 *
 */
void goSleeping() {
    isGoingToSleep = true;
    EEPROM.commit();
    delay(1000);
    // Column pins
    rtc_gpio_pulldown_dis(GPIO_NUM_5);
    rtc_gpio_pullup_en(GPIO_NUM_5);
    rtc_gpio_isolate(GPIO_NUM_5);
    rtc_gpio_pulldown_dis(GPIO_NUM_4);
    rtc_gpio_pullup_en(GPIO_NUM_4);
    rtc_gpio_isolate(GPIO_NUM_4);
    // Row pins
    rtc_gpio_pullup_dis(GPIO_NUM_12);
    rtc_gpio_pulldown_en(GPIO_NUM_12);
    clearDisplay = true;
    for (int i = 0; i < NUM_LEDS; i++) {
        leds[i] = CRGB::Black;
    }
    FastLED.show();
    delay(500);
    esp_deep_sleep_start();
}

/**
 * Switching between different boot modes
 *
 */
void switchBootMode() {
    isSwitchingBootMode = true;
    Serial.println("Resetting...");
    if (bootWiFiMode) {
        WiFi.disconnect();
        WiFi.softAPdisconnect(true);
    }
    delay(300);
    esp_sleep_enable_timer_wakeup(1);
    bootWiFiMode = !bootWiFiMode;
    esp_deep_sleep_start();
}

/**
 * Check if device is idle for a specified period to determine if it should
 * go to sleep or not.
 *
 */
void checkIdle() {
    if (!isCaffeinated &&
        currentMillis - sleepPreviousMillis > SLEEP_INTERVAL &&
        !getUSBPowerState()) {
        goSleeping();
    } else if (!isCaffeinated &&
               currentMillis - sleepPreviousMillis > SCREEN_SLEEP_INTERVAL) {
        isScreenSleeping = true;
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
 * Update sleepPreviousMillis' value to reset idle timer
 *
 */
void resetIdle() {
    sleepPreviousMillis = currentMillis;
    isScreenSleeping = false;
}

/**
 * Activate WiFi and start the server
 *
 */
void initWebServer() {
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