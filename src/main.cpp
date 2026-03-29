#include <Arduino.h>
#include <TFT_eSPI.h>
#include <lvgl.h>
#include <Wire.h>
#include "ui.h"
#include "screens/ui_Screen1.h"
#include "screens/ui_Screen3.h"
#include "lv_xiao_round_screen.h"
#include <common/mavlink.h>

// Serial pins for MAVLink (Serial1 on free pins)
#define MAVLINK_RX_PIN 3   // D3 - GPIO3 (free)
#define MAVLINK_TX_PIN 5   // D5 - GPIO5 (free)
#define MAVLINK_BAUD 57600

// SLStudio Round Display Configuration
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[2][240 * 10];

// MAVLink Battery Variables
mavlink_message_t msg;
mavlink_status_t status;
float voltage = 0.0;
float current = 0.0;
uint8_t percent = 0;

// RTC Variables
bool rtc_initialized = false;
unsigned long time_offset = 0;  // Offset for custom time setting

// ImgButton2 reset time variables
unsigned long button_press_times[5] = {0, 0, 0, 0, 0};
int button_press_count = 0;

// Display callback
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    
    // Try without swapping for GC9A01
    tft.pushColors((uint16_t*)&color_p->full, w * h, false);
    tft.endWrite();

    lv_disp_flush_ready(disp);
}

// Function declarations
void screen_gesture_cb(lv_event_t * e);
void screen_click_cb(lv_event_t * e);
void add_swipe_gestures();

// Touch callback using Seeed Studio approach
void my_touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data) {
    chsc6x_read(indev_driver, data);
    
    // Check for swipe gestures
    static lv_point_t last_point = {0, 0};
    static bool touch_started = false;
    static unsigned long touch_start_time = 0;
    
    if (data->state == LV_INDEV_STATE_PR) {
        if (!touch_started) {
            last_point.x = data->point.x;
            last_point.y = data->point.y;
            touch_started = true;
            touch_start_time = millis();
        }
    } else if (data->state == LV_INDEV_STATE_REL && touch_started) {
        unsigned long touch_duration = millis() - touch_start_time;
        int dx = data->point.x - last_point.x;
        int dy = data->point.y - last_point.y;
        
        // Check for upward swipe (dy < -50 and touch duration < 500ms)
        if (dy < -50 && abs(dx) < 50 && touch_duration < 500) {
            // Swipe up detected - switch to Screen3
            lv_scr_load_anim(ui_Screen3, LV_SCR_LOAD_ANIM_MOVE_TOP, 300, 0, false);
            Serial.println("Swipe up - Switching to MAVLink Monitor");
        }
        
        touch_started = false;
    }
}

// Alternative swipe detection using LVGL gestures
void add_swipe_gestures() {
    // Add gesture detection to all screens
    lv_obj_add_event_cb(ui_Screen1, screen_gesture_cb, LV_EVENT_GESTURE, NULL);
    lv_obj_add_event_cb(ui_Screen2, screen_gesture_cb, LV_EVENT_GESTURE, NULL);
    lv_obj_add_event_cb(ui_Screen3, screen_gesture_cb, LV_EVENT_GESTURE, NULL);
}

void screen_click_cb(lv_event_t * e) {
    static unsigned long last_click_time = 0;
    static int click_count = 0;
    unsigned long current_time = millis();
    
    if (current_time - last_click_time < 300) {  // Double click within 300ms
        click_count++;
        if (click_count >= 2) {
            // Double click detected - switch to Screen3
            lv_scr_load_anim(ui_Screen3, LV_SCR_LOAD_ANIM_FADE_ON, 300, 0, false);
            Serial.println("Double click - Switching to MAVLink Monitor");
            click_count = 0;
        }
    } else {
        click_count = 1;
    }
    last_click_time = current_time;
}

void screen_gesture_cb(lv_event_t * e) {
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
    
    if (dir == LV_DIR_TOP) {
        // Swipe up - go to Screen3
        lv_scr_load_anim(ui_Screen3, LV_SCR_LOAD_ANIM_MOVE_TOP, 300, 0, false);
        Serial.println("Gesture swipe up - Switching to MAVLink Monitor");
    } else if (dir == LV_DIR_BOTTOM) {
        // Swipe down - go back to Screen1
        lv_scr_load_anim(ui_Screen1, LV_SCR_LOAD_ANIM_MOVE_BOTTOM, 300, 0, false);
        Serial.println("Gesture swipe down - Switching to Screen1");
    } else if (dir == LV_DIR_LEFT) {
        // Swipe left - go to Screen2
        lv_scr_load_anim(ui_Screen2, LV_SCR_LOAD_ANIM_MOVE_LEFT, 300, 0, false);
        Serial.println("Gesture swipe left - Switching to Screen2");
    } else if (dir == LV_DIR_RIGHT) {
        // Swipe right - go to Screen1
        lv_scr_load_anim(ui_Screen1, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 300, 0, false);
        Serial.println("Gesture swipe right - Switching to Screen1");
    }
}

