/* Seeed Studio Round Display Library - Simplified Implementation */

#ifndef LV_XIAO_ROUND_SCREEN_H
#define LV_XIAO_ROUND_SCREEN_H

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <lvgl.h>
#include <Wire.h>

#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 240
#define CHSC6X_I2C_ID 0x2e
#define CHSC6X_MAX_POINTS_NUM 1
#define CHSC6X_READ_POINT_LEN 5
#define TOUCH_INT 44
#define XIAO_BL -1

TFT_eSPI tft = TFT_eSPI(SCREEN_WIDTH, SCREEN_HEIGHT);

void xiao_disp_init(void)
{
#if XIAO_BL > 0
    pinMode(XIAO_BL, OUTPUT); //Turn on screen backlight
    digitalWrite(XIAO_BL, HIGH);
#endif

    tft.begin();
    tft.setRotation(2);
    tft.fillScreen(TFT_BLACK);
}

void lv_xiao_disp_init(void)
{
    xiao_disp_init();
}

// Pure I2C polling - no GPIO44 interrupt needed (for dual-core)
bool chsc6x_is_pressed(void)
{
    // Read directly from I2C to check if touch is active
    uint8_t temp[CHSC6X_READ_POINT_LEN] = {0};
    uint8_t read_len = Wire.requestFrom(CHSC6X_I2C_ID, CHSC6X_READ_POINT_LEN);
    
    if(read_len == CHSC6X_READ_POINT_LEN) {
        Wire.readBytes(temp, read_len);
        return (temp[0] == 0x01); // Touch active when first byte is 0x01
    }
    return false;
}

void lv_xiao_touch_init(void)
{
    // Wire.begin() is called in main setup() BEFORE MAVLink task
    // This avoids conflicts between I2C and Serial1 on GPIO44/43
    // Note: GPIO44 interrupt NOT used - Core 0 uses GPIO44 for MAVLink UART
}

void chsc6x_convert_xy(uint8_t *x, uint8_t *y)
{
    uint8_t x_tmp = *x, y_tmp = *y, _end = 0;
    for(int i=1; i<=2; i++){  // rotation = 2 (180 degrees)
        x_tmp = *x;
        y_tmp = *y;
        _end = (i % 2) ? SCREEN_WIDTH : SCREEN_HEIGHT;
        *x = y_tmp;
        *y = _end - x_tmp;
    }
}

void chsc6x_read(lv_indev_drv_t * indev_driver, lv_indev_data_t * data)
{
    if (chsc6x_is_pressed()) {
        data->state = LV_INDEV_STATE_PR;
        uint8_t x = 0, y = 0;
        
        uint8_t temp[CHSC6X_READ_POINT_LEN] = {0};
        uint8_t read_len = Wire.requestFrom(CHSC6X_I2C_ID, CHSC6X_READ_POINT_LEN);
        if(read_len == CHSC6X_READ_POINT_LEN){
            Wire.readBytes(temp, read_len);
            if (temp[0] == 0x01) {
                chsc6x_convert_xy(&temp[2], &temp[4]);
                x = temp[2];
                y = temp[4];
            }
        }
        
        data->point.x = x;
        data->point.y = y;
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
}

#endif // LV_XIAO_ROUND_SCREEN_H
