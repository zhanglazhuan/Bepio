#ifdef ESP_PLATFORM
#include "sdkconfig.h"
#endif
#ifndef BEPIO_SETTINGS_MODEL_H_
#define BEPIO_SETTINGS_MODEL_H_

#include <stdbool.h>
#include <stddef.h>

typedef struct {
	bool hour_24;
	bool notifications;
	bool power_save;
	unsigned timezone;
} settings_model_t;

typedef struct { const char *name, *posix; } bepio_settings_timezone_t;
extern const bepio_settings_timezone_t bepio_settings_timezones[];
extern const size_t bepio_settings_timezone_count;
/* RAM-only preferences. Opening/closing an app preserves them; reboot resets. */
void settings_model_reset(settings_model_t *model);
int settings_model_set(settings_model_t *model, const char *key, bool value);

#endif
