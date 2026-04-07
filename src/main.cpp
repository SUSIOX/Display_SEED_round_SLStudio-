#include <Arduino.h>
#include <TFT_eSPI.h>
#include <lvgl.h>
#include <Wire.h>
#include "ui.h"
#include "screens/ui_Screen1.h"
#include "screens/ui_Screen2.h"
#include "screens/ui_Screen3.h"
#include "screens/ui_Screen4.h"
#include "screens/ui_Screen5.h"
#include "lv_xiao_round_screen.h"
#include <MAVLink.h>
#include "config.h"
#include "mavlink_data_types.h"
#include "MeshCoreTelemetry.h"
#include "blink_controller.h"

// Shared data protected by mutex
SemaphoreHandle_t mavlink_mutex = NULL;
MAVLinkData mavlink_data = {};  // Zero-initialize all fields

// MeshCore Telemetry Instance
MeshCoreTelemetry meshcore;

// MAVLink message buffers (used by Core 1 task)
mavlink_message_t msg;
mavlink_status_t status;

// SLStudio Round Display Configuration
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[2][240 * 10];

// RTC Variables
bool rtc_initialized = false;
unsigned long time_offset = 0;  // Offset for custom time setting

// Inactivity tracking
unsigned long last_interaction_time = 0;

// ImgButton2 reset time variables
unsigned long button_press_times[5] = {0, 0, 0, 0, 0};
int button_press_count = 0;

// Blink Controller
BlinkController blink_ctrl;

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
void screen_click_cb(lv_event_t * e);
void meshcore_telemetry_task(void *pvParameters); // Forward declaration

