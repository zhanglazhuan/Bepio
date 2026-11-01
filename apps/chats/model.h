#ifndef BEPIO_CHATS_MODEL_H_
#define BEPIO_CHATS_MODEL_H_

#include "messaging.h"
#include "apps/contacts/model.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef CONFIG_BEPIO_MESSAGES_DEMO
#define BEPIO_CHATS_MOCK_CONTACTS 3
#define BEPIO_CHATS_MOCK_MESSAGES 7
#else
#define BEPIO_CHATS_MOCK_CONTACTS 0
#define BEPIO_CHATS_MOCK_MESSAGES 0
#endif
#define BEPIO_CHATS_CONTACT_CAPACITY (CONFIG_BEPIO_CONTACTS_CAPACITY + BEPIO_CHATS_MOCK_CONTACTS)
#define BEPIO_CHATS_MESSAGE_CAPACITY (CONFIG_BEPIO_MESSAGES_INBOX_CAPACITY + CONFIG_BEPIO_MESSAGES_OUTBOX_CAPACITY + BEPIO_CHATS_MOCK_MESSAGES)

typedef struct {
	char name[CONTACTS_NAME_MAX + 1];
	char phone[CONTACTS_PHONE_MAX + 1];
	bool demo;
} bepio_chats_contact_t;

typedef struct {
	char body[BEPIO_MESSAGE_BODY_MAX + 1];
	bool outgoing;
	bool demo;
} bepio_chats_message_t;

typedef struct {
	uint32_t id;
	bepio_message_box_t box;
	int8_t mock_index; /* -1 refers to the messaging service. */
} bepio_chats_ref_t;

typedef struct {
	bepio_chats_contact_t contacts[BEPIO_CHATS_CONTACT_CAPACITY];
	size_t contact_count;
	bepio_chats_ref_t conversation[BEPIO_CHATS_MESSAGE_CAPACITY];
	size_t message_count;
	size_t selected;
} bepio_chats_model_t;

void bepio_chats_model_load(bepio_chats_model_t *model);
int bepio_chats_model_select(bepio_chats_model_t *model, size_t index);
int bepio_chats_model_contact_at(const bepio_chats_model_t *model, size_t index,
				bepio_chats_contact_t *contact);
int bepio_chats_model_message_at(const bepio_chats_model_t *model, size_t index,
				bepio_chats_message_t *message);

#endif
