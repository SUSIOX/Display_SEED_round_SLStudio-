/*
 * MAVLink TEST - XIAO ESP32-S3
 * 
 * Testovani cteni MAVLink dat z Pixhawk
 * Bez touch, bez display - jen console
 */

#include <Arduino.h>
#include <MAVLink.h>

#define RX_PIN 3    // D2 = GPIO3 (free pin)
#define TX_PIN 1    // D0 = GPIO1 (free pin)
#define BAUD 57600

mavlink_message_t msg;
mavlink_status_t status;
float voltage = 0.0;
float current = 0.0;
uint8_t percent = 0;

unsigned long last_update = 0;
bool data_received = false;
unsigned long last_heartbeat = 0;
unsigned long raw_bytes = 0;
unsigned long mavlink_msgs = 0;

void setup() {
    pinMode(21, OUTPUT);
    digitalWrite(21, HIGH);
    
    Serial.begin(115200);
    delay(2000);
    
    Serial.println("=== MAVLink TEST ===");
    Serial.printf("RX = GPIO %d, TX = GPIO %d, Baud = %d\n", RX_PIN, TX_PIN, BAUD);
    Serial.println("Cekam na data z Pixhawk...");
    Serial.println();
    
    // LED = start
    for (int i=0; i<3; i++) {
        digitalWrite(21, LOW);
        delay(100);
        digitalWrite(21, HIGH);
        delay(100);
    }
    
    // Serial1 init
    Serial1.begin(BAUD, SERIAL_8N1, RX_PIN, TX_PIN);
    delay(100);
    
    // Loopback test - pokud mas D7 a D6 propojene dratklem
    Serial.println("Loopback test...");
    Serial1.write(0xAB);
    Serial1.write(0xCD);
    delay(10);
    if (Serial1.available()) {
        Serial.println("[LOOPBACK] OK - Serial1 funguje!");
        while (Serial1.available()) Serial1.read();
    } else {
        Serial.println("[LOOPBACK] FAIL - Serial1 nefunguje nebo D7/D6 nejsou propojeny");
    }
    
    digitalWrite(21, LOW); // LED zapnuta = ready
}

void loop() {
    // Cteni MAVLink dat
    while (Serial1.available()) {
        uint8_t c = Serial1.read();
        
        raw_bytes++;
        if (raw_bytes % 5000 < 16) {
            Serial.printf("0x%02X ", c);
            if (raw_bytes % 5000 == 15) Serial.println();
        }
        if (mavlink_parse_char(MAVLINK_COMM_0, c, &msg, &status)) {
            mavlink_msgs++;
            data_received = true;
            
            if (msg.msgid == MAVLINK_MSG_ID_BATTERY_STATUS) {
                mavlink_battery_status_t bat;
                mavlink_msg_battery_status_decode(&msg, &bat);
                
                // Kontrola validity napeti - 0xFFFF = invalid (baterka nepripojena)
                if (bat.voltages[0] != 0xFFFF && bat.voltages[0] > 0 && bat.voltages[0] < 65000) {
                    voltage = bat.voltages[0] / 1000.0;
                } else {
                    voltage = 0.0; // Baterka neni pripojena
                }
                
                current = bat.current_battery / 100.0;
                
                // -1 nebo 255 = nezname nabití
                if (bat.battery_remaining >= 0 && bat.battery_remaining <= 100) {
                    percent = bat.battery_remaining;
                } else {
                    percent = 0; // Neznámé
                }
                
                Serial.printf("[BATTERY] %.2fV, %.2fA, %d%% (connected: %s)\n", 
                    voltage, current, percent, 
                    (bat.voltages[0] != 0xFFFF && bat.voltages[0] > 0) ? "YES" : "NO");
            }
            else if (msg.msgid == MAVLINK_MSG_ID_HEARTBEAT) {
                mavlink_heartbeat_t hb;
                mavlink_msg_heartbeat_decode(&msg, &hb);
                last_heartbeat = millis();
                Serial.printf("[HEARTBEAT] Type: %d, Autopilot: %d, BaseMode: %d, SystemStatus: %d\n", 
                              hb.type, hb.autopilot, hb.base_mode, hb.system_status);
            }
        }
    }
    
    // Status kazde 5 sekund
    unsigned long now = millis();
    if (now - last_update >= 5000) {
        Serial.printf("[STATUS] Serial1.available=%d, raw_bytes=%lu, mavlink_msgs=%lu\n",
                      Serial1.available(), raw_bytes, mavlink_msgs);
        if (!data_received) {
            Serial.println("[STATUS] ZADNA MAVLINK DATA");
        } else {
            Serial.printf("[STATUS] Voltage: %.2fV, %.2fA, %d%%\n", voltage, current, percent);
        }
        last_update = now;
    }
}
