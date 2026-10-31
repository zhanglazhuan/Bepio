#ifdef ESP_PLATFORM
#include "sdkconfig.h"
#endif
/* Adapted from EPOS/ZSWatch application manager. SPDX-License-Identifier: GPL-3.0-only */
#ifndef BEPIO_APP_MANAGER_H_
#define BEPIO_APP_MANAGER_H_

#include <stdbool.h>
#include <stdint.h>

/* Opaque UI handles: the LVGL launcher passes its root and input group.
 * Calls are synchronous on the caller's thread. GUI callers must own the GUI
 * lock. Descriptors and their names must remain valid for the firmware lifetime.
 */
typedef int (*bepio_application_start_fn)(void *root, void *group);
typedef void (*bepio_application_stop_fn)(void);
typedef bool (*bepio_application_back_fn)(void);
typedef bool (*bepio_application_factory_reset_fn)(void);
typedef void (*bepio_application_ui_fn)(void);
typedef void (*bepio_app_manager_cb_fn)(void);

typedef enum {
	BEPIO_APP_CATEGORY_ROOT,
	BEPIO_APP_CATEGORY_TOOLS,
	BEPIO_APP_CATEGORY_FITNESS,
	BEPIO_APP_CATEGORY_SYSTEM,
	BEPIO_APP_CATEGORY_GAMES,
	BEPIO_APP_CATEGORY_SENSORS,
	BEPIO_APP_CATEGORY_RANDOM,
	BEPIO_APP_CATEGORY_COUNT,
	BEPIO_APP_CATEGORY_INVALID,
} bepio_app_category_t;

typedef enum {
	BEPIO_APP_STATE_STOPPED,
	BEPIO_APP_STATE_UI_VISIBLE,
	BEPIO_APP_STATE_UI_HIDDEN,
} bepio_app_state_t;

typedef struct {
	bepio_application_start_fn start_func;
	bepio_application_stop_fn stop_func;
	bepio_application_back_fn back_func;
	bepio_application_factory_reset_fn factory_reset_func;
	bepio_application_ui_fn ui_unavailable_func;
	bepio_application_ui_fn ui_available_func;
	const char *name;
	const void *icon;
	bool hidden;
	bepio_app_category_t category;
	uint8_t private_list_index;
	bepio_app_state_t current_state;
} bepio_application_t;

/* Registration errors: -EINVAL, -EEXIST, -ENOMEM. Hidden apps remain enumerable.
 * Only one app runs at a time. show returns -EBUSY until it is stopped;
 * requesting the same running app is a no-op. Failed starts call stop_func
 * to release partially initialized state, without invoking close_cb.
 * Lifecycle callbacks may query the manager; lifecycle mutations inside a
 * callback return -EBUSY. close_cb runs after a normal exit, outside the lock.
 */
int bepio_app_manager_add_application(bepio_application_t *app);
int bepio_app_manager_show(bepio_app_manager_cb_fn close_cb, void *root,
			   void *group, const char *app_name);
/* Force stop without back handling or close notification; keeps registration. */
int bepio_app_manager_delete(void);
/* Back may consume the request. No running app is a successful no-op. */
int bepio_app_manager_exit_app(void);
/* Only the currently running app can request its own closure. */
int bepio_app_manager_app_close_request(bepio_application_t *app);
int bepio_app_manager_set_ui_available(bool available);
int bepio_app_manager_get_num_apps(void);
bepio_application_t *bepio_app_manager_get_app(int index);
bepio_app_state_t bepio_app_manager_get_app_state(const bepio_application_t *app);
bool bepio_app_manager_is_app_running(void);
const char *bepio_app_manager_get_current_app_name(void);

#endif
