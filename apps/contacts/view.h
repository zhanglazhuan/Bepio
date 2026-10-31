#ifndef BEPIO_CONTACTS_VIEW_H_
#define BEPIO_CONTACTS_VIEW_H_

#include <stdbool.h>
#include "model.h"

typedef enum {
	CONTACTS_PAGE_LIST, 
	CONTACTS_PAGE_DETAIL, 
	CONTACTS_PAGE_EDIT,
	CONTACTS_PAGE_DELETE_CONFIRM, 
	CONTACTS_PAGE_DISCARD_CONFIRM,
} contacts_page_t;

typedef struct {
	contacts_page_t page;
	uint32_t selected_id;
	bool available;
	void *root;
	void *group;
	contacts_page_t edit_return;
	contacts_entry_t draft;
	bool dirty;
	bool search_editing;
	char search[CONTACTS_NAME_MAX + 1];
} contacts_view_t;

void contacts_view_init(contacts_view_t *view, void *root, void *group);
void contacts_view_deinit(contacts_view_t *view);
void contacts_view_render(contacts_view_t *view, const contacts_model_t *model);

#endif
