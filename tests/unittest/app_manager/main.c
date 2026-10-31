#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "bepio_app_manager.h"
#include "apps/settings/app.h"

int bepio_settings_register(void);

static int starts;
static int stops;
static int closes;
static int hidden;
static int visible;
static int start_result;
static bool consume_back;
static bool reenter;
static void *seen_root;
static void *seen_group;

static int start(void *root, void *group)
{
	starts++;
	seen_root = root;
	seen_group = group;
	if (reenter) {
		assert(bepio_app_manager_delete() == -EBUSY);
		assert(bepio_app_manager_show(NULL, NULL, NULL, "Alpha") == -EBUSY);
	}
	return start_result;
}

static void stop(void) { stops++; }
static bool back(void) { return consume_back; }
static void closed(void) { closes++; }
static void ui_hidden(void) { hidden++; }
static void ui_visible(void) { visible++; }
static void open_settings_on_close(void)
{
	closes++;
	assert(bepio_app_manager_show(NULL, NULL, NULL, "Settings") == 0);
}

static bepio_application_t alpha = {
	.name = "Alpha", .start_func = start, .stop_func = stop, .back_func = back,
	.category = BEPIO_APP_CATEGORY_TOOLS,
	.ui_unavailable_func = ui_hidden, .ui_available_func = ui_visible,
};
static bepio_application_t beta = {
	.name = "Beta", .start_func = start, .stop_func = stop,
	.category = BEPIO_APP_CATEGORY_SYSTEM, .hidden = true,
};

static void test_registration(void)
{
	bepio_application_t invalid = { .name = "invalid" };
	bepio_application_t duplicate = alpha;

	assert(!bepio_app_manager_is_app_running());
	assert(bepio_app_manager_get_current_app_name() == NULL);
	assert(bepio_app_manager_exit_app() == 0);
	assert(bepio_app_manager_delete() == 0);
	assert(bepio_app_manager_get_app_state(NULL) == BEPIO_APP_STATE_STOPPED);
	assert(bepio_app_manager_add_application(NULL) == -EINVAL);
	assert(bepio_app_manager_add_application(&invalid) == -EINVAL);
	assert(bepio_settings_register() == 0);
	assert(bepio_app_manager_add_application(&alpha) == 0);
	assert(bepio_app_manager_add_application(&beta) == 0);
	assert(bepio_app_manager_add_application(&alpha) == -EEXIST);
	assert(bepio_app_manager_add_application(&duplicate) == -EEXIST);
	assert(bepio_app_manager_get_num_apps() == 3);
	assert(bepio_app_manager_get_app(-1) == NULL);
	assert(bepio_app_manager_get_app(3) == NULL);
	assert(bepio_app_manager_get_app(1) == &alpha);
	assert(alpha.private_list_index == 1);
	assert(beta.private_list_index == UINT8_MAX);
}

static void test_lifecycle(void)
{
	int root, group;

	assert(bepio_app_manager_show(NULL, NULL, NULL, NULL) == -EINVAL);
	assert(bepio_app_manager_show(NULL, NULL, NULL, "missing") == -ENOENT);
	reenter = true;
	assert(bepio_app_manager_show(closed, &root, &group, "Alpha") == 0);
	reenter = false;
	assert(starts == 1 && seen_root == &root && seen_group == &group);
	assert(strcmp(bepio_app_manager_get_current_app_name(), "Alpha") == 0);
	assert(bepio_app_manager_show(NULL, NULL, NULL, "Beta") == -EBUSY);
	assert(bepio_app_manager_show(NULL, NULL, NULL, "missing") == -ENOENT);
	assert(bepio_app_manager_show(NULL, NULL, NULL, "Alpha") == 0);
	assert(starts == 1);
	assert(bepio_app_manager_app_close_request(&beta) == -ENOENT);
	assert(settings_app_close_request() == -ENOENT);
	assert(bepio_app_manager_app_close_request(NULL) == -EINVAL);
	assert(bepio_app_manager_set_ui_available(false) == 0);
	assert(bepio_app_manager_set_ui_available(false) == 0);
	assert(hidden == 1);
	assert(bepio_app_manager_get_app_state(&alpha) == BEPIO_APP_STATE_UI_HIDDEN);
	assert(bepio_app_manager_set_ui_available(true) == 0);
	assert(visible == 1);
	consume_back = true;
	assert(bepio_app_manager_exit_app() == 0);
	assert(stops == 0 && closes == 0);
	assert(bepio_app_manager_is_app_running());
	consume_back = false;
	assert(bepio_app_manager_app_close_request(&alpha) == 0);
	assert(stops == 1 && closes == 1);
	assert(bepio_app_manager_get_app_state(&alpha) == BEPIO_APP_STATE_STOPPED);
	assert(!bepio_app_manager_is_app_running());
	assert(bepio_app_manager_set_ui_available(true) == -ENOENT);
	assert(bepio_app_manager_exit_app() == 0);
	assert(closes == 1);

	start_result = -EIO;
	assert(bepio_app_manager_show(closed, NULL, NULL, "Alpha") == -EIO);
	assert(stops == 2 && closes == 1);
	assert(!bepio_app_manager_is_app_running());
	start_result = 0;
	assert(bepio_app_manager_show(closed, NULL, NULL, "Alpha") == 0);
	assert(bepio_app_manager_delete() == 0);
	assert(stops == 3 && closes == 1);
	assert(bepio_app_manager_show(open_settings_on_close, NULL, NULL, "Alpha") == 0);
	assert(bepio_app_manager_exit_app() == 0);
	assert(strcmp(bepio_app_manager_get_current_app_name(), "Settings") == 0);
	assert(closes == 2);
}

