#include "bepio_ui.h"
#include "bepio_ui_preferences.h"
#include "bepio_ui_display.h"
#include "bepio_navigation.h"
#include "bepio_lcd.h"
#include "bepio_display.h"
#include "bepio_orientation.h"
#include "bepio_ft6336.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "lvgl.h"

#define TOUCH_PERIOD_MS 10
#define TOUCH_STALE_US 100000

static const char *TAG = "ui";
static bool started;
static SemaphoreHandle_t boot_done;
static esp_err_t boot_result;
static void (*build_callback)(void);
static portMUX_TYPE touch_lock = portMUX_INITIALIZER_UNLOCKED;

static struct {
	bepio_ft6336_frame_t frame;
	esp_err_t status;
	int64_t sampled_at;
} touch_cache = {.status = ESP_ERR_INVALID_STATE};

static uint32_t ticks(void) { return (uint32_t)(esp_timer_get_time() / 1000); }

static void display_failed(esp_err_t ret)
{
	ESP_LOGE(TAG, "Display pipeline failed: %s; DMA buffers retained until reset", esp_err_to_name(ret));
	boot_result = ret;
	xSemaphoreGive(boot_done);
	/* Do not release or overwrite memory which a late DMA completion may use. */
	vTaskDelete(NULL);
}

static void touch_task(void *unused)
{
	(void)unused;
	TickType_t wake = xTaskGetTickCount();
	TickType_t period = pdMS_TO_TICKS(TOUCH_PERIOD_MS);
	unsigned failures = 0;
	if (!period) { period = 1; }
	for (;;) {
		bepio_ft6336_frame_t frame;
		esp_err_t ret = bepio_ft6336_scan(&frame);
		int64_t now = esp_timer_get_time();
		portENTER_CRITICAL(&touch_lock);
		touch_cache.status = ret;
		if (ret == ESP_OK) {
			touch_cache.frame = frame;
			touch_cache.sampled_at = now;
		}
		portEXIT_CRITICAL(&touch_lock);
		if (ret != ESP_OK) {
			failures++;
			if (failures == 1 || failures % 100 == 0) {
				ESP_LOGW(TAG, "Touch read failed (%u): %s; retrying", failures, esp_err_to_name(ret));
			}
			vTaskDelay(pdMS_TO_TICKS(100));
			wake = xTaskGetTickCount();
			continue;
		}
		if (failures) { ESP_LOGI(TAG, "Touch recovered after %u failed reads", failures); failures = 0; }
		vTaskDelayUntil(&wake, period);
	}
}

static void touch_read(lv_indev_t *input, lv_indev_data_t *data)
{
	(void)input;
	portENTER_CRITICAL(&touch_lock);
	bepio_ft6336_frame_t frame = touch_cache.frame;
	esp_err_t status = touch_cache.status;
	int64_t sampled_at = touch_cache.sampled_at;
	portEXIT_CRITICAL(&touch_lock);
	data->state = LV_INDEV_STATE_RELEASED;
	if (status != ESP_OK || !sampled_at) { return; }
	uint32_t age = (uint32_t)(esp_timer_get_time() - sampled_at);
	/* A stalled sampler must never leave LVGL with a stuck pressed contact. */
	if (age > TOUCH_STALE_US) { return; }
	uint16_t x, y;
	if (bepio_ui_preferences_touch(frame.count != 0)) { return; }
	if (frame.count && bepio_touch_to_screen(frame.points[0].x, frame.points[0].y, &x, &y)) {
		data->point.x = x;
		data->point.y = y;
		data->state = LV_INDEV_STATE_PRESSED;
	}
}

