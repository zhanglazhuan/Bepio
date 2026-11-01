#include "mn316_internal.h"
#include "mn316_network.h"
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/stream_buffer.h"
#include "freertos/task.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/entropy.h"
#include "mbedtls/error.h"
#include "mbedtls/net_sockets.h"
#include "mbedtls/ssl.h"
#include "mbedtls/x509_crt.h"
#include "bepio_devkit.h"

#define MODEM BEPIO_MN316_UART_PORT
#define AT_OK BIT0
#define AT_ERROR BIT1
#define TCP_READY BIT2
#define DNS_READY BIT3
#define CLOSED BIT4
#define CLOCK_READY BIT5
#define READER_EXIT BIT6
#define TCP_RX_CHUNK_MAX 2048
static const char *TAG = "mn316_transport";

/* The console command owns UART1 until the reader has exited. The RX task
 * drains automatic NSORF URCs concurrently with slow TLS writes.
 */
struct transport {
	EventGroupHandle_t events;
	StreamBufferHandle_t incoming;
	volatile bool stop;
	int socket;
	int sent;
	int mqtt_state;
	char ip[16];
	time_t network_time;
	int64_t network_time_at;
	/* Automatic TCP URCs can contain more than 1500 binary bytes, hex encoded. */
	char line[TCP_RX_CHUNK_MAX * 2 + 128];
	char command[850];
};

static int hex_digit(char c)
{
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'a' && c <= 'f') return c - 'a' + 10;
	if (c >= 'A' && c <= 'F') return c - 'A' + 10;
	return -1;
}

static void parse_line(struct transport *t, char *line)
{
	int id, length, offset = 0;
	if (!strcmp(line, "OK")) {
		xEventGroupSetBits(t->events, AT_OK);
	} else if (!strcmp(line, "ERROR") || !strncmp(line, "+CME ERROR:", 11) ||
		   strstr(line, "CONNECT FAIL")) {
		ESP_LOGE(TAG, "Modem: %s", line);
		xEventGroupSetBits(t->events, AT_ERROR);
	} else if (!strcmp(line, "CONNECT OK")) {
		xEventGroupSetBits(t->events, TCP_READY);
	} else if (!strncmp(line, "+MQTTSTAT:", 10)) {
		t->mqtt_state = bepio_mn316_internal_mqtt_state(line);
	} else if (sscanf(line, "+NSOCR: %d", &id) == 1) {
		t->socket = id;
	} else if (sscanf(line, "+NSOSD: %d,%d", &id, &length) == 2 && id == t->socket) {
		t->sent = length;
	} else if (sscanf(line, "+NSOCLI: %d", &id) == 1 && id == t->socket) {
		ESP_LOGW(TAG, "TCP socket %d closed by modem/peer", id);
		xEventGroupSetBits(t->events, CLOSED);
	} else if (!strncmp(line, "+NSODROP:", 9)) {
		ESP_LOGE(TAG, "Modem RX overflow; refusing corrupted TLS stream");
		xEventGroupSetBits(t->events, CLOSED);
	} else if (!strncmp(line, "+CMDNS:", 7)) {
		char *ip = line + 7;
		while (*ip == ' ' || *ip == '"') ip++;
		char *end = strchr(ip, '"');
		if (end) *end = '\0';
		unsigned a, b, c, d;
		char trailing;
		if (sscanf(ip, "%u.%u.%u.%u%c", &a, &b, &c, &d, &trailing) == 4 &&
		    a <= 255 && b <= 255 && c <= 255 && d <= 255 && strlen(ip) < sizeof(t->ip)) {
			snprintf(t->ip, sizeof(t->ip), "%s", ip);
			xEventGroupSetBits(t->events, DNS_READY);
		}
	} else if (!strncmp(line, "+CCLK:", 6)) {
		char *value = line + 6;
		while (*value == ' ' || *value == '"') value++;
		int y, mo, d, h, mi, s, quarters;
		if (sscanf(value, "%d/%d/%d,%d:%d:%d%d", &y, &mo, &d, &h, &mi, &s, &quarters) == 7 &&
		    y >= 24 && y <= 69 && mo >= 1 && mo <= 12 && d >= 1 && d <= 31 &&
		    h >= 0 && h < 24 && mi >= 0 && mi < 60 && s >= 0 && s < 60 && abs(quarters) <= 56) {
			struct tm clock = {.tm_year = y + 100, .tm_mon = mo - 1, .tm_mday = d,
				.tm_hour = h, .tm_min = mi, .tm_sec = s, .tm_isdst = 0};
			/* This MN316 firmware returns UTC clock fields; +zz reports
			 * the local zone separately. Confirmed against host UTC.
			 * Subtracting +32 again would shift messages by eight hours.
			 */
			t->network_time = mktime(&clock);
			t->network_time_at = esp_timer_get_time();
			xEventGroupSetBits(t->events, CLOCK_READY);
		}
	} else if (sscanf(line, "+NSORF: %d,%*[^,],%*d,%d,%n", &id, &length, &offset) == 2 &&
		   offset && id == t->socket) {
		unsigned char decoded[TCP_RX_CHUNK_MAX];
		if (length < 0 || length > sizeof(decoded) || strlen(line + offset) < (size_t)length * 2 + 2 ||
		    line[offset + length * 2] != ',') {
			ESP_LOGE(TAG, "Invalid NSORF frame: payload=%d encoded=%u", length,
				 (unsigned)strlen(line + offset));
			xEventGroupSetBits(t->events, CLOSED);
			return;
		}
		for (int i = 0; i < length; i++) {
			int high = hex_digit(line[offset + i * 2]);
			int low = hex_digit(line[offset + i * 2 + 1]);
			if (high < 0 || low < 0) {
				ESP_LOGE(TAG, "Invalid NSORF hex at byte %d", i);
				xEventGroupSetBits(t->events, CLOSED);
				return;
			}
			decoded[i] = (high << 4) | low;
		}
		if (xStreamBufferSend(t->incoming, decoded, length, 0) != length) {
			ESP_LOGE(TAG, "Host TLS RX overflow");
			xEventGroupSetBits(t->events, CLOSED);
		}
	}
}

