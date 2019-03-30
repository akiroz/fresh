
#define BLYNK_USE_DIRECT_CONNECT

#include <Arduino.h>
#include <WiFi.h>
#include <BlynkSimpleEsp32_BLE.h>

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

BlynkTimer timer;
String ssid;
String password;

BLYNK_WRITE(VPIN_WIFI_CONNECT) {
    if(param.asInt() == 1) {
        //WiFiBlynk.connectWiFi(ssid.c_str(), password.c_str());
        // Connect WiFi
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

void pollWiFi() {
    int wifiLED = WiFi.status() == WL_CONNECTED ? 255 : 0;
    Blynk.virtualWrite(VPIN_WIFI_STATUS, wifiLED);
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
}

void setup() {
    Serial2.begin(9600);
    Blynk.setDeviceName("Fresh");
    Blynk.begin(auth);
    timer.setInterval(1000L, pollWiFi);
    timer.setInterval(500L, pollSerial);
}

void loop() {
    Blynk.run();
    timer.run();
}

