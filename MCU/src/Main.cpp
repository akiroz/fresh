

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>

#define PIN_FAN_PWM         5
#define PIN_FAN_TACHO       18

#define VPIN_POWER          V0
#define VPIN_MODE           V1
#define VPIN_PM_1_0         V2
#define VPIN_PM_2_5         V3
#define VPIN_PM_10          V4
#define VPIN_RPM            V5
#define VPIN_FILTER_LIFE    V6
#define VPIN_SPEED          V7
#define VPIN_ALERT          V8
#define VPIN_WIFI_CONNECT   V9
#define VPIN_WIFI_SSID      V10
#define VPIN_WIFI_PASSWORD  V11
#define VPIN_WIFI_STATUS    V12

char auth[] = "bff46d9f11234072abb8f08aae805350";

hw_timer_t *tacho_timer = NULL;
BlynkTimer blynk_timer;
String ssid;
String password;

bool power_on = false;
bool auto_mode = false;
int manual_speed = 0;
int auto_speed = 0;

bool alarm_sent = false;
int alarm_level = 0;

int fan_rpm = 0;


BLYNK_WRITE(VPIN_ALERT) {
    alarm_sent = false;
    alarm_level = param.asInt();
}

BLYNK_WRITE(VPIN_SPEED) {
    manual_speed = param.asInt();
}


BLYNK_WRITE(VPIN_POWER) {
    power_on = param.asInt() == 1;
}

BLYNK_WRITE(VPIN_MODE) {
    auto_mode = param.asInt() == 1;
}

BLYNK_WRITE(VPIN_WIFI_CONNECT) {
    if(param.asInt() == 1) {
        //WiFi.begin(ssid.c_str(), password.c_str());
    }
}

BLYNK_WRITE(VPIN_WIFI_SSID) {
    ssid.remove(0);
    ssid.concat(param.asStr());
}

BLYNK_WRITE(VPIN_WIFI_PASSWORD) {
    password.remove(0);
    password.concat(param.asStr());
}

void pollStatus() {
    int wifiLED = WiFi.status() == WL_CONNECTED ? 255 : 0;
    Blynk.virtualWrite(VPIN_WIFI_STATUS, wifiLED);
    Blynk.virtualWrite(VPIN_RPM, fan_rpm);
}

void pollSerial() {
    if(Serial2.available() < 32) return;
    uint8_t buf[32];
    Serial2.readBytes(buf, 32);
    if(buf[0] != 0x42 || buf[1] != 0x4d) {
        while(Serial2.read() >= 0);
        return;
    }
    uint16_t pm_1_0 = (buf[4] << 8) | buf[5];
    uint16_t pm_2_5 = (buf[6] << 8) | buf[7];
    uint16_t pm_10 = (buf[8] << 8) | buf[9];
    Blynk.virtualWrite(VPIN_PM_1_0, pm_1_0);
    Blynk.virtualWrite(VPIN_PM_2_5, pm_2_5);
    Blynk.virtualWrite(VPIN_PM_10, pm_10);

    uint16_t max_val = max(max(pm_1_0, pm_2_5), pm_10);
    if(!alarm_sent && alarm_level > 0 && max_val > alarm_level) {
        // notify
    }
}

void fanTachoHandler() {
    if(!tacho_timer) {
        tacho_timer = timerBegin(0, 80, true);
    } else {
        int rpm = (60 * 1000000) / (2 * timerReadMicros(tacho_timer));
        timerRestart(tacho_timer);
        Blynk.virtualWrite(VPIN_RPM, rpm);
    }
}

void setup() {
    // Fan PWM
    ledcSetup(0, 25000, 8);
    ledcAttachPin(PIN_FAN_PWM, 0);
    ledcAttachPin(2, 0);
    ledcWrite(0, 0);

    // Fan Tacho
    pinMode(PIN_FAN_TACHO, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(PIN_FAN_TACHO), fanTachoHandler, RISING);


    Serial2.begin(9600);

    Blynk.begin(auth, "eunis", "aaaaaaaa");

    blynk_timer.setInterval(1000L, pollStatus);
    blynk_timer.setInterval(500L, pollSerial);
}

void loop() {
    Blynk.run();
    blynk_timer.run();

    if(!power_on) {
        ledcWrite(0, 0);
    } else if (auto_mode) {
        ledcWrite(0, auto_speed * 25.5);
    } else { // manual mode
        ledcWrite(0, manual_speed * 25.5);
    }
}