static void reader(void *argument)
{
	struct transport *t = argument;
	size_t used = 0;
	bool overflow = false;
	while (!t->stop) {
		unsigned char data[128];
		int n = uart_read_bytes(MODEM, data, sizeof(data), pdMS_TO_TICKS(20));
		if (n < 0) {
			xEventGroupSetBits(t->events, CLOSED);
			break;
		}
		for (int i = 0; i < n; i++) {
			if (data[i] == '\r' || data[i] == '\n') {
				t->line[used] = '\0';
				if (overflow) {
					ESP_LOGE(TAG, "UART response exceeds %u bytes", (unsigned)sizeof(t->line) - 1);
					xEventGroupSetBits(t->events, CLOSED);
				}
				else if (used) parse_line(t, t->line);
				used = 0;
				overflow = false;
			} else if (used < sizeof(t->line) - 1) {
				t->line[used++] = data[i];
			} else overflow = true;
		}
	}
	xEventGroupSetBits(t->events, READER_EXIT);
	vTaskDelete(NULL);
}

static int command(struct transport *t, const char *text, int timeout_ms)
{
	xEventGroupClearBits(t->events, AT_OK | AT_ERROR);
	size_t size = strlen(text);
	if (uart_write_bytes(MODEM, text, size) != size || uart_write_bytes(MODEM, "\r\n", 2) != 2) return -1;
	EventBits_t bits = xEventGroupWaitBits(t->events, AT_OK | AT_ERROR, pdTRUE, pdFALSE,
					     pdMS_TO_TICKS(timeout_ms));
	if (bits & AT_ERROR) return -EIO; /* parse_line already logged the actual rejection. */
	if (!(bits & AT_OK)) {
		ESP_LOGE(TAG, "AT transaction timed out after %d ms", timeout_ms);
		return -ETIMEDOUT;
	}
	return 0;
}

