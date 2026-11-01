#ifndef LV_UI_STYLE_GUIDE_H
#define LV_UI_STYLE_GUIDE_H
#include "lvgl.h"

#define UI_SWITCH_W 44
#define UI_SWITCH_H 24
#define UI_BTN_H_LARGE 44
#define UI_BTN_H_MEDIUM 36
#define UI_BTN_H_SMALL 30
#define UI_BOTTOM_ACTION_H 44
#define UI_BOTTOM_ACTION_GAP 12
#define UI_SHEET_BTN_W 128
#define UI_SHEET_BTN_H 40

void ui_style_set_btn_primary(lv_obj_t *btn);
void ui_style_set_btn_secondary(lv_obj_t *btn);
void ui_style_set_dropdown(lv_obj_t *dd);
void ui_style_set_switch(lv_obj_t *sw);
void ui_style_set_btn_size_large(lv_obj_t *btn);
void ui_style_set_btn_size_medium(lv_obj_t *btn);
void ui_style_set_btn_size_small(lv_obj_t *btn);
#endif
