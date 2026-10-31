#ifndef BEPIO_UI_H_
#define BEPIO_UI_H_
#include "esp_err.h"
/* Build first screen before starting the sole LVGL task; no concurrent GUI calls. */
esp_err_t bepio_ui_start(void (*build_screen)(void));
#endif