// Touch callback using Seeed Studio approach (I2C polling - no GPIO44 interrupt)
void my_touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data) {
    chsc6x_read(indev_driver, data);
    
    // Reset inactivity timer on any touch activity
    if (data->state == LV_INDEV_STATE_PR) {
        last_interaction_time = millis();
    }
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


// Switch event callback - controls GPIO1 and RELAY_PIN (GPIO5)
void switch_event_cb(lv_event_t * e) {
    lv_obj_t * obj = lv_event_get_target(e);
    bool state = lv_obj_has_state(obj, LV_STATE_CHECKED);
    
    if (state) {
        Serial.println("switch01 ON");
        digitalWrite(LED_BUILTIN, LOW);   // Turn off internal LED (inverted logic)
        digitalWrite(1, LOW);             // Turn off pin 1 (inverted logic)
        digitalWrite(RELAY_PIN, HIGH);    // Turn ON relay
    } else {
        Serial.println("switch01 OFF");
        digitalWrite(LED_BUILTIN, HIGH);  // Turn on internal LED (inverted logic)
        digitalWrite(1, HIGH);            // Turn on pin 1 (inverted logic)
        digitalWrite(RELAY_PIN, LOW);     // Turn OFF relay
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
        // Read relay pin state for status display
        bool pin5_state = digitalRead(RELAY_PIN);
        
        // Thread-safe read of MAVLink data
        float v = 0.0;
        float c = 0.0;
        uint8_t p = 0;
        bool valid = false;
        
        if (xSemaphoreTake(mavlink_mutex, 10) == pdTRUE) {
            v = mavlink_data.voltage;
            c = mavlink_data.current;
            p = mavlink_data.percent;
            valid = mavlink_data.data_valid;
            xSemaphoreGive(mavlink_mutex);
        }
        
        char mavlink_text[150];
        char battery_text[100];
        char status_text[150];
        
        // MAVLink status
        if (valid) {
            sprintf(mavlink_text, "MAVLink: Connected\nCore 1: Active\nData: Valid\nLast: Battery Status");
        } else {
            sprintf(mavlink_text, "MAVLink: Waiting...\nCore 1: Active\nData: No data yet");
        }
        if (ui_LabelMAVLink) {
            lv_label_set_text(ui_LabelMAVLink, mavlink_text);
        }
        
        // Battery info
        if (valid) {
            sprintf(battery_text, "Battery: %.1fV\nCurrent: %.1fA\nPercent: %d%%\nRemaining: %.0f min", 
                    v, c, p, (p * 60.0 / 100.0));
        } else {
            sprintf(battery_text, "Battery: --\nCurrent: --\nPercent: --\nWaiting for data...");
        }
        if (ui_LabelBatteryInfo) {
            lv_label_set_text(ui_LabelBatteryInfo, battery_text);
        }
        
        // Status with relay pin state
        sprintf(status_text, "Status: Active\nUptime: %lus\nCore 0: UI\nCore 1: MAVLink\nRelay(D9): %s", 
                current_millis / 1000, pin5_state ? "ON" : "OFF");
        if (ui_LabelStatus) {
            lv_label_set_text(ui_LabelStatus, status_text);
        }
        
        last_mavlink_update = current_millis;
    }
}

// Screen2 Battery Display Update
void update_screen2_battery() {
    static unsigned long last_s2_update = 0;
    unsigned long current_millis = millis();

    if (current_millis - last_s2_update < 2000) return;
    last_s2_update = current_millis;

    float v = 0.0;
    float c = 0.0;
    uint8_t p = 0;
    int32_t time_rem = 0;
    bool valid = false;

    if (xSemaphoreTake(mavlink_mutex, 10) == pdTRUE) {
        v = mavlink_data.voltage;
        c = mavlink_data.current;
        p = mavlink_data.percent;
        time_rem = mavlink_data.time_remaining;
        valid = mavlink_data.data_valid;
        xSemaphoreGive(mavlink_mutex);
    }

    if (!ui_Label2 || !ui_Label3 || !ui_Label4) return;

    char buf[32];
    if (valid) {
        sprintf(buf, "%.1f V", v);
        lv_label_set_text(ui_Label2, buf);

        sprintf(buf, "%d %%", p);
        lv_label_set_text(ui_Label3, buf);

        if (time_rem > 0) {
            sprintf(buf, "%d min", time_rem / 60);
        } else if (c > 0.1f && p > 0) {
            // Estimate from current draw: (percent * capacity_factor) / current
            sprintf(buf, "%.0f min", (p / 100.0f) * 3600.0f / 60.0f);
        } else {
            sprintf(buf, "-- min");
        }
        lv_label_set_text(ui_Label4, buf);
    } else {
        lv_label_set_text(ui_Label2, "-- V");
        lv_label_set_text(ui_Label3, "--%");
        lv_label_set_text(ui_Label4, "-- min");
    }
}

// C-callable wrapper to send MAVLink reboot command
extern "C" void send_mavlink_reboot(void) {
    mavlink_message_t msg;
    uint8_t buf[MAVLINK_MAX_PACKET_LEN];
    
    mavlink_msg_command_long_pack(
        255, 1, &msg,
        1, 1,  // target_system=1, target_component=1
        246,   // MAV_CMD_PREFLIGHT_REBOOT_SHUTDOWN
        1,     // confirmation
        1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f  // param1=1 (reboot autopilot)
    );
    
    uint16_t len = mavlink_msg_to_send_buffer(buf, &msg);
    Serial1.write(buf, len);
}

// C-callable wrapper to send MAVLink servo command
extern "C" void send_mavlink_servo_command(uint8_t servo_id, uint16_t pwm_value) {
    mavlink_message_t msg;
    uint8_t buf[MAVLINK_MAX_PACKET_LEN];
    
    // MAV_CMD_DO_SET_SERVO = 183
    // Param1: servo number (1-16)
    // Param2: PWM value in microseconds (1000-2000)
    mavlink_msg_command_long_pack(
        255, 1, &msg,
        1, 1,  // target_system=1, target_component=1
        183,   // MAV_CMD_DO_SET_SERVO
        0,     // confirmation
        (float)servo_id,  // param1: servo number
        (float)pwm_value, // param2: PWM value
        0.0f, 0.0f, 0.0f, 0.0f, 0.0f  // unused params
    );
    
    uint16_t len = mavlink_msg_to_send_buffer(buf, &msg);
    Serial1.write(buf, len);
    
    Serial.printf("[MAVLINK] Servo %d set to %d us\n", servo_id, pwm_value);
}

// C-callable wrapper to send MAVLink motor test command
extern "C" void send_mavlink_motor_test(uint8_t motor_id, uint16_t throttle_percent, uint16_t duration_sec) {
    mavlink_message_t msg;
    uint8_t buf[MAVLINK_MAX_PACKET_LEN];
    
    // MAV_CMD_DO_MOTOR_TEST = 209
    // Param1: motor instance (1-4 for quad)
    // Param2: throttle type (0=throttle percent)
    // Param3: throttle value (0-100)
    // Param4: timeout in seconds
    // Param5: motor count (0 = all)
    // Param6: test order (0=sequence)
    mavlink_msg_command_long_pack(
        255, 1, &msg,
        1, 1,  // target_system=1, target_component=1
        209,   // MAV_CMD_DO_MOTOR_TEST
        0,     // confirmation
        (float)motor_id,        // param1: motor instance
        0.0f,                   // param2: throttle type (0=percent)
        (float)throttle_percent, // param3: throttle value
        (float)duration_sec,    // param4: timeout
        0.0f,                   // param5: motor count
        0.0f,                   // param6: test order
        0.0f                    // param7: unused
    );
    
    uint16_t len = mavlink_msg_to_send_buffer(buf, &msg);
    Serial1.write(buf, len);
    
    Serial.printf("[MAVLINK] Motor test M%d: %d%% for %ds\n", motor_id, throttle_percent, duration_sec);
}
// This task runs continuously on Core 0, reading MAVLink data from GPIO44/43
void mavlink_rx_task(void *parameter) {
    Serial.printf("[MAVLINK] Task started on core %d\n", xPortGetCoreID());
    
    unsigned long last_mavlink_log = 0;
    int mavlink_bytes = 0;
    int mavlink_msgs_count = 0;
    
    mavlink_message_t msg;
    mavlink_status_t status;
    
    while(1) {
        while (Serial1.available()) {
            uint8_t c = Serial1.read();
            mavlink_bytes++;
            if (mavlink_parse_char(MAVLINK_COMM_0, c, &msg, &status)) {
                mavlink_msgs_count++;
                
                // Protect all shared data updates with mutex
                if (xSemaphoreTake(mavlink_mutex, (TickType_t)10) == pdTRUE) {
                    if (msg.msgid == MAVLINK_MSG_ID_BATTERY_STATUS) {
                        mavlink_battery_status_t bat;
                        mavlink_msg_battery_status_decode(&msg, &bat);
                        
                        // Basic battery data
                        if (bat.voltages[0] != 0xFFFF && bat.voltages[0] > 0 && bat.voltages[0] < 65000) {
                            mavlink_data.voltage = bat.voltages[0] / 1000.0;
                        } else {
                            mavlink_data.voltage = 0.0;
                        }
                        mavlink_data.current = bat.current_battery / 100.0;
                        mavlink_data.percent = (bat.battery_remaining >= 0 && bat.battery_remaining <= 100) ? bat.battery_remaining : 0;
                        
                        // Extended battery info
                        int16_t temp = bat.temperature;
                        if (temp == 32767 || temp == -32768 || temp == 0) {
                            mavlink_data.temperature = 0; // 0 = unknown
                        } else {
                            mavlink_data.temperature = temp; // Keep in 0.01C units
                        }
                        mavlink_data.time_remaining = bat.time_remaining;
                        mavlink_data.charge_state = bat.charge_state;
                        
                        // Cell voltages (first 6 cells)
                        mavlink_data.cell_count = 0;
                        for (int i = 0; i < 6; i++) {
                            if (bat.voltages[i] != 0xFFFF && bat.voltages[i] > 0) {
                                mavlink_data.cell_voltages[i] = bat.voltages[i];
                                mavlink_data.cell_count++;
                            } else {
                                mavlink_data.cell_voltages[i] = 0;
                            }
                        }
                        
                        mavlink_data.last_update = millis();
                        mavlink_data.data_valid = true;
                    }
                    else if (msg.msgid == MAVLINK_MSG_ID_HEARTBEAT) {
                        // Decode heartbeat and store actual system info
                        mavlink_heartbeat_t hb;
                        mavlink_msg_heartbeat_decode(&msg, &hb);
                        
                        mavlink_data.last_heartbeat = millis();
                        mavlink_data.heartbeat_received = true;
                        mavlink_data.heartbeat_count++;  // Count total heartbeats
                        mavlink_data.autopilot_type = hb.autopilot;  // MAV_AUTOPILOT enum
                        mavlink_data.system_type = hb.type;          // MAV_TYPE enum
                        mavlink_data.base_mode = hb.base_mode;
                        mavlink_data.system_status = hb.system_status;
                        
                        // Update reboot button visibility based on autopilot type
                        // We must be careful calling UI functions from Core 0, 
                        // but setting a flag or calling simple non-drawing logic is okay.
                        // Actually, lvgl calls should ideally be on Core 1 where lv_timer_handler runs.
                        // For safety, we will let loop() handle the UI visibility if needed, or rely on LVGL thread safety if configured.
                        
                        // Request autopilot version info once after first heartbeat
                        static bool version_requested = false;
                        if (!version_requested && !mavlink_data.version_received) {
                            mavlink_message_t req_msg;
                            uint8_t req_buf[MAVLINK_MAX_PACKET_LEN];
                            
                            mavlink_msg_command_long_pack(
                                255, 1, &req_msg,
                                1, 1,  // target_system=1, target_component=1
                                512,   // MAV_CMD_REQUEST_MESSAGE
                                0,     // confirmation
                                148.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f  // param1=148 (AUTOPILOT_VERSION)
                            );
                            
                            uint16_t req_len = mavlink_msg_to_send_buffer(req_buf, &req_msg);
                            Serial1.write(req_buf, req_len);
                            version_requested = true;
                        }
                    }
                    else if (msg.msgid == MAVLINK_MSG_ID_SYS_STATUS) {
                        mavlink_sys_status_t sys;
                        mavlink_msg_sys_status_decode(&msg, &sys);
                        mavlink_data.load = sys.load / 10;
                        mavlink_data.voltage_battery = sys.voltage_battery;
                        mavlink_data.current_battery = sys.current_battery;
                        mavlink_data.drop_rate_comm = sys.drop_rate_comm;
                        mavlink_data.errors_comm = sys.errors_comm;
                    }
                    else if (msg.msgid == MAVLINK_MSG_ID_SCALED_PRESSURE) {
                        mavlink_scaled_pressure_t press;
                        mavlink_msg_scaled_pressure_decode(&msg, &press);
                        mavlink_data.pressure_abs = press.press_abs;
                        mavlink_data.temperature_press = press.temperature;
                    }
                    else if (msg.msgid == MAVLINK_MSG_ID_RAW_IMU) {
                        mavlink_raw_imu_t imu;
                        mavlink_msg_raw_imu_decode(&msg, &imu);
                        mavlink_data.temperature_imu = imu.temperature;
                    }
                    else if (msg.msgid == MAVLINK_MSG_ID_SYSTEM_TIME) {
                        mavlink_system_time_t time;
                        mavlink_msg_system_time_decode(&msg, &time);
                        mavlink_data.time_unix_usec = time.time_unix_usec;
                    }
                    else if (msg.msgid == MAVLINK_MSG_ID_AUTOPILOT_VERSION) {
                        mavlink_autopilot_version_t ver;
                        mavlink_msg_autopilot_version_decode(&msg, &ver);
                        mavlink_data.flight_sw_version = ver.flight_sw_version;
                        mavlink_data.board_version = ver.board_version;
                        mavlink_data.vendor_id = ver.vendor_id;
                        mavlink_data.product_id = ver.product_id;
                        mavlink_data.version_received = true;
                    }
                    else if (msg.msgid == MAVLINK_MSG_ID_ATTITUDE) {
                        mavlink_attitude_t att;
                        mavlink_msg_attitude_decode(&msg, &att);
                        // Save attitude for Core 1 UI update instead of direct LVGL call
                        mavlink_data.roll = att.roll;
                        mavlink_data.pitch = att.pitch;
                        mavlink_data.yaw = att.yaw;
                    }
                    else if (msg.msgid == MAVLINK_MSG_ID_GLOBAL_POSITION_INT) {
                        mavlink_global_position_int_t pos;
                        mavlink_msg_global_position_int_decode(&msg, &pos);
                        
                        mavlink_data.altitude_relative = pos.relative_alt;
                        mavlink_data.heading = pos.hdg;
                        mavlink_data.gps_lat = pos.lat;
                        mavlink_data.gps_lon = pos.lon;
                        mavlink_data.gps_alt = pos.alt;
                        mavlink_data.gps_vx = pos.vx;
                        mavlink_data.gps_vy = pos.vy;
                        mavlink_data.gps_vz = pos.vz;
                    }
                    else if (msg.msgid == MAVLINK_MSG_ID_GPS_RAW_INT) {
                        mavlink_gps_raw_int_t gps;
                        mavlink_msg_gps_raw_int_decode(&msg, &gps);
                        mavlink_data.gps_fix_type = gps.fix_type;
                        mavlink_data.gps_satellites = gps.satellites_visible;
                    }
                    else if (msg.msgid == MAVLINK_MSG_ID_HOME_POSITION) {
                        mavlink_home_position_t home;
                        mavlink_msg_home_position_decode(&msg, &home);
                        mavlink_data.home_lat = home.latitude;
                        mavlink_data.home_lon = home.longitude;
                        mavlink_data.home_alt = home.altitude;
                        if (!mavlink_data.home_set) {
                            mavlink_data.home_set = true;
                            mavlink_data.pending_home_send = true; // Trigger MeshCore TX
                        }
                    }
                    else if (msg.msgid == MAVLINK_MSG_ID_EXTENDED_SYS_STATE) {
                        mavlink_extended_sys_state_t state;
                        mavlink_msg_extended_sys_state_decode(&msg, &state);
                        
                        uint8_t old_state = mavlink_data.vtol_state;
                        mavlink_data.vtol_state = state.vtol_state;
                        mavlink_data.is_transitioning = (state.vtol_state == 3);
                        
                        if ((state.vtol_state == 3 && old_state != 3) || 
                            (state.vtol_state == 2 && old_state == 3)) {
                            mavlink_data.pending_transition_send = true; // Trigger MeshCore TX
                        }
                    }
                    
                    xSemaphoreGive(mavlink_mutex);
                }
            }
        }
        
        if (millis() - last_mavlink_log >= 5000) {
            Serial.printf("[MAVLINK] Stats: %d bytes, %d msgs in last 5s\n", mavlink_bytes, mavlink_msgs_count);
            mavlink_bytes = 0;
            mavlink_msgs_count = 0;
            last_mavlink_log = millis();
        }
        
        // Small delay to yield to other Core 0 services (like WiFi/BT background processing if any)
        vTaskDelay(2 / portTICK_PERIOD_MS);
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
    delay(500);
    
    Serial.println("\n========== DUAL-CORE SETUP ==========");
    Serial.printf("[CORE0] Setup running on core %d\n", xPortGetCoreID());
    
    // Create mutex BEFORE starting tasks
    mavlink_mutex = xSemaphoreCreateMutex();
    if (mavlink_mutex == NULL) {
        Serial.println("[ERROR] Failed to create mutex!");
        while(1) { delay(1000); }
    }
    Serial.println("[CORE0] Mutex created successfully");
    
    // Initialize MAVLink data structure
    memset(&mavlink_data, 0, sizeof(mavlink_data));
    mavlink_data.last_update = 0;
    mavlink_data.data_valid = false;
    mavlink_data.last_heartbeat = 0;
    mavlink_data.heartbeat_received = false;
    mavlink_data.version_received = false;
    
    // Initialize Serial1 for MAVLink FIRST (before Wire/display, matching test sketch)
    Serial.printf("[CORE0] Initializing Serial1 on RX=%d, TX=%d at %d baud\n",
                  MAVLINK_RX_PIN, MAVLINK_TX_PIN, MAVLINK_BAUD);
    Serial1.begin(MAVLINK_BAUD, SERIAL_8N1, MAVLINK_RX_PIN, MAVLINK_TX_PIN);
    delay(100);
    Serial.println("[CORE0] Serial1 initialized");

    // Initialize Serial2 for MeshCore Telemetry
    Serial.printf("[CORE0] Initializing Serial2 on RX=%d, TX=%d\n",
                  MESHCORE_RX_PIN, MESHCORE_TX_PIN);
    Serial2.begin(MESHCORE_BAUD, SERIAL_8N1, MESHCORE_RX_PIN, MESHCORE_TX_PIN);
    meshcore.begin(&Serial2, MESHCORE_BAUD);
    Serial.println("[CORE0] MeshCore Telemetry initialized");

    // Initialize display
    Serial.println("[CORE0] Initializing display...");
    lv_xiao_disp_init();
    
    // Initialize Wire for touch
    Serial.println("[CORE0] Initializing Wire (I2C) for touch...");
    pinMode(TOUCH_INT, INPUT);  // TOUCH_INT GPIO44 - active LOW, touch controller has own pullup
    Wire.begin();               // Uses default SDA=GPIO5, SCL=GPIO6
    Wire.setTimeout(50);        // 50ms I2C timeout (prevents UI freeze on timeout)
    delay(100);
    Serial.println("[CORE0] Wire initialized");
    
    // MAVLink reading is now done in loop() - no separate task needed
    
    // Initialize RTC
    init_rtc();
    
    // Initialize pins for switch output
    pinMode(LED_BUILTIN, OUTPUT);
    pinMode(1, OUTPUT);
    pinMode(RELAY_PIN, OUTPUT);  // Relay output
    digitalWrite(LED_BUILTIN, LOW);
    digitalWrite(1, LOW);
    digitalWrite(RELAY_PIN, LOW);  // Relay off initially
    
    // Initialize trigger pin
    pinMode(TRIGGER_PIN, INPUT_PULLUP);
    
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
    
    // Set display background to black (prevents white flashes during screen swipes)
    lv_disp_set_bg_color(lv_disp_get_default(), lv_color_hex(0x000000));
    lv_disp_set_bg_opa(lv_disp_get_default(), 255);
    
    // Initialize interaction timer
    last_interaction_time = millis();
    
    // Connect switch event callback
    lv_obj_add_event_cb(ui_Switch1, switch_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    
    // Initialize Blink Controller
    blink_init(&blink_ctrl, millis() / 1000.0);
    
    // Connect ImgButton2 time reset callback
    lv_obj_add_event_cb(ui_ImgButton2, imgbutton2_press_cb, LV_EVENT_CLICKED, NULL);
    
    // Create Communication tasks on Core 0
    xTaskCreatePinnedToCore(
        mavlink_rx_task,           // Function
        "MavlinkRX",               // Name
        4096,                      // Stack size
        NULL,                      // Parameter
        2,                         // Priority (Higher than MeshCore)
        NULL,                      // Task handle
        0                          // Core ID (COMM Core)
    );
    
    xTaskCreatePinnedToCore(
        meshcore_telemetry_task,   // Function
        "MeshCoreTX",              // Name
        4096,                      // Stack size
        NULL,                      // Parameter
        1,                         // Priority
        NULL,                      // Task handle
        0                          // Core ID (COMM Core)
    );
    
    Serial.println("SLStudio Display Initialized");
}

void loop() {
    static unsigned long last_trigger_check = 0;
    static bool last_trigger_state = HIGH;
    
    // Check trigger pin every 50ms (debounce + reduce I2C interference)
    if (millis() - last_trigger_check >= 50) {
        last_trigger_check = millis();
        bool trigger_state = digitalRead(TRIGGER_PIN);
        
        // Detekce náběžné hrany (LOW → HIGH) - aktivní při připojení k GND
        if (trigger_state == LOW && last_trigger_state == HIGH) {
            // Toggle switch state
            if (lv_obj_has_state(ui_Switch1, LV_STATE_CHECKED)) {
                lv_obj_clear_state(ui_Switch1, LV_STATE_CHECKED);
            } else {
                lv_obj_add_state(ui_Switch1, LV_STATE_CHECKED);
            }
            // Vyvolat callback manuálně
            lv_event_send(ui_Switch1, LV_EVENT_VALUE_CHANGED, NULL);
            Serial.println("Trigger pin activated - switch toggled");
        }
        last_trigger_state = trigger_state;
    }
    
    // Odlehčeno pro App Core: čtení z uzamčených dat pro UI update. MAVLink a MeshCore běží asynchronně na Jádře 0.
    if (xSemaphoreTake(mavlink_mutex, (TickType_t)5) == pdTRUE) {
        // UI updates that used to be scattered in the parser
        if (mavlink_data.heartbeat_received) {
            update_reboot_button_visibility();
        }
        if (mavlink_data.data_valid) {
            update_attitude_display(
                mavlink_data.roll * (180.0f / (float)M_PI),
                mavlink_data.pitch * (180.0f / (float)M_PI),
                mavlink_data.yaw * (180.0f / (float)M_PI)
            );
            update_altitude_heading(mavlink_data.altitude_relative, mavlink_data.heading);
            update_screen3_gps(
                mavlink_data.gps_lat, mavlink_data.gps_lon, mavlink_data.gps_alt,
                mavlink_data.gps_satellites, mavlink_data.gps_fix_type
            );
        }
        xSemaphoreGive(mavlink_mutex);
    }
    
    // Update realistic eye blink animation
    if (ui_EyeClosedLayer && ui_ImgButton2) {
        blink_update(&blink_ctrl, millis() / 1000.0);
        
        lv_opa_t opa;
        if (lv_obj_has_state(ui_ImgButton2, LV_STATE_PRESSED)) {
            // Force eye closed when user is touching it
            opa = 255;
            // Optional: reset blink state so it doesn't blink immediately after release
            // blink_init(&blink_ctrl, millis() / 1000.0); 
        } else {
            // Use realistic blink animation
            opa = (lv_opa_t)(blink_get_alpha(&blink_ctrl) * 255.0);
        }
        
        lv_obj_set_style_img_opa(ui_EyeClosedLayer, opa, LV_PART_MAIN);
    }
    
    // Debug: Show all received message IDs
    static uint16_t msg_counts[256] = {0};
    static unsigned long last_msg_log = 0;
    if (millis() - last_msg_log >= 10000) {
        Serial.println("[MAVLINK] Message types received:");
        for (int i = 0; i < 256; i++) {
            if (msg_counts[i] > 0) {
                Serial.printf("  ID %3d: %d msgs\n", i, msg_counts[i]);
                msg_counts[i] = 0;
            }
        }
        last_msg_log = millis();
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

    // Check MAVLink connection timeout - spinner control
    unsigned long time_since_heartbeat = millis() - mavlink_data.last_heartbeat;
    static bool was_disconnected = false;
    
    if (mavlink_data.heartbeat_received) {
        if (ui_Spinner1) lv_obj_clear_flag(ui_Spinner1, LV_OBJ_FLAG_HIDDEN);
        
        if (time_since_heartbeat > 4000) {
            // Disconnected (timeout) -> turn into a full solid red circle
            if (!was_disconnected && ui_Spinner1) {
                // Color it red
                lv_obj_set_style_arc_color(ui_Spinner1, lv_color_hex(0xff0000), LV_PART_INDICATOR);
                lv_obj_set_style_arc_color(ui_Spinner1, lv_color_hex(0xff0000), LV_PART_MAIN);
                // Make it a full 360 degree circle (hides the spinning gap)
                lv_arc_set_bg_angles(ui_Spinner1, 0, 360);
                lv_arc_set_angles(ui_Spinner1, 0, 360);
                was_disconnected = true;
            }
        } else if (was_disconnected) {
            // Reconnected -> restore original blue spinning animation
            if (ui_Spinner1) {
                // Remove local arc color styles to fall back to the default theme colors
                lv_obj_remove_local_style_prop(ui_Spinner1, LV_STYLE_ARC_COLOR, LV_PART_INDICATOR);
                lv_obj_remove_local_style_prop(ui_Spinner1, LV_STYLE_ARC_COLOR, LV_PART_MAIN);
                
                // Restore original arc background angle (default is 360)
                lv_arc_set_bg_angles(ui_Spinner1, 0, 360);
                // Restore original indicator angle (SquareLine default for this spinner is 90)
                lv_arc_set_angles(ui_Spinner1, 0, 90);
            }
            was_disconnected = false;
        }
    } else {
        // No heartbeat at all - hide spinner and reset data
        mavlink_data.data_valid = false;
        mavlink_data.voltage = 0.0;
        mavlink_data.current = 0.0;
        mavlink_data.percent = 0;
        mavlink_data.temperature = 0;
        mavlink_data.load = 0;
        mavlink_data.errors_comm = 1;
        mavlink_data.temperature_imu = 0;
        mavlink_data.temperature_press = 0;
        mavlink_data.pressure_abs = 0;
        if (ui_Spinner1) lv_obj_add_flag(ui_Spinner1, LV_OBJ_FLAG_HIDDEN);
        was_disconnected = false;
    }

    // Update RTC time display every 10 seconds
    static unsigned long last_time_update = 0;
    unsigned long current_millis = millis();
    
    if (current_millis - last_time_update >= 10000) {
        update_time_display();
        update_date_display();
        last_time_update = current_millis;
    }

    // Update MAVLink monitor
    update_mavlink_monitor();

    // Update Screen2 battery display
    update_screen2_battery();

    // Update Screen5 servo/motor timers
    ui_Screen5_update_timers();

    // --- AUTO-NAVIGATION (Inactivity Timeouts) ---
    unsigned long inactivity_duration = millis() - last_interaction_time;
    lv_obj_t * active_screen = lv_scr_act();

    if (active_screen == ui_Screen5) { // Servo Control Screen
        if (inactivity_duration >= UI_SCREEN5_INACTIVITY_MS) {
            Serial.println("[TIMER] Screen 5 inactivity -> Screen 2");
            _ui_screen_change(&ui_Screen2, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0, &ui_Screen2_screen_init);
            last_interaction_time = millis(); // Reset to prevent rapid double-switch
        }
    } 
    else if (active_screen == ui_Screen2) { // Battery Screen
        if (inactivity_duration >= UI_SCREEN2_INACTIVITY_MS) {
            Serial.println("[TIMER] Screen 2 inactivity -> Screen 1");
            _ui_screen_change(&ui_Screen1, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0, &ui_Screen1_screen_init);
            last_interaction_time = millis();
        }
    }

    // LVGL timer handler - Core 0 handles all UI
    lv_timer_handler();
    delay(5);
    
    // Read ESP32 internal temperature every 5 seconds
    static unsigned long last_temp_read = 0;
    if (millis() - last_temp_read >= 5000) {
        float temp = temperatureRead();  // ESP32 internal temperature
        if (temp > -40 && temp < 125) {  // Valid range check
            if (xSemaphoreTake(mavlink_mutex, (TickType_t)1) == pdTRUE) {
                mavlink_data.esp32_temperature = temp;
                xSemaphoreGive(mavlink_mutex);
            }
        }
        last_temp_read = millis();
    }
    
    // Debug: Log core ID occasionally
    static unsigned long last_core_log = 0;
    if (millis() - last_core_log >= 30000) {
        Serial.printf("[LOOP] Running on core %d\n", xPortGetCoreID());
        if (mavlink_data.data_valid) {
            Serial.printf("[MAVLINK] Data: %.2fV, %.2fA, %d%%\n",
                          mavlink_data.voltage, mavlink_data.current, mavlink_data.percent);
        }
        last_core_log = millis();
    }
}

// RTOS Task for MeshCore Telemetry (Runs on Core 1)
void meshcore_telemetry_task(void *pvParameters) {
    Serial.println("[CORE1] MeshCore Telemetry Task started");
    
    while(1) {
        // Handle incoming commands from MeshCore (!SET_INT, etc.)
        meshcore.handleSerialInput();
        
        // Handle outgoing NMEA telemetry
        if (xSemaphoreTake(mavlink_mutex, (TickType_t)10) == pdTRUE) {
            // Check for event-based pending transmissions first
            if (mavlink_data.pending_home_send) {
                meshcore.sendHome(mavlink_data);
                mavlink_data.pending_home_send = false;
            }
            if (mavlink_data.pending_transition_send) {
                meshcore.sendTransition(mavlink_data);
                mavlink_data.pending_transition_send = false;
            }
            
            // Handle periodic updates
            meshcore.update(mavlink_data);
            
            xSemaphoreGive(mavlink_mutex);
        }
        
        // Small delay to prevent CPU hogging (100Hz loop)
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}
