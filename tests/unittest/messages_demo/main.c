#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "messaging.h"
#include "demo.h"

int main(void)
{
	assert(bepio_messaging_init() == 0);
	bepio_message_payload_t saved = {.name = "Saved", .phone = "13600000000", .body = "Already here"};
	uint32_t id;
	assert(bepio_messaging_receive("saved-1", &saved, 0, false, &id) == 0);
	assert(bepio_messages_demo_seed() == 5);
	bepio_messaging_stats_t stats;
	bepio_messaging_stats(&stats);
	assert(stats.inbox == 4 && stats.outbox == 2);
	bepio_message_t message;
	assert(bepio_messaging_get(BEPIO_MESSAGE_INBOX, id, false, &message) == 0);
	assert(!strcmp(message.payload.body, "Already here"));
	assert(bepio_messages_demo_seed() == 5);
	bepio_messaging_stats(&stats);
	assert(stats.inbox == 4 && stats.outbox == 2);
	puts("PASS: demo conversations coexist with existing messages");
	return 0;
}
