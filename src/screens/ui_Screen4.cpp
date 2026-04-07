// Screen4 - IMU Artificial Horizon
// Using LVGL Canvas with primitive drawing (proper LVGL approach from forum)

#include <stdio.h>
#include <Arduino.h>
#include "ui_Screen4.h"
#include "ui_Screen1.h"
#include "ui_Screen3.h"
#include "../ui.h"
#include <math.h>

#include "ui_Screen5.h"

// Forward declarations for navigation
extern lv_obj_t *ui_Screen5;
extern void ui_Screen5_screen_init(void);
extern lv_obj_t *ui_Screen1;
extern void ui_Screen1_screen_init(void);
extern lv_obj_t *ui_Screen3;
extern void ui_Screen3_screen_init(void);

lv_obj_t *ui_Screen4 = NULL;
static lv_point_t ui_Screen4_press_point;
static bool ui_Screen4_press_active = false;
static uint32_t ui_Screen4_load_time = 0;

// Canvas for horizon drawing
static lv_obj_t *s4_canvas = NULL;
static lv_obj_t *s4_roll_label = NULL;
static lv_obj_t *s4_pitch_label = NULL;
static lv_obj_t *s4_yaw_label = NULL;
static lv_obj_t *s4_alt_label = NULL;    // Altitude label
static lv_obj_t *s4_hdg_label = NULL;    // Heading label

// Aircraft symbol (fixed, not on canvas)
static lv_obj_t *s4_aircraft_l = NULL;
static lv_obj_t *s4_aircraft_r = NULL;
static lv_obj_t *s4_aircraft_dot = NULL;

// Canvas dimensions
#define CANVAS_WIDTH 240
#define CANVAS_HEIGHT 240
#define CANVAS_CENTER_X (CANVAS_WIDTH / 2)
#define CANVAS_CENTER_Y (CANVAS_HEIGHT / 2)

// Static draw buffer for canvas (aligned for DMA)
static uint8_t s4_canvas_buf[CANVAS_WIDTH * CANVAS_HEIGHT * 2] __attribute__((aligned(32)));
static lv_color_t *s4_canvas_buf_ptr = (lv_color_t *)s4_canvas_buf;

// Current attitude values
static float s4_roll = 0.0f;
static float s4_pitch = 0.0f;
static float s4_yaw = 0.0f;  // Yaw for heading tape
static uint32_t s4_last_draw_time = 0;
static float s4_last_drawn_roll = 0.0f;
static float s4_last_drawn_pitch = 0.0f;

static void ui_enable_event_bubble_recursive(lv_obj_t *parent) {
    uint32_t child_count = lv_obj_get_child_cnt(parent);
    for (uint32_t i = 0; i < child_count; i++) {
        lv_obj_t *child = lv_obj_get_child(parent, i);
        lv_obj_add_flag(child, LV_OBJ_FLAG_EVENT_BUBBLE);
        ui_enable_event_bubble_recursive(child);
    }
}

static void ui_event_Screen4(lv_event_t *e) {
    lv_event_code_t event_code = lv_event_get_code(e);
    lv_indev_t *indev = lv_indev_get_act();
    lv_point_t point;
    if (!indev) return;
    if ((lv_tick_get() - ui_Screen4_load_time) < 400) return;
    if (event_code == LV_EVENT_PRESSED) {
        lv_indev_get_point(indev, &ui_Screen4_press_point);
        ui_Screen4_press_active = true;
    }
    if (event_code == LV_EVENT_PRESS_LOST) {
        ui_Screen4_press_active = false;
    }
    if (event_code == LV_EVENT_RELEASED && ui_Screen4_press_active) {
        int dx, dy;
        ui_Screen4_press_active = false;
        lv_indev_get_point(indev, &point);
        dx = point.x - ui_Screen4_press_point.x;
        dy = point.y - ui_Screen4_press_point.y;
        if (LV_ABS(dy) > 40 && LV_ABS(dy) > LV_ABS(dx)) {
            if (dy > 0) {
                _ui_screen_change(&ui_Screen5, LV_SCR_LOAD_ANIM_MOVE_BOTTOM, 300, 0, &ui_Screen5_screen_init);
            } else {
                _ui_screen_change(&ui_Screen3, LV_SCR_LOAD_ANIM_MOVE_TOP, 300, 0, &ui_Screen3_screen_init);
            }
        }
    }
}

