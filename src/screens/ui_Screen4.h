#ifndef _UI_SCREEN4_H
#define _UI_SCREEN4_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

extern lv_obj_t *ui_Screen4;

void ui_Screen4_screen_init(void);
void ui_Screen4_screen_destroy(void);
void update_attitude_display(float roll_deg, float pitch_deg, float yaw_deg);
void update_altitude_heading(int32_t altitude_mm, int32_t heading_cdeg);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif
