#include "bepio_ui_preferences.h"
#include "lv_status_bar.h"
#include "bepio_lcd.h"
#include "lvgl.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
static bool format_24 = true, saving, sleeping, waking;
static char zone[64] = "CST-8";
static void refresh(lv_timer_t *timer)
{
	(void)timer;
	time_t now = time(NULL);
	struct tm local;
	char text[24] = "--:--";
	if (now >= 1704067200 && localtime_r(&now, &local)) {
		strftime(text, sizeof(text), format_24 ? "%H:%M" : "%I:%M %p", &local);
	}
	bepio_status_bar_set_time(text);
	if (saving && !sleeping && lv_display_get_inactive_time(NULL) >= 30000) {
		if (bepio_lcd_backlight(false) == ESP_OK) { sleeping = true; }
	}
}
void bepio_ui_preferences_init(void)
{
	setenv("TZ", zone, 1); tzset();
	lv_timer_create(refresh, 250, NULL);
}
void bepio_ui_preferences_apply(const char *timezone, bool hour_24, bool power_save)
{
	if (strcmp(zone, timezone)) {
		snprintf(zone, sizeof(zone), "%s", timezone);
		setenv("TZ", zone, 1); tzset();
	}
	format_24 = hour_24; saving = power_save;
	if (!saving && sleeping && bepio_lcd_backlight(true) == ESP_OK) { sleeping = false; }
	refresh(NULL);
}
bool bepio_ui_preferences_touch(bool pressed)
{
	if (!pressed) { waking = false; return false; }
	if (sleeping) {
		if (bepio_lcd_backlight(true) == ESP_OK) { sleeping = false; }
		waking = true;
	}
	if (waking) { lv_display_trigger_activity(NULL); }
	return waking;
}
