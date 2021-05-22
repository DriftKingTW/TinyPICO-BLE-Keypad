#include <Arduino.h>
#include <BleKeyboard.h>
#include <TinyPICO.h>
#include <U8g2lib.h>

#define BLE_NAME "TinyPICO"
#define AUTHOR "DriftKingTW"
#define ACTIVE LOW

#define ROWS 5
#define COLS 7

U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/U8X8_PIN_NONE);
BleKeyboard bleKeyboard(BLE_NAME, AUTHOR);
TinyPICO tp = TinyPICO();

// Stucture for key stroke
struct Key {
    uint8_t keyStroke;
    bool state;
    char *keyInfo;
};

Key key1, key2, key3, key4, key5, key6, key7, key8, key9, key10, key11, key12, key13, key14, key15, key16, key17, key18, key19, key20, key21, key22, key23, key24, key25, key26, key27, key28, key29, key30, key31, dummy;

Key keyMap[ROWS][COLS] = {{key1, key2, key3, key4, key5, key6, dummy},
                          {key7, key8, key9, key10, key11, key12, key13},
                          {key14, key15, key16, key17, key18, key19, dummy},
                          {key20, key21, key22, key23, key24, key25, key26},
                          {key27, key28, key29, dummy, key30, dummy, key31}};

uint8_t keyLayout[ROWS][COLS] = {
    {KEY_ESC, 49, 50, 51, 52, 53, 49},
    {KEY_TAB, 'q', 'w', 'e', 'r', 't', 8},
    {KEY_LEFT_CTRL, 'a', 's', 'd', 'f', 'g', 49},
    {KEY_LEFT_SHIFT, 'z', 'x', 'c', 'v', 'b', KEY_RETURN},
    {'h', KEY_LEFT_ALT, KEY_LEFT_GUI, 49, 32, 'h', 49}};

char *keyInfo[ROWS][COLS] = {
    {(char *)"ESC", (char *)"1", (char *)"2", (char *)"3", (char *)"4",
     (char *)"5"},

    {(char *)"TAB", (char *)"Q", (char *)"W", (char *)"E", (char *)"R",
     (char *)"T", (char *)"DELETE"},

    {(char *)"CTRL", (char *)"A", (char *)"S", (char *)"D", (char *)"F",
     (char *)"G"},

    {(char *)"SHIFT", (char *)"Z", (char *)"X", (char *)"C", (char *)"V",
     (char *)"B", (char *)"RETURN"},

    {(char *)"H", (char *)"OPTION", (char *)"COMMAND", (char *)"SPACE",
     (char *)"H"}};

byte inputs[] = {23, 19, 18, 5, 32, 33, 25};  // declaring inputs and outputs
const int inputCount = sizeof(inputs) / sizeof(inputs[0]);
byte outputs[] = {4, 14, 15, 27, 26};  // row
const int outputCount = sizeof(outputs) / sizeof(outputs[0]);

// Function declaration
void initKeys();
void lightSleepSetup();
void renderScreen(char *msg);
void keyPress(Key &key);
void keyRelease(Key &key);
void showBatteryState();

float getBatteryPercentage() {
    float minVoltage = 3.700;
    float fullVolatge = 4.200;
    float batteryVolatage = tp.GetBatteryVoltage();

    return (batteryVolatage - minVoltage) / (fullVolatge - minVoltage) * 100;
}

void setup() {
    Serial.begin(115200);

    Serial.println("Set CPU clock speed to 80Mhz to reduce power consumption");
    setCpuFrequencyMhz(80);

    Serial.println("Starting BLE work...");
    bleKeyboard.begin();

    Serial.println("Starting u8g2...");
    u8g2.begin();

    Serial.println("Configuring input pin...");
    initKeys();

    Serial.println("Configuring wake method...");
    lightSleepSetup();

    // Sleep test
    // Serial.println("Go to sleep after 1 sec!");
    // delay(1000);
    // esp_light_sleep_start();
}

void loop() {
    renderScreen((char *)"Connecting..");

    if (bleKeyboard.isConnected()) renderScreen((char *)"= Connected =");

    // Check every keystroke is pressed or not
    while (bleKeyboard.isConnected()) {
        for (int r = 0; r < ROWS; r++) {
            digitalWrite(outputs[r], LOW);  // Setting one row low
            delayMicroseconds(5);  // Time for electronics to settle down

            for (int c = 0; c < COLS; c++) {
                if (digitalRead(inputs[c]) == ACTIVE) {
                    // Calling keyPressed function if one of the inputs
                    // reads low
                    if (r == 4 && c == 0) {
                        // Show battery on LED if the specific key is pressed
                        showBatteryState();
                    } else {
                        keyPress(keyMap[r][c]);
                    }
                } else {
                    keyRelease(keyMap[r][c]);
                }
            }

            digitalWrite(outputs[r], HIGH);  // Setting the row back to high
            delayMicroseconds(5);
        }
        delay(10);
    }

    delay(500);
}

/**
 * Initialize every Key instance that used in this program
 *
 */
void initKeys() {
    // KeyC.keyStroke = 'b';
    // KeyC.state = false;

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
 * Enable ESP wakeup for light sleep mode
 *
 */
void lightSleepSetup() {
    gpio_wakeup_enable(GPIO_NUM_32, GPIO_INTR_LOW_LEVEL);
    gpio_wakeup_enable(GPIO_NUM_33, GPIO_INTR_LOW_LEVEL);
    gpio_wakeup_enable(GPIO_NUM_14, GPIO_INTR_LOW_LEVEL);
    esp_sleep_enable_gpio_wakeup();
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
    // renderScreen(key.keyInfo);
}

/**
 * Release key
 *
 * @param {Key} key the key to be released
 */
void keyRelease(Key &key) {
    if (key.state == true) {
        bleKeyboard.release(key.keyStroke);
        // renderScreen((char *)"Waiting for input...");
    }
    key.state = false;
    return;
}

/**
 * Show battery state using onboard RGB LED
 *
 */
void showBatteryState() {
    tp.DotStar_SetPower(true);
    float batteryV = getBatteryPercentage();
    Serial.println((String)batteryV + "%");
    tp.DotStar_SetBrightness(50);
    if (batteryV > 90) {
        tp.DotStar_SetPixelColor(0, 255, 0);
    } else if (batteryV > 75) {
        tp.DotStar_SetPixelColor(0, 200, 0);
    } else if (batteryV > 50) {
        tp.DotStar_SetPixelColor(219, 245, 0);
    } else if (batteryV > 25) {
        tp.DotStar_SetPixelColor(236, 141, 0);
    } else {
        tp.DotStar_SetPixelColor(255, 0, 0);
    }
    delay(1000);
    tp.DotStar_SetPower(false);
}

/**
 * Print message on oled screen.
 *
 * @param {char} array to print on oled screen
 */
void renderScreen(char *msg) {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB08_tr);
    u8g2.setFontPosCenter();
    u8g2.drawStr(64 - u8g2.getStrWidth(msg) / 2, 32, msg);
    u8g2.setCursor(0, 15);
    // float bat = tp.GetBatteryVoltage();
    // if (tp.IsChargingBattery())
    //     u8g2.print((char *)"Charging");
    // else
    //     u8g2.print((char *)"Not Charging");
    // u8g2.print(bat, 2);
    u8g2.sendBuffer();
}