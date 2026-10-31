/* Global top-layer status bar, following DeepStoa. SPDX-License-Identifier: GPL-3.0-only */
#ifndef BEPIO_LV_STATUS_BAR_H_
#define BEPIO_LV_STATUS_BAR_H_
#include "lvgl.h"
#define BEPIO_STATUS_BAR_HEIGHT 20
void bepio_status_bar_init(void);
/* LVGL owner thread only. Updates only the clock. */
void bepio_status_bar_set_time(const char *text);
/* NULL text and negative battery mean unknown.
 * Network text must describe verified state, not a fabricated radio signal. */
void bepio_status_bar_update(const char *time_text, const char *network_text, int battery_percent, bool charging);
void bepio_status_bar_set_visible(bool visible);
#endif
