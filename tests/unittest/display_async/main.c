#include "bepio_lcd.h"
#include "sdk_mock.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(int argc, char **argv)
{
	assert(argc == 2);
	uint16_t pixels[320 * 16] = {0};
	assert(bepio_lcd_draw_async(0, 0, 1, 1, pixels) == ESP_ERR_INVALID_STATE);
	assert(bepio_lcd_init() == ESP_OK);
	assert(bepio_lcd_init() == ESP_ERR_INVALID_STATE);
	assert(bepio_lcd_wait_done(0) == ESP_ERR_INVALID_STATE);
	assert(bepio_lcd_draw_async(0, 0, 240, 320, pixels) == ESP_ERR_INVALID_ARG);
	assert(bepio_lcd_draw_async(0, 0, 1, 1, NULL) == ESP_ERR_INVALID_ARG);
	assert(bepio_lcd_draw_async(-1, 0, 1, 1, pixels) == ESP_ERR_INVALID_ARG);
	assert(bepio_lcd_draw_async(0, 0, 241, 1, pixels) == ESP_ERR_INVALID_ARG);
	if (!strcmp(argv[1], "submit-error")) {
		mock_submit_error = ESP_FAIL;
		assert(bepio_lcd_draw_async(0, 0, 16, 320, pixels) == ESP_FAIL);
		assert(bepio_lcd_draw_async(0, 0, 1, 1, pixels) == ESP_ERR_INVALID_STATE);
	} else {
		assert(bepio_lcd_draw_async(0, 0, 16, 320, pixels) == ESP_OK);
		assert(mock_pending);
		assert(bepio_lcd_draw_async(0, 0, 16, 320, pixels) == ESP_ERR_INVALID_STATE);
		assert(bepio_lcd_wait_done(0) == ESP_ERR_NOT_FINISHED);
		if (!strcmp(argv[1], "timeout")) {
			mock_auto_complete = false;
			assert(bepio_lcd_wait_done(1) == ESP_ERR_TIMEOUT);
			mock_complete(); /* Late ISR must not make a poisoned driver reusable. */
			assert(bepio_lcd_wait_done(200) == ESP_ERR_INVALID_STATE);
			assert(bepio_lcd_draw_async(0, 0, 1, 1, pixels) == ESP_ERR_INVALID_STATE);
		} else {
			mock_complete();
			assert(bepio_lcd_wait_done(0) == ESP_OK);
			assert(mock_isr_yields > 0);
			assert(bepio_lcd_wait_done(0) == ESP_ERR_INVALID_STATE);
			assert(bepio_lcd_draw_async(0, 0, 16, 320, pixels) == ESP_OK);
			assert(bepio_lcd_wait_done(0) == ESP_ERR_NOT_FINISHED); /* No stale completion token. */
			assert(bepio_lcd_wait_done(200) == ESP_OK);
			assert(bepio_lcd_draw(0, 0, 16, 320, pixels) == ESP_OK);
		}
	}
	printf("PASS LCD async ownership, completion/error/late IRQ: %s\n", argv[1]);
}
