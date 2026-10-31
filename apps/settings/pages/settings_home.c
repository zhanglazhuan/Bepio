#include "settings_home.h"
#include "settings_page_common.h"
#include <string.h>
static void open_page(lv_event_t *event)
{
	bepio_settings_controller_page(settings_app_get_controller(),
		(settings_page_t)(uintptr_t)lv_event_get_user_data(event));
}
void bepio_settings_home_refresh(void)
{
	bepio_account_snapshot_t account;
	bepio_account_snapshot(&account);
	if (strcmp(lv_label_get_text(bepio_settings_ui.id), account.id)) {
		lv_label_set_text(bepio_settings_ui.id, account.id);
	}
	char state[64];
	snprintf(state, sizeof(state), "%s  /  %s",
		account.online ? "Online" : "Offline", account.confirmed ? "Registered" : "Registration pending");
	if (strcmp(lv_label_get_text(bepio_settings_ui.state), state)) {
		lv_label_set_text(bepio_settings_ui.state, state);
	}
}
void bepio_settings_home_render(void)
{
	lv_obj_t *root = bepio_settings_ui.root;
	lv_obj_set_style_pad_all(root, 4, 0);
	lv_obj_set_style_pad_row(root, 2, 0);
	lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
	lv_obj_t *card = bepio_settings_button(root, "", 0, 0, lv_obj_get_width(root) - 8, 44,
		open_page, (void *)(uintptr_t)SETTINGS_PAGE_ID);
	lv_obj_set_style_bg_color(card, lv_color_hex(0xe4eadc), 0);
	bepio_settings_ui.id = bepio_settings_label(card, "", 10, 3, 284);
	bepio_settings_ui.state = bepio_settings_label(card, "", 10, 24, 284);
	for (uint32_t i = 0; i < lv_obj_get_child_count(card); i++) {
		lv_obj_t *label = lv_obj_get_child(card, i);
		lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
	}
	const char *names[] = {"Wi-Fi", "Datetime", "Updates", "Battery", "Storage", "Usage", "About"};
	const settings_page_t pages[] = {SETTINGS_PAGE_WIFI, SETTINGS_PAGE_DATETIME, SETTINGS_PAGE_UPDATES,
		SETTINGS_PAGE_BATTERY, SETTINGS_PAGE_STORAGE, SETTINGS_PAGE_USAGE, SETTINGS_PAGE_ABOUT};
	for (unsigned i = 0; i < sizeof(pages) / sizeof(pages[0]); i++) {
		lv_obj_t *row = bepio_settings_button(root, "", 0, 0, lv_obj_get_width(root) - 8, 33,
			open_page, (void *)(uintptr_t)pages[i]);
		lv_obj_set_style_bg_color(row, lv_color_hex(0xe4eadc), 0);
		bepio_settings_label(row, names[i], 10, 8, 254);
		bepio_settings_label(row, LV_SYMBOL_RIGHT, lv_obj_get_width(root) - 32, 8, 16);
	}
	bepio_settings_home_refresh();
}
