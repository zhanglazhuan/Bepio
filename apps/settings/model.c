#include "model.h"
#include <errno.h>
#include <string.h>

const bepio_settings_timezone_t bepio_settings_timezones[] = {
	{"Asia/Shanghai (UTC+8)", "CST-8"},
	{"UTC", "UTC0"},
	{"Asia/Tokyo (UTC+9)", "JST-9"},
	{"Asia/Kolkata (UTC+5:30)", "IST-5:30"},
	{"Europe/London", "GMT0BST,M3.5.0/1,M10.5.0/2"},
	{"Europe/Berlin", "CET-1CEST,M3.5.0/2,M10.5.0/3"},
	{"America/New_York", "EST5EDT,M3.2.0/2,M11.1.0/2"},
	{"America/Los_Angeles", "PST8PDT,M3.2.0/2,M11.1.0/2"},
	{"Australia/Sydney", "AEST-10AEDT,M10.1.0/2,M4.1.0/3"},
};
const size_t bepio_settings_timezone_count = sizeof(bepio_settings_timezones) / sizeof(bepio_settings_timezones[0]);

void settings_model_reset(settings_model_t *model)
{
	*model = (settings_model_t) {
		.hour_24 = true,
		.notifications = true,
	};
}

int settings_model_set(settings_model_t *model, const char *key, bool value)
{
	if (!model || !key) {
		return -EINVAL;
	}
	if (strcmp(key, "hour_24") == 0) {
		model->hour_24 = value;
	} else if (strcmp(key, "notifications") == 0) {
		model->notifications = value;
	} else if (strcmp(key, "power_save") == 0) {
		model->power_save = value;
	} else {
		return -ENOENT;
	}
	return 0;
}
