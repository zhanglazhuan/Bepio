#ifndef BEPIO_CONTACTS_VIEW_H_
#define BEPIO_CONTACTS_VIEW_H_

#include <stdbool.h>
#include "model.h"
#include "page_navigator.h"

typedef struct bepio_contacts_page_context bepio_contacts_page_context_t;

typedef struct {
	bool available;
	void *root;
	void *group;
	contacts_model_t *model;
	page_navigator_t page_nav;
	page_navigator_page_t page_registry[5];
} contacts_view_t;

void contacts_view_init(contacts_view_t *view, void *root, void *group);
void contacts_view_deinit(contacts_view_t *view);

#endif
