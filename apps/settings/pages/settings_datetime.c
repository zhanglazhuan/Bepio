#include "settings_datetime.h"
#include "settings_page_common.h"
#include <time.h>
static void timezone_changed(lv_event_t *event)
{
	bepio_settings_controller_timezone(settings_app_get_controller(),
		lv_dropdown_get_selected(lv_event_get_target_obj(event)));
}
static void format_changed(lv_event_t *event)
{
	settings_controller_set(settings_app_get_controller(), "hour_24",
		(uintptr_t)lv_event_get_user_data(event) != 0);
}
void bepio_settings_datetime_render(void)
{
	lv_obj_t *root = bepio_settings_ui.root;
	bepio_settings_label(root, "Time zone", 8, 5, 304);
	lv_obj_t *zone = lv_dropdown_create(root);
	lv_obj_set_pos(zone, 8, 28); lv_obj_set_size(zone, 304, 32);
	lv_dropdown_clear_options(zone);
	for (size_t i = 0; i < bepio_settings_timezone_count; i++) {
		lv_dropdown_add_option(zone, bepio_settings_timezones[i].name, LV_DROPDOWN_POS_LAST);
	}
	lv_dropdown_set_selected(zone, bepio_settings_ui.model->timezone);
	lv_obj_add_event_cb(zone, timezone_changed, LV_EVENT_VALUE_CHANGED, NULL);
	bepio_settings_label(root, "Time format", 8, 74, 304);
	lv_obj_t *h24 = bepio_settings_button(root, "24h", 8, 98, 148, 32, format_changed, (void *)1);
	lv_obj_t *h12 = bepio_settings_button(root, "12h", 164, 98, 148, 32, format_changed, NULL);
	lv_obj_set_style_bg_color(bepio_settings_ui.model->hour_24 ? h12 : h24, lv_color_hex(0x738295), 0);
	bepio_settings_label(root, time(NULL) < 1704067200 ? "Waiting for clock synchronization" : "Applied to the status bar clock", 8, 140, 304);
}
