#ifndef CONFIG_H
#define CONFIG_H

// ==========================================
// HARDWARE PIN MAPPING (XIAO ESP32-S3)
// ==========================================

// Display pins (SPI)
#define TFT_CS          2   // D1
#define TFT_DC          4   // D3
#define TFT_BL          -1  // Hardware BL control disabled due to pin conflicts
#define TFT_SCLK        7   // D8
#define TFT_MOSI        9   // D10

// Touch controller I2C (CHSC6X)
#define TOUCH_INT       44  // D7 - active LOW interrupt
#define I2C_SDA         5   // D4 - Wire default SDA
#define I2C_SCL         6   // D5 - Wire default SCL

// MAVLink UART pins (Serial1)
#define MAVLINK_RX_PIN  3   // D2
#define MAVLINK_TX_PIN  1   // D0
#define MAVLINK_BAUD    57600

// MeshCore Telemetry Serial (Serial2) - Bottom pads MTDI/MTCK
#define MESHCORE_RX_PIN 41  // MTDI pad
#define MESHCORE_TX_PIN 39  // MTCK pad
#define MESHCORE_BAUD   115200

// External GPIO
#define RELAY_PIN       8   // D9 - Only free connector pin
#define TRIGGER_PIN     10  // GPIO10 (D9) - External trigger input

// ==========================================
// UI & NAVIGATION SETTINGS
// ==========================================

// Inactivity timeouts (in milliseconds)
#define UI_SCREEN5_INACTIVITY_MS 180000  // 3 minutes: Servo Control -> Battery
#define UI_SCREEN2_INACTIVITY_MS 30000   // 30 seconds: Battery -> Home (Screen 1)

// Blackout (Power Saving) Settings
#define UI_BLACKOUT_TIMEOUT_MS       300000  // 5 minutes inactivity (ground)
#define UI_FLIGHT_BLACKOUT_TIMEOUT_MS 3000   // 3 seconds inactivity (flight)
#define UI_LONG_PRESS_BLACKOUT_MS    3000    // 3 seconds long press on Home screen

// Flight Detection Thresholds
#define FLIGHT_DETECTION_ALT_MM      5000    // 5 meters relative altitude
#define FLIGHT_DETECTION_SPEED_MMS   3000    // 3 m/s ground speed

// ==========================================
// SAFETY SETTINGS
// ==========================================
#define MOTOR_TEST_DURATION_MS   5000    // Duration of motor test pulse
#define MOTOR_TEST_THROTTLE_PCT  10      // Power level for motor test

// ==========================================
// TELEMETRY OUTPUT MODE
// ==========================================
// Default is Geowork JSON format (KISS frame type 0x01)
// Uncomment to use legacy NMEA format instead (KISS frame type 0x00)
// #define TELEMETRY_MODE_NMEA

#endif // CONFIG_H