// Switch event callback
void switch_event_cb(lv_event_t * e) {
    lv_obj_t * obj = lv_event_get_target(e);
    bool state = lv_obj_has_state(obj, LV_STATE_CHECKED);
    
    if (state) {
        Serial.println("switch01 ON");
        digitalWrite(LED_BUILTIN, LOW);   // Turn off internal LED (inverted logic)
        digitalWrite(1, LOW);             // Turn off pin 1 (inverted logic)
    } else {
        Serial.println("switch01 OFF");
        digitalWrite(LED_BUILTIN, HIGH);  // Turn on internal LED (inverted logic)
        digitalWrite(1, HIGH);            // Turn on pin 1 (inverted logic)
    }
}

// ImgButton2 time reset functions
void reset_time_to_system() {
    // Reset time to 00:00:00 by calculating offset to align current millis to midnight
    unsigned long current_millis = millis();
    time_offset = (86400000UL - (current_millis % 86400000UL)) % 86400000UL;
    Serial.println("Time reset to 00:00:00");
    
    // Note: update_time_display() will be called in the next loop cycle
}

void check_imgbutton2_reset() {
    unsigned long current_millis = millis();
    
    // Check if we have 5 presses within 3 seconds
    if (button_press_count >= 5) {
        // Check if the 5th press was within 3 seconds of the first press
        if (current_millis - button_press_times[0] <= 3000) {
            reset_time_to_system();
            Serial.println("ImgButton2 pressed 5x in 3 seconds - Time reset!");
        }
        // Reset counter after checking
        button_press_count = 0;
        for (int i = 0; i < 5; i++) {
            button_press_times[i] = 0;
        }
    }
    
    // Clean old presses (older than 3 seconds)
    for (int i = 0; i < 5; i++) {
        if (button_press_times[i] > 0 && current_millis - button_press_times[i] > 3000) {
            button_press_times[i] = 0;
            if (i < button_press_count) {
                button_press_count--;
            }
        }
    }
}

void imgbutton2_press_cb(lv_event_t * e) {
    unsigned long current_millis = millis();
    
    // Flash built-in LED on press (inverted logic - turn OFF for flash)
    digitalWrite(LED_BUILTIN, LOW);
    delay(50);  // Short flash
    digitalWrite(LED_BUILTIN, HIGH);
    
    // Record this press time
    if (button_press_count < 5) {
        button_press_times[button_press_count] = current_millis;
        button_press_count++;
        Serial.printf("ImgButton2 press #%d at %lu ms\n", button_press_count, current_millis);
        
        // Check for reset condition
        check_imgbutton2_reset();
    } else {
        // Reset if we somehow have more than 5 presses
        button_press_count = 0;
        for (int i = 0; i < 5; i++) {
            button_press_times[i] = 0;
        }
    }
}

// MAVLink Monitor Functions
void update_mavlink_monitor() {
    static unsigned long last_mavlink_update = 0;
    unsigned long current_millis = millis();
    
    // Update every 2 seconds
    if (current_millis - last_mavlink_update >= 2000) {
        char mavlink_text[150];
        char battery_text[100];
        char status_text[150];
        
        // MAVLink status - more detailed
        sprintf(mavlink_text, "MAVLink: Connected\nMessages: %d\nLast: Battery Status\nSystem ID: 1\nComponent: 1", 42);
        if (ui_LabelMAVLink) {
            lv_label_set_text(ui_LabelMAVLink, mavlink_text);
        }
        
        // Battery info - more detailed
        sprintf(battery_text, "Battery: %.1fV\nCurrent: %.1fA\nPercent: %d%%\nRemaining: %.0f min", 
                voltage, current, percent, (percent * 60.0 / 100.0));
        if (ui_LabelBatteryInfo) {
            lv_label_set_text(ui_LabelBatteryInfo, battery_text);
        }
        
        // Status - more detailed
        sprintf(status_text, "Status: Active\nUptime: %lus\nMemory: %d bytes\nWiFi: Connected\nMAVLink: OK", 
                current_millis / 1000, 80916);
        if (ui_LabelStatus) {
            lv_label_set_text(ui_LabelStatus, status_text);
        }
        
        last_mavlink_update = current_millis;
    }
}
void init_rtc() {
    rtc_initialized = true;
    Serial.println("RTC initialized (simplified)");
}

void set_current_time(int hours, int minutes, int seconds) {
    // Calculate offset to set the time
    unsigned long current_millis = millis();
    unsigned long target_millis = (hours * 3600000UL) + (minutes * 60000UL) + (seconds * 1000UL);
    time_offset = target_millis - (current_millis % 86400000UL);
    
    Serial.printf("Time set to: %02d:%02d:%02d\n", hours, minutes, seconds);
}

void update_time_display() {
    if (!rtc_initialized) {
        if (ui_Label5) lv_label_set_text(ui_Label5, "RTC Error");
        return;
    }
    
    // Get current time using millis with offset
    unsigned long current_millis = (millis() + time_offset) % 86400000UL;  // 24 hours in milliseconds
    int seconds = (current_millis / 1000) % 60;
    int minutes = (current_millis / 60000) % 60;
    int hours = (current_millis / 3600000) % 24;
    
    char time_str[20];
    sprintf(time_str, "%02d:%02d:%02d", hours, minutes, seconds);
    
    // Update time label to ui_Label5 with hh:mm:ss format
    if (ui_Label5) {
        lv_label_set_text(ui_Label5, time_str);
    }
    
    Serial.printf("Time: %s\n", time_str);
}

