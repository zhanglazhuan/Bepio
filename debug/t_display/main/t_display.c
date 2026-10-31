#include "bepio_display.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static esp_lcd_panel_handle_t panel;
static esp_lcd_panel_io_handle_t io;
static SemaphoreHandle_t transferred;
static uint16_t *pixels;
static int width, height;
static bool fault;
static const char *TAG = "t_display";

static bool on_done(esp_lcd_panel_io_handle_t handle, esp_lcd_panel_io_event_data_t *event, void *context)
{
	(void)handle; (void)event; (void)context;
	BaseType_t wake = pdFALSE;
	xSemaphoreGiveFromISR(transferred, &wake);
	return wake == pdTRUE;
}

static esp_err_t initialize(void)
{
	width = BEPIO_DISPLAY_WIDTH; height = BEPIO_DISPLAY_HEIGHT;
	pixels = heap_caps_malloc(width * 16 * sizeof(uint16_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
	if (!pixels) { return ESP_ERR_NO_MEM; }
	spi_bus_config_t bus = {.sclk_io_num = BEPIO_DISPLAY_SCK_GPIO,
		.mosi_io_num = BEPIO_DISPLAY_MOSI_GPIO, .miso_io_num = BEPIO_DISPLAY_MISO_GPIO,
		.quadwp_io_num = -1, .quadhd_io_num = -1, .max_transfer_sz = width * 16 * 2};
	esp_err_t ret = spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO);
	if (ret != ESP_OK) { return ret; }
	esp_lcd_panel_io_spi_config_t cfg = {.dc_gpio_num = BEPIO_DISPLAY_DC_GPIO,
		.cs_gpio_num = BEPIO_DISPLAY_CS_GPIO, .pclk_hz = 10000000, .spi_mode = 0,
		.lcd_cmd_bits = 8, .lcd_param_bits = 8, .trans_queue_depth = 1,
		.on_color_trans_done = on_done};
	ret = esp_lcd_new_panel_io_spi(SPI2_HOST, &cfg, &io);
	if (ret != ESP_OK) { return ret; }
	esp_lcd_panel_dev_config_t dev = {.reset_gpio_num = BEPIO_DISPLAY_RESET_GPIO,
		.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB, .bits_per_pixel = 16};
	ret = esp_lcd_new_panel_st7789(io, &dev, &panel);
	if (ret == ESP_OK) { ret = esp_lcd_panel_reset(panel); }
	if (ret == ESP_OK) { ret = esp_lcd_panel_init(panel); }
	/* Portrait: X spans 240 pixels, Y spans 320 pixels. */
	if (ret == ESP_OK) { ret = esp_lcd_panel_swap_xy(panel, false); }
	if (ret == ESP_OK) { ret = esp_lcd_panel_mirror(panel, false, false); }
	if (ret == ESP_OK) { ret = esp_lcd_panel_disp_on_off(panel, true); }
	if (ret != ESP_OK) { fault = true; return ret; }
	ESP_LOGI(TAG, "ST7789T3 %dx%d SPI2 10 MHz", width, height);
	return ESP_OK;
}

static esp_err_t pattern(void)
{
	if (!panel || fault) { return ESP_ERR_INVALID_STATE; }
	const uint16_t colors[] = {0xf800, 0x07e0, 0x001f, 0xffff, 0x0000};
	for (int y = 0; y < height; y += 16) {
		int rows = height - y < 16 ? height - y : 16;
		for (int row = 0; row < rows; row++) {
			for (int x = 0; x < width; x++) {
				uint16_t color = colors[(x * 5) / width];
				pixels[row * width + x] = (color >> 8) | (color << 8);
			}
		}
		esp_err_t ret = esp_lcd_panel_draw_bitmap(panel, 0, y, width, y + rows, pixels);
		if (ret != ESP_OK) { fault = true; return ret; }
		if (xSemaphoreTake(transferred, pdMS_TO_TICKS(2000)) != pdTRUE) {
			/* DMA may still own pixels: latch fault; never reuse/free until reset. */
			fault = true; return ESP_ERR_TIMEOUT;
		}
	}
	return ESP_OK;
}

void app_main(void)
{
	transferred = xSemaphoreCreateBinary();
	ESP_ERROR_CHECK(transferred ? ESP_OK : ESP_ERR_NO_MEM);
	ESP_ERROR_CHECK(gpio_set_level(BEPIO_DISPLAY_LED_GPIO, 0));
	gpio_config_t pin = {.pin_bit_mask = 1ULL << BEPIO_DISPLAY_LED_GPIO, .mode = GPIO_MODE_OUTPUT};
	ESP_ERROR_CHECK(gpio_config(&pin));
	esp_err_t ret = initialize();
	if (ret == ESP_OK) { ret = pattern(); }
	if (ret == ESP_OK) { ret = gpio_set_level(BEPIO_DISPLAY_LED_GPIO, 1); }
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "Display test failed: %s; reset to retry", esp_err_to_name(ret));
		return;
	}
	ESP_LOGI(TAG, "Portrait 240x320 ready: red, green, blue, white, black bars; backlight GPIO HIGH");
}