static int await_event(struct transport *t, EventBits_t expected, int timeout_ms)
{
	EventBits_t bits = xEventGroupWaitBits(t->events, expected | CLOSED | AT_ERROR,
					     pdFALSE, pdFALSE, pdMS_TO_TICKS(timeout_ms));
	return (bits & expected) && !(bits & (CLOSED | AT_ERROR)) ? 0 : -1;
}

static int switch_baud(struct transport *t, int baud)
{
	char text[64];
	int n = snprintf(text, sizeof(text), "AT+NATSPEED=%d,0,0\r\n", baud);
	/* NATSPEED changes immediately; its OK may arrive at either speed.
	 * Verify using a new AT transaction after both UARTs have switched.
	 */
	if (uart_write_bytes(MODEM, text, n) != n ||
	    uart_wait_tx_done(MODEM, pdMS_TO_TICKS(2000)) != ESP_OK ||
	    uart_set_baudrate(MODEM, baud) != ESP_OK) return -1;
	vTaskDelay(pdMS_TO_TICKS(100));
	return command(t, "AT", 2000);
}

static int tcp_send(void *context, const unsigned char *buffer, size_t length)
{
	struct transport *t = context;
	if (xEventGroupGetBits(t->events) & CLOSED) return MBEDTLS_ERR_NET_SEND_FAILED;
	size_t count = length < 384 ? length : 384;
	int n = snprintf(t->command, sizeof(t->command), "AT+NSOSD=%d,%u,", t->socket, (unsigned)count);
	for (size_t i = 0; i < count; i++) {
		snprintf(t->command + n, sizeof(t->command) - n, "%02X", buffer[i]);
		n += 2;
	}
	t->sent = -1;
	if (command(t, t->command, 30000) || t->sent != count) return MBEDTLS_ERR_NET_SEND_FAILED;
	return count;
}

static int tcp_receive(void *context, unsigned char *buffer, size_t length, uint32_t timeout_ms)
{
	struct transport *t = context;
	if (xEventGroupGetBits(t->events) & CLOSED) return MBEDTLS_ERR_NET_RECV_FAILED;
	size_t n = xStreamBufferReceive(t->incoming, buffer, length, pdMS_TO_TICKS(timeout_ms ? timeout_ms : 1000));
	if (!n && (xEventGroupGetBits(t->events) & CLOSED)) return MBEDTLS_ERR_NET_RECV_FAILED;
	return n ? (int)n : MBEDTLS_ERR_SSL_TIMEOUT;
}

static int stream_read(void *context, unsigned char *data, size_t size)
{
	int ret = mbedtls_ssl_read(context, data, size);
	if (ret == MBEDTLS_ERR_SSL_TIMEOUT) return -ETIMEDOUT;
	if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) return -EAGAIN;
	return ret < 0 ? -EIO : ret;
}

static int stream_write(void *context, const unsigned char *data, size_t size)
{
	int ret = mbedtls_ssl_write(context, data, size);
	return ret < 0 ? -EIO : ret;
}

