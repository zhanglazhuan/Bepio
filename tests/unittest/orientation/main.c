#include <assert.h>
#include <stdio.h>
#include "bepio_orientation.h"
int main(void)
{
	uint16_t x, y;
	for (unsigned ry = 0; ry < 320; ry++) {
		for (unsigned rx = 0; rx < 240; rx++) {
			assert(bepio_touch_to_screen(rx, ry, &x, &y));
			assert(x == 319 - ry && y == rx);
			assert(y == rx && 319u - x == ry);
		}
	}
	assert(!bepio_touch_to_screen(240, 0, &x, &y));
	assert(!bepio_touch_to_screen(0, 320, &x, &y));
	assert(!bepio_touch_to_screen(0, 0, NULL, &y));
	uint16_t src[48], dst[48];
	for (unsigned i = 0; i < 48; i++) { src[i] = i; }
	bepio_screen_to_panel_pixels(src, dst, 8, 6);
	/* A flush tile at logical (70,30) starts at panel (30,242).
	 * Every panel pixel must map back to its original logical pixel. */
	for (unsigned row = 0; row < 8; row++) {
		for (unsigned col = 0; col < 6; col++) {
			assert(bepio_touch_to_screen(30 + col, 242 + row, &x, &y));
			assert(dst[row * 6 + col] == src[(y - 30) * 8 + x - 70]);
		}
	}
	/* Fused conversion must equal reference rotation followed by byte swap,
	 * including odd/tall/narrow rectangles and nontrivial pixel high bytes. */
	uint16_t original[320 * 16], rotated[320 * 16], wire[320 * 16];
	for (size_t i = 0; i < 320 * 16; i++) { original[i] = (uint16_t)(i * 19 + 0x1234); }
	const size_t sizes[][2] = {{320,16}, {17,7}, {1,31}, {73,1}, {3,5}};
	for (size_t k = 0; k < sizeof(sizes) / sizeof(sizes[0]); k++) {
		size_t count = sizes[k][0] * sizes[k][1];
		bepio_screen_to_panel_pixels(original, rotated, sizes[k][0], sizes[k][1]);
		bepio_screen_to_panel_wire(original, wire, sizes[k][0], sizes[k][1]);
		for (size_t i = 0; i < count; i++) {
			assert(wire[i] == (uint16_t)((rotated[i] >> 8) | (rotated[i] << 8)));
			assert(original[i] == (uint16_t)(i * 19 + 0x1234));
		}
	}
	puts("PASS all touch coordinates, bounds, and rotated flush/touch inverse and fused RGB565 wire packing");
}

