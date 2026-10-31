#include "controller.h"
#include <errno.h>
#include "bepio_mutex.h"

BEPIO_MUTEX_DEFINE(settings_lock);

void settings_controller_init(settings_controller_t *controller,
			      settings_model_t *model, settings_view_t *view)
{
	bepio_mutex_lock(settings_lock);
	controller->model = model;
	controller->view = view;
	controller->active = true;
	settings_view_render(view, model);
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
	if (controller->active && controller->view->page != SETTINGS_PAGE_HOME) {
		controller->view->page = controller->view->page == SETTINGS_PAGE_WIFI_CONNECT ? SETTINGS_PAGE_WIFI : SETTINGS_PAGE_HOME;
		settings_view_render(controller->view, controller->model);
		consumed = true;
	}
	bepio_mutex_unlock(settings_lock);
	return consumed;
}

void settings_controller_set_ui_available(settings_controller_t *controller, bool available)
{
	bepio_mutex_lock(settings_lock);
	if (controller->active) {
		controller->view->available = available;
		settings_view_render(controller->view, controller->model);
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
			.page = controller->view->page,
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
		settings_view_render(controller->view, controller->model);
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
		controller->view->page = page;
		settings_view_render(controller->view, controller->model);
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
		settings_view_render(controller->view, controller->model);
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
		settings_view_render(controller->view, controller->model);
	}
	bepio_mutex_unlock(settings_lock);
}

int bepio_settings_controller_wifi_select(settings_controller_t *controller, const bepio_wifi_ap_t *ap)
{
	if (!ap || !ap->ssid[0]) { return -EINVAL; }
	bepio_mutex_lock(settings_lock);
	int ret = controller->active ? 0 : -EACCES;
	if (!ret) {
		controller->view->wifi_ap = *ap;
		controller->view->page = SETTINGS_PAGE_WIFI_CONNECT;
		settings_view_render(controller->view, controller->model);
	}
	bepio_mutex_unlock(settings_lock);
	return ret;
}
