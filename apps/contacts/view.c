#include "view.h"
#include "pages/contacts_list.h"
#include "pages/contacts_detail.h"
#include "pages/contacts_edit.h"
#include "lvgl.h"
#include <stdlib.h>

static void release_context(void *context)
{
	free(context);
}

static void release_on_delete(lv_event_t *event)
{
	lv_async_call(release_context, lv_event_get_user_data(event));
}

void contacts_view_init(contacts_view_t *view, void *root, void *group)
{
	*view = (contacts_view_t){.available = true, .root = root, .group = group};
	page_navigator_init(&view->page_nav, view->page_registry, 3, view);
	page_navigator_register_page(&view->page_nav, CONTACTS_PAGE_LIST + 1, bepio_contacts_list_build);
	page_navigator_register_page(&view->page_nav, CONTACTS_PAGE_DETAIL + 1, bepio_contacts_detail_build);
	page_navigator_register_page(&view->page_nav, CONTACTS_PAGE_EDIT + 1, bepio_contacts_edit_build);
}

void contacts_view_deinit(contacts_view_t *view)
{
	if (view->page_nav.nav_ctx) {
		lv_obj_t *screen = lv_screen_active();
		if (screen) { lv_obj_add_event_cb(screen, release_on_delete, LV_EVENT_DELETE, view->page_nav.nav_ctx); }
		else { free(view->page_nav.nav_ctx); }
	}
	page_navigator_deinit(&view->page_nav);
	view->available = false;
	view->root = NULL;
	view->model = NULL;
}
