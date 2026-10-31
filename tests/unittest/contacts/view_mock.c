/* Test double for controller tests. GUI is exercised by contacts_preview. */
#include "apps/contacts/view.h"
void contacts_view_init(contacts_view_t *view, void *root, void *group)
{
	*view = (contacts_view_t){.page = CONTACTS_PAGE_LIST, .available = true,
		.root = root, .group = group};
}
void contacts_view_deinit(contacts_view_t *view) { *view = (contacts_view_t){0}; }
void contacts_view_render(contacts_view_t *view, const contacts_model_t *model)
{
	(void)view; (void)model;
}
