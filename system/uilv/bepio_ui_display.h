#ifndef BEPIO_UI_DISPLAY_H_
#define BEPIO_UI_DISPLAY_H_
#include "esp_err.h"
#include "lvgl.h"
#include <stdint.h>
#define BEPIO_UI_REFRESH_MS 20
#define BEPIO_UI_DMA_TIMEOUT_MS 200

/* Sole GUI task owns all calls. LCD must already be initialized.
 * Failure callback must stop the GUI owner; DMA buffers are retained until reset. */
esp_err_t bepio_ui_display_init(lv_display_t *display, void (*fatal)(esp_err_t));
/* Drain with a bounded wait at boot; nonblocking completion polling in the GUI loop. */
esp_err_t bepio_ui_display_poll(bool wait);
#endif
