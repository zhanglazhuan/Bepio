#include "mn316_tls.h"
#include "pager_message.h"

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
#include "sdkconfig.h"
#include "bepio_devkit.h"
#ifndef CONFIG_BEPIO_MN316_BAUD
#define CONFIG_BEPIO_MN316_BAUD BEPIO_MN316_BAUD_RATE
#endif

#if __has_include("emqx_local.h")
#include "emqx_local.h"
#endif
#ifndef BEPIO_EMQX_USERNAME
#define BEPIO_EMQX_USERNAME ""
#define BEPIO_EMQX_PASSWORD ""
#endif

#define MODEM UART_NUM_1
#define HOST "p235d1e1.ala.cn-hangzhou.emqxsl.cn"
#define TOPIC_UP "pager/demo/PC/inbox"
#define TOPIC_DOWN "pager/demo/MN316/inbox"
#define TOPIC_ACK TOPIC_UP
#define AT_OK BIT0
#define AT_ERROR BIT1
#define TCP_READY BIT2
#define DNS_READY BIT3
#define CLOSED BIT4
#define CLOCK_READY BIT5
#define READER_EXIT BIT6

extern const unsigned char ca_start[] asm("_binary_emqx_ca_pem_start");
extern const unsigned char ca_end[] asm("_binary_emqx_ca_pem_end");
static const char *TAG = "mn316_tls";

/* The console command owns UART1 until the reader has exited. The RX task
 * drains automatic NSORF URCs concurrently with slow TLS writes.
 */
