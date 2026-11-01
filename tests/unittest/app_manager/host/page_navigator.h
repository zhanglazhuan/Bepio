#ifndef TEST_PAGE_NAVIGATOR_H_
#define TEST_PAGE_NAVIGATOR_H_

#include <stdbool.h>

typedef void *app_handle_t;
typedef void lv_event_t;
typedef void *(*page_builder_cb_t)(app_handle_t app, void *user_data);
typedef struct {
	int page_id;
	page_builder_cb_t builder;
} page_navigator_page_t;
typedef struct {
	page_navigator_page_t *registry;
	int registry_size, registry_count;
	int stack[10], stack_top, current_page;
	void *nav_ctx;
	app_handle_t app_handler;
} page_navigator_t;

void page_navigator_init(page_navigator_t *nav, page_navigator_page_t *registry,
	int max_pages, app_handle_t app);
int page_navigator_register_page(page_navigator_t *nav, int page_id,
	page_builder_cb_t builder);
void page_navigator_push(page_navigator_t *nav, int page_id);
void page_navigator_navigate_to(page_navigator_t *nav, app_handle_t app,
	int page_id, void *user_data);
bool page_navigator_navigate_pop(page_navigator_t *nav, app_handle_t app);
void page_navigator_deinit(page_navigator_t *nav);

#endif
