#include "apps/notifications/controller.h"
#include "apps/notifications/demo.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
int main(void)
{
	bepio_notifications_init();
	bepio_notification_payload_t p = {.id = "one", .from = "101", .app = "Chat",
		.title = "Ada", .text = "Hello", .datetime = "2026-11-02T01:00:00Z"};
	bepio_notification_t item;
	assert(bepio_notifications_get_at(0, &item) == -ENOENT);
	assert(!bepio_notifications_receive(&p));
	assert(!bepio_notifications_get_at(0, &item) && !item.read);
	uint32_t first = item.id;
	bepio_notifications_controller_t c;
	bepio_notifications_controller_open(&c);
	assert(!bepio_notifications_controller_select(&c, first));
	uint32_t revision = bepio_notifications_revision();
	assert(!bepio_notifications_receive(&p));
	assert(bepio_notifications_revision() == revision);
	assert(!bepio_notifications_get_at(0, &item) && item.read);
	strcpy(p.text, "Updated");
	assert(!bepio_notifications_receive(&p));
	assert(!bepio_notifications_get_at(0, &item) && item.id == first && !item.read);
	strcpy(p.app, "Mail");
	assert(!bepio_notifications_receive(&p));
	assert(!bepio_notifications_get_at(0, &item) && item.id != first);
	strcpy(p.from, "102");
	assert(!bepio_notifications_receive(&p));
	assert(!bepio_notifications_get_at(2, &item) && item.id == first);
	assert(bepio_notifications_controller_back(&c));
	assert(!bepio_notifications_controller_back(&c));
	c.active = false;
	assert(bepio_notifications_controller_select(&c, first) == -EACCES);
	bepio_notifications_controller_open(&c);
	assert(!bepio_notifications_controller_mark_read(&c, first));
	assert(!c.selected);
	assert(!bepio_notifications_controller_select(&c, first));
	assert(!bepio_notifications_controller_delete(&c, first));
	assert(!c.selected);
	assert(bepio_notifications_get(first, false, &item) == -ENOENT);
	for (unsigned i = 0; i < BEPIO_NOTIFICATIONS_CAPACITY + 3; i++) {
		snprintf(p.id, sizeof(p.id), "notice-%u", i);
		assert(!bepio_notifications_receive(&p));
	}
	assert(!bepio_notifications_get_at(BEPIO_NOTIFICATIONS_CAPACITY - 1, &item));
	assert(!strcmp(item.payload.id, "notice-3"));
	assert(bepio_notifications_get_at(BEPIO_NOTIFICATIONS_CAPACITY, &item) == -ENOENT);
	memset(p.title, 'X', sizeof(p.title));
	assert(bepio_notifications_receive(&p) == -EINVAL);
	assert(bepio_notifications_receive(NULL) == -EINVAL);
	bepio_notifications_init();
	assert(bepio_notifications_demo_seed() == 4);
	assert(bepio_notifications_demo_seed() == 4);
	assert(!bepio_notifications_get_at(0, &item));
	assert(!strcmp(item.payload.app, "System"));
	assert(bepio_notifications_get_at(4, &item) == -ENOENT);
	puts("PASS: notification update/dedup, source isolation, read state, bounded eviction and controller lifecycle");
}
