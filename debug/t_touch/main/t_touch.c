#include <stdio.h>
#include <stdbool.h>
#include "bepio_display.h"
#include "bepio_ft6336.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "t_touch";
static bool pressed;

static esp_err_t sample(void)
{
	bepio_ft6336_frame_t frame;
	esp_err_t ret = bepio_ft6336_scan(&frame);
	if (ret != ESP_OK) { return ret; }
	if (!frame.count && pressed) { puts("release"); }
	pressed = frame.count != 0;
	for (unsigned i = 0; i < frame.count; i++) {
		const bepio_ft6336_point_t *p = &frame.points[i];
		printf("points=%u id=%u x=%u y=%u event=%u\n",
			frame.count, p->track_id, p->x, p->y, p->event);
	}
	return ESP_OK;
}

void app_main(void)
{
	const bepio_ft6336_config_t config = {
		.i2c_port = BEPIO_DISPLAY_TP_I2C_PORT,
		.sda = BEPIO_DISPLAY_TP_SDA_GPIO, .scl = BEPIO_DISPLAY_TP_SCL_GPIO,
		.rst = BEPIO_DISPLAY_TP_RST_GPIO, .interrupt = BEPIO_DISPLAY_TP_INT_GPIO,
	};
	esp_err_t ret = bepio_ft6336_init(&config);
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "Touch init failed: %s; check power/GND, SDA38/SCL45/RST3/INT39 and pull-ups", esp_err_to_name(ret));
		return;
	}
	ESP_LOGI(TAG, "Touch test ready: touch panel to see raw coordinates");
	for (;;) {
		ret = sample();
		if (ret != ESP_OK) {
			ESP_LOGE(TAG, "Touch read failed: %s; reset to retry", esp_err_to_name(ret));
			return;
		}
		vTaskDelay(pdMS_TO_TICKS(20));
	}
}
