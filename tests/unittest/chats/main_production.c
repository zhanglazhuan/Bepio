#include "apps/chats/controller.h"
#include "apps/contacts/app.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>

static contacts_controller_t contacts_controller;
contacts_controller_t *contacts_app_get_controller(void) { return &contacts_controller; }
int contacts_controller_lookup_at(contacts_controller_t *controller, size_t index, contacts_entry_t *entry)
{
	(void)controller; (void)index; (void)entry;
	return -ENOENT;
}
int bepio_messaging_get_at(bepio_message_box_t box, size_t index, bepio_message_t *message)
{
	(void)box; (void)index; (void)message;
	return -ENOENT;
}
int bepio_messaging_get(bepio_message_box_t box, uint32_t id, bool mark_read,
			bepio_message_t *message)
{
	(void)box; (void)id; (void)mark_read; (void)message;
	return -ENOENT;
}
int bepio_messages_voice_play(const char *text) { (void)text; return -ENOTSUP; }
int bepio_messaging_submit(uint32_t draft_id, const bepio_message_payload_t *payload, uint32_t *id)
{
	(void)draft_id; (void)payload; (void)id;
	return -ENOTSUP;
}

int main(void)
{
	static bepio_chats_model_t model;
	bepio_chats_controller_t controller;
	bepio_chats_controller_open(&controller, &model);
	assert(model.contact_count == 0 && model.message_count == 0);
	assert(bepio_chats_controller_select(&controller, 0) == -ENOENT);
	bepio_chats_controller_close(&controller);
	puts("PASS: production Chats contains no demonstration contacts or messages");
	return 0;
}
