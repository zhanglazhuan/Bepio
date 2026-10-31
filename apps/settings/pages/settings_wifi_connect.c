/* BSSID/channel selection follows the NomadCast connection flow. */
#include "settings_wifi_connect.h"
#include "settings_page_common.h"
#include "system/uilv/imgs/icon_24x24/bepio_icons_24.h"
#include <errno.h>
#include <limits.h>
#include <string.h>
static lv_obj_t *password, *keyboard, *notice, *connect_button;
static uint32_t shown_revision;
static void connect_clicked(lv_event_t *event)
{
	(void)event;
	const bepio_wifi_ap_t *ap = &bepio_settings_ui.view->wifi_ap;
	int error = bepio_wifi_connect(ap, ap->secured ? lv_textarea_get_text(password) : "");
	if (error) {
		lv_label_set_text(notice, error == -EINVAL ? "8-63 chars / 64 hex digits" :
			error == -EBUSY ? "Connecting..." : "Cannot connect; return to Wi-Fi and retry");
	} else { bepio_settings_wifi_connect_refresh(); }
}
static void focused(lv_event_t *event)
{
	(void)event; lv_obj_remove_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
}
static void keyboard_event(lv_event_t *event)
{
	if (lv_event_get_code(event) == LV_EVENT_READY) { connect_clicked(event); }
	else { lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN); }
}
static void show_password(lv_event_t *event)
{
	lv_obj_t *icon = lv_event_get_user_data(event);
	bool hidden = lv_textarea_get_password_mode(password);
	lv_textarea_set_password_mode(password, !hidden);
	lv_image_set_src(icon, hidden ? &ic_visibility_24 : &ic_visibility_off_24);
}
void bepio_settings_wifi_connect_refresh(void)
{
	bepio_wifi_snapshot_t snapshot; bepio_wifi_snapshot(&snapshot);
	if (shown_revision == snapshot.revision) { return; }
	shown_revision = snapshot.revision;
	const bepio_wifi_ap_t *ap = &bepio_settings_ui.view->wifi_ap;
	bool connected = snapshot.connected && !strcmp(ap->ssid, snapshot.ssid);
	if (snapshot.busy) {
		lv_obj_add_state(connect_button, LV_STATE_DISABLED); lv_obj_add_state(password, LV_STATE_DISABLED);
		lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
	} else {
		lv_obj_remove_state(connect_button, LV_STATE_DISABLED);
		if (ap->secured) { lv_obj_remove_state(password, LV_STATE_DISABLED); }
	}
	if (connected) {
		lv_label_set_text_fmt(notice, "Connected: %s%s", snapshot.ip, snapshot.storage_error ? " / save failed" : "");
		lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
	} else if (snapshot.busy || snapshot.error || !snapshot.enabled) { lv_label_set_text(notice, snapshot.status); }
	else { lv_label_set_text(notice, ap->secured ? "" : "Open network; no password needed"); }
}
void bepio_settings_wifi_connect_render(void)
{
	lv_obj_t *root = bepio_settings_ui.root;
	const bepio_wifi_ap_t *ap = &bepio_settings_ui.view->wifi_ap;
	lv_obj_t *ssid = bepio_settings_label(root, ap->ssid, 8, 0, 304);
	lv_label_set_long_mode(ssid, LV_LABEL_LONG_DOT);
	password = lv_textarea_create(root);
	lv_obj_set_pos(password, 8, 20); lv_obj_set_size(password, lv_obj_get_width(root) - 16, 31);
	lv_obj_set_style_pad_all(password, 5, 0);
	lv_obj_set_style_pad_right(password, 40, 0);
	lv_textarea_set_one_line(password, true); lv_textarea_set_password_mode(password, true);
	lv_textarea_set_max_length(password, BEPIO_WIFI_PASSWORD_MAX);
	lv_textarea_set_placeholder_text(password, "Password");
	lv_obj_add_event_cb(password, focused, LV_EVENT_FOCUSED, NULL);
	lv_obj_add_event_cb(password, focused, LV_EVENT_CLICKED, NULL);
	lv_obj_t *show = lv_button_create(root);
	lv_obj_set_pos(show, lv_obj_get_width(root) - 43, 20); lv_obj_set_size(show, 32, 31);
	lv_obj_set_style_pad_all(show, 0, 0);
	lv_obj_set_style_bg_opa(show, LV_OPA_TRANSP, 0);
	lv_obj_set_style_shadow_width(show, 0, 0);
	lv_obj_set_ext_click_area(show, 6);
	lv_obj_t *icon = lv_image_create(show);
	lv_image_set_src(icon, &ic_visibility_off_24);
	lv_obj_set_style_image_recolor(icon, lv_color_hex(0x405d43), 0);
	lv_obj_set_style_image_recolor_opa(icon, LV_OPA_COVER, 0);
	lv_obj_remove_flag(icon, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_center(icon);
	lv_obj_add_event_cb(show, show_password, LV_EVENT_CLICKED, icon);
	connect_button = bepio_settings_button(root, "Connect", 8, 59, 96, 28, connect_clicked, NULL);
	notice = bepio_settings_label(root, "", 112, 59, 200);
	lv_label_set_long_mode(notice, LV_LABEL_LONG_DOT);
	keyboard = lv_keyboard_create(root);
	lv_obj_set_align(keyboard, LV_ALIGN_TOP_LEFT);
	lv_obj_set_pos(keyboard, 0, lv_obj_get_height(root) - 84);
	lv_obj_set_size(keyboard, lv_obj_get_width(root), 84);
	lv_keyboard_set_textarea(keyboard, password);
	lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
	lv_obj_add_event_cb(keyboard, keyboard_event, LV_EVENT_READY, NULL);
	lv_obj_add_event_cb(keyboard, keyboard_event, LV_EVENT_CANCEL, NULL);
	if (!ap->secured) { lv_obj_add_state(password, LV_STATE_DISABLED); lv_obj_add_state(show, LV_STATE_DISABLED); lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN); }
	shown_revision = UINT32_MAX;
	bepio_settings_wifi_connect_refresh();
}
