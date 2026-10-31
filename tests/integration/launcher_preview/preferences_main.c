#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "lvgl.h"
#include "bepio_ui_preferences.h"
#include "compat.h"
static bool light = true;
static int failure;
static char clock_text[32];
int bepio_lcd_backlight(bool on)
{
	if (failure) { return -1; }
	light = on;
	return 0;
}
void bepio_status_bar_set_time(const char *text) { snprintf(clock_text, sizeof(clock_text), "%s", text); }
int bepio_test_setenv(const char *name, const char *value, int overwrite)
{
	(void)overwrite; return _putenv_s(name, value);
}
struct tm *bepio_test_localtime_r(const time_t *value, struct tm *result)
{
	return localtime_s(result, value) == 0 ? result : NULL;
}
int main(void)
{
	lv_init(); lv_display_create(320, 240);
	bepio_ui_preferences_init();
	bepio_ui_preferences_apply("UTC0", true, true);
	assert(light && strlen(clock_text) == 5);
	lv_tick_inc(29000); lv_timer_handler(); assert(light);
	lv_tick_inc(1100); lv_timer_handler(); assert(!light);
	assert(bepio_ui_preferences_touch(true) && light);
	assert(bepio_ui_preferences_touch(true));
	assert(!bepio_ui_preferences_touch(false));
	assert(!bepio_ui_preferences_touch(true));
	bepio_ui_preferences_apply("UTC0", false, false);
	assert(strstr(clock_text, "AM") || strstr(clock_text, "PM"));
	lv_tick_inc(31000); lv_timer_handler(); assert(light);
	bepio_ui_preferences_apply("UTC0", true, true); assert(!light);
	bepio_ui_preferences_apply("UTC0", true, false); assert(light);
	failure = 1;
	bepio_ui_preferences_apply("UTC0", true, true); assert(light);
	failure = 0;
	lv_tick_inc(300); lv_timer_handler(); assert(!light);
	assert(bepio_ui_preferences_touch(true) && light);
	puts("PASS actual UI preference runtime: 12/24h clock, 30s idle, wake touch suppression, disable and backlight failure");
}
