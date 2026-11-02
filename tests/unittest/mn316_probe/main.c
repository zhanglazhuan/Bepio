#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "mn316.h"
#include "driver/uart.h"

static int64_t now_us;
static int modem_baud, host_baud, restore_count;
static bool installed;
static const char *reply;
static size_t reply_at;

int64_t esp_timer_get_time(void) { return now_us; }
void vTaskDelay(int ticks) { now_us += (int64_t)ticks * 1000; }
bool uart_is_driver_installed(uart_port_t port) { assert(port == 1); return installed; }
int uart_driver_install(uart_port_t port, int rx, int tx, int queue, void *handle, int flags)
{
	assert(port == 1 && rx == 16384 && tx == 0 && queue == 0 && !handle && flags == 0);
	installed = true;
	return ESP_OK;
}
int uart_param_config(uart_port_t port, const uart_config_t *config)
{
	assert(port == 1); host_baud = config->baud_rate; return ESP_OK;
}
int uart_set_pin(uart_port_t port, int tx, int rx, int rts, int cts)
{
	assert(port == 1 && tx == 17 && rx == 18 && rts == -1 && cts == -1);
	return ESP_OK;
}
int uart_flush_input(uart_port_t port)
{
	assert(port == 1); reply = NULL; reply_at = 0; return ESP_OK;
}
int uart_write_bytes(uart_port_t port, const void *data, size_t size)
{
	assert(port == 1);
	if (host_baud == modem_baud && size == 4 && !memcmp(data, "AT\r\n", 4)) {
		reply = "\r\nOK\r\n"; reply_at = 0;
	} else if (host_baud == modem_baud &&
		   size == strlen("AT+NATSPEED=9600,0,0\r\n") &&
		   !memcmp(data, "AT+NATSPEED=9600,0,0\r\n", size)) {
		modem_baud = 9600; restore_count++;
	}
	return (int)size;
}
int uart_wait_tx_done(uart_port_t port, int ticks)
{
	assert(port == 1 && ticks > 0); return ESP_OK;
}
int uart_read_bytes(uart_port_t port, void *data, size_t size, int ticks)
{
	assert(port == 1 && size == 1 && ticks > 0);
	now_us += 10000;
	if (!reply || !reply[reply_at]) return 0;
	*(char *)data = reply[reply_at++];
	return 1;
}
int uart_set_baudrate(uart_port_t port, int baud)
{
	assert(port == 1); host_baud = baud; return ESP_OK;
}

static void reset(int baud)
{
	now_us = 0; modem_baud = baud; host_baud = 0; restore_count = 0;
	installed = false; reply = NULL; reply_at = 0;
}
int main(void)
{
	reset(9600);
	assert(mn316_probe() == 0 && host_baud == 9600 && restore_count == 0);
	reset(115200);
	assert(mn316_probe() == 0 && host_baud == 9600 && modem_baud == 9600 &&
	       restore_count == 1);
	reset(0);
	assert(mn316_probe() == -ETIMEDOUT && host_baud == 9600);
	puts("PASS: MN316 AT probe at 9600, fast-baud recovery and absent modem timeout");
	return 0;
}
