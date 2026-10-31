/* Explicit offline fixtures shared by firmware demo and the LVGL host preview. */
#ifndef BEPIO_MESSAGES_DEMO_H_
#define BEPIO_MESSAGES_DEMO_H_
#include "messaging.h"
#include <string.h>

static inline unsigned bepio_messages_demo_seed(void)
{
	static bool seeded;
	static unsigned fixture_count;
	if (seeded) { return fixture_count; }
	seeded = true;
	const bepio_message_payload_t incoming[] = {
		{.name = "Alex", .phone = "13800138000", .body = "下午三点喝咖啡吗？"},
		{.name = "Home", .phone = "13900139000", .body = "Dinner is ready. Let me know when you arrive."},
		{.name = "Bepio Lab", .phone = "13700137000", .body = "Your device is ready. Welcome to Bepio!"},
	};
	uint32_t id;
	const char *keys[] = {"bepio-demo-alex", "bepio-demo-home", "bepio-demo-lab"};
	for (size_t i = 0; i < sizeof(incoming) / sizeof(incoming[0]); i++) {
		bool present = false;
		bepio_message_t current;
		for (size_t j = 0; !bepio_messaging_get_at(BEPIO_MESSAGE_INBOX, j, &current); j++) {
			if (current.simulated && !strcmp(current.remote_id, keys[i]) &&
			    !strcmp(current.payload.phone, incoming[i].phone)) { present = true; break; }
		}
		if (!present) { (void)bepio_messaging_receive(keys[i], &incoming[i], 0, true, &id); }
	}
	bepio_message_t message;
	for (size_t i = 0; !bepio_messaging_get_at(BEPIO_MESSAGE_INBOX, i, &message); i++) {
		if (message.simulated && !strcmp(message.remote_id, keys[0])) {
			(void)bepio_messaging_get(BEPIO_MESSAGE_INBOX, message.id, true, &message);
			break;
		}
	}
	const bepio_message_payload_t reply = {
		.name = "Alex", .phone = "13800138000", .body = "好的，我三点到。"
	};
	bool present = false;
	for (size_t i = 0; !bepio_messaging_get_at(BEPIO_MESSAGE_OUTBOX, i, &message); i++) {
		if (!strcmp(message.payload.phone, reply.phone) &&
		    !strcmp(message.payload.body, reply.body)) { present = true; break; }
	}
	if (!present) {
		bepio_messaging_stats_t stats;
		bepio_messaging_stats(&stats);
		if (!bepio_messaging_submit(0, &reply, &id) && !stats.queued &&
		    !bepio_messaging_begin(true, &message) && message.id == id) {
			(void)bepio_messaging_complete(message.id, message.attempt, true, true);
		}
	}
	const bepio_message_payload_t outgoing = {
		.name = "Home", .phone = "13900139000", .body = "On my way. See you soon!"
	};
	present = false;
	for (size_t i = 0; !bepio_messaging_get_at(BEPIO_MESSAGE_OUTBOX, i, &message); i++) {
		if (!strcmp(message.payload.phone, outgoing.phone) &&
		    !strcmp(message.payload.body, outgoing.body)) { present = true; break; }
	}
	if (!present) {
		bepio_messaging_stats_t stats;
		bepio_messaging_stats(&stats);
		if (!bepio_messaging_submit(0, &outgoing, &id) && !stats.queued &&
		    !bepio_messaging_begin(true, &message) && message.id == id) {
			(void)bepio_messaging_complete(message.id, message.attempt, true, false);
		}
	}
	for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
		for (size_t j = 0; !bepio_messaging_get_at(BEPIO_MESSAGE_INBOX, j, &message); j++) {
			if (message.simulated && !strcmp(message.remote_id, keys[i]) &&
			    !strcmp(message.payload.phone, incoming[i].phone)) { fixture_count++; break; }
		}
	}
	const bepio_message_payload_t *sent[] = {&reply, &outgoing};
	for (size_t i = 0; i < sizeof(sent) / sizeof(sent[0]); i++) {
		for (size_t j = 0; !bepio_messaging_get_at(BEPIO_MESSAGE_OUTBOX, j, &message); j++) {
			if (!strcmp(message.payload.phone, sent[i]->phone) &&
			    !strcmp(message.payload.body, sent[i]->body)) { fixture_count++; break; }
		}
	}
	return fixture_count;
}
#endif
