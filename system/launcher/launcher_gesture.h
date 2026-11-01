#ifndef BEPIO_LAUNCHER_GESTURE_H_
#define BEPIO_LAUNCHER_GESTURE_H_
#include "lvgl.h"

/* Install once on the LVGL owner task after the pointer indev is created. */
void launcher_gesture_attach_input(lv_indev_t *input);
#endif
