#include "bepio_devkit.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mn316.h"

static const char *TAG = "t_mn316";

void app_main(void)
{
	ESP_LOGI(TAG, "%s: UART%d TX=%d RX=%d baud=%d 8N1",
		 BEPIO_BOARD_NAME, BEPIO_MN316_UART_PORT,
		 BEPIO_MN316_TX_GPIO, BEPIO_MN316_RX_GPIO,
		 BEPIO_MN316_BAUD_RATE);
	for (int attempt = 1; attempt <= 3; attempt++) {
		int result = mn316_probe();
		ESP_LOGI(TAG, "AT probe %d/3: %s (%d)", attempt,
			 result == 0 ? "PASS" : "FAIL", result);
		if (result == 0) break;
		if (attempt < 3) vTaskDelay(pdMS_TO_TICKS(1000));
	}
}
