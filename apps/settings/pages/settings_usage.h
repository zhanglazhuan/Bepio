#ifndef BEPIO_SETTINGS_USAGE_H_
#define BEPIO_SETTINGS_USAGE_H_
#include "../view.h"
#include "lvgl.h"
void bepio_settings_usage_refresh(settings_page_context_t *ctx);
lv_obj_t *bepio_settings_usage_build(void *app, void *payload);
#endif
