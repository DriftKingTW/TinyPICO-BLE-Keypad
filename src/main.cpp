#include <Arduino.h>
#include <BleKeyboard.h>
#include <TinyPICO.h>
#include <U8g2lib.h>

#include <cstring>

#define BLE_NAME "TinyPICO BLE"
#define AUTHOR "DriftKingTW"
#define ACTIVE LOW

#define ROWS 5
#define COLS 7

// U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/U8X8_PIN_NONE);
U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C u8g2(U8G2_R0, /* reset=*/U8X8_PIN_NONE);
BleKeyboard bleKeyboard(BLE_NAME, AUTHOR);
TinyPICO tp = TinyPICO();

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

// Default Layout
uint8_t keyLayout_1[ROWS][COLS] = {
    {KEY_ESC, 49, 50, 51, 52, 53, 49},
    {KEY_TAB, 'q', 'w', 'e', 'r', 't', 8},
    {KEY_LEFT_CTRL, 'a', 's', 'd', 'f', 'g', 49},
    {KEY_LEFT_SHIFT, 'z', 'x', 'c', 'v', 'b', KEY_RETURN},
    {'h', KEY_LEFT_ALT, KEY_LEFT_GUI, 49, 32, KEY_LEFT_GUI, 49}};

// Procreate Shortcuts
uint8_t keyLayout_2[ROWS][COLS] = {
    {KEY_ESC, 49, 50, 51, 52, 53, 49},
    {KEY_TAB, 's', 'w', 'e', '[', ']', 8},
    {KEY_LEFT_CTRL, '[', ']', 'l', 'b', 'g', 49},
    {KEY_LEFT_SHIFT, 'z', 'x', 'c', 'v', 'b', KEY_RETURN},
    {'h', KEY_LEFT_ALT, KEY_LEFT_GUI, 49, 32, KEY_LEFT_GUI, 49}};

byte currentLayout = 2;

// Default Layout
String keyInfo_1[ROWS][COLS] = {
    {"ESC", "1", "2", "3", "4", "5", "NULL"},

    {"TAB", "Q", "W", "E", "R", "T", "DELETE"},

    {"CTRL", "A", "S", "D", "F", "G", "NULL"},

    {"SHIFT", "Z", "X", "C", "V", "B", "RETURN"},

    {"FN", "OPTION", "COMMAND", "NULL", "SPACE", "H", "NULL"}};

// Procreate Shortcuts
String keyInfo_2[ROWS][COLS] = {
    {"ESC", "1", "2", "3", "4", "5", "NULL"},

    {"TAB", "Select", "W", "Eraser", "Brush Down", "Brush Up", "DELETE"},

    {"CTRL", "Brush Down", "Brush Up", "Layers", "Brush", "G", "NULL"},

    {"SHIFT", "Z", "X", "C(Colors)", "V(Transform)", "B", "RETURN"},

    {"FN", "OPTION", "COMMAND", "NULL", "SPACE", "COMMAND", "NULL"}};

byte inputs[] = {23, 19, 18, 5, 32, 33, 25};  // declaring inputs and outputs
const int inputCount = sizeof(inputs) / sizeof(inputs[0]);
byte outputs[] = {4, 14, 15, 27, 26};  // row
const int outputCount = sizeof(outputs) / sizeof(outputs[0]);

// Auto sleep after idle params
long previousMillis = 0;
unsigned long currentMillis = 0;
const long INTERVAL = 10 * 60 * 1000;

// Function declaration
void initKeys(uint8_t keyLayout[ROWS][COLS], String keyInfo[ROWS][COLS]);
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

void setup() {
    Serial.begin(115200);

    Serial.println("Set CPU clock speed to 80Mhz to reduce power consumption");
    setCpuFrequencyMhz(80);

    Serial.println("Starting BLE work...");
    bleKeyboard.begin();

    Serial.println("Starting u8g2...");
    u8g2.begin();

    Serial.println("Configuring input pin...");
    initKeys(keyLayout_2, keyInfo_2);

    esp_sleep_enable_ext1_wakeup(0x8000, ESP_EXT1_WAKEUP_ANY_HIGH);
}

