#ifndef BEPIO_MESSAGES_VIEW_H_
#define BEPIO_MESSAGES_VIEW_H_
#include "model.h"

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
	bool available;
	void *root, *group;
	void *controller; /* Bound by controller_init; view callbacks use public actions. */
} messages_view_t;

void messages_view_init(messages_view_t *view, void *root, void *group, bool editing);
void messages_view_deinit(messages_view_t *view);
void messages_view_render(const messages_view_t *view);
#endif
