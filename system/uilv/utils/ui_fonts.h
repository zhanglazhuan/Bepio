#ifndef BEPIO_UI_FONTS_H_
#define BEPIO_UI_FONTS_H_
#include "lvgl.h"

/* Bepio has no DeepStoa icon font assets; use LVGL's linked symbol glyphs. */
#define MY_SYMBOL_GRID LV_SYMBOL_LIST
#define MY_SYMBOL_CLOCK LV_SYMBOL_HOME
#define MY_SYMBOL_LANDSCAPE LV_SYMBOL_IMAGE
#define MY_SYMBOL_INFO LV_SYMBOL_WARNING
#define MY_SYMBOL_DOING LV_SYMBOL_REFRESH
#define MY_SYMBOL_EDIT LV_SYMBOL_EDIT
#define MY_SYMBOL_WIFI_1_BAR LV_SYMBOL_WIFI
#define MY_SYMBOL_WIFI_2_BAR LV_SYMBOL_WIFI
#define MY_SYMBOL_WIFI_3_BAR LV_SYMBOL_WIFI
#define MY_SYMBOL_WIFI_OFF LV_SYMBOL_CLOSE
#define MY_SYMBOL_BATTERY_FULL LV_SYMBOL_BATTERY_FULL

extern lv_font_t custom_font_normal;
void ui_fonts_init(void);
#endif