static esp_err_t initialize_ui(void)
{
	esp_err_t ret = bepio_lcd_init();
	if (ret != ESP_OK) { return ret; }
	lv_init();
	lv_tick_set_cb(ticks);
	lv_display_t *display = lv_display_create(BEPIO_SCREEN_WIDTH, BEPIO_SCREEN_HEIGHT);
	if (!display) { return ESP_ERR_NO_MEM; }
	ret = bepio_ui_display_init(display, display_failed);
	if (ret != ESP_OK) { return ret; }
	bepio_ui_preferences_init();
	build_callback();
	lv_refr_now(display);
	ret = bepio_ui_display_poll(true);
	if (ret != ESP_OK) { return ret; }
	ret = bepio_lcd_backlight(true);
	if (ret != ESP_OK) { return ret; }
	ESP_LOGI(TAG, "Launcher displayed: landscape 320x240 RGB, SPI2 %uMHz, async DMA + double draw buffers; refresh=%ums touch=%ums",
		BEPIO_DISPLAY_SPI_HZ / 1000000, BEPIO_UI_REFRESH_MS, TOUCH_PERIOD_MS);
#ifdef CONFIG_BEPIO_UI_OPTIMIZE
	ESP_LOGI(TAG, "Graphics components: -O2 (CONFIG_BEPIO_UI_OPTIMIZE)");
#endif
	/* Light the screen before attempting optional touch initialization. */
	const bepio_ft6336_config_t touch = {.i2c_port = BEPIO_DISPLAY_TP_I2C_PORT,
		.sda = BEPIO_DISPLAY_TP_SDA_GPIO, .scl = BEPIO_DISPLAY_TP_SCL_GPIO,
		.rst = BEPIO_DISPLAY_TP_RST_GPIO, .interrupt = BEPIO_DISPLAY_TP_INT_GPIO};
	ret = bepio_ft6336_init(&touch);
	if (ret == ESP_OK) {
		lv_indev_t *input = lv_indev_create();
		if (!input) { return ESP_ERR_NO_MEM; }
		lv_indev_set_type(input, LV_INDEV_TYPE_POINTER);
		lv_indev_set_read_cb(input, touch_read);
		lv_timer_set_period(lv_indev_get_read_timer(input), TOUCH_PERIOD_MS);
		bepio_navigation_attach_input(input);
		if (xTaskCreate(touch_task, "bepio_touch", 3072, NULL, 4, NULL) != pdPASS) { return ESP_ERR_NO_MEM; }
	} else {
		ESP_LOGW(TAG, "Touch unavailable (%s); launcher remains visible", esp_err_to_name(ret));
	}
	return ESP_OK;
}

static void ui_task(void *arg)
{
	(void)arg;
	boot_result = initialize_ui();
	ESP_LOGI(TAG, "UI stack minimum free after startup: %u bytes",
		(unsigned)uxTaskGetStackHighWaterMark(NULL));
	xSemaphoreGive(boot_done);
	if (boot_result != ESP_OK) { vTaskDelete(NULL); }
	for (;;) {
		uint32_t wait = lv_timer_handler();
		esp_err_t ret = bepio_ui_display_poll(false);
		if (ret != ESP_OK) { display_failed(ret); }
		if (wait < 1) { wait = 1; }
		if (wait > TOUCH_PERIOD_MS) { wait = TOUCH_PERIOD_MS; }
		TickType_t delay = pdMS_TO_TICKS(wait);
		vTaskDelay(delay ? delay : 1);
	}
}

esp_err_t bepio_ui_start(void (*build_screen)(void))
{
	if (!build_screen) { return ESP_ERR_INVALID_ARG; }
	if (started) { return ESP_ERR_INVALID_STATE; }
	started = true;
	build_callback = build_screen;
	boot_done = xSemaphoreCreateBinary();
	if (!boot_done) { return ESP_ERR_NO_MEM; }
	if (xTaskCreate(ui_task, "bepio_ui", 10240, NULL, 5, NULL) != pdPASS) { return ESP_ERR_NO_MEM; }
	if (xSemaphoreTake(boot_done, pdMS_TO_TICKS(8000)) != pdTRUE) { return ESP_ERR_TIMEOUT; }
	return boot_result;
}
