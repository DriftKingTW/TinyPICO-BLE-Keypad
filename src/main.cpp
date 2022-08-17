#include <Arduino.h>
#include <ArduinoJson.h>
#include <BleKeyboard.h>
#include <SPIFFS.h>
#include <TinyPICO.h>
#include <U8g2lib.h>

#include <cstring>
#include <fstream>

#define BLE_NAME "TinyPICO BLE"
#define AUTHOR "DriftKingTW"
#define ACTIVE LOW

#define ROWS 5
#define COLS 7

String keyMapJSON;

U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C u8g2(U8G2_R0,
                                            /* reset=*/U8X8_PIN_NONE);
BleKeyboard bleKeyboard(BLE_NAME, AUTHOR);
TinyPICO tp = TinyPICO();

TaskHandle_t Task0;

RTC_DATA_ATTR unsigned int timeSinceBoot = 0;
RTC_DATA_ATTR unsigned int savedLayout = 0;

// Stucture for key stroke
struct Key {
    uint8_t keyStroke;
    bool state;
    String keyInfo;
} key1, key2, key3, key4, key5, key6, key7, key8, key9, key10, key11, key12,
    key13, key14, key15, key16, key17, key18, key19, key20, key21, key22, key23,
    key24, key25, key26, key27, key28, key29, key30, key31, dummy;

Key keyMap[ROWS][COLS] = {{key1, key2, key3, key4, key5, key6, dummy},
                          {key7, key8, key9, key10, key11, key12, key13},
                          {key14, key15, key16, key17, key18, key19, dummy},
                          {key20, key21, key22, key23, key24, key25, key26},
                          {key27, key28, key29, dummy, key30, dummy, key31}};

String currentKeyInfo = "";
byte currentLayoutIndex = 0;
byte layoutLength = 0;
const short jsonDocSize = 4096;

byte inputs[] = {23, 19, 18, 5, 32, 33, 25};  // declaring inputs and outputs
const int inputCount = sizeof(inputs) / sizeof(inputs[0]);
byte outputs[] = {4, 14, 15, 27, 26};  // row
const int outputCount = sizeof(outputs) / sizeof(outputs[0]);

// Auto sleep timer
long sleepPreviousMillis = 0;
unsigned long sleepCurrentMillis = 0;
const long SLEEP_INTERVAL = 10 * 60 * 1000;

// Battery timer
long batteryPreviousMillis = 0;
unsigned long batteryCurrentMillis = 0;
const long BATTERY_INTERVAL = 5 * 1000;

// Low battery LED blink timer
long ledPreviousMillis = 0;
unsigned long ledCurrentMillis = 0;
const long LED_INTERVAL = 5 * 1000;

bool isLowBattery = false;

// Function declaration
void coreZeroTask(void *);
void initKeys();
void goSleeping();
void checkIdle();
void resetIdle();
void renderScreen(String msg);
void keyPress(Key &key);
void keyRelease(Key &key);
void showBatteryState();
void breathLEDAnimation();
float getBatteryVoltage();
int getBatteryPercentage();
void showLowBatteryWarning();
void checkBattery();

void setup() {
    Serial.begin(115200);

    Serial.println("Starting BLE work...");
    bleKeyboard.begin();

    Serial.println("Starting u8g2...");
    u8g2.begin();

    Serial.println("Configuring ext1 wakeup source...");
    esp_sleep_enable_ext1_wakeup(0x8000, ESP_EXT1_WAKEUP_ANY_HIGH);

    Serial.println("Configuring CPU core 0's task...");
    xTaskCreatePinnedToCore(
        coreZeroTask,    /* Task function. */
        "SecondaryTask", /* name of task. */
        10000,           /* Stack size of task */
        NULL,            /* parameter of the task */
        1,               /* priority of the task */
        &Task0,          /* Task handle to keep track of created task */
        0);              /* pin task to core 0 */

    if (!SPIFFS.begin(true)) {
        Serial.println("An Error has occurred while mounting SPIFFS");
        return;
    }

    Serial.println("Loading \"keyconfig.json\" from SPIFFS...");
    File file = SPIFFS.open("/keyconfig.json");
    if (!file) {
        Serial.println("Failed to open file for reading");
        return;
    }

    keyMapJSON = "";
    while (file.available()) {
        keyMapJSON += (char)file.read();
    }
    file.close();

    Serial.println("Configuring input pin...");
    currentLayoutIndex = 1;
    initKeys();

    Serial.println("Setup finished!");
}

// coreZeroTask: Check battery status every 5 seconds
void coreZeroTask(void *pvParameters) {
    Serial.println((String) "Secondary Task running on core " +
                   xPortGetCoreID());

    int previousMillis = millis();

    while (true) {
        checkIdle();
        checkBattery();
        showLowBatteryWarning();
        if (millis() - previousMillis > 1000) {
            timeSinceBoot++;
            previousMillis = millis();
        }
        delay(100);
    }
}

