/* Bottom-edge exit confirmation adapted from DeepStoa launcher_gesture.
 * SPDX-License-Identifier: GPL-3.0-only */
#ifndef BEPIO_NAVIGATION_H_
#define BEPIO_NAVIGATION_H_
#include "lvgl.h"
/* Called after explicit confirmation, asynchronously on the LVGL thread. */
void bepio_navigation_set_exit_handler(void (*handler)(void), const char *app_name);
void bepio_navigation_attach_input(lv_indev_t *input);
#endif
