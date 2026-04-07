#ifndef UI_SCREEN5_H
#define UI_SCREEN5_H

#ifdef __cplusplus
extern "C" {
#endif

#include <lvgl.h>

// Screen5 - Servo & Motor Test
extern lv_obj_t *ui_Screen5;

// Functions
void ui_Screen5_screen_init(void);
void ui_Screen5_screen_destroy(void);
void ui_Screen5_update_timers(void);  // Call from main loop for timeouts

#ifdef __cplusplus
}
#endif

#endif // UI_SCREEN5_H
