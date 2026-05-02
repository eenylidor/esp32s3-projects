#include <Arduino.h>

#define LED 2

int i = 0;

void setup() {
    Serial.begin(115200);
    pinMode(LED, OUTPUT);

    Serial.println("Hello, ESP32-S3!");
}

void loop() {
    Serial.println(String("Hello, shuki ") + i);
    i = i + 1;

    if (i % 10 == 0) {
        digitalWrite(LED, HIGH);
        delay(1500);
        digitalWrite(LED, LOW);
    } else {
        delay(500);
    }
}