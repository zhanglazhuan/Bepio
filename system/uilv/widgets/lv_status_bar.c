/* Top-layer persistence and change-only labels from DeepStoa lv_status_bar.
 * SPDX-License-Identifier: GPL-3.0-only */
#include "lv_status_bar.h"
#include <stdio.h>
#include <string.h>
static lv_obj_t *bar, *clock_label, *network_label, *battery_label;
static void changed(lv_obj_t *label, const char *value)
{
	if (strcmp(lv_label_get_text(label), value)) { lv_label_set_text(label, value); }
}
void bepio_status_bar_set_time(const char *text)
{
	if (bar) { changed(clock_label, text); }
}
void bepio_status_bar_update(const char *time_text, const char *network_text, int battery_percent, bool charging)
{
	if (!bar) { return; }
	changed(clock_label, time_text ? time_text : "--:--");
	changed(network_label, network_text ? network_text : "NB --");
	char battery[32];
	if (battery_percent < 0 || battery_percent > 100) { snprintf(battery, sizeof(battery), "--%% " LV_SYMBOL_BATTERY_EMPTY); }
	else { snprintf(battery, sizeof(battery), "%d%% %s", battery_percent, charging ? LV_SYMBOL_CHARGE : LV_SYMBOL_BATTERY_FULL); }
	changed(battery_label, battery);
}
static void deleted(lv_event_t *event)
{
	(void)event;
	bar = clock_label = network_label = battery_label = NULL;
}
void bepio_status_bar_init(void)
{
	if (bar) { return; }
	bar = lv_obj_create(lv_layer_top());
	lv_obj_remove_style_all(bar);
	lv_obj_set_size(bar, lv_pct(100), LV_STATUS_BAR_HEIGHT);
	lv_obj_set_style_bg_color(bar, lv_color_hex(0x263c2e), 0);
	lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
	lv_obj_set_style_text_color(bar, lv_color_hex(0xdce6cd), 0);
	lv_obj_set_style_pad_hor(bar, 8, 0);
	lv_obj_remove_flag(bar, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_add_event_cb(bar, deleted, LV_EVENT_DELETE, NULL);
	clock_label = lv_label_create(bar);
	lv_obj_align(clock_label, LV_ALIGN_LEFT_MID, 0, 0);
	battery_label = lv_label_create(bar);
	lv_obj_align(battery_label, LV_ALIGN_RIGHT_MID, 0, 0);
	network_label = lv_label_create(bar);
	lv_obj_align(network_label, LV_ALIGN_CENTER, 0, 0);
	bepio_status_bar_update(NULL, NULL, -1, false);
}
void bepio_status_bar_set_visible(bool visible)
{
	if (!bar) { return; }
	if (visible) { lv_obj_remove_flag(bar, LV_OBJ_FLAG_HIDDEN); }
	else { lv_obj_add_flag(bar, LV_OBJ_FLAG_HIDDEN); }
}
