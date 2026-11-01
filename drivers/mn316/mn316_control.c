#include "mn316_internal.h"
#include "mn316_network.h"

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "bepio_devkit.h"
#include "driver/uart.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "mn316.h"
#include "mbedtls/platform_util.h"

static SemaphoreHandle_t modem_lock;
static portMUX_TYPE modem_init_lock = portMUX_INITIALIZER_UNLOCKED;

int bepio_mn316_command_locked(const char *command, const char *completion,
			  char *reply, size_t capacity, int timeout_ms)
{
	if ((command && (strncmp(command, "AT", 2) || strchr(command, '\r') ||
	    strchr(command, '\n'))) || (!command && !completion) ||
	    timeout_ms < 1 || !reply || capacity < 2) {
		return -EINVAL;
	}
	reply[0] = '\0';
	if (command) {
		size_t length = strlen(command);
		if (length > 1021) return -EMSGSIZE;
		char wire[1024];
		memcpy(wire, command, length);
		wire[length++] = '\r';
		wire[length++] = '\n';
		int written = uart_write_bytes(BEPIO_MN316_UART_PORT, wire, length);
		mbedtls_platform_zeroize(wire, sizeof(wire));
		if (written != (int)length ||
		    uart_wait_tx_done(BEPIO_MN316_UART_PORT, pdMS_TO_TICKS(2000)) != ESP_OK) {
			return -EIO;
		}
	}
	char line[1024];
	size_t used = 0, copied = 0;
	bool overflow = false;
	int64_t deadline = esp_timer_get_time() + (int64_t)timeout_ms * 1000;
	while (esp_timer_get_time() < deadline) {
		uint8_t byte;
		if (uart_read_bytes(BEPIO_MN316_UART_PORT, &byte, 1, pdMS_TO_TICKS(20)) != 1) continue;
		if (byte != '\r' && byte != '\n') {
			if (used < sizeof(line) - 1) line[used++] = (char)byte;
			else overflow = true;
			continue;
		}
		if (overflow) { used = 0; overflow = false; return -EOVERFLOW; }
		line[used] = '\0';
		if (!used) continue;
		if (copied + used + 2 <= capacity) {
			memcpy(reply + copied, line, used);
			copied += used;
			reply[copied++] = '\n';
			reply[copied] = '\0';
		}
		used = 0;
		if (!strcmp(line, "ERROR") || !strncmp(line, "+CME ERROR:", 11) ||
		    !strncmp(line, "+CMS ERROR:", 11)) return -EIO;
		if (completion) {
			size_t expected = strlen(completion);
			if (expected && completion[expected - 1] == '*') {
				if (!strncmp(line, completion, expected - 1)) return 0;
			} else if (!strcmp(line, completion)) return 0;
		} else if (!strcmp(line, "OK")) return 0;
	}
	return -ETIMEDOUT;
}

int bepio_mn316_driver_acquire(void)
{
	if (!modem_lock) return -ENODEV;
	return xSemaphoreTake(modem_lock, pdMS_TO_TICKS(2000)) == pdTRUE ? 0 : -EBUSY;
}

int bepio_mn316_driver_init(void)
{
	if (!modem_lock) {
		SemaphoreHandle_t created = xSemaphoreCreateMutex();
		if (!created) return -ENOMEM;
		portENTER_CRITICAL(&modem_init_lock);
		if (!modem_lock) {
			modem_lock = created;
			created = NULL;
		}
		portEXIT_CRITICAL(&modem_init_lock);
		if (created) vSemaphoreDelete(created);
	}
	if (xSemaphoreTake(modem_lock, pdMS_TO_TICKS(3000)) != pdTRUE) return -EBUSY;
	int ret = mn316_probe();
	xSemaphoreGive(modem_lock);
	return ret;
}

int bepio_mn316_driver_at(const char *command, char *reply, size_t capacity, int timeout_ms)
{
	int ret = bepio_mn316_driver_acquire();
	if (ret) return ret;
	ret = bepio_mn316_command_locked(command, NULL, reply, capacity, timeout_ms);
	bepio_mn316_driver_release();
	return ret;
}

int bepio_mn316_driver_network_wait(int timeout_ms)
{
	if (timeout_ms < 1) return -EINVAL;
	int ret = bepio_mn316_driver_acquire();
	if (ret) return ret;
	int64_t deadline = esp_timer_get_time() + (int64_t)timeout_ms * 1000;
	char reply[256];
	ret = -ETIMEDOUT;
	while (esp_timer_get_time() < deadline) {
		if (!bepio_mn316_command_locked("AT+CEREG?", NULL, reply, sizeof(reply), 2000) &&
		    bepio_mn316_registered(reply) &&
		    !bepio_mn316_command_locked("AT+CGATT?", NULL, reply, sizeof(reply), 2000) &&
		    bepio_mn316_attached(reply) &&
		    !bepio_mn316_command_locked("AT+CGPADDR=0", NULL, reply, sizeof(reply), 2000) &&
		    bepio_mn316_has_ip(reply)) {
			ret = 0;
			break;
		}
		vTaskDelay(pdMS_TO_TICKS(500));
	}
	bepio_mn316_driver_release();
	return ret;
}


void bepio_mn316_driver_release(void)
{
	xSemaphoreGive(modem_lock);
}
