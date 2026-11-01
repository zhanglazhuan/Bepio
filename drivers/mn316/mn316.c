#include "mn316.h"
#include <errno.h>
#include <stdbool.h>
#include <string.h>
#include "driver/uart.h"
#include "esp_timer.h"
#include "bepio_devkit.h"

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
	static const char command[] = "AT\r\n";
	char response[128];
	size_t used = 0;
	bool overflow = false;
	unsigned char byte;

	/* Driver initialization serializes probing with active UART sessions. */
	if (!uart_is_driver_installed(port) &&
	    uart_driver_install(port, 16384, 0, 0, NULL, 0) != ESP_OK) {
		return -ENODEV;
	}
	if (uart_param_config(port, &config) != ESP_OK ||
	    uart_set_pin(port, BEPIO_MN316_TX_GPIO, BEPIO_MN316_RX_GPIO,
			 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE) != ESP_OK) {
		return -EIO;
	}
	if (uart_flush_input(port) != ESP_OK ||
	    uart_write_bytes(port, command, sizeof(command) - 1) != sizeof(command) - 1) {
		return -EIO;
	}
	if (uart_wait_tx_done(port, pdMS_TO_TICKS(200)) != ESP_OK) {
		return -ETIMEDOUT;
	}
	int64_t deadline = esp_timer_get_time() + 2000000;
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
