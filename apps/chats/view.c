#include "view.h"
#include "pages/chats_main.h"
#include "lvgl.h"
#include <stdlib.h>

static bepio_chats_view_t view;

static void release_context(void *context)
{
	free(context);
}

static void on_screen_delete(lv_event_t *event)
{
	lv_async_call(release_context, lv_event_get_user_data(event));
}

int bepio_chats_view_open(void *root, void *group, bepio_chats_controller_t *controller)
{
	(void)root;
	(void)group;
	view.controller = controller;
	page_navigator_init(&view.page_nav, view.page_registry, 1, &view);
	page_navigator_register_page(&view.page_nav, 1, bepio_chats_main_build);
	page_navigator_navigate_to(&view.page_nav, &view, 1, NULL);
	return view.page_nav.current_page == 1 ? 0 : -1;
}

void bepio_chats_view_close(void)
{
	if (view.page_nav.nav_ctx) {
		lv_obj_t *screen = lv_screen_active();
		if (screen) {
			lv_obj_add_event_cb(screen, on_screen_delete,
				LV_EVENT_DELETE, view.page_nav.nav_ctx);
		} else {
			free(view.page_nav.nav_ctx);
		}
	}
	page_navigator_deinit(&view.page_nav);
	view.controller = NULL;
}
