/* Bottom-edge launcher gesture adapted from DeepStoa launcher_gesture.c.
 * SPDX-License-Identifier: GPL-3.0-only */
#include "launcher_gesture.h"
#include "bepio_launcher.h"
#include "bepio_app_manager.h"
#include "lv_bottom_sheet.h"
#include "lv_ui_style_guide.h"
#include <stdio.h>
#include <stdlib.h>

#define SWIPE_START_BOTTOM_PX 24
#define SWIPE_MIN_UP_PX 40
#define SWIPE_MAX_SIDE_PX 40

static lv_point_t start_point;
static bool tracking;
static lv_obj_t *exit_overlay;

static void overlay_deleted(lv_event_t *event)
{
	if (lv_event_get_target_obj(event) == exit_overlay) { exit_overlay = NULL; }
}

static void exit_confirmed(lv_event_t *event)
{
	lv_bottom_sheet_close(lv_event_get_user_data(event));
	bepio_launcher_return_home();
}

static void exit_cancelled(lv_event_t *event)
{
	lv_bottom_sheet_close(lv_event_get_user_data(event));
}

static void show_exit_dialog(void)
{
	if (exit_overlay || !bepio_app_manager_is_app_running()) { return; }
	lv_bottom_sheet_t *sheet = lv_bottom_sheet_create(lv_layer_top());
	if (!sheet) { return; }
	exit_overlay = sheet->overlay;
	lv_obj_add_event_cb(exit_overlay, overlay_deleted, LV_EVENT_DELETE, NULL);
	lv_bottom_sheet_add_header(sheet, "Exit app?");
	lv_obj_t *content = lv_bottom_sheet_get_content(sheet);
	lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
	lv_obj_set_style_pad_row(content, 12, 0);
	char message[96];
	const char *name = bepio_app_manager_get_current_app_name();
	snprintf(message, sizeof(message), "Close %s and return home?", name ? name : "app");
	lv_obj_t *label = lv_label_create(content);
	lv_label_set_text(label, message);
	lv_obj_set_width(label, lv_pct(100));
	lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
	lv_obj_t *row = lv_obj_create(content);
	lv_obj_remove_style_all(row);
	lv_obj_set_size(row, lv_pct(100), UI_SHEET_BTN_H);
	lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
	lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_EVENLY,
		LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
	lv_obj_t *cancel = lv_button_create(row);
	lv_obj_set_size(cancel, UI_SHEET_BTN_W, UI_SHEET_BTN_H);
	ui_style_set_btn_secondary(cancel);
	lv_obj_add_event_cb(cancel, exit_cancelled, LV_EVENT_CLICKED, sheet);
	lv_obj_center(lv_label_create(cancel));
	lv_label_set_text(lv_obj_get_child(cancel, 0), "Cancel");
	lv_obj_t *exit = lv_button_create(row);
	lv_obj_set_size(exit, UI_SHEET_BTN_W, UI_SHEET_BTN_H);
	ui_style_set_btn_primary(exit);
	lv_obj_add_event_cb(exit, exit_confirmed, LV_EVENT_CLICKED, sheet);
	lv_obj_center(lv_label_create(exit));
	lv_label_set_text(lv_obj_get_child(exit, 0), "Exit");
}

static void input_event(lv_event_t *event)
{
	lv_indev_t *input = lv_event_get_target(event);
	lv_event_code_t code = lv_event_get_code(event);
	if (code == LV_EVENT_PRESSED) {
		lv_indev_get_point(input, &start_point);
		int height = lv_display_get_vertical_resolution(lv_indev_get_display(input));
		tracking = bepio_app_manager_is_app_running() && !exit_overlay &&
			start_point.y >= height - SWIPE_START_BOTTOM_PX;
	} else if (code == LV_EVENT_RELEASED && tracking) {
		tracking = false;
		lv_point_t end;
		lv_indev_get_point(input, &end);
		if (start_point.y - end.y >= SWIPE_MIN_UP_PX &&
		    abs(end.x - start_point.x) <= SWIPE_MAX_SIDE_PX) {
			show_exit_dialog();
			lv_indev_reset(input, NULL);
			lv_event_stop_processing(event);
		}
	}
}

void launcher_gesture_attach_input(lv_indev_t *input)
{
	if (!input) { return; }
	lv_indev_remove_event_cb_with_user_data(input, input_event, NULL);
	lv_indev_add_event_cb(input, input_event, LV_EVENT_ALL, NULL);
}
