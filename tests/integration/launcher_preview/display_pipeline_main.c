#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <string.h>
#include "lvgl.h"
#include "bepio_lcd.h"
#include "bepio_ui_display.h"
#include "sdk_mock.h"

static jmp_buf fatal_return;
static esp_err_t failure;
static unsigned overlapped_renders;
static void fatal(esp_err_t error) { failure = error; longjmp(fatal_return, 1); }
static void render_event(lv_event_t *event)
{
	(void)event;
	if (mock_pending) { overlapped_renders++; }
	mock_now += 500; /* Deterministic simulated CPU work, with DMA in flight. */
}
int main(int argc, char **argv)
{
	assert(argc == 2);
	lv_init();
	lv_display_t *display = lv_display_create(320, 240); assert(display);
	assert(bepio_lcd_init() == ESP_OK);
	if (!strcmp(argv[1], "oom")) {
		mock_heap_fail_after = 1;
		assert(bepio_ui_display_init(display, fatal) == ESP_ERR_NO_MEM);
		assert(mock_allocations == 0 && mock_submissions == 0);
		puts("PASS pipeline partial allocation cleanup"); return 0;
	}
	assert(bepio_ui_display_init(display, fatal) == ESP_OK);
	assert(lv_display_is_double_buffered(display));
	assert(mock_allocations == 3);
	lv_obj_add_event_cb(lv_screen_active(), render_event, LV_EVENT_DRAW_MAIN, NULL);
	lv_obj_set_style_bg_opa(lv_screen_active(), LV_OPA_COVER, 0);
	lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0xff0000), 0);
	if (!strcmp(argv[1], "submit-error")) {
		mock_submit_error = ESP_FAIL;
		if (!setjmp(fatal_return)) { lv_refr_now(display); assert(0); }
		assert(failure == ESP_FAIL && mock_allocations == 3);
		assert(bepio_ui_display_poll(false) == ESP_FAIL);
		puts("PASS pipeline submit failure latches and retains DMA memory"); return 0;
	}
	lv_refr_now(display);
	assert(mock_pending);
	assert(overlapped_renders > 0);
	assert(mock_submissions >= 15);
	assert(bepio_ui_display_poll(false) == ESP_OK && mock_pending);
	if (!strcmp(argv[1], "wait-timeout")) {
		mock_auto_complete = false;
		lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0x0000ff), 0);
		if (!setjmp(fatal_return)) { lv_refr_now(display); assert(0); }
		assert(failure == ESP_ERR_TIMEOUT && mock_allocations == 3);
		mock_complete();
		assert(bepio_ui_display_poll(false) == ESP_ERR_TIMEOUT);
		puts("PASS pipeline flush-wait timeout stops owner and retains late DMA buffer"); return 0;
	}
	if (!strcmp(argv[1], "timeout")) {
		mock_auto_complete = false;
		mock_now += BEPIO_UI_DMA_TIMEOUT_MS * 1000LL + 1;
		assert(bepio_ui_display_poll(false) == ESP_ERR_TIMEOUT);
		assert(mock_allocations == 3);
		mock_complete();
		assert(bepio_ui_display_poll(true) == ESP_ERR_TIMEOUT);
		puts("PASS pipeline stalled/late DMA retains memory and stays faulted"); return 0;
	}
	/* Start another frame before the previous frame's final DMA is reaped.
	 * The previous transfer must retain its borrowed wire pixels. */
	lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0x0000ff), 0);
	lv_refr_now(display);
	assert(mock_pending);
	mock_complete();
	assert(bepio_ui_display_poll(false) == ESP_OK && !mock_pending);
	for (int i = 0; i < 240 * 320; i++) { assert(mock_panel[i] == 0x1f00); } /* Blue RGB565 in wire order. */
	assert(bepio_ui_display_poll(true) == ESP_OK);
	puts("PASS actual LVGL + LCD pipeline: render/DMA overlap, wire ownership, rotated output, completion, polling");
}
