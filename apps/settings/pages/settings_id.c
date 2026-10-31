#include "settings_id.h"
#include "settings_page_common.h"
#include <errno.h>
void bepio_settings_id_refresh(void)
{
	bepio_account_snapshot_t a;
	bepio_account_snapshot(&a);
	lv_label_set_text_fmt(bepio_settings_ui.summary, "Account ID: %s\n%s\nConnection: %s\n%s%s",
		a.id, a.confirmed ? "Server confirmed" : "Factory ID (registration pending)",
		a.online ? "Online" : "Offline", a.busy ? "Checking with server..." :
		a.last_result == 0 ? "" : a.last_result == -EEXIST ? "ID already in use" :
		a.last_result == -EAGAIN ? "One change per 24 hours" :
		a.last_result == -ETIMEDOUT ? "Server timeout; ID unchanged" : "Change rejected; ID unchanged",
		a.storage_error ? "\nLocal storage error" : "");
}
static void submit(lv_event_t *event)
{
	(void)event;
	int ret = bepio_account_request(lv_textarea_get_text(bepio_settings_ui.entry));
	lv_label_set_text(bepio_settings_ui.notice, ret == 0 ? "Waiting for server confirmation" :
		ret == -ENETDOWN ? "Offline: backend connection required" :
		ret == -EAGAIN ? "One change per 24 hours" :
		ret == -EBUSY ? "A request is already pending" :
		ret == -EALREADY ? "This is your current ID" : "Use 4-32 lowercase letters, digits or _");
	bepio_settings_id_refresh();
}
static void focus(lv_event_t *event)
{
	lv_keyboard_set_textarea(bepio_settings_ui.keyboard, lv_event_get_target_obj(event));
	lv_obj_remove_flag(bepio_settings_ui.keyboard, LV_OBJ_FLAG_HIDDEN);
}
static void keyboard_done(lv_event_t *event)
{
	(void)event; lv_obj_add_flag(bepio_settings_ui.keyboard, LV_OBJ_FLAG_HIDDEN);
}

void bepio_settings_id_render(void)
{
	lv_obj_t *root = bepio_settings_ui.root;
	lv_obj_set_style_pad_hor(root, 12, 0);
	lv_obj_set_style_pad_ver(root, 8, 0);
	lv_obj_set_style_pad_row(root, 8, 0);
	lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
	bepio_settings_ui.summary = bepio_settings_label(root, "", 0, 0, 0);
	lv_obj_set_width(bepio_settings_ui.summary, lv_pct(100));
	lv_obj_set_style_margin_bottom(bepio_settings_ui.summary, -4, 0);
	bepio_settings_id_refresh();
	bepio_settings_ui.entry = lv_textarea_create(root);
	lv_obj_set_width(bepio_settings_ui.entry, lv_pct(100));
	lv_textarea_set_one_line(bepio_settings_ui.entry, true);
	lv_textarea_set_max_length(bepio_settings_ui.entry, BEPIO_ACCOUNT_ID_MAX);
	lv_textarea_set_placeholder_text(bepio_settings_ui.entry, "New account ID");
	lv_obj_add_event_cb(bepio_settings_ui.entry, focus, LV_EVENT_FOCUSED, NULL);
	bepio_settings_button(root, "Confirm ID change", 0, 0, 180, 32, submit, NULL);
	bepio_settings_ui.notice = bepio_settings_label(root,
		"4-32: a-z, 0-9, _; once per 24h after approval.", 0, 0, 0);
	lv_obj_set_width(bepio_settings_ui.notice, lv_pct(100));
	bepio_settings_ui.keyboard = lv_keyboard_create(root);
	lv_obj_set_width(bepio_settings_ui.keyboard, lv_pct(100));
	lv_obj_add_flag(bepio_settings_ui.keyboard, LV_OBJ_FLAG_HIDDEN);
	lv_obj_add_event_cb(bepio_settings_ui.keyboard, keyboard_done, LV_EVENT_READY, NULL);
	lv_obj_add_event_cb(bepio_settings_ui.keyboard, keyboard_done, LV_EVENT_CANCEL, NULL);
}