static void test_settings(void)
{
	settings_controller_t *controller = settings_app_get_controller();
	settings_snapshot_t snapshot;
	bepio_application_t *app = bepio_app_manager_get_app(0);

	assert(settings_controller_snapshot(controller, &snapshot) == 0);
	assert(snapshot.preferences.hour_24 && snapshot.preferences.notifications);
	assert(!snapshot.preferences.power_save && snapshot.preferences.timezone == 0);
	assert(settings_controller_set(controller, "hour_24", false) == 0);
	assert(settings_controller_set(controller, "notifications", false) == 0);
	assert(settings_controller_set(controller, "unknown", true) == -ENOENT);
	assert(settings_controller_set(controller, NULL, true) == -EINVAL);
	assert(settings_controller_snapshot(controller, NULL) == -EINVAL);
	assert(settings_controller_set(controller, "power_save", true) == 0);
	assert(bepio_settings_controller_timezone(controller, 3) == 0);
	assert(bepio_settings_controller_timezone(controller, (unsigned)bepio_settings_timezone_count) == -EINVAL);
	assert(bepio_settings_controller_page(controller, SETTINGS_PAGE_DATETIME) == 0);
	assert(settings_controller_snapshot(controller, &snapshot) == 0);
	assert(snapshot.page == SETTINGS_PAGE_DATETIME && snapshot.preferences.timezone == 3 && snapshot.preferences.power_save);
	assert(bepio_settings_controller_page(controller, (settings_page_t)99) == -EINVAL);
	assert(settings_controller_about(controller) == 0);
	assert(settings_app_close_request() == 0);
	assert(bepio_app_manager_is_app_running());
	assert(settings_controller_snapshot(controller, &snapshot) == 0);
	assert(snapshot.page == SETTINGS_PAGE_HOME);
	assert(bepio_app_manager_set_ui_available(false) == 0);
	assert(settings_controller_snapshot(controller, &snapshot) == 0);
	assert(!snapshot.available);
	assert(bepio_app_manager_set_ui_available(true) == 0);
	assert(bepio_app_manager_exit_app() == 0);
	assert(!bepio_app_manager_is_app_running());
	assert(settings_controller_set(controller, "hour_24", true) == -EACCES);
	assert(settings_controller_snapshot(controller, &snapshot) == -EACCES);
	assert(settings_controller_about(controller) == -EACCES);
	assert(bepio_app_manager_show(NULL, NULL, NULL, "Settings") == 0);
	assert(settings_controller_snapshot(controller, &snapshot) == 0);
	assert(!snapshot.preferences.hour_24 && !snapshot.preferences.notifications);
	assert(app->factory_reset_func());
	assert(settings_controller_snapshot(controller, &snapshot) == 0);
	assert(snapshot.preferences.hour_24 && snapshot.preferences.notifications);
	assert(!snapshot.preferences.power_save && snapshot.preferences.timezone == 0);
	assert(bepio_app_manager_delete() == 0);
}

static void test_capacity(void)
{
	static bepio_application_t extra[6];
	static const char *const names[] = {"A", "B", "C", "D", "E", "F"};

	for (int i = 0; i < 6; i++) {
		extra[i] = alpha;
		extra[i].name = names[i];
		assert(bepio_app_manager_add_application(&extra[i]) == (i < 5 ? 0 : -ENOMEM));
	}
	assert(bepio_app_manager_get_num_apps() == 8);
}

int main(void)
{
	test_registration();
	test_lifecycle();
	test_settings();
	test_capacity();
	puts("PASS: registration, lifecycle, failure cleanup, Settings, capacity");
	return 0;
}
