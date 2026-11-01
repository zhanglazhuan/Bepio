#include <stdio.h>
#include <stdlib.h>
#include "bepio_devkit.h"
#include "bepio_mn316_manager.h"
#include "esp_console.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "t_mn316";

static int check_modem(int argc, char **argv)
{
	(void)argc;
	(void)argv;
	static const char *commands[] = {"AT", "ATE0", "AT+CSQ", "AT+CEREG?", "AT+CGATT?", "AT+CGPADDR=0"};
	for (size_t i = 0; i < sizeof(commands) / sizeof(commands[0]); i++) {
		char reply[512];
		int ret = bepio_mn316_manager_at(commands[i], reply, sizeof(reply), 3000);
		ESP_LOGI(TAG, "%s: result=%d\n%s", commands[i], ret, reply);
	}
	return 0;
}

static int emqx_send(int argc, char **argv)
{
	char device_id[33];
	if (bepio_mn316_manager_device_id(device_id, sizeof(device_id))) return 1;
	int seconds = 30;
	if (argc > 3) return 1;
	if (argc >= 2) {
		char *end;
		long value = strtol(argv[1], &end, 10);
		if (!argv[1][0] || *end || value < 1 || value > 600) return 1;
		seconds = (int)value;
	}
	const char *text = argc == 3 ? argv[2] : NULL;
	ESP_LOGI(TAG, "Device ID=%s; To=%s; topic=%s; Text=%s",
		device_id, BEPIO_MN316_TEST_PEER, BEPIO_MN316_TEST_INBOX,
		text ? text : "MN316 TLS test message");
	int ret = bepio_mn316_manager_test(seconds, text);
	ESP_LOGI(TAG, "EMQX_SEND_DONE result=%d", ret);
	return ret ? 1 : 0;
}

void app_main(void)
{
	char device_id[33] = "unavailable";
	(void)bepio_mn316_manager_device_id(device_id, sizeof(device_id));
	ESP_LOGI(TAG, "%s: UART%d TX=%d RX=%d baud=%d 8N1; ID=%s",
		 BEPIO_BOARD_NAME, BEPIO_MN316_UART_PORT,
		 BEPIO_MN316_TX_GPIO, BEPIO_MN316_RX_GPIO,
		 BEPIO_MN316_BAUD_RATE, device_id);
	for (int attempt = 1; attempt <= 3; attempt++) {
		int ret = bepio_mn316_manager_init();
		ESP_LOGI(TAG, "AT probe %d/3: %s (%d)", attempt, ret ? "FAIL" : "PASS", ret);
		if (!ret) break;
		if (attempt < 3) vTaskDelay(pdMS_TO_TICKS(1000));
	}
	esp_console_repl_t *repl = NULL;
	esp_console_repl_config_t config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
	config.prompt = "mn316>";
	config.task_stack_size = 16384;
	ESP_ERROR_CHECK(esp_console_register_help_command());
	const esp_console_cmd_t cmd = {
		.command = "emqx_send", .help = "Send to numeric test peer; [listen-seconds=30] [text]",
		.func = emqx_send,
	};
	ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
	const esp_console_cmd_t check_cmd = {
		.command = "check", .help = "Inspect AT, signal, registration, attachment and IP",
		.func = check_modem,
	};
	ESP_ERROR_CHECK(esp_console_cmd_register(&check_cmd));
	esp_console_dev_uart_config_t uart_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_console_new_repl_uart(&uart_config, &config, &repl));
	ESP_ERROR_CHECK(esp_console_start_repl(repl));
}
