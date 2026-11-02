#include "mn316_mqtt.h"
#include "../../modules/pager_id.h"
#include <stdio.h>
#include <string.h>

#include "bepio_mn316_config.h"

static size_t mqtt_field(unsigned char *out, const char *text)
{
	size_t size = strlen(text);
	out[0] = size >> 8; out[1] = size & 255;
	memcpy(out + 2, text, size);
	return size + 2;
}

/* Application session: QoS 1 publishes complete only on matching PUBACK. */
int bepio_mn316_mqtt_session(const bepio_mn316_mqtt_io_t *io,
			     const bepio_mn316_mqtt_hooks_t *hooks)
{
	unsigned char body[1024], type;
	struct bepio_pager_message message;
	char json[BEPIO_PAGER_WIRE_MAX];
	bool pending = false;
	uint16_t packet = 1;
	int ret = -1;
	int64_t ack_deadline = 0;
	int64_t ping_deadline = 0;
	int64_t next_ping = io->now_us(io->context) + 20000000;
	hooks->online(true);
	for (;;) {
		if (!pending) {
			int taken = hooks->take(&message);
			if (taken < 0) goto out;
			if (taken) {
				pending = true;
				char topic[128];
				if (!bepio_pager_id_valid(message.to, BEPIO_PAGER_NAME_MAX)) goto out;
				snprintf(topic, sizeof(topic), BEPIO_MN316_TOPIC_PREFIX "%.64s/inbox",
					message.to);
				if (bepio_pager_message_encode(&message, json, sizeof(json))) goto out;
				size_t n = mqtt_field(body, topic);
				if (++packet == 0) packet = 2;
				body[n++] = packet >> 8; body[n++] = packet & 255;
				size_t length = strlen(json);
				if (n + length > sizeof(body)) goto out;
				memcpy(body + n, json, length);
				if (io->send(io->context, 0x32, body, n + length)) goto out;
				hooks->record(length, 0);
				ack_deadline = io->now_us(io->context) + 30000000;
			}
		}
		size_t size;
		int received = io->receive(io->context, &type, body, &size, 1000);
		if (received < 0) goto out;
		if (received && type == 0xd0 && size == 0) ping_deadline = 0;
		if (received && type == 0x40 && size == 2 && pending &&
		    (((unsigned)body[0] << 8) | body[1]) == packet) {
			hooks->complete(true);
			pending = false;
		} else if (received && (type & 0xf0) == 0x30 && size >= 2) {
			size_t topic_size = ((size_t)body[0] << 8) | body[1];
			unsigned qos = (type >> 1) & 3;
			size_t offset = 2 + topic_size;
			if (qos > 1 || offset + (qos ? 2 : 0) > size) goto out;
			unsigned char ack[2] = {0};
			if (qos) { memcpy(ack, body + offset, 2); offset += 2; }
				if (topic_size == strlen(io->inbox) && !memcmp(body + 2, io->inbox, topic_size)) {
				hooks->record(0, size - offset);
				if (!bepio_pager_message_parse(body + offset, size - offset, &message) &&
				    !strcmp(message.to, io->device_id)) {
					if (hooks->receive(&message)) goto out;
				}
			}
			if (qos && io->send(io->context, 0x40, ack, 2)) goto out;
		}
		if (pending && io->now_us(io->context) >= ack_deadline) goto out;
		if (ping_deadline && io->now_us(io->context) >= ping_deadline) goto out;
		if (io->now_us(io->context) >= next_ping) {
			if (io->send(io->context, 0xc0, body, 0)) goto out;
			next_ping = io->now_us(io->context) + 20000000;
			ping_deadline = io->now_us(io->context) + 15000000;
		}
	}
out:
	hooks->online(false);
	if (pending) hooks->complete(false);
	return ret;
}

