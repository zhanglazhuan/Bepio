/* DeepStoa/EPOS bottom-edge gesture port for the 240x320 LCD.
 * SPDX-License-Identifier: GPL-3.0-only */
#include "bepio_navigation.h"
#include <stdlib.h>
static void (*exit_handler)(void);
static const char *current_name;
static lv_obj_t *overlay;
static lv_point_t start_point;
static bool tracking;
static uint32_t generation;
static void close_overlay(void)
{
	if (overlay) { lv_obj_delete(overlay); overlay = NULL; }
}
void bepio_navigation_set_exit_handler(void (*handler)(void), const char *app_name)
{
	close_overlay();
	generation++;
	exit_handler = handler;
	current_name = app_name;
	tracking = false;
}
static void confirmed_async(void *unused)
{
	if ((uint32_t)(uintptr_t)unused == generation && exit_handler) { exit_handler(); }
}
static void confirm(lv_event_t *event)
{
	(void)event;
	close_overlay();
	lv_async_call(confirmed_async, (void *)(uintptr_t)generation);
}
static void cancel(lv_event_t *event)
{
	(void)event;
	close_overlay();
}
static void show_exit(void)
{
	if (overlay || !exit_handler) { return; }
	overlay = lv_obj_create(lv_layer_top());
	lv_obj_remove_style_all(overlay);
	lv_obj_set_size(overlay, lv_pct(100), lv_pct(100));
	lv_obj_set_style_bg_color(overlay, lv_color_black(), 0);
	lv_obj_set_style_bg_opa(overlay, LV_OPA_50, 0);
	lv_obj_add_flag(overlay, LV_OBJ_FLAG_CLICKABLE);
	lv_obj_remove_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_t *sheet = lv_obj_create(overlay);
	lv_obj_set_size(sheet, lv_pct(100), 144);
	lv_obj_align(sheet, LV_ALIGN_BOTTOM_MID, 0, 0);
	lv_obj_set_style_bg_color(sheet, lv_color_hex(0x1b2940), 0);
	lv_obj_set_style_border_width(sheet, 0, 0);
	lv_obj_set_style_pad_all(sheet, 16, 0);
	lv_obj_remove_flag(sheet, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_update_layout(sheet);
	lv_obj_t *title = lv_label_create(sheet);
	lv_label_set_text(title, "Exit app?");
	lv_obj_set_style_text_color(title, lv_color_hex(0xe9eff8), 0);
	lv_obj_t *message = lv_label_create(sheet);
	lv_obj_set_pos(message, 0, 28);
	lv_obj_set_width(message, lv_pct(100));
	lv_label_set_text_fmt(message, "Close %s and return home?", current_name ? current_name : "app");
	lv_obj_set_style_text_color(message, lv_color_hex(0xa8bad2), 0);
	const char *labels[] = {"Cancel", "Exit"};
	for (int i = 0; i < 2; i++) {
		lv_obj_t *button = lv_button_create(sheet);
		lv_obj_set_size(button, 96, 38);
		lv_obj_set_pos(button, i ? lv_obj_get_content_width(sheet) - 96 : 0, 74);
		lv_obj_set_style_bg_color(button, lv_color_hex(i ? 0x346bd3 : 0x3b5272), 0);
		lv_obj_set_style_shadow_width(button, 0, 0);
		lv_obj_add_event_cb(button, i ? confirm : cancel, LV_EVENT_CLICKED, NULL);
		lv_obj_t *label = lv_label_create(button);
		lv_label_set_text(label, labels[i]);
		lv_obj_center(label);
	}
}
static void input_event(lv_event_t *event)
{
	lv_indev_t *input = lv_event_get_target(event);
	lv_event_code_t code = lv_event_get_code(event);
	if (code == LV_EVENT_PRESSED) {
		lv_indev_get_point(input, &start_point);
		int height = lv_display_get_vertical_resolution(lv_indev_get_display(input));
		tracking = exit_handler && !overlay && start_point.y >= height - 24;
	} else if (code == LV_EVENT_RELEASED && tracking) {
		tracking = false;
		lv_point_t end;
		lv_indev_get_point(input, &end);
		if (start_point.y - end.y >= 40 && abs(end.x - start_point.x) <= 40) {
			show_exit();
			/* Consume the release so the gesture never activates an app control. */
			lv_indev_reset(input, NULL);
			lv_event_stop_processing(event);
		}
	}
}
void bepio_navigation_attach_input(lv_indev_t *input)
{
	if (!input) { return; }
	lv_indev_remove_event_cb_with_user_data(input, input_event, NULL);
	lv_indev_add_event_cb(input, input_event, LV_EVENT_ALL, NULL);
}
