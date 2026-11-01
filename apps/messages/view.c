#include "view.h"
#include "controller.h"
#if defined(ESP_PLATFORM) || defined(BEPIO_MESSAGES_PREVIEW)
#include "pages/messages_list.h"
#include "pages/messages_detail.h"
#include "pages/messages_compose.h"
#include "pages/messages_confirm.h"
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
#endif

void messages_view_init(messages_view_t *view, void *root, void *group)
{
	*view = (messages_view_t){.available = true, .root = root, .group = group};
#if defined(ESP_PLATFORM) || defined(BEPIO_MESSAGES_PREVIEW)
	page_navigator_init(&view->page_nav, view->page_registry, 5, view);
	page_navigator_register_page(&view->page_nav, MESSAGES_PAGE_HOME + 1, messages_list_build);
	page_navigator_register_page(&view->page_nav, MESSAGES_PAGE_BOX + 1, messages_list_build);
	page_navigator_register_page(&view->page_nav, MESSAGES_PAGE_DETAIL + 1, messages_detail_build);
	page_navigator_register_page(&view->page_nav, MESSAGES_PAGE_COMPOSE + 1, messages_compose_build);
	page_navigator_register_page(&view->page_nav, MESSAGES_PAGE_CONFIRM + 1, messages_confirm_build);
#endif
}

void messages_view_deinit(messages_view_t *view)
{
#if defined(ESP_PLATFORM) || defined(BEPIO_MESSAGES_PREVIEW)
	if (view->page_nav.nav_ctx) {
		lv_obj_t *screen = lv_screen_active();
		if (screen) { lv_obj_add_event_cb(screen, release_on_delete, LV_EVENT_DELETE, view->page_nav.nav_ctx); }
		else { free(view->page_nav.nav_ctx); }
	}
	page_navigator_deinit(&view->page_nav);
#endif
	*view = (messages_view_t){0};
}
