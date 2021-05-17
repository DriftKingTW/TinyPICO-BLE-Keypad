#include <Arduino.h>
#include <BleKeyboard.h>
#include <TinyPICO.h>
#include <U8g2lib.h>

#define BLE_NAME "TinyPICO"
#define AUTHOR "DriftKingTW"
#define ACTIVE LOW

// #define ROWS 5
// #define COLS 7

U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/U8X8_PIN_NONE);
BleKeyboard bleKeyboard(BLE_NAME, AUTHOR);
TinyPICO tp = TinyPICO();

// Stucture for key stroke
struct Key {
    byte pin;
    uint8_t keyStroke;
    bool state;
    char *keyInfo;
};

// struct Key key1;
// struct Key key2;
// struct Key key3;
// struct Key key4;
// struct Key key5;
// struct Key key6;
// struct Key key7;
// struct Key key8;
// struct Key key9;
// struct Key key10;
// struct Key key11;
// struct Key key12;
// struct Key key13;
// struct Key key14;
// struct Key key15;
// struct Key key16;
// struct Key key17;
// struct Key key18;
// struct Key key19;
// struct Key key20;
// struct Key key21;
// struct Key key22;
// struct Key key23;
// struct Key key24;
// struct Key key25;
// struct Key key26;
// struct Key key27;
// struct Key key28;
// struct Key key29;
// struct Key key30;
// struct Key key31;

// Key keyMap[ROWS][COLS] = {{key1, key2, key3, key4, key5, key6},
//                           {key7, key8, key9, key10, key11, key12, key13},
//                           {key14, key15, key16, key17, key18, key19},
//                           {key20, key21, key22, key23, key24, key25, key26},
//                           {key27, key28, key29, key30, key31}};

// uint8_t keyLayout[ROWS][COLS] = {{KEY_ESC, 49, 50, 51, 52, 53},
//                                  {KEY_TAB, 'q', 'w', 'e', 'r', 't', 127},
//                                  {KEY_LEFT_CTRL, 'a', 's', 'd', 'f', 'g'},
//                                  {KEY_LEFT_SHIFT, 'z', 'x', 'c', 'v', 'b', 13},
//                                  {'h', KEY_LEFT_ALT, KEY_LEFT_GUI, 32, 'h'}};

// byte inputs[] = {23, 19, 18, 5, 32, 33, 25};  // declaring inputs and outputs
// const int inputCount = sizeof(inputs) / sizeof(inputs[0]);
// byte outputs[] = {4, 17, 15, 27, 26};  // row
// const int outputCount = sizeof(outputs) / sizeof(outputs[0]);

// Function declaration
void initKeys();
void lightSleepSetup();
void renderScreen(char *msg);
void handleKeyPress(Key &key);

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
    Serial.println("Go to sleep after 1 sec!");
    delay(1000);
    esp_light_sleep_start();
}

void loop() {
    renderScreen((char *)"Connecting..");

    if (bleKeyboard.isConnected()) renderScreen((char *)"= Connected =");

    // Check every keystroke is pressed or not
    while (bleKeyboard.isConnected()) {
        handleKeyPress(KeyCmd);
        handleKeyPress(KeyC);
        handleKeyPress(KeyV);
        delay(10);
    }

    delay(500);
}

/**
 * Initialize every Key instance that used in this program
 *
 */
void initKeys() {
    KeyC.pin = 32;
    KeyC.keyStroke = 'b';
    KeyC.state = false;
    KeyC.keyInfo = (char *)"B";

    KeyV.pin = 33;
    KeyV.keyStroke = 'e';
    KeyV.state = false;
    KeyV.keyInfo = (char *)"E";

    KeyCmd.pin = 14;
    // KeyCmd.keyStroke = KEY_LEFT_GUI;
    KeyCmd.keyStroke = 32;
    KeyCmd.state = false;
    KeyCmd.keyInfo = (char *)"Space";

    pinMode(KeyCmd.pin, INPUT_PULLUP);
    pinMode(KeyC.pin, INPUT_PULLUP);
    pinMode(KeyV.pin, INPUT_PULLUP);

    // for (int i = 0; i < outputCount; i++) {
    //     pinMode(outputs[i], OUTPUT);
    //     digitalWrite(outputs[i], HIGH);
    // }

    // for (int i = 0; i < inputCount; i++) {
    //     pinMode(inputs[i], INPUT_PULLUP);
    // }

    // for (short r = 0; r < ROWS; r++) {
    //     for (short c = 0; c < COLS; c++) {
    //         keyMap[r][c].keyStroke = keyLayout[r][c];
    //         keyMap[r][c].state = false;
    //     }
    // }
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
 * Handle the button press event and returns the updated button state.
 *
 * @param {Key} key the key to be pressed
 */
void handleKeyPress(Key &key) {
    if (digitalRead(key.pin) == ACTIVE) {
        if (key.state == false) {
            bleKeyboard.press(key.keyStroke);
        }
        key.state = true;
        renderScreen(key.keyInfo);
        return;
    } else if (key.state == true) {
        bleKeyboard.release(key.keyStroke);
        renderScreen((char *)"Waiting for input...");
    }
    key.state = false;
    return;
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