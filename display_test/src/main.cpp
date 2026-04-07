/*
 * SIMPLE DISPLAY TEST - XIAO ESP32-S3 + Round Display
 * Test bez LVGL - jen TFT_eSPI
 */

#include <Arduino.h>
#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI();

#define XIAO_BL 6  // Backlight pin

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n=== SIMPLE DISPLAY TEST ===");
    
    // Turn on backlight
    Serial.println("[INIT] Turning on backlight...");
    pinMode(XIAO_BL, OUTPUT);
    digitalWrite(XIAO_BL, HIGH);
    Serial.println("[OK] Backlight ON");
    
    // Init display
    Serial.println("[INIT] tft.begin()...");
    tft.begin();
    Serial.println("[OK] TFT initialized");
    
    // Set rotation
    tft.setRotation(2);
    Serial.println("[OK] Rotation set to 2");
    
    // Fill screen with color
    Serial.println("[INIT] Filling screen RED...");
    tft.fillScreen(TFT_RED);
    Serial.println("[OK] Screen filled RED");
    
    // Draw some text
    Serial.println("[INIT] Drawing text...");
    tft.setTextColor(TFT_WHITE, TFT_RED);
    tft.setTextSize(2);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("DISPLAY TEST", 120, 100);
    tft.drawString("WORKING!", 120, 140);
    Serial.println("[OK] Text drawn");
    
    Serial.println("=== TEST COMPLETE ===");
    Serial.println("If you see RED screen with WHITE text, display works!");
}

void loop() {
    // Blink LED to show loop is running
    static unsigned long lastBlink = 0;
    static bool ledState = false;
    
    if (millis() - lastBlink >= 1000) {
        ledState = !ledState;
        digitalWrite(LED_BUILTIN, ledState ? HIGH : LOW);
        Serial.println(ledState ? "LED ON" : "LED OFF");
        lastBlink = millis();
    }
}
