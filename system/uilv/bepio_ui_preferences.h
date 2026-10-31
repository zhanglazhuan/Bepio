#ifndef BEPIO_UI_PREFERENCES_H_
#define BEPIO_UI_PREFERENCES_H_
#include <stdbool.h>
/* LVGL owner thread only. First touch wakes the screen without activating controls. */
void bepio_ui_preferences_init(void);
void bepio_ui_preferences_apply(const char *timezone, bool hour_24, bool power_save);
bool bepio_ui_preferences_touch(bool pressed);
#endif
