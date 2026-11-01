#include "controller.h"
#include <errno.h>
#include "bepio_mutex.h"
#include "system/uilv/bepio_ui_preferences.h"

BEPIO_MUTEX_DEFINE(settings_lock);

static void apply_preferences(const settings_model_t *model)
{
	bepio_ui_preferences_apply(bepio_settings_timezones[model->timezone].posix,
		model->hour_24, model->power_save);
}

/* The navigator owns the page id/stack; controller reports failed builds to callers. */
static int navigate_locked(settings_controller_t *controller, settings_page_t page,
	const bepio_wifi_ap_t *payload)
{
	settings_view_t *view = controller->view;
	if (!view->available) { return -EACCES; }
	page_navigator_t *nav = &view->page_nav;
	int old_page = nav->current_page, old_top = nav->stack_top;
	void *old_context = nav->nav_ctx;
	void *old_root = view->root;
	if (old_page && old_page != (int)page) { page_navigator_push(nav, old_page); }
	page_navigator_navigate_to(nav, view, page, (void *)payload);
	if (nav->current_page == (int)page &&
		(old_page != (int)page || nav->nav_ctx != old_context)) { return 0; }
	nav->stack_top = old_top;
	view->root = old_root;
	return -ENOMEM;
}

static void refresh_locked(settings_controller_t *controller)
{
	settings_view_t *view = controller->view;
	if (!view->available) { return; }
	/* The selected AP is owned by the open connection page. Its controls stay live. */
	if (view->page_nav.current_page == SETTINGS_PAGE_WIFI_CONNECT) { return; }
	(void)navigate_locked(controller, (settings_page_t)view->page_nav.current_page, NULL);
}

void settings_controller_init(settings_controller_t *controller,
			      settings_model_t *model, settings_view_t *view)
{
	bepio_mutex_lock(settings_lock);
	controller->model = model;
	controller->view = view;
	controller->active = true;
	view->model = model;
	apply_preferences(model);
	(void)navigate_locked(controller, SETTINGS_PAGE_HOME, NULL);
	bepio_mutex_unlock(settings_lock);
}

void settings_controller_deinit(settings_controller_t *controller)
{
	bepio_mutex_lock(settings_lock);
	controller->active = false;
	settings_view_deinit(controller->view);
	bepio_mutex_unlock(settings_lock);
}

bool settings_controller_back(settings_controller_t *controller)
{
	bool consumed = false;

	bepio_mutex_lock(settings_lock);
	if (controller->active && controller->view->available) {
		settings_view_t *view = controller->view;
		page_navigator_t *nav = &view->page_nav;
		int old_top = nav->stack_top;
		void *old_root = view->root;
		consumed = page_navigator_navigate_pop(nav, view);
		if (!consumed) {
			nav->stack_top = old_top;
			view->root = old_root;
		}
	}
	bepio_mutex_unlock(settings_lock);
	return consumed;
}

void settings_controller_set_ui_available(settings_controller_t *controller, bool available)
{
	bepio_mutex_lock(settings_lock);
	if (controller->active) {
		controller->view->available = available;
	}
	bepio_mutex_unlock(settings_lock);
}

int settings_controller_snapshot(settings_controller_t *controller, settings_snapshot_t *snapshot)
{
	int ret = 0;

	if (!snapshot) {
		return -EINVAL;
	}
	bepio_mutex_lock(settings_lock);
	if (!controller->active) {
		ret = -EACCES;
	} else {
		*snapshot = (settings_snapshot_t) {
			.preferences = *controller->model,
			.page = (settings_page_t)controller->view->page_nav.current_page,
			.available = controller->view->available,
		};
	}
	bepio_mutex_unlock(settings_lock);
	return ret;
}

int settings_controller_set(settings_controller_t *controller, const char *key, bool value)
{
	int ret;

	bepio_mutex_lock(settings_lock);
	ret = controller->active ? settings_model_set(controller->model, key, value) : -EACCES;
	if (ret == 0) {
		apply_preferences(controller->model);
	}
	bepio_mutex_unlock(settings_lock);
	return ret;
}

int bepio_settings_controller_page(settings_controller_t *controller, settings_page_t page)
{
	if (page < SETTINGS_PAGE_HOME || page > SETTINGS_PAGE_STORAGE) { return -EINVAL; }
	bepio_mutex_lock(settings_lock);
	int ret = controller->active ? 0 : -EACCES;
	if (!ret) {
		ret = navigate_locked(controller, page, NULL);
	}
	bepio_mutex_unlock(settings_lock);
	return ret;
}
int bepio_settings_controller_timezone(settings_controller_t *controller, unsigned timezone)
{
	if (timezone >= bepio_settings_timezone_count) { return -EINVAL; }
	bepio_mutex_lock(settings_lock);
	int ret = controller->active ? 0 : -EACCES;
	if (!ret) {
		controller->model->timezone = timezone;
		apply_preferences(controller->model);
	}
	bepio_mutex_unlock(settings_lock);
	return ret;
}
int settings_controller_about(settings_controller_t *controller)
{
	return bepio_settings_controller_page(controller, SETTINGS_PAGE_ABOUT);
}

void settings_controller_reset(settings_controller_t *controller)
{
	bepio_mutex_lock(settings_lock);
	settings_model_reset(controller->model);
	if (controller->active) {
		apply_preferences(controller->model);
		refresh_locked(controller);
	}
	bepio_mutex_unlock(settings_lock);
}

int bepio_settings_controller_wifi_select(settings_controller_t *controller, const bepio_wifi_ap_t *ap)
{
	if (!ap || !ap->ssid[0]) { return -EINVAL; }
	bepio_mutex_lock(settings_lock);
	int ret = controller->active ? 0 : -EACCES;
	if (!ret) {
		ret = navigate_locked(controller, SETTINGS_PAGE_WIFI_CONNECT, ap);
	}
	bepio_mutex_unlock(settings_lock);
	return ret;
}
