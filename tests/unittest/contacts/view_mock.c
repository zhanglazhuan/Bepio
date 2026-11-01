/* Test double for controller tests. GUI is exercised by contacts_preview. */
#include "apps/contacts/view.h"
void contacts_view_init(contacts_view_t *view, void *root, void *group)
{
	*view = (contacts_view_t){.available = true,
		.root = root, .group = group};
}
void contacts_view_deinit(contacts_view_t *view) { *view = (contacts_view_t){0}; }
