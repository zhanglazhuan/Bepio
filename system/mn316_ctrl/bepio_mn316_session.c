#include "bepio_mn316_session.h"
#include "bepio_mn316_config.h"
#include "../../modules/pager_id.h"
#include "mn316_mqtt.h"
#include "mn316.h"
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include "esp_timer.h"

extern const unsigned char ca_start[] asm("_binary_emqx_ca_pem_start");
extern const unsigned char ca_end[] asm("_binary_emqx_ca_pem_end");
static const char *TAG = "mn316_session";

struct mqtt {
	const bepio_mn316_stream_t *stream;
	unsigned char pending[2048];
	size_t used;
};

static int tls_write_all(const bepio_mn316_stream_t *stream, const unsigned char *data, size_t size)
{
	while (size) {
		int n = stream->write(stream->context, data, size);
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
	return tls_write_all(m->stream, packet, used + size);
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
		int n = m->stream->read(m->stream->context, m->pending + m->used, sizeof(m->pending) - m->used);
		if (n > 0) m->used += n;
		else if (n != -ETIMEDOUT && n != -EAGAIN) return -1;
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

static int session_send(void *context, unsigned char type, const unsigned char *body, size_t size)
{
	return mqtt_send(context, type, body, size);
}

static int session_receive(void *context, unsigned char *type, unsigned char *body,
			   size_t *size, int timeout_ms)
{
	return mqtt_receive(context, type, body, size, timeout_ms);
}

static int64_t session_now(void *context)
{
	(void)context;
	return esp_timer_get_time();
}

static int mqtt_test(const bepio_mn316_stream_t *stream, const bepio_mn316_tls_credentials_t *credentials, int seconds)
{
	if (!credentials || !bepio_pager_id_valid(credentials->device_id, BEPIO_PAGER_NAME_MAX)) return -EINVAL;
	char inbox[128];
	snprintf(inbox, sizeof(inbox), BEPIO_MN316_TOPIC_PREFIX "%.64s/inbox", credentials->device_id);
	struct mqtt *m = calloc(1, sizeof(*m));
	if (!m) return -1;
	m->stream = stream;
	unsigned char body[1024], type;
	size_t size;
	int ret = -1;
	size_t n = mqtt_field(body, "MQTT");
	body[n++] = 4; body[n++] = 0xc2; body[n++] = 0; body[n++] = 60;
	n += mqtt_field(body + n, credentials->device_id);
	n += mqtt_field(body + n, credentials->username);
	n += mqtt_field(body + n, credentials->password);
	if (mqtt_send(m, 0x10, body, n) || mqtt_receive(m, &type, body, &size, 30000) != 1 ||
	    type != 0x20 || size != 2) {
		ESP_LOGE(TAG, "MQTT CONNACK malformed or timed out");
		goto out;
	}
	if (body[1] != 0) {
		ESP_LOGE(TAG, "MQTT CONNACK rejected code=%u (4=bad credentials, 5=not authorized)", body[1]);
		ret = -EACCES;
		goto out;
	}
	ESP_LOGI(TAG, "MQTT login PASS via MN316 TCP + ESP32 TLS");
	body[0] = 0; body[1] = 1;
	n = 2 + mqtt_field(body + 2, inbox);
	body[n++] = credentials->hooks ? 1 : 0;
	if (mqtt_send(m, 0x82, body, n) || mqtt_receive(m, &type, body, &size, 30000) != 1 ||
	    type != 0x90 || size != 3 || body[0] != 0 || body[1] != 1 ||
	    body[2] != (credentials->hooks ? 1 : 0)) {
		ESP_LOGE(TAG, "MQTT SUBACK failed");
		goto out;
	}
	if (credentials->hooks) {
		const bepio_mn316_mqtt_io_t io = {
			.context = m, .device_id = credentials->device_id, .inbox = inbox,
			.send = session_send, .receive = session_receive, .now_us = session_now,
		};
		ret = bepio_mn316_mqtt_session(&io, credentials->hooks);
		goto out;
	}
	struct bepio_pager_message message = {0};
	char json[BEPIO_PAGER_WIRE_MAX];
	strcpy(message.from, credentials->device_id);
	strcpy(message.to, BEPIO_MN316_TEST_PEER);
	strcpy(message.text, credentials->test_text ? credentials->test_text : "MN316 TLS test message");
	if (bepio_pager_message_now(&message) || bepio_pager_message_encode(&message, json, sizeof(json)) ||
	    publish(m, BEPIO_MN316_TEST_INBOX, (const unsigned char *)json, strlen(json))) goto out;
	ESP_LOGI(TAG, "Subscribed %s; published %s", inbox, BEPIO_MN316_TEST_INBOX);
	ESP_LOGI(TAG, "Listening %ds; incoming messages will be echoed to %s", seconds, BEPIO_MN316_TEST_INBOX);
	int64_t deadline = esp_timer_get_time() + (int64_t)seconds * 1000000;
	int64_t next_ping = esp_timer_get_time() + 20000000;
	while (esp_timer_get_time() < deadline) {
		int received = mqtt_receive(m, &type, body, &size, 1000);
		if (received < 0) goto out;
		if (received && type == 0x30 && size >= 2) {
			size_t topic_size = ((size_t)body[0] << 8) | body[1];
			if (topic_size + 2 > size) goto out;
			if (topic_size == strlen(inbox) && !memcmp(body + 2, inbox, topic_size)) {
				unsigned char *payload = body + 2 + topic_size;
				size_t count = size - 2 - topic_size;
				if (bepio_pager_message_parse(payload, count, &message)) {
					ESP_LOGW(TAG, "MESSAGE REJECTED: invalid From/To/Text/Datetime/At");
					goto heartbeat;
				}
				if (strcmp(message.to, credentials->device_id)) {
					ESP_LOGW(TAG, "MESSAGE IGNORED: To is not this device");
					goto heartbeat;
				}
				ESP_LOGI(TAG, "DOWNLINK PASS bytes=%u", (unsigned)count);
				/* Reply location belongs to this device, not to the sender.
				 * No location sensor is connected in this test application.
				 */
				strcpy(message.to, message.from);
				strcpy(message.from, credentials->device_id);
				message.has_location = false;
				message.address[0] = '\0';
				if (bepio_pager_message_now(&message) ||
				    bepio_pager_message_encode(&message, json, sizeof(json)) ||
				    publish(m, BEPIO_MN316_TEST_INBOX, (const unsigned char *)json, strlen(json))) goto out;
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

struct session_config {
	const bepio_mn316_tls_credentials_t *credentials;
	int seconds;
};

static int run_session(void *context, const bepio_mn316_stream_t *stream)
{
	const struct session_config *config = context;
	return mqtt_test(stream, config->credentials, config->seconds);
}

int bepio_mn316_session_run(const bepio_mn316_tls_credentials_t *credentials, int seconds)
{
	if (!credentials || !credentials->username || !credentials->password ||
	    !credentials->username[0] || !credentials->password[0] ||
	    strlen(credentials->username) > 128 || strlen(credentials->password) > 128 ||
	    (credentials->test_text && strlen(credentials->test_text) > BEPIO_PAGER_TEXT_MAX) ||
	    seconds < 1 || seconds > 600 ||
	    (credentials->hooks && (!credentials->hooks->online || !credentials->hooks->take ||
	     !credentials->hooks->complete || !credentials->hooks->receive || !credentials->hooks->record))) {
		ESP_LOGE(TAG, "Invalid MQTT credentials or listen duration");
		return -EINVAL;
	}
	const bepio_mn316_transport_config_t transport = {
		.host = BEPIO_MN316_EMQX_HOST, .port = BEPIO_MN316_EMQX_PORT,
		.ca = ca_start, .ca_size = (size_t)(ca_end - ca_start),
		.restore_baud = credentials->uart_baud,
	};
	struct session_config session = {.credentials = credentials, .seconds = seconds};
	return bepio_mn316_driver_tls(&transport, run_session, &session);
}
