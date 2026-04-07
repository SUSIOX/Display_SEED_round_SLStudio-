// Screen5 - Servo & Motor Test
// 3 servos (2x Aileron + DropGear) + 4 motor test (5s low RPM)

#include <stdio.h>
#include <string.h>
#include <Arduino.h>
#include <MAVLink.h>
#include "config.h"
#include "../ui.h"
#include "ui_Screen5.h"

// External MAVLink send function from main.cpp
extern void send_mavlink_servo_command(uint8_t servo_id, uint16_t pwm_value);
extern void send_mavlink_motor_test(uint8_t motor_id, uint16_t throttle_percent, uint16_t duration_sec);

lv_obj_t *ui_Screen5 = NULL;
static lv_obj_t *ui_Panel5 = NULL;

// Servo objects
static lv_obj_t *ui_ServoLabel1 = NULL;  // Aileron Left
static lv_obj_t *ui_ServoSlider1 = NULL;
static lv_obj_t *ui_ServoValue1 = NULL;

static lv_obj_t *ui_ServoLabel2 = NULL;  // Aileron Right  
static lv_obj_t *ui_ServoSlider2 = NULL;
static lv_obj_t *ui_ServoValue2 = NULL;

static lv_obj_t *ui_DropGearBtn = NULL;  // DropGear toggle
static lv_obj_t *ui_DropGearLabel = NULL;
static bool drop_gear_state = false;

// Motor objects
static lv_obj_t *ui_MotorBtn[4] = {NULL, NULL, NULL, NULL};
static lv_obj_t *ui_MotorLabel[4] = {NULL, NULL, NULL, NULL};
static bool motor_running[4] = {false, false, false, false};
static uint32_t motor_start_time[4] = {0, 0, 0, 0};

// Safety objects
static lv_obj_t *ui_SafetyBtn = NULL;
static lv_obj_t *ui_SafetyLabel = NULL;
static bool safety_unlocked = false;
static uint32_t safety_unlock_time = 0;

// Stop button
static lv_obj_t *ui_StopBtn = NULL;

// Timer for safety and motor timeout
static uint32_t ui_Screen5_load_time = 0;
static lv_point_t ui_Screen5_press_point;
static bool ui_Screen5_press_active = false;

// Forward declarations
static void ui_event_Screen5(lv_event_t *e);
static void servo_slider_cb(lv_event_t *e);
static void drop_gear_btn_cb(lv_event_t *e);
static void motor_btn_cb(lv_event_t *e);
static void update_motor_timers(void);
static void stop_all_motors(void);
static void center_servos(void);

// Navigation
extern lv_obj_t *ui_Screen4;
extern void ui_Screen4_screen_init(void);
extern lv_obj_t *ui_Screen1;
extern void ui_Screen1_screen_init(void);

///////////////////// HELPER FUNCTIONS ////////////////////

static void ui_enable_event_bubble_recursive(lv_obj_t *parent) {
    uint32_t child_count = lv_obj_get_child_cnt(parent);
    for (uint32_t i = 0; i < child_count; i++) {
        lv_obj_t *child = lv_obj_get_child(parent, i);
        lv_obj_add_flag(child, LV_OBJ_FLAG_EVENT_BUBBLE);
        ui_enable_event_bubble_recursive(child);
    }
}

static void update_servo_label(lv_obj_t *label, int16_t value) {
    char buf[16];
    sprintf(buf, "%d%%", value);
    lv_label_set_text(label, buf);
}

static void set_all_controls_enabled(bool enabled) {
    if (ui_ServoSlider1) {
        if (enabled) lv_obj_clear_state(ui_ServoSlider1, LV_STATE_DISABLED);
        else lv_obj_add_state(ui_ServoSlider1, LV_STATE_DISABLED);
    }
    if (ui_ServoSlider2) {
        if (enabled) lv_obj_clear_state(ui_ServoSlider2, LV_STATE_DISABLED);
        else lv_obj_add_state(ui_ServoSlider2, LV_STATE_DISABLED);
    }
    if (ui_DropGearBtn) {
        if (enabled) lv_obj_clear_state(ui_DropGearBtn, LV_STATE_DISABLED);
        else lv_obj_add_state(ui_DropGearBtn, LV_STATE_DISABLED);
    }
    for (int i = 0; i < 4; i++) {
        if (ui_MotorBtn[i]) {
            if (enabled) lv_obj_clear_state(ui_MotorBtn[i], LV_STATE_DISABLED);
            else lv_obj_add_state(ui_MotorBtn[i], LV_STATE_DISABLED);
        }
    }
}

