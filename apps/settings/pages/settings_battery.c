#include "settings_battery.h"
#include "settings_page_common.h"
static void changed(lv_event_t *event)
{
	settings_controller_set(settings_app_get_controller(), "power_save",
		lv_obj_has_state(lv_event_get_target_obj(event), LV_STATE_CHECKED));
}
void bepio_settings_battery_render(void)
{
	lv_obj_t *root = bepio_settings_ui.root;
	bepio_settings_label(root, "Power saving", 12, 16, 220);
	lv_obj_t *toggle = lv_switch_create(root);
	lv_obj_set_pos(toggle, 250, 12); lv_obj_set_size(toggle, 50, 28);
	if (bepio_settings_ui.model->power_save) { lv_obj_add_state(toggle, LV_STATE_CHECKED); }
	lv_obj_add_event_cb(toggle, changed, LV_EVENT_VALUE_CHANGED, NULL);
	bepio_settings_label(root, "Turn off the screen after 30 seconds of inactivity. Touch once to wake it.", 12, 60, 296);
	bepio_settings_label(root, "Does not suspend the modem or audio.\nPreference resets after restart.", 12, 118, 296);
}
