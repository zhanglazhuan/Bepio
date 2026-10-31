#include "contacts_edit.h"
#include "contacts_page_common.h"

static void keyboard_event(lv_event_t *event)
{
	if (!bepio_contacts_ui.view || bepio_contacts_ui.rebuilding) { return; }
	lv_event_code_t code = lv_event_get_code(event);
	if (code != LV_EVENT_READY && code != LV_EVENT_CANCEL) { return; }
	if (code == LV_EVENT_CANCEL) { contacts_controller_back(contacts_app_get_controller()); }
	else {
		int ret = bepio_contacts_page_sync_draft();
		if (!ret) { ret = contacts_controller_save(contacts_app_get_controller()); }
		bepio_contacts_page_report(ret);
	}
}
static const char * const phone_map[] = {
	"1", "2", "3", LV_SYMBOL_BACKSPACE, "\n",
	"4", "5", "6", LV_SYMBOL_LEFT, "\n",
	"7", "8", "9", LV_SYMBOL_RIGHT, "\n",
	"+", "0", LV_SYMBOL_CLOSE, LV_SYMBOL_OK, ""
};
static const lv_buttonmatrix_ctrl_t phone_controls[] = {
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, LV_BUTTONMATRIX_CTRL_CLICK_TRIG | 1, LV_BUTTONMATRIX_CTRL_CLICK_TRIG | 1,
};

static lv_obj_t *keyboard(void)
{
	lv_obj_t *obj = lv_keyboard_create(bepio_contacts_ui.root);
	lv_keyboard_set_map(obj, LV_KEYBOARD_MODE_NUMBER, phone_map, phone_controls);
	lv_obj_set_align(obj, LV_ALIGN_TOP_LEFT);
	if (bepio_contacts_page_wide()) {
		lv_obj_set_pos(obj, 0, lv_obj_get_height(bepio_contacts_ui.root) - 84);
		lv_obj_set_size(obj, lv_obj_get_width(bepio_contacts_ui.root), 84);
	} else if (bepio_page_has_header(bepio_contacts_ui.root)) {
		lv_obj_set_pos(obj, 0, lv_obj_get_height(bepio_contacts_ui.root) - 96);
		lv_obj_set_size(obj, 240, 96);
	} else { lv_obj_set_pos(obj, 0, 164); lv_obj_set_size(obj, 240, 116); }
	lv_obj_set_style_pad_all(obj, 2, 0);
	lv_obj_set_style_pad_row(obj, 3, 0);
	lv_obj_set_style_pad_column(obj, 3, 0);
	lv_obj_add_event_cb(obj, keyboard_event, LV_EVENT_READY, NULL);
	lv_obj_add_event_cb(obj, keyboard_event, LV_EVENT_CANCEL, NULL);
	return obj;
}

static void form_event(lv_event_t *event)
{
	if (!bepio_contacts_ui.view || bepio_contacts_ui.rebuilding) { return; }
	lv_obj_t *target = lv_event_get_target(event);
	if (lv_event_get_code(event) == LV_EVENT_FOCUSED || lv_event_get_code(event) == LV_EVENT_CLICKED) {
		lv_keyboard_set_mode(bepio_contacts_ui.keyboard, target == bepio_contacts_ui.phone ? LV_KEYBOARD_MODE_NUMBER : LV_KEYBOARD_MODE_TEXT_LOWER);
		lv_keyboard_set_textarea(bepio_contacts_ui.keyboard, target);
	} else if (lv_event_get_code(event) == LV_EVENT_VALUE_CHANGED) {
		int ret = bepio_contacts_page_sync_draft();
		if (ret) { bepio_contacts_page_report(ret); }
		else { lv_label_set_text(bepio_contacts_ui.error, ""); }
	}
}

static lv_obj_t *field(const char *placeholder, const char *value, int y, unsigned maximum)
{
	lv_obj_t *obj = lv_textarea_create(bepio_contacts_ui.root);
	lv_obj_set_pos(obj, 12, y); lv_obj_set_size(obj, lv_obj_get_width(bepio_contacts_ui.root) - 24, 36);
	lv_textarea_set_one_line(obj, true);
	lv_textarea_set_max_length(obj, maximum);
	lv_textarea_set_placeholder_text(obj, placeholder);
	lv_textarea_set_text(obj, value);
	lv_obj_set_style_pad_all(obj, 8, 0);
	lv_obj_set_style_radius(obj, 9, 0);
	lv_obj_set_style_bg_color(obj, lv_color_white(), 0);
	lv_obj_set_style_text_color(obj, lv_color_hex(INK), 0);
	return obj;
}