static int tls_session_locked(const bepio_mn316_transport_config_t *config,
	int (*session)(void *, const bepio_mn316_stream_t *), void *context)
{
	int restore_baud = config->restore_baud > 0 ? config->restore_baud : BEPIO_MN316_BAUD_RATE;
	struct transport *t = calloc(1, sizeof(*t));
	if (!t) return -ENOMEM;
	t->socket = -1;
	t->events = xEventGroupCreate();
	t->incoming = xStreamBufferCreate(32768, 1);
	bool running = false;
	bool fast_uart = false;
	int ret = -1;
	mbedtls_ssl_context ssl;
	mbedtls_ssl_config conf;
	mbedtls_x509_crt ca;
	mbedtls_entropy_context entropy;
	mbedtls_ctr_drbg_context drbg;
	mbedtls_ssl_init(&ssl); mbedtls_ssl_config_init(&conf); mbedtls_x509_crt_init(&ca);
	mbedtls_entropy_init(&entropy); mbedtls_ctr_drbg_init(&drbg);
	if (!t->events || !t->incoming || xTaskCreate(reader, "mn316_tls_rx", 6144, t, 12, NULL) != pdPASS) goto cleanup;
	running = true;
	setenv("TZ", "UTC0", 1); tzset();
	ESP_LOGI(TAG, "Exclusive UART1 cellular transport; no Wi-Fi/network sockets");
	if (command(t, "ATE0", 2000)) goto cleanup;
	/* The modem has only 1700 bytes of queued TCP RX. Hex certificate
	 * bursts cannot drain reliably at 9600 baud, even with a host RX task.
	 * Use a non-persistent fast UART and restore the configured speed.
	 */
	if (command(t, "AT+STANDBY=0,1200", 3000)) goto cleanup;
	fast_uart = true;
	if (switch_baud(t, 115200)) goto cleanup;
	ESP_LOGI(TAG, "Modem UART temporarily switched to 115200 for TLS");
	/* No built-in client is normally allocated: DISC/DEL in state 0 produces
	 * CME 8002. Preserve real query/cleanup errors rather than masking them.
	 */
	t->mqtt_state = -1;
	if (command(t, "AT+MQTTSTAT?", 3000) || t->mqtt_state < 0) {
		ESP_LOGE(TAG, "Cannot query internal MQTT state");
		goto cleanup;
	}
	if (t->mqtt_state == 0) {
		ESP_LOGI(TAG, "No internal MQTT client; skipping DISC/DEL");
	} else {
		if (t->mqtt_state >= 3) {
			if (command(t, "AT+MQTTDISC", 5000)) goto cleanup;
			vTaskDelay(pdMS_TO_TICKS(1000));
		}
		if (command(t, "AT+MQTTDEL", 5000)) goto cleanup;
		t->mqtt_state = -1;
		if (command(t, "AT+MQTTSTAT?", 3000) || t->mqtt_state != 0) {
			ESP_LOGE(TAG, "Internal MQTT client cleanup did not reach state 0");
			goto cleanup;
		}
	}
	xEventGroupClearBits(t->events, CLOCK_READY | CLOSED | AT_ERROR);
	if (command(t, "AT+CCLK?", 3000) || await_event(t, CLOCK_READY, 3000)) {
		ESP_LOGE(TAG, "No valid modem network time; certificate date checks remain enabled");
		goto cleanup;
	}
	struct timeval tv = {.tv_sec = t->network_time + (esp_timer_get_time() - t->network_time_at) / 1000000};
	if (settimeofday(&tv, NULL)) goto cleanup;
	ESP_LOGI(TAG, "Network UTC epoch=%lld", (long long)tv.tv_sec);
	xEventGroupClearBits(t->events, DNS_READY);
	snprintf(t->command, sizeof(t->command), "AT+CMDNS=\"%s\"", config->host);
	if (command(t, t->command, 5000) || await_event(t, DNS_READY, 120000)) {
		ESP_LOGE(TAG, "Cellular DNS failed"); goto cleanup;
	}
	ESP_LOGI(TAG, "Cellular DNS %s -> %s", config->host, t->ip);
	if (command(t, "AT+NSOCR=STREAM,6,0,2", 5000) || t->socket < 0) goto cleanup;
	snprintf(t->command, sizeof(t->command), "AT+NSOCFG=%d,1,1", t->socket);
	if (command(t, t->command, 5000)) goto cleanup;
	xEventGroupClearBits(t->events, TCP_READY | CLOSED);
	snprintf(t->command, sizeof(t->command), "AT+NSOCO=%d,%s,%u", t->socket, t->ip, (unsigned)config->port);
	if (command(t, t->command, 5000) || await_event(t, TCP_READY, 60000)) {
		ESP_LOGE(TAG, "Cellular TCP connect failed"); goto cleanup;
	}
	ESP_LOGI(TAG, "MN316 TCP connected; starting verified TLS handshake");
	ret = mbedtls_ctr_drbg_seed(&drbg, mbedtls_entropy_func, &entropy,
				  (const unsigned char *)"bepio-mn316-tls", 16);
	if (ret) goto cleanup;
	ret = mbedtls_x509_crt_parse(&ca, config->ca, config->ca_size);
	if (ret) goto cleanup;
	ret = mbedtls_ssl_config_defaults(&conf, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT);
	if (ret) goto cleanup;
	mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_REQUIRED);
	mbedtls_ssl_conf_ca_chain(&conf, &ca, NULL);
	mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &drbg);
	mbedtls_ssl_conf_read_timeout(&conf, 30000);
	ret = mbedtls_ssl_setup(&ssl, &conf);
	if (ret) goto cleanup;
	ret = mbedtls_ssl_set_hostname(&ssl, config->host);
	if (ret) goto cleanup;
	mbedtls_ssl_set_bio(&ssl, t, tcp_send, NULL, tcp_receive);
	int64_t handshake_deadline = esp_timer_get_time() + 180000000;
	do {
		ret = mbedtls_ssl_handshake(&ssl);
	} while ((ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) &&
		 esp_timer_get_time() < handshake_deadline);
	if (ret) goto cleanup;
	if (mbedtls_ssl_get_verify_result(&ssl)) {
		ret = MBEDTLS_ERR_X509_CERT_VERIFY_FAILED;
		goto cleanup;
	}
	ESP_LOGI(TAG, "TLS PASS %s %s; CA, hostname and validity verified", mbedtls_ssl_get_version(&ssl), mbedtls_ssl_get_ciphersuite(&ssl));
	mbedtls_ssl_conf_read_timeout(&conf, 1000);
	const bepio_mn316_stream_t stream = {.context = &ssl, .read = stream_read, .write = stream_write};
	ret = session(context, &stream);
	if (!ret) mbedtls_ssl_close_notify(&ssl);
