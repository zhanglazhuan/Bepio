/*
 * This file is part of ZSWatch project <https://github.com/zswatch/>.
 * Copyright (c) 2025 EPOS Project.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

/* Bepio port: synchronous lifecycle, bounded registration, optional UI. */
#include "bepio_app_manager.h"

#include <errno.h>
#include <string.h>
#include "bepio_mutex.h"

static bepio_application_t *apps[CONFIG_BEPIO_MAX_APPS];
static int num_apps;
static uint8_t num_visible_apps;
static bepio_application_t *current_app;
static bepio_app_manager_cb_fn close_cb_func;
static bool transitioning;
BEPIO_MUTEX_DEFINE(app_manager_lock);

int bepio_app_manager_add_application(bepio_application_t *app)
{
	int ret = 0;

	if (!app || !app->name || !app->name[0] || !app->start_func ||
	    !app->stop_func || app->category >= BEPIO_APP_CATEGORY_COUNT ||
	    app->category < BEPIO_APP_CATEGORY_ROOT) {
		return -EINVAL;
	}
	bepio_mutex_lock(app_manager_lock);
	if (transitioning) {
		ret = -EBUSY;
		goto out;
	}
	for (int i = 0; i < num_apps; i++) {
		if (apps[i] == app || strcmp(apps[i]->name, app->name) == 0) {
			ret = -EEXIST;
			goto out;
		}
	}
	if (num_apps == CONFIG_BEPIO_MAX_APPS) {
		ret = -ENOMEM;
		goto out;
	}
	app->current_state = BEPIO_APP_STATE_STOPPED;
	app->private_list_index = app->hidden ? UINT8_MAX : num_visible_apps++;
	apps[num_apps++] = app;
out:
	bepio_mutex_unlock(app_manager_lock);
	return ret;
}

int bepio_app_manager_show(bepio_app_manager_cb_fn close_cb, void *root,
			   void *group, const char *app_name)
{
	bepio_application_t *app = NULL;
	int ret = 0;

	if (!app_name) {
		return -EINVAL;
	}
	bepio_mutex_lock(app_manager_lock);
	if (transitioning) {
		ret = -EBUSY;
		goto out;
	}
	for (int i = 0; i < num_apps; i++) {
		if (strcmp(apps[i]->name, app_name) == 0) {
			app = apps[i];
			break;
		}
	}
	if (!app) {
		ret = -ENOENT;
		goto out;
	}
	if (current_app) {
		ret = current_app == app ? 0 : -EBUSY;
		goto out;
	}
	transitioning = true;
	current_app = app;
	app->current_state = BEPIO_APP_STATE_UI_VISIBLE;
	ret = app->start_func(root, group);
	if (ret < 0) {
		app->current_state = BEPIO_APP_STATE_STOPPED;
		app->stop_func();
		current_app = NULL;
	} else {
		close_cb_func = close_cb;
	}
	transitioning = false;
out:
	bepio_mutex_unlock(app_manager_lock);
	return ret;
}

static void stop_current_app(void)
{
	bepio_application_t *app = current_app;

	app->current_state = BEPIO_APP_STATE_STOPPED;
	current_app = NULL;
	close_cb_func = NULL;
	app->stop_func();
}

int bepio_app_manager_delete(void)
{
	int ret = 0;

	bepio_mutex_lock(app_manager_lock);
	if (transitioning) {
		ret = -EBUSY;
	} else if (current_app) {
		transitioning = true;
		stop_current_app();
		transitioning = false;
	}
	bepio_mutex_unlock(app_manager_lock);
	return ret;
}

static int exit_app(bepio_application_t *requester)
{
	bepio_app_manager_cb_fn callback = NULL;
	int ret = 0;

	bepio_mutex_lock(app_manager_lock);
	if (transitioning) {
		ret = -EBUSY;
		goto out;
	}
	if (requester && requester != current_app) {
		ret = -ENOENT;
		goto out;
	}
	if (!current_app) {
		goto out;
	}
	transitioning = true;
	if (!current_app->back_func || !current_app->back_func()) {
		callback = close_cb_func;
		stop_current_app();
	}
	transitioning = false;
out:
	bepio_mutex_unlock(app_manager_lock);
	if (callback) {
		callback();
	}
	return ret;
}

int bepio_app_manager_exit_app(void)
{
	return exit_app(NULL);
}

int bepio_app_manager_app_close_request(bepio_application_t *app)
{
	return app ? exit_app(app) : -EINVAL;
}

int bepio_app_manager_set_ui_available(bool available)
{
	bepio_application_ui_fn callback;
	bepio_app_state_t state = available ? BEPIO_APP_STATE_UI_VISIBLE :
					    BEPIO_APP_STATE_UI_HIDDEN;
	int ret = 0;

	bepio_mutex_lock(app_manager_lock);
	if (transitioning) {
		ret = -EBUSY;
	} else if (!current_app) {
		ret = -ENOENT;
	} else if (current_app->current_state != state) {
		transitioning = true;
		current_app->current_state = state;
		callback = available ? current_app->ui_available_func :
				       current_app->ui_unavailable_func;
		if (callback) {
			callback();
		}
		transitioning = false;
	}
	bepio_mutex_unlock(app_manager_lock);
	return ret;
}

int bepio_app_manager_get_num_apps(void)
{
	int count;

	bepio_mutex_lock(app_manager_lock);
	count = num_apps;
	bepio_mutex_unlock(app_manager_lock);
	return count;
}

bepio_application_t *bepio_app_manager_get_app(int index)
{
	bepio_application_t *app = NULL;

	bepio_mutex_lock(app_manager_lock);
	if (index >= 0 && index < num_apps) {
		app = apps[index];
	}
	bepio_mutex_unlock(app_manager_lock);
	return app;
}

bepio_app_state_t bepio_app_manager_get_app_state(const bepio_application_t *app)
{
	bepio_app_state_t state = BEPIO_APP_STATE_STOPPED;

	bepio_mutex_lock(app_manager_lock);
	if (app) {
		state = app->current_state;
	}
	bepio_mutex_unlock(app_manager_lock);
	return state;
}

bool bepio_app_manager_is_app_running(void)
{
	bool running;

	bepio_mutex_lock(app_manager_lock);
	running = current_app != NULL;
	bepio_mutex_unlock(app_manager_lock);
	return running;
}

const char *bepio_app_manager_get_current_app_name(void)
{
	const char *name;

	bepio_mutex_lock(app_manager_lock);
	name = current_app ? current_app->name : NULL;
	bepio_mutex_unlock(app_manager_lock);
	return name;
}