///////////////////// EVENT HANDLERS ////////////////////

static void servo_slider_cb(lv_event_t *e) {
    lv_obj_t *slider = lv_event_get_target(e);
    int16_t value = lv_slider_get_value(slider);
    
    // Convert percentage (0-100) to PWM (1000-2000)
    uint16_t pwm = 1000 + (value * 10);
    
    if (slider == ui_ServoSlider1) {
        update_servo_label(ui_ServoValue1, value);
        send_mavlink_servo_command(1, pwm);  // Servo 1 = Aileron Left
    } else if (slider == ui_ServoSlider2) {
        update_servo_label(ui_ServoValue2, value);
        send_mavlink_servo_command(2, pwm);  // Servo 2 = Aileron Right
    }
}

static void drop_gear_btn_cb(lv_event_t *e) {
    (void)e;
    // SAFETY BYPASSED FOR TESTING - was: if (!safety_unlocked) return;
    
    drop_gear_state = !drop_gear_state;
    uint16_t pwm = drop_gear_state ? 2000 : 1000;
    
    send_mavlink_servo_command(3, pwm);  // Servo 3 = DropGear
    
    if (drop_gear_state) {
        lv_obj_set_style_bg_color(ui_DropGearBtn, lv_color_hex(0xFF0000), LV_PART_MAIN);
        lv_label_set_text(ui_DropGearLabel, "DROP: OPEN");
    } else {
        lv_obj_set_style_bg_color(ui_DropGearBtn, lv_color_hex(0x2a2a2a), LV_PART_MAIN);
        lv_label_set_text(ui_DropGearLabel, "DROP: CLOSED");
    }
}

static void motor_btn_cb(lv_event_t *e) {
    lv_obj_t *btn = lv_event_get_target(e);
    
    int motor_id = -1;
    for (int i = 0; i < 4; i++) {
        if (btn == ui_MotorBtn[i]) {
            motor_id = i;
            break;
        }
    }
    if (motor_id < 0) return;
    
    if (motor_running[motor_id]) {
        // Toggle OFF: Stop motor if already running
        motor_running[motor_id] = false;
        lv_obj_set_style_bg_color(ui_MotorBtn[motor_id], lv_color_hex(0x2a2a2a), LV_PART_MAIN);
        char buf[8];
        sprintf(buf, "M%d", motor_id + 1);
        lv_label_set_text(ui_MotorLabel[motor_id], buf);
        send_mavlink_motor_test(motor_id + 1, 0, 0);
    } else {
        // Toggle ON: Start motor test
        motor_running[motor_id] = true;
        motor_start_time[motor_id] = millis();
        lv_obj_set_style_bg_color(ui_MotorBtn[motor_id], lv_color_hex(0xFF5500), LV_PART_MAIN);
        lv_label_set_text(ui_MotorLabel[motor_id], "RUN");
        send_mavlink_motor_test(motor_id + 1, MOTOR_TEST_THROTTLE_PCT, MOTOR_TEST_DURATION_MS / 1000);
    }
}

// Removed safety and stop buttons as per user request to keep UI clean

static void stop_all_motors(void) {
    for (int i = 0; i < 4; i++) {
        motor_running[i] = false;
        if (ui_MotorBtn[i]) {
            lv_obj_set_style_bg_color(ui_MotorBtn[i], lv_color_hex(0x2a2a2a), LV_PART_MAIN);
        }
        if (ui_MotorLabel[i]) {
            char buf[8];
            sprintf(buf, "M%d", i + 1);
            lv_label_set_text(ui_MotorLabel[i], buf);
        }
    }
    // Send stop commands via MAVLink
    for (int i = 0; i < 4; i++) {
        send_mavlink_motor_test(i + 1, 0, 0);
    }
}

static void center_servos(void) {
    // Center sliders
    if (ui_ServoSlider1) lv_slider_set_value(ui_ServoSlider1, 50, LV_ANIM_OFF);
    if (ui_ServoSlider2) lv_slider_set_value(ui_ServoSlider2, 50, LV_ANIM_OFF);
    
    // Send center PWM (1500)
    send_mavlink_servo_command(1, 1500);
    send_mavlink_servo_command(2, 1500);
    if (!drop_gear_state) {
        send_mavlink_servo_command(3, 1500);
    }
    
    // Update labels
    update_servo_label(ui_ServoValue1, 50);
    update_servo_label(ui_ServoValue2, 50);
}

// Removed automatic safety state management

