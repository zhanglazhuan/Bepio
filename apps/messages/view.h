#ifndef BEPIO_MESSAGES_VIEW_H_
#define BEPIO_MESSAGES_VIEW_H_
#include "model.h"
#include "page_navigator.h"

typedef struct messages_page_context messages_page_context_t;
typedef struct {
	bool available;
	void *root, *group;
	void *controller; /* Bound by controller_init; view callbacks use public actions. */
	page_navigator_t page_nav;
	page_navigator_page_t page_registry[5];
} messages_view_t;

void messages_view_init(messages_view_t *view, void *root, void *group);
void messages_view_deinit(messages_view_t *view);
#endif
