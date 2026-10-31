#ifndef BEPIO_LCD_H_
#define BEPIO_LCD_H_
#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>
/* Single task owns submit/wait. At most one tile is in flight.
 * draw_async borrows DMA-capable pixels until wait_done returns ESP_OK.
 * A failed submit or bounded wait poisons the driver: retain the DMA buffer
 * and stop drawing until reset. No callback may touch LVGL from the ISR. */
esp_err_t bepio_lcd_init(void);
esp_err_t bepio_lcd_draw_async(int x1, int y1, int x2, int y2, const void *pixels);
/* timeout_ms=0 polls: ESP_ERR_NOT_FINISHED leaves the transfer in flight.
 * Nonzero timeout returns ESP_ERR_TIMEOUT and poisons the driver on expiry. */
esp_err_t bepio_lcd_wait_done(uint32_t timeout_ms);
/* Synchronous compatibility wrapper, with a bounded 2-second wait. */
esp_err_t bepio_lcd_draw(int x1, int y1, int x2, int y2, const void *pixels);
esp_err_t bepio_lcd_backlight(bool on);
#endif