void bepio_contacts_edit_page_render(void)
{
	bepio_page_set_title(bepio_contacts_ui.root, bepio_contacts_ui.view->draft.id ? "Edit Contact" : "New Contact", 40);
	bool compact = bepio_page_has_header(bepio_contacts_ui.root);
	if (bepio_contacts_page_wide()) {
		bepio_contacts_ui.error = bepio_contacts_page_text(bepio_contacts_ui.root, "Name and mobile number", 8, 0, 304, MUTED);
		bepio_contacts_ui.name = field("Name", bepio_contacts_ui.view->draft.name, 20, CONTACTS_NAME_MAX);
		bepio_contacts_ui.phone = field("Mobile number", bepio_contacts_ui.view->draft.phone, 20, CONTACTS_PHONE_MAX);
		lv_obj_set_pos(bepio_contacts_ui.name, 8, 20); lv_obj_set_size(bepio_contacts_ui.name, 148, 32);
		lv_obj_set_pos(bepio_contacts_ui.phone, 164, 20); lv_obj_set_size(bepio_contacts_ui.phone, 148, 32);
		lv_obj_set_style_pad_ver(bepio_contacts_ui.name, 6, 0); lv_obj_set_style_pad_ver(bepio_contacts_ui.phone, 6, 0);
		lv_textarea_set_accepted_chars(bepio_contacts_ui.phone, "+0123456789");
		bepio_contacts_page_button(bepio_contacts_ui.root, "Cancel", 8, 57, 148, 27, MUTED, ACT_BACK);
		bepio_contacts_page_button(bepio_contacts_ui.root, "Save", 164, 57, 148, 27, GREEN, ACT_SAVE);
		bepio_contacts_ui.keyboard = keyboard(); lv_keyboard_set_textarea(bepio_contacts_ui.keyboard, bepio_contacts_ui.name);
		lv_obj_add_event_cb(bepio_contacts_ui.name, form_event, LV_EVENT_ALL, NULL);
		lv_obj_add_event_cb(bepio_contacts_ui.phone, form_event, LV_EVENT_ALL, NULL);
		return;
	}
	lv_obj_t *title = bepio_contacts_page_text(bepio_contacts_ui.root, bepio_contacts_ui.view->draft.id ? "Edit contact" : "New contact", 12, compact ? 0 : 7, 0, INK);
	lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
	bepio_contacts_ui.error = bepio_contacts_page_text(bepio_contacts_ui.root, "", 12, compact ? 23 : 29, 216, RED);
	bepio_contacts_ui.name = field("Name", bepio_contacts_ui.view->draft.name, compact ? 40 : 46, CONTACTS_NAME_MAX);
	bepio_contacts_ui.phone = field("Mobile number", bepio_contacts_ui.view->draft.phone, compact ? 72 : 88, CONTACTS_PHONE_MAX);
	if (compact) {
		lv_obj_set_height(bepio_contacts_ui.name, 30); lv_obj_set_height(bepio_contacts_ui.phone, 30);
		lv_obj_set_style_pad_ver(bepio_contacts_ui.name, 6, 0); lv_obj_set_style_pad_ver(bepio_contacts_ui.phone, 6, 0);
	}
	lv_textarea_set_accepted_chars(bepio_contacts_ui.phone, "+0123456789");
	bepio_contacts_page_button(bepio_contacts_ui.root, "Cancel", 12, compact ? 104 : 130, 100, compact ? 28 : 30, MUTED, ACT_BACK);
	bepio_contacts_page_button(bepio_contacts_ui.root, "Save", 124, compact ? 104 : 130, 104, compact ? 28 : 30, GREEN, ACT_SAVE);
	bepio_contacts_ui.keyboard = keyboard();
	lv_keyboard_set_textarea(bepio_contacts_ui.keyboard, bepio_contacts_ui.name);
	lv_obj_add_event_cb(bepio_contacts_ui.name, form_event, LV_EVENT_ALL, NULL);
	lv_obj_add_event_cb(bepio_contacts_ui.phone, form_event, LV_EVENT_ALL, NULL);
}
