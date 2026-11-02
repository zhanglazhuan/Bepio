#ifdef ESP_PLATFORM
#include "sdkconfig.h"
#endif
#ifndef BEPIO_MESSAGES_MODEL_H_
#define BEPIO_MESSAGES_MODEL_H_
#include "messaging.h"

typedef enum {
	MESSAGES_PENDING_NONE,
	MESSAGES_PENDING_SEND,
	MESSAGES_PENDING_DELETE,
	MESSAGES_PENDING_LEAVE
} messages_pending_t;
typedef enum {
	MESSAGES_PAGE_HOME,
	MESSAGES_PAGE_BOX,
	MESSAGES_PAGE_DETAIL,
	MESSAGES_PAGE_COMPOSE,
	MESSAGES_PAGE_CONFIRM
} messages_page_t;
typedef struct {
	messages_page_t page, return_page;
	bepio_message_box_t box;
	uint32_t selected_id;
} messages_flow_t;
typedef struct {
	messages_flow_t flow;
	bool editing, dirty;
	uint32_t draft_id;
	bepio_message_payload_t editor;
	messages_pending_t pending;
	bepio_message_box_t pending_box;
	uint32_t pending_id;
} messages_model_t;

int messages_model_compose(messages_model_t *model, const bepio_message_payload_t *payload,
			   uint32_t draft_id, bool dirty);
int messages_model_text(messages_model_t *model, const char *text);
void messages_model_discard(messages_model_t *model);
void messages_model_contact_name(const bepio_message_t *message,
	bepio_message_box_t box, char *name, size_t capacity);
#endif
