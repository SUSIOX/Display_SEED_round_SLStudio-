/*
 * ESP32-S3 Xiao Round Display - VERIFIED Pin Mapping
 * =====================================================
 * Source: pins_arduino.h from framework-arduinoespressif32/variants/XIAO_ESP32S3/
 *
 * CONNECTOR PIN MAPPING (D0-D10 labels on board):
 * ================================================
 * Label | GPIO | Function in this project
 * ------+------+------------------------------------------
 * D0    |  1   | MAVLINK_TX (Serial1)
 * D1    |  2   | TFT_CS  (SPI Chip Select)
 * D2    |  3   | MAVLINK_RX (Serial1)
 * D3    |  4   | TFT_DC  (SPI Data/Command)
 * D4    |  5   | I2C SDA *** RESERVED - Wire default SDA ***
 * D5    |  6   | I2C SCL / TFT_BL *** RESERVED ***
 * D6    | 43   | USB UART TX (Serial debug)
 * D7    | 44   | TOUCH_INT (active LOW from CHSC6X)
 * D8    |  7   | TFT_SCLK (SPI Clock)
 * D9    |  8   | FREE - RELAY_PIN (output for relay)
 * D10   |  9   | TFT_MOSI (SPI Data)
 *
 * !! CRITICAL WARNINGS !!
 * =======================
 * D4 (GPIO5) = Wire default SDA - NEVER set as OUTPUT/INPUT_PULLUP!
 * D5 (GPIO6) = Wire default SCL AND TFT backlight - NEVER use as GPIO!
 * D7 (GPIO44) = TOUCH_INT - configure as INPUT only, active LOW
 * D8 (GPIO7)  = TFT_SCLK - NEVER use for other purposes
 * D10 (GPIO9) = TFT_MOSI - NEVER use for other purposes
 *
 * ONLY FREE CONNECTOR PIN: D9 (GPIO8) = RELAY output
 */

#ifndef XIAO_PINOUT_H
#define XIAO_PINOUT_H

// Display pins (SPI) - DO NOT USE FOR OTHER FUNCTIONS
#define XIAO_TFT_CS     2   // D1
#define XIAO_TFT_DC     4   // D3
#define XIAO_TFT_BL     6   // D5 (shared with I2C SCL!)
#define XIAO_TFT_SCLK   7   // D8
#define XIAO_TFT_MOSI   9   // D10

// Touch controller I2C (CHSC6X) - Wire.begin() defaults
#define XIAO_TOUCH_INT  44  // D7 - active LOW interrupt
#define XIAO_I2C_SDA    5   // D4 - Wire default SDA (CONFIRMED from pins_arduino.h)
#define XIAO_I2C_SCL    6   // D5 - Wire default SCL (CONFIRMED from pins_arduino.h)

// MAVLink UART pins
#define XIAO_MAVLINK_RX  3  // D2
#define XIAO_MAVLINK_TX  1  // D0

// Only free connector pin for external GPIO
#define XIAO_FREE_RELAY  8  // D9 - MISO not used by TFT, use for relay output

// EXTRA SERIAL 3 (UART2) - Bottom pads MTDI/MTCK
#define XIAO_EXTRA_SERIAL_RX  41  // MTDI pad - GPIO 41
#define XIAO_EXTRA_SERIAL_TX  39  // MTCK pad - GPIO 39

#endif // XIAO_PINOUT_H
