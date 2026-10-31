#include "bepio_ui_display.h"
#include "bepio_lcd.h"
#include "bepio_orientation.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"

static struct {
	lv_display_t *display;
	uint16_t *wire;
	void (*fatal)(esp_err_t);
	bool pending;
	esp_err_t fault;
	int64_t submitted_at;
} ui_display;

esp_err_t bepio_ui_display_poll(bool wait)
{
	if (ui_display.fault != ESP_OK) { return ui_display.fault; }
	if (!ui_display.pending) { return ESP_OK; }
	int64_t before = esp_timer_get_time();
	esp_err_t ret = bepio_lcd_wait_done(wait ? BEPIO_UI_DMA_TIMEOUT_MS : 0);
	if (ret == ESP_ERR_NOT_FINISHED) {
		if (before - ui_display.submitted_at < BEPIO_UI_DMA_TIMEOUT_MS * 1000LL) { return ESP_OK; }
		ret = ESP_ERR_TIMEOUT;
	}
	if (ret != ESP_OK) { ui_display.fault = ret; return ret; }
	ui_display.pending = false;
	/* Called only by the GUI owner, never from the SPI ISR. */
	lv_display_flush_ready(ui_display.display);
	return ESP_OK;
}

static void failed(esp_err_t ret)
{
	ui_display.fault = ret;
	ui_display.fatal(ret);
}

static void flush_wait(lv_display_t *display)
{
	(void)display;
	esp_err_t ret = bepio_ui_display_poll(true);
	if (ret != ESP_OK) { failed(ret); }
}

static void flush(lv_display_t *display, const lv_area_t *area, uint8_t *pixels)
{
	if (ui_display.fault != ESP_OK) { failed(ui_display.fault); return; }
	/* LVGL waits before reusing the shared wire buffer, while the second draw
	 * buffer can be rendered during the previous DMA transfer. */
	if (ui_display.pending) { failed(ESP_ERR_INVALID_STATE); return; }
	size_t count = lv_area_get_size(area);
	if (count > BEPIO_SCREEN_WIDTH * BEPIO_DISPLAY_BUFFER_LINES) { failed(ESP_ERR_INVALID_SIZE); return; }
	bepio_screen_to_panel_wire((const uint16_t *)pixels, ui_display.wire,
		lv_area_get_width(area), lv_area_get_height(area));
	(void)display;
	ui_display.submitted_at = esp_timer_get_time();
	ui_display.pending = true;
	esp_err_t ret = bepio_lcd_draw_async(area->y1, BEPIO_DISPLAY_HEIGHT - 1 - area->x2,
		area->y2 + 1, BEPIO_DISPLAY_HEIGHT - area->x1, ui_display.wire);
	if (ret != ESP_OK) { failed(ret); return; }
}

esp_err_t bepio_ui_display_init(lv_display_t *display, void (*fatal)(esp_err_t))
{
	if (!display || !fatal || ui_display.display) { return ESP_ERR_INVALID_STATE; }
	const size_t bytes = BEPIO_SCREEN_WIDTH * BEPIO_DISPLAY_BUFFER_LINES * sizeof(uint16_t);
	const unsigned caps = MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL;
	void *first = heap_caps_malloc(bytes, caps);
	void *second = heap_caps_malloc(bytes, caps);
	uint16_t *wire = heap_caps_malloc(bytes, caps);
	if (!first || !second || !wire) {
		heap_caps_free(first); heap_caps_free(second); heap_caps_free(wire);
		return ESP_ERR_NO_MEM;
	}
	ui_display.display = display;
	ui_display.wire = wire;
	ui_display.fatal = fatal;
	lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
	lv_display_set_buffers(display, first, second, bytes, LV_DISPLAY_RENDER_MODE_PARTIAL);
	lv_display_set_flush_cb(display, flush);
	lv_display_set_flush_wait_cb(display, flush_wait);
	lv_timer_set_period(lv_display_get_refr_timer(display), BEPIO_UI_REFRESH_MS);
	return ESP_OK;
}
