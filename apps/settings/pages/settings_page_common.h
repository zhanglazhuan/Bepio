#ifndef BEPIO_SETTINGS_PAGE_COMMON_H_
#define BEPIO_SETTINGS_PAGE_COMMON_H_
#include "settings_pages_internal.h"
lv_obj_t *bepio_settings_label(lv_obj_t *parent, const char *value, int x, int y, int width);
lv_obj_t *bepio_settings_button(lv_obj_t *parent, const char *value, int x, int y, int width, int height, lv_event_cb_t cb, void *data);
#endif
