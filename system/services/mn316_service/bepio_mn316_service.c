#include "bepio_mn316_service.h"

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
#include "onenet.h"

static SemaphoreHandle_t modem_lock;
static bool mqtt_open;

static int command_locked(const char *command, const char *completion,
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
		if (uart_write_bytes(BEPIO_MN316_UART_PORT, wire, length) != (int)length ||
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

static int take_lock(void)
{
	if (!modem_lock) return -ENODEV;
	return xSemaphoreTake(modem_lock, pdMS_TO_TICKS(2000)) == pdTRUE ? 0 : -EBUSY;
}

int bepio_mn316_service_init(void)
{
	if (!modem_lock) {
		modem_lock = xSemaphoreCreateMutex();
		if (!modem_lock) return -ENOMEM;
	}
	return mn316_probe();
}

int bepio_mn316_service_at(const char *command, char *reply, size_t capacity, int timeout_ms)
{
	int ret = take_lock();
	if (ret) return ret;
	ret = command_locked(command, NULL, reply, capacity, timeout_ms);
	xSemaphoreGive(modem_lock);
	return ret;
}

int bepio_mn316_service_network_wait(int timeout_ms)
{
	if (timeout_ms < 1) return -EINVAL;
	int ret = take_lock();
	if (ret) return ret;
	int64_t deadline = esp_timer_get_time() + (int64_t)timeout_ms * 1000;
	char reply[256];
	ret = -ETIMEDOUT;
	while (esp_timer_get_time() < deadline) {
		if (!command_locked("AT+CEREG?", NULL, reply, sizeof(reply), 2000) &&
		    (strstr(reply, "+CEREG: 0,1") || strstr(reply, "+CEREG: 0,5") ||
		     strstr(reply, "+CEREG: 1,1") || strstr(reply, "+CEREG: 1,5")) &&
		    !command_locked("AT+CGATT?", NULL, reply, sizeof(reply), 2000) &&
		    strstr(reply, "+CGATT: 1") &&
		    !command_locked("AT+CGPADDR=0", NULL, reply, sizeof(reply), 2000) &&
		    strstr(reply, "+CGPADDR: 0,") && !strstr(reply, "0.0.0.0")) {
			ret = 0;
			break;
		}
		vTaskDelay(pdMS_TO_TICKS(500));
	}
	xSemaphoreGive(modem_lock);
	return ret;
}

int bepio_mn316_service_onenet_login(const char *device, const char *key_base64,
					const char *expires)
{
	char command[512], reply[256];
	if (bepio_onenet_config_with_key(command, sizeof(command), device,
					key_base64, expires)) return -EINVAL;
	int ret = take_lock();
	if (ret) { memset(command, 0, sizeof(command)); return ret; }
	mqtt_open = false;
	ret = command_locked("ATE0", NULL, reply, sizeof(reply), 2000);
	if (!ret) ret = command_locked("AT+CGATT?", NULL, reply, sizeof(reply), 2000);
	if (!ret && !strstr(reply, "+CGATT: 1")) ret = -ENETDOWN;
	if (!ret) ret = command_locked("AT+MQTTSTAT?", NULL, reply, sizeof(reply), 2000);
	if (!ret && !strstr(reply, "+MQTTSTAT: 0")) {
		/* A prior client can survive host reset. Cleanup is bounded. */
		command_locked("AT+MQTTDISC", NULL, reply, sizeof(reply), 5000);
		ret = command_locked("AT+MQTTDEL", NULL, reply, sizeof(reply), 5000);
	}
	if (!ret) ret = command_locked(command, NULL, reply, sizeof(reply), 5000);
	memset(command, 0, sizeof(command));
	if (!ret) ret = command_locked("AT+MQTTOPEN=1,1,0,0,0,\"\",\"\"",
					"+MQTTOPEN:OK", reply, sizeof(reply), 60000);
	mqtt_open = ret == 0;
	xSemaphoreGive(modem_lock);
	return ret;
}

int bepio_mn316_service_mqtt_subscribe(const char *topic)
{
	if (!topic || !topic[0] || strlen(topic) > 150 || strchr(topic, '"')) return -EINVAL;
	int ret = take_lock();
	if (ret) return ret;
	if (!mqtt_open) ret = -ENOTCONN;
	else {
		char command[192], reply[256];
		snprintf(command, sizeof(command), "AT+MQTTSUB=\"%s\",0", topic);
		ret = command_locked(command, NULL, reply, sizeof(reply), 5000);
		if (!ret) ret = command_locked(NULL, "+MQTTSUBACK:*", reply, sizeof(reply), 20000);
		if (!ret) {
			int packet, code;
			char *ack = strstr(reply, "+MQTTSUBACK:");
			if (!ack || sscanf(ack, "+MQTTSUBACK: %d,%d", &packet, &code) != 2 ||
			    code < 0 || code > 2) ret = -EIO;
		}
	}
	xSemaphoreGive(modem_lock);
	return ret;
}

int bepio_mn316_service_mqtt_publish(const char *topic, const char *payload)
{
	if (!topic || !payload || !topic[0] || strlen(topic) > 150 ||
	    strchr(topic, '"') || strlen(payload) > 350) return -EINVAL;
	int ret = take_lock();
	if (ret) return ret;
	if (!mqtt_open) ret = -ENOTCONN;
	else {
		char command[1024], reply[256];
		size_t size = strlen(payload);
		int used = snprintf(command, sizeof(command), "AT+MQTTPUB=\"%s\",1,0,0,%u,\"",
				    topic, (unsigned)size);
		if (used < 0 || (size_t)used + size * 2 + 2 > sizeof(command)) ret = -EMSGSIZE;
		else {
			static const char hex[] = "0123456789ABCDEF";
			for (size_t i = 0; i < size; i++) {
				unsigned char byte = (unsigned char)payload[i];
				command[used++] = hex[byte >> 4];
				command[used++] = hex[byte & 15];
			}
			command[used++] = '"'; command[used] = '\0';
			ret = command_locked(command, NULL, reply, sizeof(reply), 5000);
		}
	}
	xSemaphoreGive(modem_lock);
	return ret;
}

int bepio_mn316_service_tls_test(const bepio_mn316_tls_credentials_t *credentials,
				int listen_seconds)
{
	int ret = take_lock();
	if (ret) return ret;
	mqtt_open = false;
	ret = bepio_mn316_tls_run(credentials, listen_seconds) ? -EIO : 0;
	xSemaphoreGive(modem_lock);
	return ret;
}
