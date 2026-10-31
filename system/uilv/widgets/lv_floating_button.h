/* Adapted from DeepStoa widgets/lv_floating_button. SPDX-License-Identifier: GPL-3.0-only */
#ifndef BEPIO_LV_FLOATING_BUTTON_H_
#define BEPIO_LV_FLOATING_BUTTON_H_
#include "lvgl.h"
#define BEPIO_FLOATING_BUTTON_SIZE 48
#define BEPIO_FLOATING_BUTTON_MARGIN 12
#define BEPIO_FLOATING_BUTTON_TOUCH_PADDING 8
/* LVGL owner thread only. Fixed bottom-right action button, excluded from parent
 * layout and scrolling. Parent owns its lifetime and should provide room for
 * SIZE + 2*MARGIN. Callback receives LV_EVENT_CLICKED and the supplied user_data.
 * NULL symbol uses +. Image descriptors must outlive the button. NULL parent or
 * image returns NULL. Use normal LVGL flags/states to hide/disable the button. */
lv_obj_t *bepio_floating_button_create(lv_obj_t *parent, const char *symbol,
	lv_event_cb_t callback, void *user_data);
lv_obj_t *bepio_floating_button_create_image(lv_obj_t *parent, const lv_image_dsc_t *image,
	lv_event_cb_t callback, void *user_data);
#endif