void loop() {
    Serial.println((String) "Keyboard scan running on core " +
                   xPortGetCoreID());

    renderScreen("Connecting..");
    breathLEDAnimation();

    if (bleKeyboard.isConnected()) {
        renderScreen("= Connected =");
        tp.DotStar_SetPower(false);
    }

    // Check every keystroke is pressed or not when connected
    while (bleKeyboard.isConnected()) {
        for (int r = 0; r < ROWS; r++) {
            digitalWrite(outputs[r], LOW);  // Setting one row low
            for (int c = 0; c < COLS; c++) {
                if (digitalRead(inputs[c]) == ACTIVE) {
                    if (r == 1 && c == 0) {  // Enter deep sleep mode
                        goSleeping();
                    } else if (r == 4 && c == 0) {  // Switch layout
                        if (currentLayoutIndex < layoutLength - 1) {
                            currentLayoutIndex++;
                        } else {
                            currentLayoutIndex = 0;
                        }
                        initKeys();
                        delay(300);
                    } else {  // Standard key press
                        keyPress(keyMap[r][c]);
                        resetIdle();
                    }
                } else {
                    keyRelease(keyMap[r][c]);
                }
                delayMicroseconds(10);
            }
            digitalWrite(outputs[r], HIGH);  // Setting the row back to high
            delayMicroseconds(10);
        }
    }
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
    String layoutStr = doc[currentLayoutIndex]["title"];
    renderScreen("Layout: " + layoutStr);

    Serial.begin(115200);
    setCpuFrequencyMhz(80);
    Serial.println("CPU clock speed set to 80Mhz");
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
    if (currentKeyInfo != "key.keyInfo") {
        renderScreen(key.keyInfo);
        currentKeyInfo = key.keyInfo;
    }
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
    currentKeyInfo = "";
    return;
}

/**
 * Show battery state using onboard OLED display
 *
 */
void showBatteryState() {
    String result = "";
    char *batteryIcon = "\u005A";
    bool plugged = digitalRead(9);
    bool charging = tp.IsChargingBattery();
    int batteryPercentage = getBatteryPercentage();
    if (plugged && charging) {
        result = "Charging";
        batteryIcon = "\u0060";
    } else if (plugged) {
        result = "Plugged in";
        batteryIcon = "\u0060";
    } else {
        result = "Bat. " + String(batteryPercentage) + "%";
        if (batteryPercentage > 75) {
            batteryIcon = "\u005B";
        } else if (batteryPercentage > 50) {
            batteryIcon = "\u005A";
        } else if (batteryPercentage > 25) {
            batteryIcon = "\u005A";
        }
    }

    int n = result.length();
    char char_array[n];
    strcpy(char_array, result.c_str());

    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_open_iconic_all_2x_t);
    u8g2.setFontPosCenter();
    u8g2.drawStr((64 - u8g2.getStrWidth(batteryIcon) / 2) - 32, 16,
                 batteryIcon);
    u8g2.setFont(u8g2_font_ncenB08_tr);
    u8g2.setFontPosCenter();
    u8g2.drawStr((64 - u8g2.getStrWidth(char_array) / 2) + 8, 16, char_array);
    Serial.println(result);
    u8g2.sendBuffer();

    if (batteryPercentage <= 20) {
        isLowBattery = true;
    } else {
        isLowBattery = false;
    }
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
    u8g2.drawStr(64 - u8g2.getStrWidth(char_array) / 2, 16, char_array);
    u8g2.sendBuffer();
}

/**
 * Return Battery Current Voltage
 *
 */
float getBatteryVoltage() { return tp.GetBatteryVoltage(); }

/**
 * Return Battery Current Percentage
 *
 */
int getBatteryPercentage() {
    const float minVoltage = 3.4, fullVolatge = 4.0;
    // Get average battery voltage value from 10 time periods for more stable
    // result
    float batteryVoltage = 0;
    for (int i = 0; i < 100; i++) {
        batteryVoltage += getBatteryVoltage();
        delayMicroseconds(1);
    }
    batteryVoltage /= 100.0;

    Serial.println(batteryVoltage);

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
 * Check if device is idle for a specified period to determine if it should go
 * to sleep or not.
 *
 */
void checkIdle() {
    sleepCurrentMillis = millis();
    if (sleepCurrentMillis - sleepPreviousMillis > SLEEP_INTERVAL) {
        goSleeping();
    }
}

/**
 * Check battery status for a specified period
 *
 */
void checkBattery() {
    batteryCurrentMillis = millis();
    if (batteryCurrentMillis - batteryPreviousMillis > BATTERY_INTERVAL) {
        showBatteryState();
        Serial.println((String) "Time since boot: " + timeSinceBoot +
                       " seconds");
        batteryPreviousMillis = batteryCurrentMillis;
    }
}

/**
 * Blink LED to indicate that battery is low
 *
 */
void showLowBatteryWarning() {
    if (!isLowBattery) {
        tp.DotStar_SetPower(false);
        return;
    }
    tp.DotStar_SetPower(true);
    ledCurrentMillis = millis();
    if (ledCurrentMillis - ledPreviousMillis > 1000 &&
        ledCurrentMillis - ledPreviousMillis <= 1200) {
        tp.DotStar_SetBrightness(5);
        tp.DotStar_SetPixelColor(255, 0, 0);
    } else if (ledCurrentMillis - ledPreviousMillis > 1200 &&
               ledCurrentMillis - ledPreviousMillis <= 1300) {
        tp.DotStar_SetPixelColor(0, 0, 0);
    } else if (ledCurrentMillis - ledPreviousMillis > 1300 &&
               ledCurrentMillis - ledPreviousMillis <= 1500) {
        tp.DotStar_SetPixelColor(255, 0, 0);
    } else if (ledCurrentMillis - ledPreviousMillis > 1700) {
        tp.DotStar_SetPixelColor(0, 0, 0);
        ledPreviousMillis = ledCurrentMillis;
    }
}

/**
 * Update sleepPreviousMillis' value to reset idle timer
 *
 */
void resetIdle() { sleepPreviousMillis = sleepCurrentMillis; }
