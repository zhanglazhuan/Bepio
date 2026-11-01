#ifndef BEPIO_SETTINGS_WIFI_CONNECT_H_
#define BEPIO_SETTINGS_WIFI_CONNECT_H_
#include "../view.h"
#include "lvgl.h"
const bepio_wifi_ap_t *bepio_settings_wifi_connect_ap(const settings_page_context_t *ctx);
void bepio_settings_wifi_connect_refresh(settings_page_context_t *ctx);
lv_obj_t *bepio_settings_wifi_connect_build(void *app, void *payload);
#endif
