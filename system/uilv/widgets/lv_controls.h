#ifndef BEPIO_LV_CONTROLS_H_
#define BEPIO_LV_CONTROLS_H_

#include "lvgl.h"

/* Create styled controls on the LVGL owner thread. The parent owns their lifetime. */
lv_obj_t *bepio_ui_label(lv_obj_t *parent, const char *value, int x, int y, int width);
lv_obj_t *bepio_ui_button(lv_obj_t *parent, const char *value, int x, int y,
	int width, int height, lv_event_cb_t cb, void *data);

#endif
