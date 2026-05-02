#include <Arduino.h>
#include <TFT_eSPI.h>
#include "logic.h" // Link to your C logic

TFT_eSPI tft = TFT_eSPI();

// GPIO definitions for the LilyGo S3
#define BUTTON_1_PIN 0  // Top Button
#define BUTTON_2_PIN 14 // Bottom Button

void setup() {
    // POWER SEQUENCING: These pins MUST be high to power the LCD hardware.
    pinMode(15, OUTPUT);
    digitalWrite(15, HIGH); // LCD Power Domain
    pinMode(38, OUTPUT);
    digitalWrite(38, HIGH); // Backlight
    delay(200);             // Give the LCD controller time to "boot"

    // INPUT SETUP: Using internal Pull-up resistors.
    // The pin stays at 3.3V (HIGH) until the button connects it to GND (LOW).
    pinMode(BUTTON_1_PIN, INPUT_PULLUP);
    pinMode(BUTTON_2_PIN, INPUT_PULLUP);

    tft.init();
    tft.setRotation(1); // Landscape mode
    tft.fillScreen(TFT_BLACK);
}

void loop() {
    // TRANSLATION LAYER:
    // digitalRead returns LOW when pressed. We use "== LOW" to turn that
    // into a boolean 'true' (1) to send to our C function.
    int b1 = (digitalRead(BUTTON_1_PIN) == LOW);
    int b2 = (digitalRead(BUTTON_2_PIN) == LOW);

    // CALLING THE C BRIDGE:
    // We pass the hardware state into our pure logic function.
    const char* message = get_message(b1, b2);

    // OUTPUT LAYER:
    tft.setCursor(30, 60);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setTextSize(3);

    // We print the string that our C code decided on.
    tft.println(message);

    delay(100); // Wait 100ms before checking again (Polling frequency = 10Hz)
}