cleanup:
	if (running) {
		if (t->socket >= 0) {
			snprintf(t->command, sizeof(t->command), "AT+NSOCL=%d", t->socket);
			command(t, t->command, 5000);
		}
		if (fast_uart) {
			if (switch_baud(t, restore_baud)) {
				ESP_LOGE(TAG, "UART restore failed; power-cycle modem before retry");
				ret = -1;
			} else {
				command(t, "AT+STANDBY=1", 3000);
				ESP_LOGI(TAG, "Modem UART restored to %d", restore_baud);
			}
		}
		t->stop = true;
		/* Reader exits after at most one 20 ms UART read. */
		xEventGroupWaitBits(t->events, READER_EXIT, pdFALSE, pdTRUE, portMAX_DELAY);
	}
	mbedtls_ssl_free(&ssl); mbedtls_ssl_config_free(&conf); mbedtls_x509_crt_free(&ca);
	mbedtls_ctr_drbg_free(&drbg); mbedtls_entropy_free(&entropy);
	if (t->incoming) vStreamBufferDelete(t->incoming);
	if (t->events) vEventGroupDelete(t->events);
	free(t);
	if (ret) {
		char reason[160];
		mbedtls_strerror(ret, reason, sizeof(reason));
		ESP_LOGE(TAG, "TLS/session failed ret=%d (%s)", ret, reason);
	} else ESP_LOGI(TAG, "TLS session completed");
	return ret;
}


int bepio_mn316_driver_tls(const bepio_mn316_transport_config_t *config,
	int (*session)(void *, const bepio_mn316_stream_t *), void *context)
{
	if (!config || !config->host || !config->host[0] || strlen(config->host) > 127 ||
	    !config->port || !config->ca || !config->ca_size || !session) return -EINVAL;
	for (const char *p = config->host; *p; p++) {
		if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
		      (*p >= '0' && *p <= '9') || *p == '.' || *p == '-')) return -EINVAL;
	}
	int ret = bepio_mn316_driver_acquire();
	if (ret) return ret;
	ret = tls_session_locked(config, session, context);
	bepio_mn316_driver_release();
	return ret;
}
