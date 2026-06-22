#include <main.hpp>

RTC_DATA_ATTR unsigned int timeSinceBoot = 0;
RTC_DATA_ATTR bool bootWiFiMode = false;

U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C u8g2(U8G2_R0);

UsbKeyboardOutput usbOutput;
BleKeyboardOutput bleOutput;

PCF8574 pcf8574RotaryExtension(ENCODER_EXTENSION_ADDR);
volatile bool isRotaryExtensionConnected = false;

TaskHandle_t TaskGeneralStatusCheck;
TaskHandle_t TaskLED;
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

ConfigStore configStore;
volatile bool isFnKeyPressed = false;
bool isDetectingLastConnectedDevice = true;
RTC_DATA_ATTR byte currentLayoutIndex = 0;
RTC_DATA_ATTR byte currentActiveDevice = 0;
RTC_DATA_ATTR String currentActiveDeviceAddress = "";
RTC_DATA_ATTR volatile bool isUsbMode = true;

// Active keyboard output for the current mode. Routes key events to USB or BLE
// so callers no longer branch on isUsbMode.
static KeyboardOutput &kbd() {
    return isUsbMode ? static_cast<KeyboardOutput &>(usbOutput)
                     : static_cast<KeyboardOutput &>(bleOutput);
}

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

// Updated in ledTask, read by every task's timing checks across both cores.
volatile unsigned long currentMillis = 0;

volatile bool isLowBattery = false;
int batteryPercentage = 101;

// Mode/status flags shared between tasks on both cores. volatile guarantees
// each task reads the current value rather than a cached copy. (Aligned bool
// access is already atomic on the ESP32, so no torn reads.)
volatile bool keymapsNeedsUpdate = false;
volatile bool configUpdated = false;
volatile bool isSoftAPEnabled = false;
volatile bool isGoingToSleep = false;
volatile bool clearDisplay = false;
volatile bool isSwitchingBootMode = false;
volatile bool isCaffeinated = false;
volatile bool isOutputLocked = false;
volatile bool isScreenInverted = false;
volatile bool isScreenDisabled = false;
volatile bool isScreenSleeping = false;

// OLED screen content lives in the Display module (display_state.h). Icon codes:
// loading: 0, ble: 1, wifi: 2, ap: 3, charging: 4, plugged in: 5,
// low battery: 6, sleep: 7, config: 8, caffeinated: 9, locked: 10,
// usb connected: 11

CRGB leds[NUM_LEDS];

