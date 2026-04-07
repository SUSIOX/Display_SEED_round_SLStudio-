/*
 * MAVLink RTOS TEST - Core 0 - XIAO ESP32-S3
 * 
 * Testovani cteni MAVLink dat z Pixhawk na Core 0 (RTOS task)
 * Bez touch, bez display - jen console
 * RX = D2 (GPIO3), TX = D0 (GPIO1)
 */

#include <Arduino.h>
#include <MAVLink.h>

#define RX_PIN 3    // D2 = GPIO3 (free pin)
#define TX_PIN 1    // D0 = GPIO1 (free pin)
#define BAUD 57600

// MAVLink data
mavlink_message_t msg;
mavlink_status_t status;
volatile float voltage = 0.0;
volatile float current = 0.0;
volatile uint8_t percent = 0;
volatile bool data_valid = false;

// Stats
volatile unsigned long mavlink_bytes = 0;
volatile unsigned long mavlink_msgs = 0;
unsigned long last_update = 0;

// RTOS task handle
TaskHandle_t mavlinkTaskHandle = NULL;

// MAVLink task - runs on Core 0
void mavlink_task(void *parameter) {
    Serial.printf("[MAVLINK TASK] Started on core %d\n", xPortGetCoreID());
    
    // Initialize Serial1
    Serial1.begin(BAUD, SERIAL_8N1, RX_PIN, TX_PIN);
    delay(100);
    Serial.println("[MAVLINK TASK] Serial1 initialized");
    
    unsigned long local_bytes = 0;
    unsigned long local_msgs = 0;
    unsigned long last_log = 0;
    
    while (true) {
        // Read MAVLink data
        while (Serial1.available()) {
            uint8_t c = Serial1.read();
            local_bytes++;
            
            if (mavlink_parse_char(MAVLINK_COMM_0, c, &msg, &status)) {
                local_msgs++;
                
                if (msg.msgid == MAVLINK_MSG_ID_BATTERY_STATUS) {
                    mavlink_battery_status_t bat;
                    mavlink_msg_battery_status_decode(&msg, &bat);
                    
                    if (bat.voltages[0] != 0xFFFF && bat.voltages[0] > 0 && bat.voltages[0] < 65000) {
                        voltage = bat.voltages[0] / 1000.0;
                    } else {
                        voltage = 0.0;
                    }
                    current = bat.current_battery / 100.0;
                    if (bat.battery_remaining >= 0 && bat.battery_remaining <= 100) {
                        percent = bat.battery_remaining;
                    } else {
                        percent = 0;
                    }
                    data_valid = true;
                }
            }
        }
        
        // Update global stats every 5 seconds
        if (millis() - last_log >= 5000) {
            mavlink_bytes = local_bytes;
            mavlink_msgs = local_msgs;
            local_bytes = 0;
            local_msgs = 0;
            last_log = millis();
        }
        
        // Yield to other tasks
        vTaskDelay(1);
    }
}

void setup() {
    // LED init
    pinMode(21, OUTPUT);
    digitalWrite(21, HIGH);
    
    Serial.begin(115200);
    delay(2000);
    
    Serial.println("=== MAVLink RTOS TEST - Core 0 ===");
    Serial.printf("RX = GPIO %d, TX = GPIO %d, Baud = %d\n", RX_PIN, TX_PIN, BAUD);
    Serial.println("MAVLink task will run on Core 0");
    Serial.println();
    
    // LED blink = start
    for (int i = 0; i < 3; i++) {
        digitalWrite(21, LOW);
        delay(100);
        digitalWrite(21, HIGH);
        delay(100);
    }
    
    // Create MAVLink task on Core 0
    Serial.println("[SETUP] Creating MAVLink task on Core 0...");
    xTaskCreatePinnedToCore(
        mavlink_task,      // Task function
        "MAVLink",         // Task name
        4096,              // Stack size
        NULL,              // Parameters
        1,                 // Priority
        &mavlinkTaskHandle,// Task handle
        0                  // Core 0
    );
    
    if (mavlinkTaskHandle != NULL) {
        Serial.println("[SETUP] MAVLink task created successfully on Core 0");
    } else {
        Serial.println("[SETUP] ERROR: Failed to create MAVLink task!");
    }
    
    digitalWrite(21, LOW); // LED on = ready
}

void loop() {
    // Status every 5 seconds
    unsigned long now = millis();
    if (now - last_update >= 5000) {
        Serial.printf("[STATUS] Core: %d, raw_bytes=%lu, mavlink_msgs=%lu\n",
                      xPortGetCoreID(), mavlink_bytes, mavlink_msgs);
        
        if (data_valid) {
            Serial.printf("[STATUS] Voltage: %.2fV, %.2fA, %d%%\n", voltage, current, percent);
        } else {
            Serial.println("[STATUS] ZADNA MAVLINK DATA");
        }
        
        last_update = now;
    }
    
    delay(100); // Loop runs on Core 1 (default)
}
