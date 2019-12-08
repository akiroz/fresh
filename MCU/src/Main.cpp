

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include <Wire.h>
#include <U8g2lib.h>

#define PIN_FAN_PWM         D5
#define PIN_FAN_TACHO       D6
#define PIN_OLED_SDA        D1
#define PIN_OLED_SCL        D2
#define PIN_BTN_UP          D7
#define PIN_BTN_DOWN        D3
#define PIN_BTN_MODE        D4

#define VPIN_POWER          V0
#define VPIN_MODE           V1
#define VPIN_PM_1_0         V2
#define VPIN_PM_2_5         V3
#define VPIN_PM_10          V4
#define VPIN_RPM            V5
#define VPIN_FILTER_LIFE    V6
#define VPIN_SPEED          V7
#define VPIN_ALERT          V8

char auth[] = "xxxxxxxxxxxxxxxxxxxxxxx";

BlynkTimer blynk_timer;
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2 (
        U8G2_R0,
        U8X8_PIN_NONE,
        PIN_OLED_SCL,
        PIN_OLED_SDA);

namespace state {
    bool power_on = false;
    bool auto_mode = false;
    uint8_t manual_speed = 0;
    uint8_t auto_speed = 0;
    bool alarm_sent = false;
    uint16_t alarm_level = 0;
    uint16_t tacho_value = 0;
    uint16_t fan_rpm = 0;
    uint16_t pm_1_0 = 0;
    uint16_t pm_2_5 = 0;
    uint16_t pm_10 = 0;
}

void fanTachoHandler() {
    state::tacho_value += 1;
}

BLYNK_WRITE(VPIN_ALERT) {
    state::alarm_sent = false;
    state::alarm_level = param.asInt();
}

BLYNK_WRITE(VPIN_SPEED) {
    state::manual_speed = param.asInt();
}


BLYNK_WRITE(VPIN_POWER) {
    state::power_on = param.asInt() == 1;
}

BLYNK_WRITE(VPIN_MODE) {
    state::auto_mode = param.asInt() == 1;
}

void pollStatus() {
    state::fan_rpm = state::tacho_value * 30;
    state::tacho_value = 0;
    Blynk.virtualWrite(VPIN_RPM, state::fan_rpm);
}

void pollSerial() {
    if(Serial.available() < 32) return;
    uint8_t buf[32];
    Serial.readBytes(buf, 32);
    if(buf[0] != 0x42 || buf[1] != 0x4d) {
        while(Serial.read() >= 0);
        return;
    }
    state::pm_1_0 = (buf[4] << 8) | buf[5];
    state::pm_2_5 = (buf[6] << 8) | buf[7];
    state::pm_10 = (buf[8] << 8) | buf[9];
    Blynk.virtualWrite(VPIN_PM_1_0, state::pm_1_0);
    Blynk.virtualWrite(VPIN_PM_2_5, state::pm_2_5);
    Blynk.virtualWrite(VPIN_PM_10,  state::pm_10);

    uint16_t max_val = max(max(state::pm_1_0, state::pm_2_5), state::pm_10);
    state::auto_speed = max_val / 10;
    if(
            state::power_on && !state::alarm_sent &&
            state::alarm_level > 0 && max_val > state::alarm_level)
    {
        Blynk.notify("PM Level Alarm");
        state::alarm_sent = true;
    }
}

void pollButton() {
    if(digitalRead(PIN_BTN_MODE) == 0) {
        if(!state::power_on) {
            state::power_on = true;
            state::auto_mode = true;
        } else if(state::auto_mode) {
            state::auto_mode = false;
        } else {
            state::power_on = false;
        }
        Blynk.virtualWrite(VPIN_POWER, state::power_on);
        Blynk.virtualWrite(VPIN_MODE, state::auto_mode);
    } else if(digitalRead(PIN_BTN_UP) == 0) {
        if(!state::auto_mode && state::manual_speed < 10) {
            state::manual_speed += 1;
            Blynk.virtualWrite(VPIN_SPEED, state::manual_speed);
        }
    } else if(digitalRead(PIN_BTN_DOWN) == 0) {
        if(!state::auto_mode && state::manual_speed > 0) {
            state::manual_speed -= 1;
            Blynk.virtualWrite(VPIN_SPEED, state::manual_speed);
        }
    }
}

void setup() {
    // OLED Screen
    u8g2.begin();
    u8g2.setFont(u8g2_font_7x14_mf);
    u8g2.drawStr(0, 10, "Initializing...");
    u8g2.sendBuffer();
    
    // Fan PWM
    pinMode(PIN_FAN_PWM, OUTPUT);
    analogWriteFreq(25000);
    analogWrite(PIN_FAN_PWM, 410);

    // Fan Tacho
    pinMode(PIN_FAN_TACHO, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(PIN_FAN_TACHO), fanTachoHandler, RISING);

    // PM2.5 Sensor
    Serial.begin(9600);
    blynk_timer.setInterval(500L, pollSerial);

    // Buttons
    pinMode(PIN_BTN_UP, INPUT_PULLUP);
    pinMode(PIN_BTN_DOWN, INPUT_PULLUP);
    pinMode(PIN_BTN_MODE, INPUT_PULLUP);
    blynk_timer.setInterval(200L, pollButton);

    // Blynk
    Blynk.begin(auth, "xxxxx", "xxxxxxxxxx");
    blynk_timer.setInterval(1000L, pollStatus);
    
}

void loop() {
    Blynk.run();
    blynk_timer.run();

    if(!state::power_on) {
        analogWrite(PIN_FAN_PWM, 0);
    } else if (state::auto_mode) {
        analogWrite(PIN_FAN_PWM, state::auto_speed * 102.4);
    } else { // manual mode
        analogWrite(PIN_FAN_PWM, state::manual_speed * 102.4);
    }

    u8g2.clearBuffer();
    if(!state::power_on) {
        u8g2.drawStr(0, 10, "Standby Mode");
    } else {
        u8g2.setCursor(0, 10);
        u8g2.print(state::auto_mode ? "Auto " : "Manu ");
        u8g2.print(u8x8_u8toa(state::auto_mode ? state::auto_speed : state::manual_speed, 2));
        u8g2.setCursor(72, 10);
        u8g2.print(u8x8_u16toa(state::fan_rpm, 4));
        u8g2.print(" RPM");
        u8g2.setCursor(0, 30);
        u8g2.print("PM 1.0: ");
        u8g2.print(u8x8_u16toa(state::pm_1_0, 3));
        u8g2.setCursor(0, 46);
        u8g2.print("PM 2.5: ");
        u8g2.print(u8x8_u16toa(state::pm_2_5, 3));
        u8g2.setCursor(0, 62);
        u8g2.print("PM 10:  ");
        u8g2.print(u8x8_u16toa(state::pm_10, 3));
    }
    u8g2.sendBuffer();
}