void loop() {
    renderScreen("Connecting..");
    breathLEDAnimation();

    checkIdle();

    if (bleKeyboard.isConnected()) {
        renderScreen("= Connected =");
        tp.DotStar_SetPower(false);
    }

    // Check every keystroke is pressed or not when connected
    while (bleKeyboard.isConnected()) {
        checkIdle();
        for (int r = 0; r < ROWS; r++) {
            digitalWrite(outputs[r], LOW);  // Setting one row low
            for (int c = 0; c < COLS; c++) {
                if (digitalRead(inputs[c]) == ACTIVE) {
                    // Calling keyPressed function if one of the inputs reads
                    // low
                    if (r == 1 && c == 0) {
                        goSleeping();
                    } else if (r == 4 && c == 1) {
                        // Show battery level if the Fn key is pressed
                        showBatteryState();
                    } else if (r == 4 && c == 0) {
                        if (currentLayout == 1) {
                            currentLayout = 2;
                            initKeys(keyLayout_2, keyInfo_2);
                            renderScreen("Layout 1");
                            delay(500);
                        } else {
                            currentLayout = 1;
                            initKeys(keyLayout_1, keyInfo_1);
                            renderScreen("Layout 2");
                            delay(500);
                        }
                    } else {
                        keyPress(keyMap[r][c]);
                        resetIdle();
                    }
                } else {
                    keyRelease(keyMap[r][c]);
                }
                delayMicroseconds(5);
            }
            digitalWrite(outputs[r], HIGH);  // Setting the row back to high
            delayMicroseconds(5);
        }
    }
}

/**
 * Check if device is idle for a specified period to determine if it should go
 * to sleep or not.
 *
 */
void checkIdle() {
    currentMillis = millis();
    if (currentMillis - previousMillis > INTERVAL) {
        goSleeping();
    }
}

/**
 * Update previousMillis' value to reset idle timer
 *
 */
void resetIdle() { previousMillis = currentMillis; }

/**
 * Initialize every Key instance that used in this program
 *
 */
void initKeys(uint8_t keyLayout[ROWS][COLS], String keyInfo[ROWS][COLS]) {
    for (int i = 0; i < outputCount; i++) {
        pinMode(outputs[i], OUTPUT);
        digitalWrite(outputs[i], HIGH);
    }

    for (int i = 0; i < inputCount; i++) {
        pinMode(inputs[i], INPUT_PULLUP);
    }

    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++) {
            keyMap[r][c].keyStroke = keyLayout[r][c];
            keyMap[r][c].keyInfo = keyInfo[r][c];
            keyMap[r][c].state = false;
        }
    }
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
 * Press key
 *
 * @param {Key} key the key to be pressed
 */
void keyPress(Key &key) {
    if (key.state == false) {
        bleKeyboard.press(key.keyStroke);
    }
    key.state = true;
    renderScreen(key.keyInfo);
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
 * Show battery state using onboard RGB LED
 *
 */
void showBatteryState() {
    String result = "";
    char *batteryIcon = "\u005A";
    bool plugged = digitalRead(9);
    bool charging = tp.IsChargingBattery();
    if (plugged && charging) {
        result = "Charged";
        batteryIcon = "\u0060";
    } else if (plugged) {
        result = "Plugged in";
        batteryIcon = "\u0060";
    } else {
        int batteryPercentage = getBatteryPercentage();
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

    delay(100);
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
    const float minVoltage = 3.15, fullVolatge = 4.05;
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

    // percentage =
    //     percentage >= 100
    //         ? 100
    //         : static_cast<int>((percentage + 0.5 - (percentage < 0)) + 5);

    // Update device's battery level
    bleKeyboard.setBatteryLevel(percentage);

    return percentage > 100 ? 100 : percentage;
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