static void update_motor_timers(void) {
    uint32_t now = millis();
    for (int i = 0; i < 4; i++) {
        if (motor_running[i] && (now - motor_start_time[i] >= MOTOR_TEST_DURATION_MS)) {
            motor_running[i] = false;
            if (ui_MotorBtn[i]) lv_obj_set_style_bg_color(ui_MotorBtn[i], lv_color_hex(0x2a2a2a), LV_PART_MAIN);
            if (ui_MotorLabel[i]) {
                char buf[8];
                sprintf(buf, "M%d", i + 1);
                lv_label_set_text(ui_MotorLabel[i], buf);
            }
            send_mavlink_motor_test(i + 1, 0, 0);
        }
    }
}

///////////////////// SCREEN INITIALIZATION ////////////////////

void ui_Screen5_screen_init(void) {
    ui_Screen5 = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(ui_Screen5, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui_Screen5, 255, LV_PART_MAIN);
    
    // Create main panel
    ui_Panel5 = lv_obj_create(ui_Screen5);
    lv_obj_set_size(ui_Panel5, 220, 240);
    lv_obj_align(ui_Panel5, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_opa(ui_Panel5, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(ui_Panel5, 0, LV_PART_MAIN);
    lv_obj_clear_flag(ui_Panel5, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(ui_Panel5, LV_OBJ_FLAG_EVENT_BUBBLE);
    
    // Title
    lv_obj_t *title = lv_label_create(ui_Panel5);
    lv_label_set_text(title, "SERVO TEST");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(title, lv_color_hex(0x00FF00), LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 5);
    
    // Aileron Left (Servo 1)
    lv_obj_t *label1 = lv_label_create(ui_Panel5);
    lv_label_set_text(label1, "AIL L:");
    lv_obj_set_style_text_color(label1, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(label1, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(label1, LV_ALIGN_TOP_LEFT, 24, 30);
    
    ui_ServoSlider1 = lv_slider_create(ui_Panel5);
    lv_obj_set_size(ui_ServoSlider1, 90, 15);
    lv_obj_align(ui_ServoSlider1, LV_ALIGN_TOP_LEFT, 62, 30);
    lv_slider_set_range(ui_ServoSlider1, 0, 100);
    lv_slider_set_value(ui_ServoSlider1, 50, LV_ANIM_OFF);
    lv_obj_add_event_cb(ui_ServoSlider1, servo_slider_cb, LV_EVENT_VALUE_CHANGED, NULL);
    
    ui_ServoValue1 = lv_label_create(ui_Panel5);
    lv_label_set_text(ui_ServoValue1, "50%");
    lv_obj_set_style_text_color(ui_ServoValue1, lv_color_hex(0x00FF00), LV_PART_MAIN);
    lv_obj_set_style_text_font(ui_ServoValue1, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(ui_ServoValue1, LV_ALIGN_TOP_LEFT, 155, 30);
    
    // Aileron Right (Servo 2)
    lv_obj_t *label2 = lv_label_create(ui_Panel5);
    lv_label_set_text(label2, "AIL R:");
    lv_obj_set_style_text_color(label2, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(label2, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(label2, LV_ALIGN_TOP_LEFT, 23, 55);
    
    ui_ServoSlider2 = lv_slider_create(ui_Panel5);
    lv_obj_set_size(ui_ServoSlider2, 90, 15);
    lv_obj_align(ui_ServoSlider2, LV_ALIGN_TOP_LEFT, 62, 55);
    lv_slider_set_range(ui_ServoSlider2, 0, 100);
    lv_slider_set_value(ui_ServoSlider2, 50, LV_ANIM_OFF);
    lv_obj_add_event_cb(ui_ServoSlider2, servo_slider_cb, LV_EVENT_VALUE_CHANGED, NULL);
    
    ui_ServoValue2 = lv_label_create(ui_Panel5);
    lv_label_set_text(ui_ServoValue2, "50%");
    lv_obj_set_style_text_color(ui_ServoValue2, lv_color_hex(0x00FF00), LV_PART_MAIN);
    lv_obj_set_style_text_font(ui_ServoValue2, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(ui_ServoValue2, LV_ALIGN_TOP_LEFT, 155, 55);
    
    // DropGear button (Servo 3)
    ui_DropGearBtn = lv_btn_create(ui_Panel5);
    lv_obj_set_size(ui_DropGearBtn, 180, 25);
    lv_obj_align(ui_DropGearBtn, LV_ALIGN_TOP_MID, 0, 80);
    lv_obj_set_style_bg_color(ui_DropGearBtn, lv_color_hex(0x2a2a2a), LV_PART_MAIN);
    lv_obj_add_event_cb(ui_DropGearBtn, drop_gear_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_clear_flag(ui_DropGearBtn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(ui_DropGearBtn, LV_OBJ_FLAG_EVENT_BUBBLE);
    
    ui_DropGearLabel = lv_label_create(ui_DropGearBtn);
    lv_label_set_text(ui_DropGearLabel, "DROP: CLOSED");
    lv_obj_set_style_text_color(ui_DropGearLabel, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(ui_DropGearLabel, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_center(ui_DropGearLabel);
    
    // Motor Test section
    lv_obj_t *motor_title = lv_label_create(ui_Panel5);
    lv_label_set_text(motor_title, "MOTOR TEST (5s)");
    lv_obj_set_style_text_font(motor_title, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(motor_title, lv_color_hex(0xFFFF00), LV_PART_MAIN);
    lv_obj_align(motor_title, LV_ALIGN_TOP_MID, 0, 116);
    
    // Motor buttons (2x2 grid) - positions from layout editor
    int motor_x[4] = {15, 115, 45, 115};
    int motor_y[4] = {137, 137, 175, 175};
    int motor_w[4] = {90, 90, 60, 60};
    
    for (int i = 0; i < 4; i++) {
        ui_MotorBtn[i] = lv_btn_create(ui_Panel5);
        lv_obj_set_size(ui_MotorBtn[i], motor_w[i], 35);
        lv_obj_align(ui_MotorBtn[i], LV_ALIGN_TOP_LEFT, motor_x[i], motor_y[i]);
        lv_obj_set_style_bg_color(ui_MotorBtn[i], lv_color_hex(0x2a2a2a), LV_PART_MAIN);
        lv_obj_add_event_cb(ui_MotorBtn[i], motor_btn_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_clear_flag(ui_MotorBtn[i], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(ui_MotorBtn[i], LV_OBJ_FLAG_EVENT_BUBBLE);
        
        ui_MotorLabel[i] = lv_label_create(ui_MotorBtn[i]);
        char buf[8];
        sprintf(buf, "M%d", i + 1);
        lv_label_set_text(ui_MotorLabel[i], buf);
        lv_obj_set_style_text_color(ui_MotorLabel[i], lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        lv_obj_set_style_text_font(ui_MotorLabel[i], &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_center(ui_MotorLabel[i]);
    }
    
    // Enable Screen 5 swipe navigation
    lv_obj_add_event_cb(ui_Screen5, ui_event_Screen5, LV_EVENT_ALL, NULL);
    
    // Controls are always enabled (Safety lock removed)
    safety_unlocked = true;
    set_all_controls_enabled(true);
    
    ui_enable_event_bubble_recursive(ui_Screen5);
    ui_Screen5_load_time = lv_tick_get();
}

static void ui_event_Screen5(lv_event_t *e) {
    lv_event_code_t event_code = lv_event_get_code(e);
    lv_indev_t *indev = lv_indev_get_act();
    lv_point_t point;
    
    if (!indev) return;
    if ((lv_tick_get() - ui_Screen5_load_time) < 400) return;
    
    if (event_code == LV_EVENT_PRESSED) {
        lv_indev_get_point(indev, &ui_Screen5_press_point);
        ui_Screen5_press_active = true;
    }
    if (event_code == LV_EVENT_PRESS_LOST) {
        ui_Screen5_press_active = false;
    }
    if (event_code == LV_EVENT_RELEASED && ui_Screen5_press_active) {
        int dx, dy;
        ui_Screen5_press_active = false;
        lv_indev_get_point(indev, &point);
        dx = point.x - ui_Screen5_press_point.x;
        dy = point.y - ui_Screen5_press_point.y;
        
        if (LV_ABS(dy) > 40 && LV_ABS(dy) > LV_ABS(dx)) {
            if (dy > 0) {
                _ui_screen_change(&ui_Screen1, LV_SCR_LOAD_ANIM_MOVE_BOTTOM, 300, 0, &ui_Screen1_screen_init);
            } else {
                _ui_screen_change(&ui_Screen4, LV_SCR_LOAD_ANIM_MOVE_TOP, 300, 0, &ui_Screen4_screen_init);
            }
        }
    }
}

// Public function to update timers (call from main loop)
void ui_Screen5_update_timers(void) {
    if (!ui_Screen5) return;
    update_motor_timers();
}

void ui_Screen5_screen_destroy(void) {
    // Stop everything before destroying
    stop_all_motors();
    
    if (ui_Screen5) {
        lv_obj_del(ui_Screen5);
        ui_Screen5 = NULL;
    }
}
