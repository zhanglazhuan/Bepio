/* Settings flow inspired by NomadCast view_wifi; service owns all radio work. */
#include "settings_wifi.h"
#include "settings_page_common.h"
#include <errno.h>
#include <limits.h>
#include <string.h>
static lv_obj_t *toggle, *status_label, *networks;
static bepio_wifi_snapshot_t snapshot;
static uint32_t shown_revision;
static void show_error(int error)
{
	if (error) { lv_label_set_text(status_label, error == -EBUSY ? "Wi-Fi is busy; please wait" : "Cannot start Wi-Fi; please retry"); }
}
static void enabled_changed(lv_event_t *event)
{
	int error = bepio_wifi_set_enabled(lv_obj_has_state(lv_event_get_target_obj(event), LV_STATE_CHECKED));
	bepio_settings_wifi_refresh();
	show_error(error);
}
static void selected(lv_event_t *event)
{
	size_t at = (size_t)(uintptr_t)lv_event_get_user_data(event);
	if (at >= snapshot.count) { return; }
	bepio_wifi_ap_t ap = snapshot.aps[at];
	if (!ap.supported) { lv_label_set_text(status_label, "This Wi-Fi security type is not supported"); return; }
	if (ap.saved || !ap.secured) {
		int error = bepio_wifi_connect(&ap, ap.saved ? NULL : "");
		if (error) { show_error(error); return; }
	}
	bepio_settings_controller_wifi_select(settings_app_get_controller(), &ap);
}
void bepio_settings_wifi_refresh(void)
{
	bepio_wifi_snapshot(&snapshot);
	if (shown_revision == snapshot.revision) { return; }
	shown_revision = snapshot.revision;
	if (snapshot.enabled) { lv_obj_add_state(toggle, LV_STATE_CHECKED); }
	else { lv_obj_remove_state(toggle, LV_STATE_CHECKED); }
	if (!snapshot.enabled) {
		lv_label_set_text(status_label, "");
	} else if (snapshot.scanning) {
		lv_label_set_text(status_label, "Scanning...");
	} else if (snapshot.connected) {
		lv_label_set_text_fmt(status_label, "%s  %s%s", snapshot.ssid, snapshot.ip,
			snapshot.storage_error ? " (save failed)" : "");
	} else { lv_label_set_text(status_label, snapshot.status); }
	lv_obj_set_style_text_align(status_label, snapshot.scanning ? LV_TEXT_ALIGN_CENTER : LV_TEXT_ALIGN_LEFT, 0);
	lv_obj_clean(networks);
	if (!snapshot.enabled) { return; }
	if (snapshot.scanning) { return; }
	for (size_t i = 0; i < snapshot.count; i++) {
		const bepio_wifi_ap_t *ap = &snapshot.aps[i];
		lv_obj_t *row = bepio_settings_button(networks, "", 0, i * 37, 304, 33, selected, (void *)(uintptr_t)i);
		lv_obj_set_style_bg_color(row, lv_color_hex(0xe4eadc), 0);
		lv_obj_t *ssid = bepio_settings_label(row, ap->ssid, 10, 8, 284);
		lv_label_set_long_mode(ssid, LV_LABEL_LONG_DOT);
		if (snapshot.connecting) { lv_obj_add_state(row, LV_STATE_DISABLED); }
	}
}
void bepio_settings_wifi_render(void)
{
	lv_obj_t *root = bepio_settings_ui.root;
	bepio_settings_label(root, "Wi-Fi", 8, 9, 80);
	toggle = lv_switch_create(root); lv_obj_set_pos(toggle, 264, 4); lv_obj_set_size(toggle, 48, 28);
	lv_obj_add_event_cb(toggle, enabled_changed, LV_EVENT_VALUE_CHANGED, NULL);
	status_label = bepio_settings_label(root, "", 8, 38, 304);
	lv_label_set_long_mode(status_label, LV_LABEL_LONG_DOT);
	networks = lv_obj_create(root);
	lv_obj_set_pos(networks, 8, 62); lv_obj_set_size(networks, 304, lv_obj_get_height(root) - 62);
	lv_obj_set_style_pad_all(networks, 0, 0); lv_obj_set_style_border_width(networks, 0, 0);
	lv_obj_set_style_bg_opa(networks, LV_OPA_TRANSP, 0); lv_obj_set_scroll_dir(networks, LV_DIR_VER);
	shown_revision = UINT32_MAX;
	bepio_settings_wifi_refresh();
}
