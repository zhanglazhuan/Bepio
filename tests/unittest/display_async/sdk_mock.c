#include "sdk_mock.h"
#include "bepio_display.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_lcd_panel_vendor.h"
#include "freertos/semphr.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

unsigned mock_isr_yields;
void mock_yield_from_isr(void) { mock_isr_yields++; }
bool mock_pending, mock_auto_complete = true;
int mock_submissions, mock_heap_fail_after = -1, mock_allocations;
esp_err_t mock_submit_error;
int64_t mock_now = 1000;
uint16_t mock_panel[240 * 320];
static mock_done_cb done;
static const uint16_t *borrowed;
static uint16_t snapshot[320 * 16];
static size_t borrowed_count;
static int bx1, by1, bx2, by2;
struct mock_semaphore { bool available; };
int64_t esp_timer_get_time(void) { return mock_now++; }
void *heap_caps_malloc(size_t size, unsigned caps)
{
	assert(caps);
	if (mock_heap_fail_after >= 0 && mock_allocations >= mock_heap_fail_after) { return NULL; }
	void *result = malloc(size); if (result) { mock_allocations++; } return result;
}
void heap_caps_free(void *buffer) { if (buffer) { mock_allocations--; free(buffer); } }
SemaphoreHandle_t xSemaphoreCreateBinary(void) { return calloc(1, sizeof(struct mock_semaphore)); }
BaseType_t xSemaphoreGiveFromISR(SemaphoreHandle_t sem, BaseType_t *wake)
{
	assert(!sem->available); sem->available = true; *wake = pdTRUE; return pdTRUE;
}
void mock_complete(void)
{
	assert(mock_pending);
	/* A later render must not mutate the buffer borrowed by DMA. */
	assert(!memcmp(borrowed, snapshot, borrowed_count * sizeof(uint16_t)));
	for (int y = by1; y < by2; y++) {
		for (int x = bx1; x < bx2; x++) { mock_panel[y * 240 + x] = borrowed[(y - by1) * (bx2 - bx1) + x - bx1]; }
	}
	mock_pending = false;
	mock_now += 1000;
	done(NULL, NULL, NULL);
}
BaseType_t xSemaphoreTake(SemaphoreHandle_t sem, TickType_t ticks)
{
	if (!sem->available && ticks && mock_auto_complete && mock_pending) { mock_complete(); }
	if (sem->available) { sem->available = false; return pdTRUE; }
	mock_now += ticks * 10000;
	return pdFALSE;
}
esp_err_t gpio_set_level(int pin, int level) { (void)pin; (void)level; return ESP_OK; }
esp_err_t gpio_config(const gpio_config_t *config) { (void)config; return ESP_OK; }
esp_err_t spi_bus_initialize(int host, const spi_bus_config_t *config, int dma)
{
	(void)dma; assert(host == SPI2_HOST);
	assert(config->max_transfer_sz == BEPIO_SCREEN_WIDTH * BEPIO_DISPLAY_BUFFER_LINES * 2); return ESP_OK;
}
esp_err_t esp_lcd_new_panel_io_spi(int host, const esp_lcd_panel_io_spi_config_t *config, esp_lcd_panel_io_handle_t *io)
{
	assert(host == SPI2_HOST && config->pclk_hz == 40000000 && config->trans_queue_depth == 1);
	done = config->on_color_trans_done; *io = (void *)1; return ESP_OK;
}
esp_err_t esp_lcd_new_panel_st7789(esp_lcd_panel_io_handle_t io, const esp_lcd_panel_dev_config_t *config, esp_lcd_panel_handle_t *panel)
{
	assert(io && config->bits_per_pixel == 16); *panel = (void *)1; return ESP_OK;
}
esp_err_t esp_lcd_panel_reset(esp_lcd_panel_handle_t panel) { assert(panel); return ESP_OK; }
esp_err_t esp_lcd_panel_init(esp_lcd_panel_handle_t panel) { assert(panel); return ESP_OK; }
esp_err_t esp_lcd_panel_swap_xy(esp_lcd_panel_handle_t panel, bool swap) { assert(panel && !swap); return ESP_OK; }
esp_err_t esp_lcd_panel_mirror(esp_lcd_panel_handle_t panel, bool x, bool y) { assert(panel && !x && !y); return ESP_OK; }
esp_err_t esp_lcd_panel_disp_on_off(esp_lcd_panel_handle_t panel, bool on) { assert(panel && on); return ESP_OK; }
esp_err_t esp_lcd_panel_draw_bitmap(esp_lcd_panel_handle_t panel, int x1, int y1, int x2, int y2, const void *pixels)
{
	assert(panel && !mock_pending);
	if (mock_submit_error) { return mock_submit_error; }
	borrowed_count = (x2 - x1) * (y2 - y1);
	assert(borrowed_count <= sizeof(snapshot) / sizeof(snapshot[0]));
	borrowed = pixels;
	memcpy(snapshot, pixels, borrowed_count * sizeof(uint16_t));
	bx1 = x1; by1 = y1; bx2 = x2; by2 = y2;
	mock_pending = true;
	mock_submissions++;
	return ESP_OK;
}
