#ifndef BEPIO_ORIENTATION_H_
#define BEPIO_ORIENTATION_H_
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "bepio_display.h"
/* Clockwise landscape, inverse of the flush mapping:
 * screen x=319-raw_y, screen y=raw_x. Panel addressing stays native. */
static inline bool bepio_touch_to_screen(uint16_t raw_x, uint16_t raw_y, uint16_t *x, uint16_t *y)
{
	if (!x || !y || raw_x >= BEPIO_DISPLAY_WIDTH || raw_y >= BEPIO_DISPLAY_HEIGHT) { return false; }
	*x = BEPIO_DISPLAY_HEIGHT - 1 - raw_y;
	*y = raw_x;
	return true;
}
/* LVGL logical rectangle rotated into a separate native-panel DMA buffer.
 * Source w*h, destination h*w. Caller owns disjoint buffers of at least w*h. */
static inline void bepio_screen_to_panel_pixels(const uint16_t *src, uint16_t *dst, size_t w, size_t h)
{
	for (size_t y = 0; y < h; y++) {
		for (size_t x = 0; x < w; x++) { dst[(w - 1 - x) * h + y] = src[y * w + x]; }
	}
}
/* Rotate and pack RGB565 into the panel's big-endian wire order in one pass.
 * Buffers must be disjoint. The LVGL source remains unchanged for reuse. */
static inline void bepio_screen_to_panel_wire(const uint16_t *src, uint16_t *dst, size_t w, size_t h)
{
	for (size_t x = 0; x < w; x++) {
		uint16_t *row = dst + (w - 1 - x) * h;
		for (size_t y = 0; y < h; y++) {
			uint16_t pixel = src[y * w + x];
			row[y] = (uint16_t)((pixel >> 8) | (pixel << 8));
		}
	}
}
#endif

