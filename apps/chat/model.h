#ifndef BEPIO_CHAT_MODEL_H_
#define BEPIO_CHAT_MODEL_H_

#include "messaging.h"
#include "apps/contacts/model.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define BEPIO_CHAT_MOCK_CONTACTS 3
#define BEPIO_CHAT_MOCK_MESSAGES 7
#define BEPIO_CHAT_CONTACT_CAPACITY (CONFIG_BEPIO_CONTACTS_CAPACITY + BEPIO_CHAT_MOCK_CONTACTS)
#define BEPIO_CHAT_MESSAGE_CAPACITY (CONFIG_BEPIO_MESSAGES_INBOX_CAPACITY + CONFIG_BEPIO_MESSAGES_OUTBOX_CAPACITY + BEPIO_CHAT_MOCK_MESSAGES)

typedef struct {
	char name[CONTACTS_NAME_MAX + 1];
	char phone[CONTACTS_PHONE_MAX + 1];
	bool demo;
} bepio_chat_contact_t;

typedef struct {
	char body[BEPIO_MESSAGE_BODY_MAX + 1];
	bool outgoing;
	bool demo;
} bepio_chat_message_t;

typedef struct {
	uint32_t id;
	bepio_message_box_t box;
	int8_t mock_index; /* -1 refers to the messaging service. */
} bepio_chat_ref_t;

typedef struct {
	bepio_chat_contact_t contacts[BEPIO_CHAT_CONTACT_CAPACITY];
	size_t contact_count;
	bepio_chat_ref_t conversation[BEPIO_CHAT_MESSAGE_CAPACITY];
	size_t message_count;
	size_t selected;
} bepio_chat_model_t;

void bepio_chat_model_load(bepio_chat_model_t *model);
int bepio_chat_model_select(bepio_chat_model_t *model, size_t index);
int bepio_chat_model_contact_at(const bepio_chat_model_t *model, size_t index,
				bepio_chat_contact_t *contact);
int bepio_chat_model_message_at(const bepio_chat_model_t *model, size_t index,
				bepio_chat_message_t *message);

#endif