///////////////////// HORIZON DRAW ////////////////////

// Helper: set pixel if within bounds
static void set_pixel(lv_color_t *buf, int x, int y, lv_color_t color) {
    if (x >= 0 && x < CANVAS_WIDTH && y >= 0 && y < CANVAS_HEIGHT) {
        buf[y * CANVAS_WIDTH + x] = color;
    }
}

// Helper: draw line
static void draw_line_buf(lv_color_t *buf, int x0, int y0, int x1, int y1, lv_color_t color) {
    int dx = abs(x1 - x0), sx = (x0 < x1) ? 1 : -1;
    int dy = -abs(y1 - y0), sy = (y0 < y1) ? 1 : -1;
    int err = dx + dy;
    while (1) {
        set_pixel(buf, x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

// Draw roll scale (arc with tick marks at top, inspired by A320 PFD)
static void draw_roll_scale(lv_color_t *buf, int cx, int cy, float roll_deg, lv_color_t color) {
    int radius = 70;  // Radius of roll scale arc
    int tick_len = 8; // Length of tick marks
    
    // Major roll angles (in degrees) - A320 style
    const int roll_angles[] = {-60, -45, -30, -20, -10, 0, 10, 20, 30, 45, 60};
    const int num_angles = sizeof(roll_angles) / sizeof(roll_angles[0]);
    
    // Draw tick marks for each angle
    for (int i = 0; i < num_angles; i++) {
        int angle = roll_angles[i];
        float angle_rad = (angle + roll_deg) * 0.0174533f; // Apply current roll
        
        // Calculate tick position on arc
        int tick_x = cx + (int)(radius * sinf(angle_rad));
        int tick_y = cy - (int)(radius * cosf(angle_rad));
        
        // Calculate tick end (pointing toward center)
        int tick_end_x = cx + (int)((radius - tick_len) * sinf(angle_rad));
        int tick_end_y = cy - (int)((radius - tick_len) * cosf(angle_rad));
        
        // Draw thicker tick for major angles (0, 30, 45, 60)
        int thickness = (abs(angle) == 0 || abs(angle) >= 30) ? 2 : 1;
        
        for (int t = 0; t < thickness; t++) {
            draw_line_buf(buf, tick_x + t, tick_y, tick_end_x + t, tick_end_y, color);
        }
    }
    
    // Draw small triangle pointer at top center (bank indicator)
    int pointer_y = cy - radius - 5;
    for (int i = -3; i <= 3; i++) {
        set_pixel(buf, cx + i, pointer_y + abs(i), color);
        set_pixel(buf, cx + i, pointer_y + abs(i) + 1, color);
    }
}

// Draw pitch scale (lines perpendicular to horizon, A320 style)
static void draw_pitch_scale(lv_color_t *buf, int x1, int y1, int x2, int y2, 
                              float pitch_deg, float roll_rad, lv_color_t color) {
    // Pitch scale intervals (degrees)
    const int pitch_intervals[] = {-20, -15, -10, -5, 5, 10, 15, 20};
    const int num_intervals = sizeof(pitch_intervals) / sizeof(pitch_intervals[0]);
    
    float cos_r = cosf(roll_rad);
    float sin_r = sinf(roll_rad);
    
    // Center point of horizon line
    int cx = (x1 + x2) / 2;
    int cy = (y1 + y2) / 2;
    
    for (int i = 0; i < num_intervals; i++) {
        int pitch_mark = pitch_intervals[i];
        float pitch_offset = (pitch_mark - pitch_deg) * 3.0f; // 3 pixels per degree
        
        // Calculate position along the pitch axis (perpendicular to horizon)
        int mark_cx = cx + (int)(pitch_offset * sin_r);
        int mark_cy = cy - (int)(pitch_offset * cos_r);
        
        // Line length based on importance (longer lines for 10° increments)
        int line_len = (abs(pitch_mark) % 10 == 0) ? 25 : 15;
        
        // Draw horizontal pitch line (parallel to horizon)
        int mx1 = mark_cx - (int)(line_len * cos_r);
        int my1 = mark_cy - (int)(line_len * sin_r);
        int mx2 = mark_cx + (int)(line_len * cos_r);
        int my2 = mark_cy + (int)(line_len * sin_r);
        
        draw_line_buf(buf, mx1, my1, mx2, my2, color);
    }
}

// Draw heading tape (A320 style) - horizontal compass strip at top of canvas
static void draw_heading_tape(lv_color_t *buf, int center_x, int y_pos, float heading_deg, lv_color_t color) {
    int tape_height = 20;
    int tape_width = 140;  // Visible width of tape
    int half_width = tape_width / 2;
    int pixels_per_degree = 3;  // Scale: 3 pixels per degree
    
    // Draw tape background (dark gray/black strip)
    for (int y = y_pos; y < y_pos + tape_height && y < CANVAS_HEIGHT; y++) {
        for (int x = center_x - half_width - 5; x <= center_x + half_width + 5 && x < CANVAS_WIDTH; x++) {
            if (x >= 0) buf[y * CANVAS_WIDTH + x] = lv_color_hex(0x1a1a1a);
        }
    }
    
    // Draw major heading marks (every 10 degrees)
    for (int angle = -60; angle <= 420; angle += 10) {
        // Calculate screen position based on heading
        int offset = (int)((angle - heading_deg) * pixels_per_degree);
        int x = center_x + offset;
        
        if (x < center_x - half_width || x > center_x + half_width) continue;
        
        int tick_height = (angle % 30 == 0) ? 12 : 8;  // Taller ticks for 30° increments
        
        // Draw tick mark
        for (int y = y_pos + tape_height - tick_height; y < y_pos + tape_height; y++) {
            if (y >= 0 && y < CANVAS_HEIGHT && x >= 0 && x < CANVAS_WIDTH) {
                buf[y * CANVAS_WIDTH + x] = color;
            }
        }
    }
    
    // Draw cardinal directions (N, E, S, W) and intermediate
    const struct { int angle; const char* label; } directions[] = {
        {0, "N"}, {45, "NE"}, {90, "E"}, {135, "SE"},
        {180, "S"}, {225, "SW"}, {270, "W"}, {315, "NW"}
    };
    
    for (int i = 0; i < 8; i++) {
        int angle = directions[i].angle;
        int offset = (int)((angle - heading_deg) * pixels_per_degree);
        int x = center_x + offset;
        
        if (x < center_x - half_width - 10 || x > center_x + half_width + 10) continue;
        
        // Draw direction indicator (small triangle)
        for (int dy = 0; dy < 4; dy++) {
            for (int dx = -dy; dx <= dy; dx++) {
                int px = x + dx;
                int py = y_pos + 2 + dy;
                if (py >= 0 && py < CANVAS_HEIGHT && px >= 0 && px < CANVAS_WIDTH) {
                    buf[py * CANVAS_WIDTH + px] = lv_color_hex(0xFFFF00);  // Yellow for cardinal
                }
            }
        }
    }
    
    // Draw center heading indicator (triangle pointing down, A320 style)
    int cx = center_x;
    int top_y = y_pos - 3;
    for (int dy = 0; dy < 6; dy++) {
        for (int dx = -dy; dx <= dy; dx++) {
            int px = cx + dx;
            int py = top_y + dy;
            if (py >= 0 && py < CANVAS_HEIGHT && px >= 0 && px < CANVAS_WIDTH) {
                buf[py * CANVAS_WIDTH + px] = lv_color_hex(0x00FF00);  // Green center marker
            }
        }
    }
    
    // Draw current heading value above center
    // (This would need font rendering - simplified to just highlight current heading area)
    for (int x = cx - 15; x <= cx + 15 && x >= 0 && x < CANVAS_WIDTH; x++) {
        for (int y = y_pos; y < y_pos + 2 && y < CANVAS_HEIGHT; y++) {
            buf[y * CANVAS_WIDTH + x] = lv_color_hex(0x00FF00);  // Green bar at top
        }
    }
}

// Draw horizon on canvas using simple pixel drawing
static void draw_horizon_on_canvas(void) {
    if (!s4_canvas) return;
    
    // Get canvas buffer
    lv_color_t *buf = s4_canvas_buf_ptr;
    if (!buf) return;
    
    // Calculate horizon line based on pitch
    int pitch_offset = (int)(s4_pitch * 2.0f);
    int horizon_y = CANVAS_CENTER_Y + pitch_offset;
    
    // Clamp horizon to canvas bounds
    if (horizon_y < 10) horizon_y = 10;
    if (horizon_y > CANVAS_HEIGHT - 10) horizon_y = CANVAS_HEIGHT - 10;
    
    // Calculate roll rotation - negate to get correct direction
    // When roll left (negative), horizon goes right
    float roll_rad = (-s4_roll) * 0.0174533f;
    float cos_r = cosf(roll_rad);
    float sin_r = sinf(roll_rad);
    int line_half_width = CANVAS_WIDTH / 2 - 10;
    
    // Horizon line endpoints
    int x1 = CANVAS_CENTER_X - (int)(line_half_width * cos_r);
    int y1 = horizon_y - (int)(line_half_width * sin_r);
    int x2 = CANVAS_CENTER_X + (int)(line_half_width * cos_r);
    int y2 = horizon_y + (int)(line_half_width * sin_r);
    
    // Colors - swapped (sky is brown, ground is blue)
    lv_color_t sky_color = lv_color_hex(0x8B4513);   // brown (was blue)
    lv_color_t ground_color = lv_color_hex(0x0080FF); // blue (was brown)
    lv_color_t black = lv_color_hex(0x000000);
    lv_color_t white = lv_color_hex(0xFFFFFF);
    
    // Clear canvas (fill with black)
    for (int y = 0; y < CANVAS_HEIGHT; y++) {
        for (int x = 0; x < CANVAS_WIDTH; x++) {
            buf[y * CANVAS_WIDTH + x] = black;
        }
    }
    
    // Helper: check if point is above line (for triangle filling)
    #define IS_ABOVE_LINE(px, py) (((x2 - x1) * ((py) - y1) - (y2 - y1) * ((px) - x1)) > 0)
    
    // Fill canvas with sky/ground colors
    for (int y = 0; y < CANVAS_HEIGHT; y++) {
        for (int x = 0; x < CANVAS_WIDTH; x++) {
            bool above = IS_ABOVE_LINE(x, y);
            if (above) {
                buf[y * CANVAS_WIDTH + x] = sky_color;
            } else {
                buf[y * CANVAS_WIDTH + x] = ground_color;
            }
        }
    }
    
    // Draw horizon line (white pixels along the line)
    // Bresenham-like line drawing
    int dx = abs(x2 - x1), dy = abs(y2 - y1);
    int sx = (x1 < x2) ? 1 : -1;
    int sy = (y1 < y2) ? 1 : -1;
    int err = dx - dy;
    
    int cx = x1, cy = y1;
    while (1) {
        // Draw thick line (3 pixels)
        for (int oy = -1; oy <= 1; oy++) {
            int py = cy + oy;
            if (py >= 0 && py < CANVAS_HEIGHT && cx >= 0 && cx < CANVAS_WIDTH) {
                buf[py * CANVAS_WIDTH + cx] = white;
            }
        }
        
        if (cx == x2 && cy == y2) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; cx += sx; }
        if (e2 < dx) { err += dx; cy += sy; }
    }
    
    // Draw roll scale (A320 style) - arc with tick marks
    draw_roll_scale(buf, CANVAS_CENTER_X, CANVAS_CENTER_Y, s4_roll, white);
    
    // Draw pitch scale (A320 style) - lines parallel to horizon
    draw_pitch_scale(buf, x1, y1, x2, y2, s4_pitch, roll_rad, white);
    
    // Draw heading tape (A320 style) - horizontal compass at bottom
    draw_heading_tape(buf, CANVAS_CENTER_X, 200, s4_yaw, white);
    
    // Invalidate canvas to trigger redraw
    lv_obj_invalidate(s4_canvas);
}

///////////////////// SCREEN INIT ////////////////////

void ui_Screen4_screen_init(void) {
    ui_Screen4 = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(ui_Screen4, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Screen4, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_flag(ui_Screen4, LV_OBJ_FLAG_SCROLLABLE);

    // --- Title ---
    lv_obj_t *title = lv_label_create(ui_Screen4);
    lv_label_set_text(title, "Attitude / IMU");
    lv_obj_set_style_text_color(title, lv_color_hex(0x00ff00), LV_PART_MAIN);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 18);

    // --- Canvas for horizon ---
    s4_canvas = lv_canvas_create(ui_Screen4);
    // Use RGB565 format for speed (16-bit, no transparency needed)
    lv_canvas_set_buffer(s4_canvas, s4_canvas_buf, CANVAS_WIDTH, CANVAS_HEIGHT, LV_IMG_CF_TRUE_COLOR);
    lv_obj_center(s4_canvas);
    lv_canvas_fill_bg(s4_canvas, lv_color_hex(0x000000), LV_OPA_COVER);

    // --- Aircraft symbol (fixed in center, yellow) ---
    s4_aircraft_l = lv_obj_create(ui_Screen4);
    lv_obj_set_size(s4_aircraft_l, 25, 4);
    lv_obj_align(s4_aircraft_l, LV_ALIGN_CENTER, -15, 0);
    lv_obj_set_style_bg_color(s4_aircraft_l, lv_color_hex(0xFFFF00), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(s4_aircraft_l, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(s4_aircraft_l, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(s4_aircraft_l, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_flag(s4_aircraft_l, LV_OBJ_FLAG_SCROLLABLE);

    s4_aircraft_r = lv_obj_create(ui_Screen4);
    lv_obj_set_size(s4_aircraft_r, 25, 4);
    lv_obj_align(s4_aircraft_r, LV_ALIGN_CENTER, 15, 0);
    lv_obj_set_style_bg_color(s4_aircraft_r, lv_color_hex(0xFFFF00), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(s4_aircraft_r, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(s4_aircraft_r, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(s4_aircraft_r, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_flag(s4_aircraft_r, LV_OBJ_FLAG_SCROLLABLE);

    s4_aircraft_dot = lv_obj_create(ui_Screen4);
    lv_obj_set_size(s4_aircraft_dot, 8, 8);
    lv_obj_align(s4_aircraft_dot, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(s4_aircraft_dot, lv_color_hex(0xFFFF00), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(s4_aircraft_dot, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(s4_aircraft_dot, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(s4_aircraft_dot, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_flag(s4_aircraft_dot, LV_OBJ_FLAG_SCROLLABLE);

    // --- Labels at bottom (moved up and larger font) ---
    s4_roll_label = lv_label_create(ui_Screen4);
    lv_label_set_text(s4_roll_label, "R:0.0");
    lv_obj_set_style_text_color(s4_roll_label, lv_color_hex(0xffffff), LV_PART_MAIN);
    lv_obj_set_style_text_font(s4_roll_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(s4_roll_label, LV_ALIGN_BOTTOM_LEFT, 30, -50);

    s4_pitch_label = lv_label_create(ui_Screen4);
    lv_label_set_text(s4_pitch_label, "P:0.0");
    lv_obj_set_style_text_color(s4_pitch_label, lv_color_hex(0xffffff), LV_PART_MAIN);
    lv_obj_set_style_text_font(s4_pitch_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(s4_pitch_label, LV_ALIGN_BOTTOM_MID, 0, -50);

    s4_yaw_label = lv_label_create(ui_Screen4);
    lv_label_set_text(s4_yaw_label, "Y:0.0");
    lv_obj_set_style_text_color(s4_yaw_label, lv_color_hex(0xffffff), LV_PART_MAIN);
    lv_obj_set_style_text_font(s4_yaw_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(s4_yaw_label, LV_ALIGN_BOTTOM_RIGHT, -30, -50);

    // --- Altitude and Heading labels at top (black, larger font, lower position) ---
    s4_alt_label = lv_label_create(ui_Screen4);
    lv_label_set_text(s4_alt_label, "A:0m");
    lv_obj_set_style_text_color(s4_alt_label, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_text_font(s4_alt_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(s4_alt_label, LV_ALIGN_TOP_LEFT, 30, 55);

    s4_hdg_label = lv_label_create(ui_Screen4);
    lv_label_set_text(s4_hdg_label, "H:0");
    lv_obj_set_style_text_color(s4_hdg_label, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_text_font(s4_hdg_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(s4_hdg_label, LV_ALIGN_TOP_RIGHT, -30, 55);

    lv_obj_add_event_cb(ui_Screen4, ui_event_Screen4, LV_EVENT_ALL, NULL);
    ui_enable_event_bubble_recursive(ui_Screen4);
    ui_Screen4_load_time = lv_tick_get();
}

///////////////////// UPDATE ////////////////////

void update_attitude_display(float roll_deg, float pitch_deg, float yaw_deg) {
    // Only draw if Screen4 is currently active
    if (lv_scr_act() != ui_Screen4) return;
    
    // Rate limit: max 20fps (50ms between draws)
    uint32_t now = lv_tick_get();
    if (now - s4_last_draw_time < 50) return;
    
    // Only draw if values changed significantly (>0.5 degrees)
    if (fabs(roll_deg - s4_last_drawn_roll) < 0.5f && fabs(pitch_deg - s4_last_drawn_pitch) < 0.5f) {
        // Still update labels even if we don't redraw horizon
        if (s4_roll_label && s4_pitch_label && s4_yaw_label) {
            char buf[24];
            sprintf(buf, "R:%.1f", roll_deg);
            lv_label_set_text(s4_roll_label, buf);
            sprintf(buf, "P:%.1f", pitch_deg);
            lv_label_set_text(s4_pitch_label, buf);
            sprintf(buf, "Y:%.1f", yaw_deg);
            lv_label_set_text(s4_yaw_label, buf);
        }
        return;
    }
    
    s4_last_draw_time = now;
    s4_last_drawn_roll = roll_deg;
    s4_last_drawn_pitch = pitch_deg;
    
    // Store values for canvas drawing
    s4_roll = roll_deg;
    s4_pitch = pitch_deg;
    s4_yaw = yaw_deg;  // Store yaw for heading tape
    s4_last_drawn_roll = roll_deg;
    s4_last_drawn_pitch = pitch_deg;
    
    // Redraw horizon on canvas
    draw_horizon_on_canvas();

    if (!s4_roll_label || !s4_pitch_label || !s4_yaw_label) return;

    char buf[24];
    sprintf(buf, "R:%.1f", roll_deg);
    lv_label_set_text(s4_roll_label, buf);

    sprintf(buf, "P:%.1f", pitch_deg);
    lv_label_set_text(s4_pitch_label, buf);

    sprintf(buf, "Y:%.1f", yaw_deg);
    lv_label_set_text(s4_yaw_label, buf);
}

// Update altitude and heading display
void update_altitude_heading(int32_t altitude_mm, int32_t heading_cdeg) {
    // Only update if Screen4 is currently active
    if (lv_scr_act() != ui_Screen4) return;
    
    if (!s4_alt_label || !s4_hdg_label) return;
    
    // Convert altitude from mm to meters
    int altitude_m = altitude_mm / 1000;
    
    // Convert heading from centidegrees to degrees
    int heading_deg = heading_cdeg / 100;
    if (heading_deg < 0) heading_deg = 0;
    if (heading_deg > 360) heading_deg = 360;
    
    char buf[24];
    sprintf(buf, "A:%dm", altitude_m);
    lv_label_set_text(s4_alt_label, buf);
    
    sprintf(buf, "H:%d", heading_deg);
    lv_label_set_text(s4_hdg_label, buf);
}

///////////////////// DESTROY ////////////////////

void ui_Screen4_screen_destroy(void) {
    s4_canvas = NULL;
    s4_roll_label    = NULL;
    s4_pitch_label   = NULL;
    s4_yaw_label     = NULL;
    s4_alt_label     = NULL;
    s4_hdg_label     = NULL;
    s4_aircraft_l = NULL;
    s4_aircraft_r = NULL;
    s4_aircraft_dot = NULL;
    if (ui_Screen4) {
        lv_obj_del(ui_Screen4);
        ui_Screen4 = NULL;
    }
}
