#include "app.h"
#include "bepio_app_manager.h"
#include <errno.h>
#include <string.h>
#include "bepio_console.h"

static int settings_open(void)
{
	return bepio_app_manager_show(NULL, NULL, NULL, BEPIO_SETTINGS_APP_NAME);
}

static int show_settings(const bepio_console_context_t *shell, size_t argc, char **argv)
{
	settings_snapshot_t snapshot;
	int ret = settings_open();

	BEPIO_UNUSED(argc);
	BEPIO_UNUSED(argv);
	if (ret == 0) {
		ret = settings_controller_snapshot(settings_app_get_controller(), &snapshot);
	}
	if (ret < 0) {
		bepio_console_error(shell, "Cannot open Settings (%d)", ret);
		return ret;
	}
	bepio_console_print(shell, "Settings (%s, %s): hour_24=%d notifications=%d [RAM-only]",
		    snapshot.page == SETTINGS_PAGE_HOME ? "home" : "about",
		    snapshot.available ? "visible" : "hidden",
		    snapshot.preferences.hour_24, snapshot.preferences.notifications);
	return 0;
}

static int set_setting(const bepio_console_context_t *shell, size_t argc, char **argv)
{
	int ret;

	BEPIO_UNUSED(argc);
	if (strcmp(argv[2], "0") != 0 && strcmp(argv[2], "1") != 0) {
		bepio_console_error(shell, "Value must be 0 or 1");
		return -EINVAL;
	}
	ret = settings_open();
	if (ret == 0) {
		ret = settings_controller_set(settings_app_get_controller(), argv[1],
					      argv[2][0] == '1');
	}
	if (ret < 0) {
		bepio_console_error(shell, "Setting rejected (%d); keys: hour_24, notifications", ret);
		return ret;
	}
	return show_settings(shell, 0, NULL);
}

static int about_settings(const bepio_console_context_t *shell, size_t argc, char **argv)
{
	int ret = settings_open();

	BEPIO_UNUSED(argc);
	BEPIO_UNUSED(argv);
	if (ret == 0) {
		ret = settings_controller_about(settings_app_get_controller());
	}
	if (ret == 0) {
		bepio_console_print(shell, "Bepio / ESP-IDF app framework. Preferences are RAM-only.");
	} else {
		bepio_console_error(shell, "Cannot open About (%d)", ret);
	}
	return ret;
}

static int reset_settings(const bepio_console_context_t *shell, size_t argc, char **argv)
{
	int ret = settings_open();

	if (ret < 0) {
		return ret;
	}
	settings_controller_reset(settings_app_get_controller());
	return show_settings(shell, argc, argv);
}

static int back_settings(const bepio_console_context_t *shell, size_t argc, char **argv)
{
	int ret;

	BEPIO_UNUSED(argc);
	BEPIO_UNUSED(argv);
	ret = settings_app_close_request();
	if (ret < 0) {
		bepio_console_error(shell, "Settings is not running");
		return ret;
	}
	if (ret == 0) {
		bepio_console_print(shell, "Settings %s",
			    bepio_app_manager_is_app_running() ? "home" : "closed");
	}
	return ret;
}

BEPIO_COMMANDS(settings_commands,
	BEPIO_COMMAND(show, NULL, "Open and show preferences", show_settings, 1, 0),
	BEPIO_COMMAND(set, NULL, "Set <hour_24|notifications> <0|1> (RAM only)", set_setting, 3, 0),
	BEPIO_COMMAND(about, NULL, "Open About", about_settings, 1, 0),
	BEPIO_COMMAND(defaults, NULL, "Restore RAM preferences", reset_settings, 1, 0),
	BEPIO_COMMAND(back, NULL, "Back; close when on home", back_settings, 1, 0),
	BEPIO_COMMAND_END
);
BEPIO_COMMAND_GROUP(settings, settings_commands, "Settings app", NULL);
