#ifndef BEPIO_SETTINGS_PAGES_INTERNAL_H_
#define BEPIO_SETTINGS_PAGES_INTERNAL_H_
#include "../app.h"
#include "lvgl.h"
#include "lv_page.h"
#include "bepio_account.h"
#include <stdint.h>
#include <stdio.h>
typedef struct {
	const settings_view_t *view;
	const settings_model_t *model;
	lv_obj_t *root, *summary, *entry, *notice, *keyboard, *id, *state;
	lv_timer_t *timer;
} bepio_settings_ui_t;
extern bepio_settings_ui_t bepio_settings_ui;
#endif
