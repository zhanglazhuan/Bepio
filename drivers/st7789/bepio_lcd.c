#include "bepio_lcd.h"
#include "bepio_display.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static esp_lcd_panel_handle_t panel;
static esp_lcd_panel_io_handle_t io;
static SemaphoreHandle_t transferred;
static bool attempted, fault, pending;

static bool on_done(esp_lcd_panel_io_handle_t handle, esp_lcd_panel_io_event_data_t *event, void *arg)
{
	(void)handle; (void)event; (void)arg;
	BaseType_t wake = pdFALSE;
	xSemaphoreGiveFromISR(transferred, &wake);
	/* IDF 5.5.3's SPI post callback ignores the LCD callback's bool return.
	 * Request the switch explicitly, or a GUI waiting on this semaphore can
	 * remain asleep until the next scheduler tick despite DMA being done. */
	if (wake == pdTRUE) { portYIELD_FROM_ISR(); }
	return wake == pdTRUE;
}

esp_err_t bepio_lcd_backlight(bool on)
{
	return gpio_set_level(BEPIO_DISPLAY_LED_GPIO, on);
}

esp_err_t bepio_lcd_init(void)
{
	if (attempted) { return ESP_ERR_INVALID_STATE; }
	attempted = true;
	transferred = xSemaphoreCreateBinary();
	if (!transferred) { return ESP_ERR_NO_MEM; }
	esp_err_t ret = gpio_set_level(BEPIO_DISPLAY_LED_GPIO, 0);
	gpio_config_t led = {.pin_bit_mask = 1ULL << BEPIO_DISPLAY_LED_GPIO, .mode = GPIO_MODE_OUTPUT};
	if (ret == ESP_OK) { ret = gpio_config(&led); }
	if (ret != ESP_OK) { return ret; }
	spi_bus_config_t bus = {.sclk_io_num = BEPIO_DISPLAY_SCK_GPIO,
		.mosi_io_num = BEPIO_DISPLAY_MOSI_GPIO, .miso_io_num = BEPIO_DISPLAY_MISO_GPIO,
		.quadwp_io_num = -1, .quadhd_io_num = -1,
		.max_transfer_sz = BEPIO_SCREEN_WIDTH * BEPIO_DISPLAY_BUFFER_LINES * 2};
	ret = spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO);
	if (ret != ESP_OK) { return ret; }
	esp_lcd_panel_io_spi_config_t config = {.dc_gpio_num = BEPIO_DISPLAY_DC_GPIO,
		.cs_gpio_num = BEPIO_DISPLAY_CS_GPIO, .pclk_hz = BEPIO_DISPLAY_SPI_HZ, .spi_mode = 0,
		.lcd_cmd_bits = 8, .lcd_param_bits = 8, .trans_queue_depth = 1, .on_color_trans_done = on_done};
	ret = esp_lcd_new_panel_io_spi(SPI2_HOST, &config, &io);
	if (ret != ESP_OK) { return ret; }
	esp_lcd_panel_dev_config_t device = {.reset_gpio_num = BEPIO_DISPLAY_RESET_GPIO,
		.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB, .bits_per_pixel = 16};
	ret = esp_lcd_new_panel_st7789(io, &device, &panel);
	if (ret == ESP_OK) { ret = esp_lcd_panel_reset(panel); }
	if (ret == ESP_OK) { ret = esp_lcd_panel_init(panel); }
	if (ret == ESP_OK) { ret = esp_lcd_panel_swap_xy(panel, false); }
	if (ret == ESP_OK) { ret = esp_lcd_panel_mirror(panel, false, false); }
	if (ret == ESP_OK) { ret = esp_lcd_panel_disp_on_off(panel, true); }
	fault = ret != ESP_OK;
	return ret;
}

esp_err_t bepio_lcd_draw_async(int x1, int y1, int x2, int y2, const void *pixels)
{
	if (!panel || fault || pending) { return ESP_ERR_INVALID_STATE; }
	if (!pixels || x1 < 0 || y1 < 0 || x2 > BEPIO_DISPLAY_WIDTH || y2 > BEPIO_DISPLAY_HEIGHT ||
	    x1 >= x2 || y1 >= y2 ||
	    (x2 - x1) * (y2 - y1) > BEPIO_SCREEN_WIDTH * BEPIO_DISPLAY_BUFFER_LINES) {
		return ESP_ERR_INVALID_ARG;
	}
	pending = true;
	esp_err_t ret = esp_lcd_panel_draw_bitmap(panel, x1, y1, x2, y2, pixels);
	if (ret != ESP_OK) { fault = true; }
	return ret;
}

esp_err_t bepio_lcd_wait_done(uint32_t timeout_ms)
{
	if (!panel || fault || !pending) { return ESP_ERR_INVALID_STATE; }
	TickType_t wait = pdMS_TO_TICKS(timeout_ms);
	if (timeout_ms && !wait) { wait = 1; }
	if (xSemaphoreTake(transferred, wait) != pdTRUE) {
		if (!timeout_ms) { return ESP_ERR_NOT_FINISHED; }
		fault = true;
		return ESP_ERR_TIMEOUT;
	}
	pending = false;
	return ESP_OK;
}

esp_err_t bepio_lcd_draw(int x1, int y1, int x2, int y2, const void *pixels)
{
	esp_err_t ret = bepio_lcd_draw_async(x1, y1, x2, y2, pixels);
	return ret == ESP_OK ? bepio_lcd_wait_done(2000) : ret;
}