struct transport {
	EventGroupHandle_t events;
	StreamBufferHandle_t incoming;
	volatile bool stop;
	int socket;
	int sent;
	char ip[16];
	time_t network_time;
	int64_t network_time_at;
	char line[3100];
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
	} else if (sscanf(line, "+NSOCR: %d", &id) == 1) {
		t->socket = id;
	} else if (sscanf(line, "+NSOSD: %d,%d", &id, &length) == 2 && id == t->socket) {
		t->sent = length;
	} else if (sscanf(line, "+NSOCLI: %d", &id) == 1 && id == t->socket) {
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
		unsigned char decoded[1400];
		if (length < 0 || length > sizeof(decoded) || strlen(line + offset) < (size_t)length * 2 + 2 ||
		    line[offset + length * 2] != ',') {
			xEventGroupSetBits(t->events, CLOSED);
			return;
		}
		for (int i = 0; i < length; i++) {
			int high = hex_digit(line[offset + i * 2]);
			int low = hex_digit(line[offset + i * 2 + 1]);
			if (high < 0 || low < 0) {
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
				if (overflow) xEventGroupSetBits(t->events, CLOSED);
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
	if (!(bits & AT_OK) || bits & AT_ERROR) {
		ESP_LOGE(TAG, "AT transaction failed/timeout");
		return -1;
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

struct mqtt {
	mbedtls_ssl_context *ssl;
	unsigned char pending[2048];
	size_t used;
};

static int tls_write_all(mbedtls_ssl_context *ssl, const unsigned char *data, size_t size)
{
	while (size) {
		int n = mbedtls_ssl_write(ssl, data, size);
		if (n <= 0) return n ? n : -1;
		data += n;
		size -= n;
	}
	return 0;
}

static int mqtt_send(struct mqtt *m, unsigned char type, const unsigned char *body, size_t size)
{
	unsigned char packet[1200];
	if (size > sizeof(packet) - 5) return -1;
	size_t n = size, used = 0;
	packet[used++] = type;
	do {
		unsigned char digit = n % 128;
		n /= 128;
		packet[used++] = digit | (n ? 128 : 0);
	} while (n);
	memcpy(packet + used, body, size);
	return tls_write_all(m->ssl, packet, used + size);
}

static size_t mqtt_field(unsigned char *out, const char *text)
{
	size_t n = strlen(text);
	out[0] = n >> 8;
	out[1] = n;
	memcpy(out + 2, text, n);
	return n + 2;
}

/* Retain partial MQTT packets across TCP/TLS read timeouts. */
static int mqtt_receive(struct mqtt *m, unsigned char *type, unsigned char *body, size_t *size, int timeout_ms)
{
	int64_t deadline = esp_timer_get_time() + (int64_t)timeout_ms * 1000;
	do {
		size_t remaining = 0, h = 1;
		bool complete = false;
		while (h < m->used && h <= 4) {
			unsigned char digit = m->pending[h++];
			remaining |= (size_t)(digit & 127) << (7 * (h - 2));
			if (!(digit & 128)) { complete = true; break; }
		}
		if ((!complete && h == 5) || remaining > 1024) return -1;
		if (complete && m->used >= h + remaining) {
			*type = m->pending[0];
			*size = remaining;
			memcpy(body, m->pending + h, remaining);
			m->used -= h + remaining;
			memmove(m->pending, m->pending + h + remaining, m->used);
			return 1;
		}
		int n = mbedtls_ssl_read(m->ssl, m->pending + m->used, sizeof(m->pending) - m->used);
		if (n > 0) m->used += n;
		else if (n != MBEDTLS_ERR_SSL_TIMEOUT && n != MBEDTLS_ERR_SSL_WANT_READ) return -1;
	} while (esp_timer_get_time() < deadline);
	return 0;
}

static int publish(struct mqtt *m, const char *topic, const unsigned char *payload, size_t size)
{
	unsigned char body[1024];
	if (strlen(topic) + 2 + size > sizeof(body)) return -1;
	size_t n = mqtt_field(body, topic);
	memcpy(body + n, payload, size);
	return mqtt_send(m, 0x30, body, n + size);
}

static int mqtt_test(mbedtls_ssl_context *ssl, const bepio_mn316_tls_credentials_t *credentials, int seconds)
{
	struct mqtt *m = calloc(1, sizeof(*m));
	if (!m) return -1;
	m->ssl = ssl;
	unsigned char body[1024], type;
	size_t size;
	int ret = -1;
	size_t n = mqtt_field(body, "MQTT");
	body[n++] = 4; body[n++] = 0xc2; body[n++] = 0; body[n++] = 60;
	n += mqtt_field(body + n, BEPIO_PAGER_DEVICE);
	n += mqtt_field(body + n, credentials->username);
	n += mqtt_field(body + n, credentials->password);
	if (mqtt_send(m, 0x10, body, n) || mqtt_receive(m, &type, body, &size, 30000) != 1 ||
	    type != 0x20 || size != 2 || body[1] != 0) {
		ESP_LOGE(TAG, "MQTT CONNACK rejected or timed out");
		goto out;
	}
	ESP_LOGI(TAG, "MQTT login PASS via MN316 TCP + ESP32 TLS");
	body[0] = 0; body[1] = 1;
	n = 2 + mqtt_field(body + 2, TOPIC_DOWN);
	body[n++] = 0;
	if (mqtt_send(m, 0x82, body, n) || mqtt_receive(m, &type, body, &size, 30000) != 1 ||
	    type != 0x90 || size != 3 || body[0] != 0 || body[1] != 1 || body[2] != 0) {
		ESP_LOGE(TAG, "MQTT SUBACK failed");
		goto out;
	}
	struct bepio_pager_message message = {0};
	char json[BEPIO_PAGER_WIRE_MAX];
	strcpy(message.from, BEPIO_PAGER_DEVICE);
	strcpy(message.to, "Pager_PC");
	strcpy(message.text, "MN316 TLS test message");
	if (bepio_pager_message_now(&message) || bepio_pager_message_encode(&message, json, sizeof(json)) ||
	    publish(m, TOPIC_UP, (const unsigned char *)json, strlen(json))) goto out;
	ESP_LOGI(TAG, "Subscribed %s; published %s", TOPIC_DOWN, TOPIC_UP);
	ESP_LOGI(TAG, "Listening %ds; incoming messages will be echoed to %s", seconds, TOPIC_ACK);
	int64_t deadline = esp_timer_get_time() + (int64_t)seconds * 1000000;
	int64_t next_ping = esp_timer_get_time() + 20000000;
	while (esp_timer_get_time() < deadline) {
		int received = mqtt_receive(m, &type, body, &size, 1000);
		if (received < 0) goto out;
		if (received && type == 0x30 && size >= 2) {
			size_t topic_size = ((size_t)body[0] << 8) | body[1];
			if (topic_size + 2 > size) goto out;
			if (topic_size == strlen(TOPIC_DOWN) && !memcmp(body + 2, TOPIC_DOWN, topic_size)) {
				unsigned char *payload = body + 2 + topic_size;
				size_t count = size - 2 - topic_size;
				if (bepio_pager_message_parse(payload, count, &message)) {
					ESP_LOGW(TAG, "MESSAGE REJECTED: invalid From/To/Text/Datetime/At");
					goto heartbeat;
				}
				if (strcmp(message.to, BEPIO_PAGER_DEVICE)) {
					ESP_LOGW(TAG, "MESSAGE IGNORED: To is not this device");
					goto heartbeat;
				}
				ESP_LOGI(TAG, "DOWNLINK PASS bytes=%u", (unsigned)count);
				/* Reply location belongs to this device, not to the sender.
				 * No location sensor is connected in this test application.
				 */
				strcpy(message.to, message.from);
				strcpy(message.from, BEPIO_PAGER_DEVICE);
				message.has_location = false;
				message.address[0] = '\0';
				if (bepio_pager_message_now(&message) ||
				    bepio_pager_message_encode(&message, json, sizeof(json)) ||
				    publish(m, TOPIC_ACK, (const unsigned char *)json, strlen(json))) goto out;
				ESP_LOGI(TAG, "Pager reply sent over verified TLS");
			}
		} else if (received && type == 0xd0) ESP_LOGI(TAG, "PINGRESP");
heartbeat:
		if (esp_timer_get_time() >= next_ping) {
			if (mqtt_send(m, 0xc0, body, 0)) goto out;
			next_ping = esp_timer_get_time() + 20000000;
		}
	}
	ret = mqtt_send(m, 0xe0, body, 0);
out:
	memset(body, 0, sizeof(body));
	free(m);
	return ret;
}

int bepio_mn316_tls_run(const bepio_mn316_tls_credentials_t *credentials, int seconds)
{
	int restore_baud = credentials && credentials->uart_baud > 0 ?
		credentials->uart_baud : BEPIO_MN316_BAUD_RATE;
	if (!credentials || !credentials->username || !credentials->password ||
	    !credentials->username[0] || !credentials->password[0] ||
	    strlen(credentials->username) > 128 || strlen(credentials->password) > 128 ||
	    seconds < 1 || seconds > 600) {
		ESP_LOGE(TAG, "Invalid MQTT credentials or listen duration");
		return 1;
	}
	struct transport *t = calloc(1, sizeof(*t));
	if (!t) return 1;
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
	/* The internal MQTT client is independent of our TLS/MQTT session. */
	command(t, "AT+MQTTDISC", 5000);
	vTaskDelay(pdMS_TO_TICKS(1000));
	command(t, "AT+MQTTDEL", 5000);
	xEventGroupClearBits(t->events, CLOCK_READY | CLOSED | AT_ERROR);
	if (command(t, "AT+CCLK?", 3000) || await_event(t, CLOCK_READY, 3000)) {
		ESP_LOGE(TAG, "No valid modem network time; certificate date checks remain enabled");
		goto cleanup;
	}
	struct timeval tv = {.tv_sec = t->network_time + (esp_timer_get_time() - t->network_time_at) / 1000000};
	if (settimeofday(&tv, NULL)) goto cleanup;
	ESP_LOGI(TAG, "Network UTC epoch=%lld", (long long)tv.tv_sec);
	xEventGroupClearBits(t->events, DNS_READY);
	if (command(t, "AT+CMDNS=\"" HOST "\"", 5000) || await_event(t, DNS_READY, 120000)) {
		ESP_LOGE(TAG, "Cellular DNS failed"); goto cleanup;
	}
	ESP_LOGI(TAG, "Cellular DNS %s -> %s", HOST, t->ip);
	if (command(t, "AT+NSOCR=STREAM,6,0,2", 5000) || t->socket < 0) goto cleanup;
	snprintf(t->command, sizeof(t->command), "AT+NSOCFG=%d,1,1", t->socket);
	if (command(t, t->command, 5000)) goto cleanup;
	xEventGroupClearBits(t->events, TCP_READY | CLOSED);
	snprintf(t->command, sizeof(t->command), "AT+NSOCO=%d,%s,8883", t->socket, t->ip);
	if (command(t, t->command, 5000) || await_event(t, TCP_READY, 60000)) {
		ESP_LOGE(TAG, "Cellular TCP connect failed"); goto cleanup;
	}
	ESP_LOGI(TAG, "MN316 TCP connected; starting verified TLS handshake");
	ret = mbedtls_ctr_drbg_seed(&drbg, mbedtls_entropy_func, &entropy,
				  (const unsigned char *)"bepio-mn316-tls", 16);
	if (ret) goto cleanup;
	ret = mbedtls_x509_crt_parse(&ca, ca_start, ca_end - ca_start);
	if (ret) goto cleanup;
	ret = mbedtls_ssl_config_defaults(&conf, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT);
	if (ret) goto cleanup;
	mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_REQUIRED);
	mbedtls_ssl_conf_ca_chain(&conf, &ca, NULL);
	mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &drbg);
	mbedtls_ssl_conf_read_timeout(&conf, 30000);
	ret = mbedtls_ssl_setup(&ssl, &conf);
	if (ret) goto cleanup;
	ret = mbedtls_ssl_set_hostname(&ssl, HOST);
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
	ret = mqtt_test(&ssl, credentials, seconds);
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
		ESP_LOGE(TAG, "TLS/MQTT test failed ret=%d (%s)", ret, reason);
	} else ESP_LOGI(TAG, "TLS/MQTT session completed");
	return ret ? 1 : 0;
}

int bepio_mn316_tls_command(int argc, char **argv)
{
	int seconds = 120;
	if (argc > 2) return 1;
	if (argc == 2) {
		char *end;
		long value = strtol(argv[1], &end, 10);
		if (!argv[1][0] || *end || value < 1 || value > 600) return 1;
		seconds = (int)value;
	}
	const bepio_mn316_tls_credentials_t credentials = {
		.username = BEPIO_EMQX_USERNAME,
		.password = BEPIO_EMQX_PASSWORD,
		.uart_baud = CONFIG_BEPIO_MN316_BAUD,
	};
	return bepio_mn316_tls_run(&credentials, seconds);
}
