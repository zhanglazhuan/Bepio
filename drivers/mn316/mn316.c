#include "mn316.h"
#include <errno.h>
#include <stdbool.h>
#include <string.h>
#include "driver/uart.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/task.h"
#include "bepio_devkit.h"

static int probe_at(uart_port_t port, int timeout_ms)
{
	static const char command[] = "AT\r\n";
	char response[128];
	size_t used = 0;
	bool overflow = false;
	unsigned char byte;

	if (uart_flush_input(port) != ESP_OK ||
	    uart_write_bytes(port, command, sizeof(command) - 1) != sizeof(command) - 1) {
		return -EIO;
	}
	if (uart_wait_tx_done(port, pdMS_TO_TICKS(200)) != ESP_OK) {
		return -ETIMEDOUT;
	}
	int64_t deadline = esp_timer_get_time() + (int64_t)timeout_ms * 1000;
	while (esp_timer_get_time() < deadline) {
		if (uart_read_bytes(port, &byte, 1, pdMS_TO_TICKS(10)) != 1) {
			continue;
		}
		if (byte == '\r' || byte == '\n') {
			response[used] = '\0';
			if (!overflow && strcmp(response, "OK") == 0) { return 0; }
			if (!overflow && strcmp(response, "ERROR") == 0) { return -EIO; }
			used = 0;
			overflow = false;
		} else if (used < sizeof(response) - 1) {
			response[used++] = (char)byte;
		} else {
			overflow = true;
		}
	}
	return -ETIMEDOUT;
}

int mn316_probe(void)
{
	const uart_port_t port = BEPIO_MN316_UART_PORT;
	const uart_config_t config = {
		.baud_rate = BEPIO_MN316_BAUD_RATE,
		.data_bits = UART_DATA_8_BITS,
		.parity = UART_PARITY_DISABLE,
		.stop_bits = UART_STOP_BITS_1,
		.flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
		.source_clk = UART_SCLK_DEFAULT,
	};
	/* The driver lock serializes this probe with TLS and AT sessions. */
	if (!uart_is_driver_installed(port) &&
	    uart_driver_install(port, 16384, 0, 0, NULL, 0) != ESP_OK) return -ENODEV;
	if (uart_param_config(port, &config) != ESP_OK ||
	    uart_set_pin(port, BEPIO_MN316_TX_GPIO, BEPIO_MN316_RX_GPIO,
			 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE) != ESP_OK) return -EIO;
	int ret = probe_at(port, 2000);
	if (ret != -ETIMEDOUT) return ret;
	/* An ESP32 reset can interrupt TLS while MN316 remains at 115200.
	 * Recover its nonpersistent UART speed before normal network work. */
	ESP_LOGW("mn316", "No AT response at %d; probing 115200", BEPIO_MN316_BAUD_RATE);
	if (uart_set_baudrate(port, 115200) != ESP_OK) return -EIO;
	ret = probe_at(port, 1000);
	if (ret == 0) {
		static const char restore[] = "AT+NATSPEED=9600,0,0\r\n";
		if (uart_write_bytes(port, restore, sizeof(restore) - 1) != sizeof(restore) - 1 ||
		    uart_wait_tx_done(port, pdMS_TO_TICKS(2000)) != ESP_OK) ret = -EIO;
		else {
			vTaskDelay(pdMS_TO_TICKS(100));
			if (uart_set_baudrate(port, BEPIO_MN316_BAUD_RATE) != ESP_OK) return -EIO;
			ret = probe_at(port, 2000);
			if (!ret) ESP_LOGI("mn316", "Recovered modem UART to %d", BEPIO_MN316_BAUD_RATE);
		}
	}
	if (uart_set_baudrate(port, BEPIO_MN316_BAUD_RATE) != ESP_OK) return -EIO;
	if (ret == -ETIMEDOUT) ESP_LOGE("mn316", "No AT response at 9600 or 115200; check modem power and UART wiring");
	return ret;
}