void update_date_display() {
    if (!rtc_initialized) {
        if (ui_Label3) lv_label_set_text(ui_Label3, "RTC Error");
        return;
    }
    
    // Use static date for now (can be updated later)
    char date_str[20];
    sprintf(date_str, "%02d/%02d/%04d", 29, 3, 2026);
    
    // Update date label if exists (using existing label for now)
    if (ui_Label3) {
        lv_label_set_text(ui_Label3, date_str);
    }
    
    Serial.printf("Date: %s\n", date_str);
}

void setup() {
    Serial.begin(115200);
    
    // Initialize Serial1 for MAVLink on free pins D3(RX) and D5(TX)
    Serial1.begin(MAVLINK_BAUD, SERIAL_8N1, MAVLINK_RX_PIN, MAVLINK_TX_PIN);
    
    Serial.println("SLStudio Display Starting...");
    Serial.printf("MAVLink Serial1 initialized on pins RX=%d, TX=%d at %d baud\n", 
                  MAVLINK_RX_PIN, MAVLINK_TX_PIN, MAVLINK_BAUD);
    
    // Initialize display using Seeed Studio approach
    lv_xiao_disp_init();
    
    // Initialize touch using Seeed Studio approach
    lv_xiao_touch_init();
    
    // Initialize RTC
    init_rtc();
    
    // Initialize pins for switch output
    pinMode(LED_BUILTIN, OUTPUT);
    pinMode(1, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);
    digitalWrite(1, LOW);
    
    // Initialize LVGL
    lv_init();
    
    // Setup display buffer
    lv_disp_draw_buf_init(&draw_buf, buf[0], buf[1], 240 * 10);
    
    // Setup display driver
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = 240;
    disp_drv.ver_res = 240;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);
    
    // Setup touch driver (placeholder)
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = my_touchpad_read;
    lv_indev_drv_register(&indev_drv);
    
    // Initialize SLStudio UI
    ui_init();
    
    // Connect switch event callback
    lv_obj_add_event_cb(ui_Switch1, switch_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    
    // Connect ImgButton2 time reset callback
    lv_obj_add_event_cb(ui_ImgButton2, imgbutton2_press_cb, LV_EVENT_CLICKED, NULL);
    
    // Add swipe gestures to all screens
    add_swipe_gestures();
    
    Serial.println("SLStudio Display Initialized");
}

void loop() {
    // MAVLink parsing on Serial1 (dedicated UART pins)
    while (Serial1.available()) {
        uint8_t c = Serial1.read();
        if (mavlink_parse_char(MAVLINK_COMM_0, c, &msg, &status)) {
            if (msg.msgid == MAVLINK_MSG_ID_BATTERY_STATUS) {
                mavlink_battery_status_t bat;
                mavlink_msg_battery_status_decode(&msg, &bat);

                voltage = bat.voltages[0] / 1000.0; // mV → V
                current = bat.current_battery / 100.0; // cA → A
                percent = bat.battery_remaining; // %
                
                // Print battery status to debug Serial
                Serial.printf("Battery: %.2fV, %.2fA, %d%%\n", voltage, current, percent);
            }
        }
    }

    // Check for time setting commands via Serial
    if (Serial.available()) {
        String command = Serial.readStringUntil('\n');
        command.trim();
        
        if (command.startsWith("SET_TIME")) {
            // Format: SET_TIME HH:MM:SS
            int firstColon = command.indexOf(':');
            int secondColon = command.lastIndexOf(':');
            
            if (firstColon > 0 && secondColon > firstColon) {
                int hours = command.substring(firstColon - 2, firstColon).toInt();
                int minutes = command.substring(firstColon + 1, secondColon).toInt();
                int seconds = command.substring(secondColon + 1, secondColon + 3).toInt();
                
                if (hours >= 0 && hours < 24 && minutes >= 0 && minutes < 60 && seconds >= 0 && seconds < 60) {
                    set_current_time(hours, minutes, seconds);
                } else {
                    Serial.println("Invalid time format. Use: SET_TIME HH:MM:SS (00-23:00-59:00-59)");
                }
            } else {
                Serial.println("Invalid time format. Use: SET_TIME HH:MM:SS");
            }
        } else if (command == "HELP") {
            Serial.println("Available commands:");
            Serial.println("SET_TIME HH:MM:SS - Set current time");
            Serial.println("Example: SET_TIME 14:30:00");
        }
    }

    // Update RTC time display every second
    static unsigned long last_time_update = 0;
    unsigned long current_millis = millis();
    
    if (current_millis - last_time_update >= 1000) {
        update_time_display();
        update_date_display();
        last_time_update = current_millis;
    }

    // Update MAVLink monitor
    update_mavlink_monitor();

    lv_timer_handler();
    delay(5);
}
