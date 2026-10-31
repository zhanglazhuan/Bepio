#include "view.h"
#include "pages/settings_pages_internal.h"
#include "bepio_ui_preferences.h"
#include "pages/settings_home.h"
#include "pages/settings_id.h"
#include "pages/settings_datetime.h"
#include "pages/settings_battery.h"
#include "pages/settings_updates.h"
#include "pages/settings_about.h"
#include "pages/settings_wifi.h"
#include "pages/settings_wifi_connect.h"
#include "pages/settings_usage.h"
#include "pages/settings_storage.h"

bepio_settings_ui_t bepio_settings_ui;
static void refresh(lv_timer_t *timer)
{
	(void)timer;
	if (bepio_settings_ui.view->page == SETTINGS_PAGE_HOME) { bepio_settings_home_refresh(); }
	else if (bepio_settings_ui.view->page == SETTINGS_PAGE_ID) { bepio_settings_id_refresh(); }
	else if (bepio_settings_ui.view->page == SETTINGS_PAGE_WIFI) { bepio_settings_wifi_refresh(); }
	else if (bepio_settings_ui.view->page == SETTINGS_PAGE_WIFI_CONNECT) { bepio_settings_wifi_connect_refresh(); }
	else if (bepio_settings_ui.view->page == SETTINGS_PAGE_USAGE) { bepio_settings_usage_refresh(); }
}
static void stop_timer(void)
{
	if (bepio_settings_ui.timer) { lv_timer_delete(bepio_settings_ui.timer); bepio_settings_ui.timer = NULL; }
}
void settings_view_init(settings_view_t *view, void *root, void *group)
{
	*view = (settings_view_t){.page = SETTINGS_PAGE_HOME, .available = true, .root = root, .group = group};
}
void settings_view_deinit(settings_view_t *view)
{
	stop_timer();
	bepio_settings_ui = (bepio_settings_ui_t){0};
	*view = (settings_view_t){0};
}
void settings_view_render(const settings_view_t *view, const settings_model_t *model)
{
	if (!view->available || !view->root) { return; }
	stop_timer();
	bepio_settings_ui = (bepio_settings_ui_t){.root = view->root, .view = view, .model = model};
	bepio_ui_preferences_apply(bepio_settings_timezones[model->timezone].posix, model->hour_24, model->power_save);
	lv_obj_t *root = view->root;
	lv_obj_clean(root);
	lv_obj_set_layout(root, LV_LAYOUT_NONE);
	lv_obj_set_style_pad_all(root, 0, 0);
	lv_obj_set_style_bg_color(root, lv_color_hex(0xc7d5b8), 0);
	lv_obj_set_scroll_dir(root, LV_DIR_VER);
	lv_obj_scroll_to_y(root, 0, LV_ANIM_OFF);
	lv_obj_t *right = bepio_page_header_right(root);
	if (right) { lv_obj_clean(right); }
	const char *title = "Settings";
	switch (view->page) {
	case SETTINGS_PAGE_HOME: break;
	case SETTINGS_PAGE_ID: title = "ID"; break;
	case SETTINGS_PAGE_DATETIME: title = "Datetime"; break;
	case SETTINGS_PAGE_UPDATES: title = "Updates"; break;
	case SETTINGS_PAGE_BATTERY: title = "Battery"; break;
	case SETTINGS_PAGE_ABOUT: title = "About"; break;
	case SETTINGS_PAGE_WIFI: title = "Wi-Fi"; break;
	case SETTINGS_PAGE_WIFI_CONNECT: title = "Wi-Fi Connect"; break;
	case SETTINGS_PAGE_USAGE: title = "Usage"; break;
	case SETTINGS_PAGE_STORAGE: title = "Storage"; break;
	}
	bepio_page_set_title(root, title, 40);
	switch (view->page) {
	case SETTINGS_PAGE_HOME: bepio_settings_home_render(); break;
	case SETTINGS_PAGE_ID: bepio_settings_id_render(); break;
	case SETTINGS_PAGE_DATETIME: bepio_settings_datetime_render(); break;
	case SETTINGS_PAGE_UPDATES: bepio_settings_updates_render(); break;
	case SETTINGS_PAGE_BATTERY: bepio_settings_battery_render(); break;
	case SETTINGS_PAGE_ABOUT: bepio_settings_about_render(); break;
	case SETTINGS_PAGE_WIFI: bepio_settings_wifi_render(); break;
	case SETTINGS_PAGE_WIFI_CONNECT: bepio_settings_wifi_connect_render(); break;
	case SETTINGS_PAGE_USAGE: bepio_settings_usage_render(); break;
	case SETTINGS_PAGE_STORAGE: bepio_settings_storage_render(); break;
	}
	if (view->page == SETTINGS_PAGE_HOME || view->page == SETTINGS_PAGE_ID || view->page == SETTINGS_PAGE_USAGE ||
	    view->page == SETTINGS_PAGE_WIFI || view->page == SETTINGS_PAGE_WIFI_CONNECT) {
		bepio_settings_ui.timer = lv_timer_create(refresh, view->page == SETTINGS_PAGE_WIFI || view->page == SETTINGS_PAGE_WIFI_CONNECT ? 250 : 1000, NULL);
	}
}