void setup() {
    Serial.begin(BAUD_RATE);

    delay(10);

    // Guard the shared screen state before any task can touch it.
    Display::begin();

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
    BleHid::begin();
    // bleKeyboard.set_current_active_device(currentActiveDevice);
    UsbHid::begin();

    Serial.println("Starting u8g2...");
    u8g2.begin();

    printSpacer();

    Serial.println("Starting improv serial work...");
    setupImprov();

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
    configStore.reload();

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
            Serial.println("EEPROM saved layout index: " +
                           String(savedLayoutIndex));
            currentLayoutIndex = savedLayoutIndex;
        }
    }

    printSpacer();

    Serial.println("Configuring input pin and keys...");
    initKeys();
    initMacros();

    printSpacer();

    if (bootWiFiMode) {
        initWebServer(AP_SSID, MDNS_NAME);
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
        
        // if (isDetectingLastConnectedDevice && BleHid::isConnected() &&
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
        String result = "";
        if (isGoingToSleep) {
            Display::setBottom("Going to sleep");
            Display::setIcon(7);
        } else if (isOutputLocked) {
            Display::setIcon(10);
            result = "Bat. " + (String)batteryPercentage + "%";
            Display::setTop(result);
        } else if (isCaffeinated) {
            Display::setIcon(9);
            result = "Bat. " + (String)batteryPercentage + "%";
            Display::setTop(result);
        } else if (bootWiFiMode) {
            String networkInfo = "";
            if (currentMillis - networkInfoPreviousMillis <
                NETWORK_INFO_INTERVAL) {
                networkInfo = (String)WiFi.localIP().toString().c_str();
                Display::setIcon(2);
            } else if ((currentMillis - networkInfoPreviousMillis) <
                       (NETWORK_INFO_INTERVAL * 2)) {
                networkInfo = (String)MDNS_NAME + ".local";
                Display::setIcon(2);
            } else {
                networkInfo = (String)MDNS_NAME + ".local";
                networkInfoPreviousMillis = currentMillis;
                Display::setIcon(2);
            }
            if (isSoftAPEnabled) {
                Display::setIcon(3);
            } else if (WiFi.localIP().toString() == "0.0.0.0") {
                networkInfo = "Connecting to...";
                Display::setIcon(2);
            }
            Display::setTop(networkInfo);
        } else {
            bool plugged = getUSBPowerState();
            if (!plugged) {
                isUsbMode = false;
            }
            // TODO: .IsChargingBattery();
            bool charging = false;
            if (plugged && charging) {
                result = "Charging";
                Display::setIcon(4);
            } else if (plugged) {
                if (isUsbMode) {
                    result = "Plugged in [USB]";
                    Display::setIcon(11);
                } else {
                    result = "Plugged in [BT]";
                    Display::setIcon(5);
                }
                Display::setIcon(5);
            } else if (batteryPercentage > 100) {
                result = "Reading battery...";
            } else {
                result = "Bat. " + (String)batteryPercentage + "%";
                if (isUsbMode) {
                    Display::setIcon(11);
                } else {
                    Display::setIcon(1);
                }
            }

            Display::setTop(result);
        }

        if (isSwitchingBootMode) {
            if (!bootWiFiMode) {
                Display::setIcon(8);
                Display::setBottom("> WiFi Mode <  ");
            } else {
                Display::setBottom("> Standard Mode <");
            }
        }

        // Show connecting message when BLE is disconnected
        while (!isUsbMode && !BleHid::isConnected()) {
            Display::setBottom("Connecting BLE..");
            checkIdle();
            delay(100);
        }

        // Show config updated message after keyconfig updated
        if (configUpdated) {
            Display::setBottom("Config Updated!");
            configUpdated = false;
            delay(1000);
        }

        checkIdle();

        // Show current pressed key info
        String keyInfo;
        if (Display::takeKeyInfo(keyInfo)) {
            Display::setBottom(keyInfo);
        }

        // Idle message
        if (currentMillis - sleepPreviousMillis > 5000) {
            Display::setBottom("@" + currentLayout);
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
            } else if (!BleHid::isConnected() && !isUsbMode) {
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
                emitEncoderTurn(onboardRotaryEncoders[0].rotaryCCW,
                                onboardRotaryEncoders[0].rotaryCCWInfo);
            } else if (direction.equals("CW")) {
                emitEncoderTurn(onboardRotaryEncoders[0].rotaryCW,
                                onboardRotaryEncoders[0].rotaryCWInfo);
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
                        if (!isOutputLocked) {
                            kbd().write(rotaryExtRotaryEncoders[0].rotaryCW);
                        }
                        Display::setKeyInfo(
                            rotaryExtRotaryEncoders[0].rotaryCWInfo);
                    } else if (direction.equals("CCW")) {
                        if (!isOutputLocked) {
                            kbd().write(rotaryExtRotaryEncoders[0].rotaryCCW);
                        }
                        Display::setKeyInfo(
                            rotaryExtRotaryEncoders[0].rotaryCCWInfo);
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
                            keyPress(rotaryExtRotaryEncoders[0].button);
                            break;
                        case extensionBtn1:
                            keyPress(rotaryExtKeyMap[0]);
                            break;
                        case extensionBtn2:
                            keyPress(rotaryExtKeyMap[1]);
                            break;
                        case extensionBtn3:
                            keyPress(rotaryExtKeyMap[2]);
                            break;
                    }
                    rotaryEncoderLastButtonPress = millis();
                } else if (btnState == HIGH) {
                    switch (btnArray[i]) {
                        case encoderSW:
                            if (rotaryExtRotaryEncoders[0].button.state) {
                                keyRelease(rotaryExtRotaryEncoders[0].button);
                            }
                            break;
                        case extensionBtn1:
                            if (rotaryExtKeyMap[0].state) {
                                keyRelease(rotaryExtKeyMap[0]);
                            }
                            break;
                        case extensionBtn2:
                            if (rotaryExtKeyMap[1].state) {
                                keyRelease(rotaryExtKeyMap[1]);
                            }
                            break;
                        case extensionBtn3:
                            if (rotaryExtKeyMap[2].state) {
                                keyRelease(rotaryExtKeyMap[2]);
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

        for (byte addr : devices) {
            Wire.beginTransmission(addr);
            error = Wire.endTransmission();

            if (addr == ENCODER_EXTENSION_ADDR) {
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

    // Accept Serial input for keyconfig.json
    if (Serial.available() > 0) {
        String jsonString = Serial.readString();
        jsonString.trim();

        // Config read request: dump the current keyconfig.json (wrapped in
        // markers) so the configuration tool can import what's on the device.
        // Emit as a single write to minimize interleaving with Serial output
        // from tasks running on the other core.
        if (jsonString == "READ_CONFIG") {
            Serial.print("\n<<<CONFIG_BEGIN>>>\n" +
                         loadJSONFileAsString("keyconfig") +
                         "\n<<<CONFIG_END>>>\n");
            return;
        }

        Serial.println("Received JSON:");
        Serial.println(jsonString);

        // Parse JSON
        DynamicJsonDocument doc(jsonDocSize);
        deserializeJson(doc, jsonString);

        // Check if JSON overflowed
        if (doc.overflowed() || doc.isNull()) {
            Serial.println("JSON overflowed or empty");
        } else {
            // Save JSON to SPIFFS as keyconfig.json
            File configFile = SPIFFS.open("/keyconfig.json", "w");
            if (!configFile) {
                Serial.println("Failed to open config file for writing");
            }
            if (!serializeJson(doc, configFile)) {
                Serial.println("Failed to write to config file");
            }
            configFile.close();

            // Reload keymaps
            keymapsNeedsUpdate = true;

            // Show config updated message
            configUpdated = true;
            Serial.println("Config updated!");
        }
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
                        UsbHid::releaseAll();
                        BleHid::releaseAll();
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
                    macroPressByInfo(keyMap[r][c].keyInfo);
                } else if (keyMap[r][c].keyInfo.startsWith("TT_")) {
                    // Tap-Toggle press
                    if (!isTemporaryToggled) {
                        size_t index =
                            keyMap[r][c]
                                .keyInfo.substring(3)
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

    JsonDocument &doc = configStore.doc();

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
            onboardRotaryEncoders[0].button.keyStroke =
                onboardRotaryEncoderRotaryMap[0];
            onboardRotaryEncoders[0].button.keyInfo =
                onboardRotaryEncoderInfo[0];
            onboardRotaryEncoders[0].button.state = false;
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
        rotaryExtRotaryEncoders[0].button.keyStroke = rotaryExtRotaryMap[0];
        rotaryExtRotaryEncoders[0].button.keyInfo = rotaryExtRotaryInfo[0];
        rotaryExtRotaryEncoders[0].button.state = false;
        rotaryExtRotaryEncoders[0].rotaryCCW = rotaryExtRotaryMap[1];
        rotaryExtRotaryEncoders[0].rotaryCW = rotaryExtRotaryMap[2];
        rotaryExtRotaryEncoders[0].rotaryCCWInfo = rotaryExtRotaryInfo[1];
        rotaryExtRotaryEncoders[0].rotaryCWInfo = rotaryExtRotaryInfo[2];
    }

    // Show layout title on screen
    String str = doc["keyConfig"][currentLayoutIndex]["title"];
    currentLayout = str;
    Display::setBottom("@" + currentLayout);

    EEPROM.write(EEPROM_ADDR_LAYOUT, currentLayoutIndex);
    Serial.println("Key layout loaded: " + currentLayout);
}

/**
 * Initialize every Macro instance that used in this program
 *
 */
void initMacros() {
    JsonDocument &doc = configStore.doc();
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

    Serial.println("Loading config files from SPIFFS...");
    configStore.reload();

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
        UsbHid::releaseAll();
        BleHid::releaseAll();
    }
}

void resetConfigFiles() {
    resetIdle();
    for (byte countDown = 5; 0 < countDown; countDown--) {
        Display::setKeyInfo("Reset config in " + (String)countDown);

        if (digitalRead(CFG_BTN_PIN_1) != ACTIVE ||
            digitalRead(CFG_BTN_PIN_2) != ACTIVE) {
            Display::setKeyInfo("Reset canceled");
            delay(1000);
            return;
        }
        delay(1000);
    }
    Display::setKeyInfo("Resetting config...");
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
 * Press key
 *
 * @param {Key} key the key to be pressed
 */
void keyPress(Key &key) {
    if (key.keyInfo == "FN") {
        isFnKeyPressed = true;
    }
    if (key.state == false && !isOutputLocked) {
        kbd().press(key.keyStroke);
    }
    key.state = true;
    Display::setKeyInfo(key.keyInfo);
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
        kbd().release(key.keyStroke);
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
    Display::setKeyInfo(macro.macroInfo);
    // Respect output lock (matches keyPress: still show the info, emit nothing)
    if (isOutputLocked) {
        return;
    }
    if (macro.type == 0) {
        size_t length = sizeof(macro.keyStrokes);
        for (size_t i = 0; i < length; i++) {
            kbd().press(macro.keyStrokes[i]);
            delayMicroseconds(10);
        }
        delay(50);
        kbd().releaseAll();
    } else if (macro.type == 1) {
        kbd().print(macro.stringContent);
    } else if (macro.type == 2) {
        kbd().println(macro.stringContent);
    }
    delay(100);
}

/**
 * Press the macro referenced by a "MACRO_<index>" key info string
 *
 * @param {String} info key info of the form "MACRO_<index>"
 */
void macroPressByInfo(const String &info) {
    macroPress(macroMap[info.substring(6).toInt()]);
}

/**
 * Emit a single rotary-encoder turn for the given key info. Triggers a macro
 * for "MACRO_<index>" info, otherwise taps the key code on the active output.
 *
 * @param {uint8_t} keyStroke the key code to tap
 * @param {String} info key info describing the turn
 */
void emitEncoderTurn(uint8_t keyStroke, const String &info) {
    resetIdle();
    bool isMacro = info.startsWith("MACRO_");
    if (!isOutputLocked) {
        if (isMacro) {
            macroPressByInfo(info);
        } else {
            kbd().release(keyStroke);
            kbd().write(keyStroke);
        }
    }
    if (!isMacro) {
        Display::setKeyInfo(info);
    }
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
    JsonDocument &doc = configStore.doc();
    JsonArrayConst layouts = doc["keyConfig"].as<JsonArrayConst>();
    for (size_t i = 0; i < layouts.size(); i++) {
        String str = layouts[i]["title"];
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
    String contentTop, contentBottom;
    int contentIcon;
    Display::snapshot(contentTop, contentBottom, contentIcon);

    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB08_tr);
    u8g2.setFontPosCenter();
    u8g2.drawStr(16 + 4, 24, contentBottom.c_str());

    u8g2.setFont(u8g2_font_ncenB08_tr);
    u8g2.setFontPosCenter();
    u8g2.drawStr(16 + 4, 10, contentTop.c_str());

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

    // Clamp to a valid 0-100 range (voltage below minVoltage yields negatives)
    if (percentage < 0) percentage = 0;
    if (percentage > 100) percentage = 100;

    // Update device's battery level
    BleHid::setBatteryLevel(percentage);

    return percentage;
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
