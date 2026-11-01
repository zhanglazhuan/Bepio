#include "settings_page_context.h"
#include <stdlib.h>

static void refresh(lv_timer_t *timer)
{
	settings_page_context_t *ctx = lv_timer_get_user_data(timer);
	if (ctx->refresh) { ctx->refresh(ctx); }
}

static void page_deleted(lv_event_t *event)
{
	settings_page_context_t *ctx = lv_event_get_user_data(event);
	if (ctx->timer) { lv_timer_delete(ctx->timer); ctx->timer = NULL; }
}

settings_page_context_t *bepio_settings_page_alloc(size_t size, settings_view_t *view,
	const settings_model_t *model)
{
	if (size < sizeof(settings_page_context_t)) { return NULL; }
	settings_page_context_t *ctx = calloc(1, size);
	if (ctx) { ctx->view = view; ctx->model = model; ctx->root = view->root; }
	return ctx;
}

void bepio_settings_page_prepare(settings_view_t *view, Page page,
	const char *title)
{
	view->root = page.container;
	lv_obj_set_layout(page.container, LV_LAYOUT_NONE);
	lv_obj_set_style_pad_all(page.container, 0, 0);
	lv_obj_set_scroll_dir(page.container, LV_DIR_VER);
	lv_page_set_title(page.container, title, 40);
}

lv_obj_t *bepio_settings_page_finish(settings_view_t *view, Page page,
	settings_page_context_t *ctx)
{
	if (!ctx) { lv_obj_delete(page.screen); return NULL; }
	view->page_nav.nav_ctx = ctx;
	lv_obj_add_event_cb(page.screen, page_deleted, LV_EVENT_DELETE, ctx);
	if (ctx->refresh && ctx->refresh_period_ms) {
		ctx->timer = lv_timer_create(refresh, ctx->refresh_period_ms, ctx);
	}
	return page.screen;
}
