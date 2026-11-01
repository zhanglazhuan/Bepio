#include "mn316_mqtt.h"
#include "mn316_network.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

enum { ACK, WRONG_ACK, SEND_FAIL, RX_FAIL, RX_FULL, PING_TIMEOUT, INVALID_TO,
	NUMBER };
static int mode, takes, completed, accepted, incoming, online_count, rx_acks;
static size_t tx_bytes, rx_bytes;
static int64_t now;
static unsigned char packet[2];

int bepio_pager_message_now(struct bepio_pager_message *message)
{
	strcpy(message->datetime, "2026-11-01T00:00:00Z");
	return 0;
}
int bepio_pager_message_encode(const struct bepio_pager_message *message, char *json, size_t capacity)
{
	assert(!strcmp(message->from, "100000000000001"));
	if (mode == NUMBER) assert(!strcmp(message->to, "13800138000"));
	assert(capacity > 5);
	strcpy(json, "test");
	return 0;
}
int bepio_pager_message_parse(const unsigned char *data, size_t size, struct bepio_pager_message *message)
{
	assert(size == 4 && !memcmp(data, "test", size));
	strcpy(message->to, "100000000000001");
	strcpy(message->from, "100000000000002");
	return 0;
}
static void connected(bool value) { online_count += value ? 1 : -1; }
static int take(struct bepio_pager_message *message)
{
	if (takes++ || mode == RX_FAIL || mode == RX_FULL || mode == PING_TIMEOUT) return 0;
	memset(message, 0, sizeof(*message));
	strcpy(message->from, "100000000000001");
	strcpy(message->to, mode == INVALID_TO ? "123/456" : mode == NUMBER ?
		"13800138000" : "100000000000002");
	return 1;
}
static void complete(bool acknowledged) { completed++; accepted += acknowledged; }
static int receive_message(const struct bepio_pager_message *message)
{
	assert(!strcmp(message->from, "100000000000002"));
	incoming++;
	return mode == RX_FULL ? -1 : 0;
}
static void record(size_t tx, size_t rx) { tx_bytes += tx; rx_bytes += rx; }
static int send_packet(void *context, unsigned char type, const unsigned char *body, size_t size)
{
	(void)context;
	if (type == 0x32) {
		const char *topic = mode == NUMBER ? "pager/demo/13800138000/inbox" :
			"pager/demo/100000000000002/inbox";
		size_t length = strlen(topic);
		assert(size == length + 8 && body[0] == 0 && body[1] == length);
		assert(!memcmp(body + 2, topic, length));
		memcpy(packet, body + 2 + length, 2);
		if (mode == SEND_FAIL) return -1;
	} else if (type == 0x40) {
		assert(size == 2 && body[0] == 0 && body[1] == 7);
		rx_acks++;
	} else assert(type == 0xc0);
	return 0;
}
static int receive_packet(void *context, unsigned char *type, unsigned char *body,
			  size_t *size, int timeout_ms)
{
	(void)context;
	assert(timeout_ms == 1000);
	now += 1000000;
	if (mode == RX_FAIL) return -1;
	if (now == 1000000 && (mode == ACK || mode == RX_FULL)) {
		const char *topic = "pager/demo/100000000000001/inbox";
		size_t length = strlen(topic);
		*type = 0x32; body[0] = 0; body[1] = length;
		memcpy(body + 2, topic, length);
		body[2 + length] = 0; body[3 + length] = 7;
		memcpy(body + 4 + length, "test", 4);
		*size = length + 8;
		return 1;
	}
	if (now == 2000000 && (mode == ACK || mode == WRONG_ACK || mode == NUMBER)) {
		*type = 0x40; *size = 2; memcpy(body, packet, 2);
		if (mode == WRONG_ACK) body[1]++;
		return 1;
	}
	if ((mode == ACK || mode == NUMBER) && now > 2000000) return -1;
	assert(now < 40000000);
	return 0;
}
static int64_t clock_us(void *context) { (void)context; return now; }

int main(void)
{
	assert(bepio_mn316_internal_mqtt_state("+MQTTSTAT:0\r\nOK") == 0);
	assert(bepio_mn316_internal_mqtt_state("+MQTTSTAT: 5\r\nOK") == 5);
	assert(bepio_mn316_internal_mqtt_state("+MQTTSTAT:8\r\nOK") == -1);
	assert(bepio_mn316_internal_mqtt_state("OK") == -1);
	assert(bepio_mn316_registered("+CEREG:0,1\r\nOK"));
	assert(bepio_mn316_registered("+CEREG: 1,5\r\nOK"));
	assert(!bepio_mn316_registered("+CEREG:0,2\r\nOK"));
	assert(bepio_mn316_attached("+CGATT:1\r\nOK"));
	assert(bepio_mn316_attached("+CGATT: 1\r\nOK"));
	assert(!bepio_mn316_attached("+CGATT:0\r\nOK"));
	assert(bepio_mn316_has_ip("+CGPADDR:0,\"10.208.183.102\"\r\nOK"));
	assert(bepio_mn316_has_ip("+CGPADDR: 0,10.1.2.3\r\nOK"));
	assert(!bepio_mn316_has_ip("+CGPADDR:0,\"0.0.0.0\"\r\nOK"));
	assert(!bepio_mn316_has_ip("+CGPADDR:0,\"999.1.2.3\"\r\nOK"));
	const bepio_mn316_mqtt_io_t io = {
		.context = NULL, .device_id = "100000000000001",
		.inbox = "pager/demo/100000000000001/inbox",
		.send = send_packet, .receive = receive_packet, .now_us = clock_us,
	};
	const bepio_mn316_mqtt_hooks_t hooks = {connected, take, complete, receive_message, record};
	for (mode = ACK; mode <= NUMBER; mode++) {
		takes = completed = accepted = incoming = online_count = rx_acks = 0;
		tx_bytes = rx_bytes = 0; now = 0;
		assert(bepio_mn316_mqtt_session(&io, &hooks) != 0);
		assert(online_count == 0);
		if (mode == ACK || mode == NUMBER) {
			assert(completed == 1 && accepted == 1 && tx_bytes == 4);
			if (mode == ACK) assert(incoming == 1 && rx_acks == 1 && rx_bytes == 4);
		}
		if (mode == WRONG_ACK) assert(completed == 1 && accepted == 0 && now == 30000000);
		if (mode == SEND_FAIL || mode == INVALID_TO) assert(completed == 1 && accepted == 0);
		if (mode == RX_FULL) assert(incoming == 1 && rx_acks == 0);
		if (mode == PING_TIMEOUT) assert(completed == 0 && now == 35000000);
	}
	puts("PASS: MQTT routing, QoS1 PUBACK correlation, timeout, RX ACK/storage failure, disconnect, heartbeat");
	return 0;
}
