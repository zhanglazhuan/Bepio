#include "apps/settings/view.h"
#include <stdint.h>

static uintptr_t context_id;
int settings_test_navigate_failure_page;

void page_navigator_init(page_navigator_t *nav, page_navigator_page_t *registry,
	int max_pages, app_handle_t app)
{
	*nav = (page_navigator_t){.registry = registry, .registry_size = max_pages,
		.stack_top = -1, .app_handler = app};
}
int page_navigator_register_page(page_navigator_t *nav, int page_id,
	page_builder_cb_t builder)
{
	if (nav->registry_count >= nav->registry_size) { return -1; }
	nav->registry[nav->registry_count++] = (page_navigator_page_t){page_id, builder};
	return 0;
}
void page_navigator_push(page_navigator_t *nav, int page_id)
{
	if (page_id > 0 && nav->stack_top < 9) { nav->stack[++nav->stack_top] = page_id; }
}
void page_navigator_navigate_to(page_navigator_t *nav, app_handle_t app,
	int page_id, void *user_data)
{
	(void)app; (void)user_data;
	if (page_id == settings_test_navigate_failure_page) { return; }
	nav->current_page = page_id;
	nav->nav_ctx = (void *)++context_id;
}
bool page_navigator_navigate_pop(page_navigator_t *nav, app_handle_t app)
{
	if (nav->stack_top < 0) { return false; }
	page_navigator_navigate_to(nav, app, nav->stack[nav->stack_top--], NULL);
	return true;
}
void page_navigator_deinit(page_navigator_t *nav) { nav->current_page = 0; }

void settings_view_init(settings_view_t *view, void *root, void *group)
{
	*view = (settings_view_t){.available = true, .root = root, .group = group};
	page_navigator_init(&view->page_nav, view->page_registry, 10, view);
}
void settings_view_deinit(settings_view_t *view) { *view = (settings_view_t){0}; }
void bepio_ui_preferences_apply(const char *zone, bool hour_24, bool power_save)
{
	(void)zone; (void)hour_24; (void)power_save;
}
