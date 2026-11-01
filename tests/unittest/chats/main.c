#include "apps/chats/controller.h"
#include "apps/contacts/app.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

static contacts_controller_t contacts_controller;
static const contacts_entry_t contacts[] = {
	{.id = 1, .name = "Local", .phone = "13600000000"},
};
static const bepio_message_t incoming[] = {
	{.id = 1, .payload = {.name = "Alex", .phone = "13800138000", .body = "真实收到"}},
};
static const bepio_message_t outgoing[] = {
	{.id = 2, .payload = {.name = "Alex", .phone = "13800138000", .body = "真实回复"}},
};
static char spoken[BEPIO_MESSAGE_BODY_MAX + 1];
static bepio_message_t submitted;
static unsigned started, stopped, cancelled;
static const char *transcript = "发给你了";

static int voice_start(void *context)
{
	assert(context == &started);
	started++;
	return 0;
}
static int voice_stop(void *context, char *text, size_t capacity)
{
	assert(context == &started);
	stopped++;
	snprintf(text, capacity, "%s", transcript);
	return 0;
}
static void voice_cancel(void *context)
{
	assert(context == &started);
	cancelled++;
}
static const bepio_chats_voice_input_t voice_input = {
	.start = voice_start, .stop = voice_stop, .cancel = voice_cancel,
};

contacts_controller_t *contacts_app_get_controller(void) { return &contacts_controller; }
int contacts_controller_lookup_at(contacts_controller_t *controller, size_t index, contacts_entry_t *entry)
{
	assert(controller == &contacts_controller);
	if (index >= sizeof(contacts) / sizeof(contacts[0])) { return -ENOENT; }
	*entry = contacts[index];
	return 0;
}
int bepio_messaging_get_at(bepio_message_box_t box, size_t index, bepio_message_t *message)
{
	if (box == BEPIO_MESSAGE_OUTBOX && index == 1 && submitted.id) {
		*message = submitted;
		return 0;
	}
	if (index) { return -ENOENT; }
	*message = box == BEPIO_MESSAGE_INBOX ? incoming[0] : outgoing[0];
	return 0;
}
int bepio_messaging_get(bepio_message_box_t box, uint32_t id, bool mark_read,
			bepio_message_t *message)
{
	assert(!mark_read);
	if (box == BEPIO_MESSAGE_INBOX && id == 1) { *message = incoming[0]; return 0; }
	if (box == BEPIO_MESSAGE_OUTBOX && id == 2) { *message = outgoing[0]; return 0; }
	if (box == BEPIO_MESSAGE_OUTBOX && id == submitted.id && submitted.id) {
		*message = submitted;
		return 0;
	}
	return -ENOENT;
}
int bepio_messaging_submit(uint32_t draft_id, const bepio_message_payload_t *payload, uint32_t *id)
{
	assert(draft_id == 0 && payload && id);
	assert(!strcmp(payload->phone, contacts[0].phone));
	assert(!strcmp(payload->body, transcript));
	submitted = (bepio_message_t){.id = 3, .state = BEPIO_MESSAGE_QUEUED, .payload = *payload};
	*id = submitted.id;
	return 0;
}
int bepio_messages_voice_play(const char *text)
{
	strcpy(spoken, text);
	return 0;
}

int main(void)
{
	static bepio_chats_model_t model;
	bepio_chats_controller_t controller;
	bepio_chats_controller_open(&controller, &model);
	assert(model.contact_count == 4);
	bepio_chats_contact_t contact;
	assert(!bepio_chats_controller_contact_at(&controller, 0, &contact));
	assert(!strcmp(contact.name, "Local") && !contact.demo);
	assert(bepio_chats_controller_selected(&controller) == 1);
	assert(!bepio_chats_controller_contact_at(&controller, 1, &contact));
	assert(!strcmp(contact.name, "Alex") && contact.demo);
	bepio_chats_message_t message;
	assert(!bepio_chats_controller_message_at(&controller, 0, &message));
	assert(!strcmp(message.body, "真实收到") && !message.outgoing && !message.demo);
	assert(!bepio_chats_controller_message_at(&controller, 1, &message));
	assert(!strcmp(message.body, "真实回复") && message.outgoing && !message.demo);
	assert(!bepio_chats_controller_message_at(&controller, 2, &message));
	assert(!message.outgoing && message.demo);
	assert(!bepio_chats_controller_message_at(&controller, 3, &message));
	assert(message.outgoing && message.demo);
	assert(!bepio_chats_controller_play(&controller, 3));
	assert(!strcmp(spoken, "有空，我们三点见。"));
	assert(!bepio_chats_controller_select(&controller, 0));
	assert(bepio_chats_controller_message_at(&controller, 0, &message) == -ENOENT);
	assert(bepio_chats_controller_record_start(&controller) == -ENOTSUP);
	assert(!bepio_chats_controller_set_voice_input(&controller, &voice_input, &started));
	assert(!bepio_chats_controller_record_start(&controller));
	assert(started == 1 && controller.recording && !submitted.id);
	uint32_t queued_id = 0;
	assert(!bepio_chats_controller_record_stop(&controller, &queued_id));
	assert(queued_id == 3 && stopped == 1 && !controller.recording);
	assert(bepio_chats_controller_record_stop(&controller, &queued_id) == -EINVAL);
	assert(!bepio_chats_controller_message_at(&controller, 0, &message));
	assert(message.outgoing && !strcmp(message.body, transcript));
	transcript = "";
	assert(!bepio_chats_controller_record_start(&controller));
	assert(bepio_chats_controller_record_stop(&controller, &queued_id) == -EINVAL);
	assert(!bepio_chats_controller_record_start(&controller));
	bepio_chats_controller_record_cancel(&controller);
	assert(cancelled == 1 && !controller.recording);
	transcript = "发给你了";
	assert(!bepio_chats_controller_select(&controller, 2));
	assert(!bepio_chats_controller_message_at(&controller, 0, &message));
	assert(!message.outgoing && message.demo);
	assert(!bepio_chats_controller_message_at(&controller, 1, &message));
	assert(message.outgoing && message.demo);
	assert(bepio_chats_controller_select(&controller, 99) == -ENOENT);
	assert(bepio_chats_controller_selected(&controller) == 2);
	bepio_chats_controller_close(&controller);
	assert(bepio_chats_controller_play(&controller, 0) == -EACCES);
	puts("PASS: Chats mock and real conversations, direction, selection, playback");
	return 0;
}
