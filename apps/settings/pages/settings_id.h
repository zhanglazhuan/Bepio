#ifndef BEPIO_SETTINGS_ID_H_
#define BEPIO_SETTINGS_ID_H_
#include "../view.h"
#include "lvgl.h"
void bepio_settings_id_refresh(settings_page_context_t *ctx);
lv_obj_t *bepio_settings_id_build(void *app, void *payload);
#endif
