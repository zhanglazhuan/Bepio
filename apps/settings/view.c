#include "view.h"
#include "pages/settings_page_context.h"
#include "page_navigator.h"
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
#include <stdlib.h>

static void free_context(void *context)
{
	free(context);
}

static void release_context_on_delete(lv_event_t *event)
{
	void *ctx = lv_event_get_user_data(event);
	/* Child DELETE callbacks may still use the page context. */
	lv_async_call(free_context, ctx);
}

void settings_view_init(settings_view_t *view, void *root, void *group)
{
	*view = (settings_view_t){.available = true,
		.root = root, .group = group};
	page_navigator_t *nav = &view->page_nav;
	page_navigator_init(nav, view->page_registry, 10, view);
	page_navigator_register_page(nav, SETTINGS_PAGE_HOME, bepio_settings_home_build);
	page_navigator_register_page(nav, SETTINGS_PAGE_ABOUT, bepio_settings_about_build);
	page_navigator_register_page(nav, SETTINGS_PAGE_ID, bepio_settings_id_build);
	page_navigator_register_page(nav, SETTINGS_PAGE_DATETIME, bepio_settings_datetime_build);
	page_navigator_register_page(nav, SETTINGS_PAGE_UPDATES, bepio_settings_updates_build);
	page_navigator_register_page(nav, SETTINGS_PAGE_BATTERY, bepio_settings_battery_build);
	page_navigator_register_page(nav, SETTINGS_PAGE_WIFI, bepio_settings_wifi_build);
	page_navigator_register_page(nav, SETTINGS_PAGE_WIFI_CONNECT, bepio_settings_wifi_connect_build);
	page_navigator_register_page(nav, SETTINGS_PAGE_USAGE, bepio_settings_usage_build);
	page_navigator_register_page(nav, SETTINGS_PAGE_STORAGE, bepio_settings_storage_build);
}

void settings_view_deinit(settings_view_t *view)
{
	settings_page_context_t *ctx = view->page_nav.nav_ctx;
	if (ctx) {
		if (ctx->timer) { lv_timer_delete(ctx->timer); ctx->timer = NULL; }
		lv_obj_t *screen = lv_screen_active();
		if (screen) { lv_obj_add_event_cb(screen, release_context_on_delete, LV_EVENT_DELETE, ctx); }
		else { free(ctx); }
	}
	page_navigator_deinit(&view->page_nav);
	view->root = NULL;
	view->available = false;
}
